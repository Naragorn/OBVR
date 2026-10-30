#include "game/Dismember.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/ActorRoster.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/RagdollHold.h"
#include "game/WalkersLink.h"

namespace obvr::game {
namespace {

constexpr UInt32 kCollisionObjectOf = 0x0047FAC0;  // cdecl(node)
constexpr UInt8 kCollisionObjectOfBytes[] = {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xB0, 0xA8};
constexpr UInt32 kConstraintRemove = 0x008A0760;  // bhkConstraint vtable +0x60, thiscall(this) -> bool
constexpr UInt8 kConstraintRemoveBytes[] = {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x8B, 0x50, 0x58};
constexpr UInt32 kConstraintRemoveSlot = 0x60;
constexpr UInt32 kListRemoveNode = 0x0067A850;  // thiscall(node): the node's item out of the list
constexpr UInt8 kListRemoveNodeBytes[] = {0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x7E, 0x04, 0x85, 0xFF};
constexpr UInt32 kCollisionBody = 0x10;       // collision object -> bhkRigidBody
constexpr UInt32 kBodyConstraintList = 0x10;  // bhkRigidBody -> its list's inline first node
constexpr UInt32 kConstraintInstance = 0x08;  // bhkConstraint -> hkConstraintInstance
constexpr UInt32 kInstanceOwner = 0x08;       // non-null while in a world
constexpr UInt32 kInstanceEntityA = 0x10;
constexpr UInt32 kInstanceEntityB = 0x14;
constexpr UInt32 kEntityWrapper = 0x0C;

constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kMaxBones = 128;
constexpr UInt32 kMaxDepth = 48;
constexpr float kCollapsedScale = 0.001f;

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

bool Verified() {
	static int verified = 0;
	if (verified == 0) {
		const bool ok = mem::Verify(kCollisionObjectOf, kCollisionObjectOfBytes, sizeof(kCollisionObjectOfBytes)) &&
		                mem::Verify(kConstraintRemove, kConstraintRemoveBytes, sizeof(kConstraintRemoveBytes)) &&
		                mem::Verify(kListRemoveNode, kListRemoveNodeBytes, sizeof(kListRemoveNodeBytes));
		verified = ok ? 1 : -1;
		OBVR_LOG("Dismember: the engine's joint and list functions %s", ok ? "as read" : "NOT as read - no cut");
	}
	return verified > 0;
}

struct alignas(16) FakeNode {
	UInt8 bytes[0xC0];
};

NiAVObject* AsNode(FakeNode& f) { return reinterpret_cast<NiAVObject*>(f.bytes); }

void MakeFrom(FakeNode& fake, const NiAVObject* real) {
	std::memset(fake.bytes, 0, sizeof(fake.bytes));
	std::memcpy(fake.bytes, real, sizeof(NiAVObject));
	NiAVObject* const node = AsNode(fake);
	node->parent = nullptr;
	node->refCount = 0x40000000u;
	node->controller = nullptr;
	node->extraDataList = nullptr;
	node->extraDataListLen = 0;
	node->extraDataListCapacity = 0;
}

constexpr UInt32 kCutsMost = 16;

struct Cut {
	UInt32 actor = 0;
	UInt32 root = 0;
	NiAVObject* head = nullptr;
	NiAVObject* neck = nullptr;  // Neck1, or Spine2 without one
	NiAVObject* neckTop = nullptr;   // Bip01 Neck1, or null (counted as the head's bones)
	NiAVObject* neckBase = nullptr;  // Bip01 Neck, or null
	NiPoint3 cutInNeck{0.0f, 0.0f, 0.0f};  // the head's origin in the neck's frame at the cut
	FakeNode bodySide{};  // the body's shapes' head bone: at the cut, following the neck
	FakeNode headSide{};  // the head's shapes' other bones: at the head's origin
};
Cut g_cuts[kCutsMost];

using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

NiAVObject* Named(UInt32 root, const char* name) {
	const UInt32 vtable = LooksLikeObject(root) ? Read(root) : 0;
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + addr::kNiAVObjectGetObjectVtableOffset) : 0;
	if (fn < addr::kTextStart || fn >= addr::kTextEnd) {
		return nullptr;
	}
	auto* const node =
		static_cast<NiAVObject*>(reinterpret_cast<GetObjectFn>(fn)(reinterpret_cast<void*>(root), nullptr, name));
	return LooksLikeObject(reinterpret_cast<UInt32>(node)) ? node : nullptr;
}

bool IsUnder(const NiAVObject* node, const NiAVObject* ancestor) {
	const NiAVObject* at = node;
	for (UInt32 i = 0; i < kMaxDepth && LooksLikeObject(reinterpret_cast<UInt32>(at)); ++i) {
		if (at == ancestor) {
			return true;
		}
		at = at->parent;
	}
	return false;
}

UInt32 WrapperOf(const NiAVObject* node) {
	using CollisionOfFn = UInt32(__cdecl*)(const NiAVObject* node);
	const UInt32 collision = reinterpret_cast<CollisionOfFn>(kCollisionObjectOf)(node);
	return LooksLikeObject(collision) ? Read(collision + kCollisionBody) : 0;
}

// The head's joints to Spine2 taken out of the world and out of its list.
UInt32 ReleaseNeckJoints(const NiAVObject* head, const NiAVObject* spine2) {
	const UInt32 wrapper = WrapperOf(head);
	const UInt32 spineWrapper = WrapperOf(spine2);
	if (!LooksLikeObject(wrapper) || !LooksLikeObject(spineWrapper)) {
		return 0;
	}
	using LockFn = void(__thiscall*)(void* lock);
	using RemoveFn = UInt8(__thiscall*)(void* constraint);
	using ListRemoveFn = void(__thiscall*)(void* node);
	void* const lock = reinterpret_cast<void*>(kHavokLock);
	UInt32 released = 0;
	reinterpret_cast<LockFn>(kHavokLockEnter)(lock);
	for (UInt32 pass = 0; pass < 8; ++pass) {
		UInt32 node = wrapper + kBodyConstraintList;
		UInt32 hit = 0;
		for (UInt32 i = 0; i < 32 && node != 0; ++i) {
			const UInt32 item = Read(node);
			if (LooksLikeObject(item) && LooksLikeObject(Read(item)) &&
			    Read(Read(item) + kConstraintRemoveSlot) == kConstraintRemove) {
				const UInt32 instance = Read(item + kConstraintInstance);
				if (LooksLikeObject(instance)) {
					const UInt32 a = Read(instance + kInstanceEntityA);
					const UInt32 b = Read(instance + kInstanceEntityB);
					const UInt32 aw = LooksLikeObject(a) ? Read(a + kEntityWrapper) : 0;
					const UInt32 bw = LooksLikeObject(b) ? Read(b + kEntityWrapper) : 0;
					if (aw == spineWrapper || bw == spineWrapper) {
						hit = node;
						break;
					}
				}
			}
			const UInt32 next = Read(node + 4);
			node = LooksLikeObject(next) ? next : 0;
		}
		if (hit == 0) {
			break;
		}
		const UInt32 item = Read(hit);
		const UInt32 instance = Read(item + kConstraintInstance);
		InterlockedIncrement(reinterpret_cast<volatile LONG*>(item + 4));  // kept: a queued removal may still read it
		if (Read(instance + kInstanceOwner) != 0) {
			reinterpret_cast<RemoveFn>(kConstraintRemove)(reinterpret_cast<void*>(item));
		}
		reinterpret_cast<ListRemoveFn>(kListRemoveNode)(reinterpret_cast<void*>(hit));
		++released;
	}
	reinterpret_cast<LockFn>(kHavokLockLeave)(lock);
	return released;
}

struct SwapCount {
	char names[320] = {};
	UInt32 shapes = 0;
	UInt32 bodyBones = 0;
	UInt32 headBones = 0;
};

void SwapShape(UInt32 geometry, bool headSide, Cut& c, SwapCount& n) {
	const UInt32 skin = Read(geometry + kGeometrySkinInstance);
	if (!LooksLikeObject(skin)) {
		return;
	}
	const UInt32 data = Read(skin + kSkinInstanceData);
	const UInt32 bones = Read(skin + kSkinInstanceBones);
	if (!LooksLikeObject(data) || !LooksLikeObject(bones)) {
		return;
	}
	const UInt32 count = Read(data + kSkinDataBoneCount);
	if (count == 0 || count > kMaxBones) {
		return;
	}
	UInt32* const bone = reinterpret_cast<UInt32*>(bones);
	const UInt32 bodyFake = reinterpret_cast<UInt32>(c.bodySide.bytes);
	const UInt32 headFake = reinterpret_cast<UInt32>(c.headSide.bytes);
	bool any = false;
	for (UInt32 i = 0; i < count; ++i) {
		const NiAVObject* const b = reinterpret_cast<const NiAVObject*>(bone[i]);
		if (!LooksLikeObject(bone[i]) || bone[i] == bodyFake || bone[i] == headFake) {
			continue;
		}
		const bool isHead = b == c.head;
		const bool underHead = !isHead && IsUnder(b, c.head);
		if (headSide ? SwapBoneOnHeadSide(isHead, underHead) : SwapBoneOnBodySide(isHead, underHead)) {
			bone[i] = headSide ? headFake : bodyFake;
			++(headSide ? n.headBones : n.bodyBones);
			any = true;
		}
	}
	n.shapes += any ? 1 : 0;
}

// How many of a shape's bones are the head's - the head, what hangs under
// it, Neck and Neck1 - against all it has (both 0 when its skin is not read).
void CountShapeBones(UInt32 geometry, const Cut& c, UInt32& headBones, UInt32& bones) {
	headBones = 0;
	bones = 0;
	const UInt32 skin = Read(geometry + kGeometrySkinInstance);
	if (!LooksLikeObject(skin)) {
		return;
	}
	const UInt32 data = Read(skin + kSkinInstanceData);
	const UInt32 list = Read(skin + kSkinInstanceBones);
	if (!LooksLikeObject(data) || !LooksLikeObject(list)) {
		return;
	}
	const UInt32 count = Read(data + kSkinDataBoneCount);
	if (count == 0 || count > kMaxBones) {
		return;
	}
	const UInt32* const bone = reinterpret_cast<const UInt32*>(list);
	for (UInt32 i = 0; i < count; ++i) {
		if (!LooksLikeObject(bone[i])) {
			continue;
		}
		const NiAVObject* const b = reinterpret_cast<const NiAVObject*>(bone[i]);
		++bones;
		if (b == c.head || IsUnder(b, c.head) || (b == c.neckTop && c.neckTop != nullptr) ||
		    (b == c.neckBase && c.neckBase != nullptr)) {
			++headBones;
		}
	}
}

// Without its bones read: a shape is the head's when its bound's middle lies nearer the head's
// origin than Spine2's (the head mesh carries the neck down to the
// collarbones and is not under the head node; the body's shapes reach from
// the hips) - derived from the vanilla meshes, logged by name per cut.
bool HeadShape(const NiAVObject* shape, const NiPoint3& head, const NiPoint3& spine2) {
	const NiPoint3 c = shape->worldBound.center;
	return (c - head).LengthSquared() < (c - spine2).LengthSquared();
}

void SwapSkins(const NiAVObject* node, bool underHead, Cut& c, const NiPoint3& spine2, SwapCount& n,
               UInt32 depth) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node)) || depth > kMaxDepth) {
		return;
	}
	const bool inHead = underHead || node == c.head;
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] != nullptr) {
				SwapSkins(children[i], inHead, c, spine2, n, depth + 1);
			}
		}
		return;
	}
	if (className != nullptr &&
	    (std::strncmp(className, "NiTriShape@", 11) == 0 || std::strncmp(className, "NiTriStrips@", 12) == 0)) {
		UInt32 headBones = 0;
		UInt32 bones = 0;
		CountShapeBones(reinterpret_cast<UInt32>(node), c, headBones, bones);
		const bool headSide =
			inHead || (bones > 0 ? ShapeIsHeadSide(headBones, bones) : HeadShape(node, c.head->worldTransform.pos, spine2));
		const UInt32 before = n.shapes;
		SwapShape(reinterpret_cast<UInt32>(node), headSide, c, n);
		if (n.shapes != before && std::strlen(n.names) < sizeof(n.names) - 40) {
			const UInt32 len = static_cast<UInt32>(std::strlen(n.names));
			std::snprintf(n.names + len, sizeof(n.names) - len, "%s%s(%s, %u/%u)", len > 0 ? ", " : "",
			              LooksLikeObject(reinterpret_cast<UInt32>(node->name) & ~3u) ? node->name : "?",
			              headSide ? "head" : "body", headBones, bones);
		}
	}
}

void Place(Cut& c) {
	NiAVObject* const body = AsNode(c.bodySide);
	NiAVObject* const head = AsNode(c.headSide);
	const NiTransform& neck = c.neck->worldTransform;
	body->worldTransform.rot = neck.rot;
	body->worldTransform.pos = neck.pos + neck.rot * (c.cutInNeck * neck.scale);
	body->worldTransform.scale = kCollapsedScale;
	head->worldTransform.rot = c.head->worldTransform.rot;
	head->worldTransform.pos = c.head->worldTransform.pos;
	head->worldTransform.scale = kCollapsedScale;
}

Cut* Find(UInt32 actor) {
	for (Cut& c : g_cuts) {
		if (c.actor == actor && actor != 0) {
			return &c;
		}
	}
	return nullptr;
}

}  // namespace

bool LoadedActorIsDead(UInt32 actor) {
	RosterActor actors[64];
	const UInt32 n = CollectLoadedActors(actors, 64);
	for (UInt32 i = 0; i < n; ++i) {
		if (actors[i].ref == actor) {
			return actors[i].dead;
		}
	}
	return false;
}

UInt32 CollectNecks(const NiPoint3& around, float within, NeckOf* out, UInt32 most) {
	RosterActor actors[64];
	const UInt32 n = CollectLoadedActors(actors, 64);
	UInt32 found = 0;
	for (UInt32 i = 0; i < n && found < most; ++i) {
		const UInt32 actor = actors[i].ref;
		const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
		NiAVObject* const head = Named(root, "Bip01 Head");
		NiAVObject* const spine = Named(root, "Bip01 Spine2");
		if (head == nullptr || spine == nullptr) {
			continue;
		}
		const NiPoint3 d = head->worldTransform.pos - around;
		if (d.LengthSquared() > within * within) {
			continue;
		}
		NeckOf& k = out[found++];
		k.actor = actor;
		k.dead = actors[i].dead;
		k.low = spine->worldTransform.pos;
		k.high = head->worldTransform.pos;
	}
	return found;
}

bool HeadIsCut(UInt32 actor) { return Find(actor) != nullptr; }

bool CutHead(UInt32 actor, const NiPoint3& pushUnits, char* why, UInt32 whySize) {
	auto say = [&](const char* text) {
		std::snprintf(why, whySize, "%s", text);
		return false;
	};
	if (!Verified()) {
		return say("the engine's functions are not as read");
	}
	if (Find(actor) != nullptr) {
		return say("cut already");
	}
	const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
	NiAVObject* const head = Named(root, "Bip01 Head");
	NiAVObject* const spine2 = Named(root, "Bip01 Spine2");
	NiAVObject* neck = Named(root, "Bip01 Neck1");
	if (head == nullptr || spine2 == nullptr) {
		return say("no head or no Spine2 in its skeleton");
	}
	if (neck == nullptr) {
		neck = spine2;
	}
	Cut* slot = nullptr;
	for (Cut& c : g_cuts) {
		if (c.actor == 0) {
			slot = &c;
			break;
		}
	}
	if (slot == nullptr) {
		return say("sixteen heads cut already - no room");
	}
	const UInt32 joints = ReleaseNeckJoints(head, spine2);
	Cut& c = *slot;
	c.actor = actor;
	c.root = root;
	c.head = head;
	c.neck = neck;
	c.neckTop = Named(root, "Bip01 Neck1");
	c.neckBase = Named(root, "Bip01 Neck");
	const NiTransform& nw = neck->worldTransform;
	const NiPoint3 rel = head->worldTransform.pos - nw.pos;
	const float inv = nw.scale > 0.0f ? 1.0f / nw.scale : 1.0f;
	c.cutInNeck = InverseMatrix(nw.rot) * rel * inv;
	MakeFrom(c.bodySide, head);
	MakeFrom(c.headSide, head);
	Place(c);
	SwapCount n;
	SwapSkins(reinterpret_cast<const NiAVObject*>(root), false, c, spine2->worldTransform.pos, n, 0);
	const NiPoint3 push = CutPush(pushUnits);
	const bool pushed = ThrowBodyOfNode(reinterpret_cast<UInt32>(head), push);
	OBVR_LOG("Dismember: %08X's head cut - %u neck joint(s) released, %u skinned shape(s) redirected (%u body-side "
	         "bone pointers to the neck, %u head-side to the head: %s), the head %s at %.0f %.0f %.0f units/s",
	         actor, joints, n.shapes, n.bodyBones, n.headBones, n.names, pushed ? "woken and sent off" : "NOT woken",
	         static_cast<double>(push.x), static_cast<double>(push.y), static_cast<double>(push.z));
	// Oblivion Walkers ends a walker whose head is off (its spec 4.17).
	SendWalkersHead(Read(actor + addr::kFormIdOffset), kHeadCut);
	std::snprintf(why, whySize, "cut");
	return true;
}

void StepCuts() {
	for (Cut& c : g_cuts) {
		if (c.actor == 0) {
			continue;
		}
		const UInt32 root = LooksLikeObject(c.actor) ? Read(c.actor + addr::kRefNiNodeOffset) : 0;
		if (root != c.root || !LooksLikeObject(reinterpret_cast<UInt32>(c.head)) ||
		    !LooksLikeObject(reinterpret_cast<UInt32>(c.neck))) {
			OBVR_LOG("Dismember: %08X's 3D changed - its cut forgotten", c.actor);
			c = Cut{};
			continue;
		}
		Place(c);
	}
}

void LogCuts(const char* mark) {
	for (Cut& c : g_cuts) {
		if (c.actor == 0) {
			continue;
		}
		const NiPoint3 at = AsNode(c.bodySide)->worldTransform.pos;
		const NiPoint3 head = c.head->worldTransform.pos;
		OBVR_LOG("Dismember: at the mark \"%s\" %08X's head at %.1f %.1f %.1f, the cut at %.1f %.1f %.1f - %.1f "
		         "units apart",
		         mark, c.actor, static_cast<double>(head.x), static_cast<double>(head.y), static_cast<double>(head.z),
		         static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
		         static_cast<double>(math::Sqrt((head - at).LengthSquared())));
	}
}

const char* WeaponModelPath(const UInt8* weapon) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(weapon))) {
		return nullptr;
	}
	const UInt32 path = Read(reinterpret_cast<UInt32>(weapon) + 0x34);
	return LooksLikeObject(path & ~3u) ? reinterpret_cast<const char*>(path) : nullptr;
}

}  // namespace obvr::game

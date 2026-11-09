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
#include "game/GameSound.h"
#include "game/Blood.h"
#include "game/RefFingerprint.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/RagdollHold.h"
#include "game/SkinRead.h"
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

struct Cut {
	UInt32 actor = 0;
	CutPart part = CutPart::Head;  // the head at the neck, or a hand at its wrist
	UInt32 root = 0;
	NiAVObject* head = nullptr;
	NiAVObject* neck = nullptr;  // Neck1, or Spine2 without one
	NiAVObject* neckTop = nullptr;   // Bip01 Neck1, or null (counted as the head's bones)
	NiAVObject* neckBase = nullptr;  // Bip01 Neck, or null
	NiAVObject* other = nullptr;  // a hand cut: the other hand (its bones stay; SwapBoneOnPartSide)
	NiPoint3 cutInNeck{0.0f, 0.0f, 0.0f};  // the head's origin in the neck's frame at the cut
	FakeNode bodySide{};  // the body's shapes' head bone: at the cut, following the neck
	FakeNode headSide{};  // the head's shapes' other bones: at the head's origin
	// The wound caps (AttachCap): the stump's bone, at the cut with the
	// head's turn in the neck's frame and its scale - not collapsed - and the
	// two caps hung under the actor's root (0: none).
	FakeNode capSide{};
	FakeNode partCapSide{};  // a hand cut: the hand's place and turn at capScale, for the hand's own cap
	float capScale = 1.0f;     // CapScaleFor(part)
	// A head's cap read as facing up into the head: turned over about its
	// centre (FlippedCap), both in the head bone's space.
	bool capFlip = false;
	NiPoint3 capCentre{0.0f, 0.0f, 0.0f};
	NiPoint3 capDown{0.0f, 0.0f, -1.0f};
	NiMatrix33 headRotInNeck{};
	float headScale = 1.0f;
	UInt32 caps[2] = {0, 0};
	float age = 0.0f;   // seconds since the cut
	UInt32 spurts = 0;  // the blood's spurts so far (BloodPulseDue)
	// One bone pointer OBVR wrote and its value: when the engine rebuilt the
	// actor's skins (a load reuses the reference and may reuse the root's
	// address - the harness, 2026-10-31 19:44), it no longer holds it.
	UInt32 checkSlot = 0;
	UInt32 checkValue = 0;
	UInt32 world = 0;   // CurrentWorld() when it was cut or last rechecked (CutWorldFor)
	UInt32 formId = 0;  // the actor's form id, for the engine's form table after a load
};
Cut g_cuts[kCutsMost];

// A cut whose actor may be read: one there, made in this world (CutInWorld).
bool Live(const Cut& c) { return c.actor != 0 && CutInWorld(c.world, CurrentWorld()); }

// The heads cut that are to stay off (DismemberLogic.h CutMemory), and for
// each the 3D it is waiting on: how long it has stood, and a root already
// tried (a refusal is not tried again every frame).
CutMemory g_memory;
struct Waiting {
	UInt32 id = 0;
	UInt32 root = 0;
	float seconds = 0.0f;
	bool tried = false;
};
Waiting g_waiting[kCutMemoryMost];
UInt32 g_redoLines = 16;
// CutSettings' caps, sound and most cuts at once, as StepCuts had them last.
struct Wanted {
	bool caps = true;
	bool sound = true;
	CutLimit limit;
};
Wanted g_wanted;

// The cut's sounds at the part (CutSoundsFor), placed on the actor.
void SoundCut(UInt32 actor, const NiPoint3& at, bool redo) {
	UInt32 sounds[kCutSoundsMost] = {};
	const UInt32 n = CutSoundsFor(g_wanted.sound, redo, sounds);
	UInt32 played = 0;
	for (UInt32 i = 0; i < n; ++i) {
		played += PlaySoundFormAt(sounds[i], actor, at) ? 1 : 0;
	}
	static UInt32 s_lines = 6;
	if (n > 0 && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Dismember: the cut's sound at %.0f %.0f %.0f - %u of %u played", static_cast<double>(at.x),
		         static_cast<double>(at.y), static_cast<double>(at.z), played, n);
	}
}

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
	char parted[200] = {};  // the bones of the part's side shapes sent to the part, once each
	UInt32 shapes = 0;
	UInt32 bodyBones = 0;
	UInt32 headBones = 0;
};

// A bone a part's side shape sent to the part, by name, once (for the log).
void NoteParted(SwapCount& n, const NiAVObject* bone) {
	const char* const name = LooksLikeObject(reinterpret_cast<UInt32>(bone->name) & ~3u) ? bone->name : "?";
	for (const char* at = std::strstr(n.parted, name); at != nullptr; at = std::strstr(at + 1, name)) {
		const char after = at[std::strlen(name)];
		if ((at == n.parted || at[-1] == ' ') && (after == '\0' || after == ',')) {
			return;
		}
	}
	const UInt32 len = static_cast<UInt32>(std::strlen(n.parted));
	if (len + std::strlen(name) + 3 < sizeof(n.parted)) {
		std::snprintf(n.parted + len, sizeof(n.parted) - len, "%s%s", len > 0 ? ", " : "", name);
	}
}

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
		const bool headCut = c.part == CutPart::Head;
		const bool holdsPart = !headCut && IsUnder(c.head, b);
		const bool holdsOther = !headCut && c.other != nullptr && IsUnder(c.other, b);
		const bool underStump = !headCut && c.neck != nullptr && IsUnder(b, c.neck);
		if (headSide ? SwapBoneOnPartSide(headCut, isHead, underHead, holdsPart, holdsOther, underStump)
		             : SwapBoneOnBodySide(isHead, underHead)) {
			if (headSide) {
				NoteParted(n, b);
			}
			bone[i] = headSide ? headFake : bodyFake;
			if (c.checkSlot == 0) {
				c.checkSlot = reinterpret_cast<UInt32>(&bone[i]);
				c.checkValue = bone[i];
			}
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
	NiAVObject* const cap = AsNode(c.capSide);
	cap->worldTransform.rot = neck.rot * c.headRotInNeck;
	cap->worldTransform.pos = body->worldTransform.pos;
	cap->worldTransform.scale = c.headScale * c.capScale;
	NiAVObject* const partCap = AsNode(c.partCapSide);
	partCap->worldTransform.rot = c.head->worldTransform.rot;
	partCap->worldTransform.pos = c.head->worldTransform.pos;
	partCap->worldTransform.scale = c.head->worldTransform.scale * c.capScale;
	if (c.capFlip) {
		partCap->worldTransform = FlippedCap(partCap->worldTransform, c.capCentre, c.capDown);
	}
}

// The wound caps (Nadi, 2026-10-31: "4. go"): vanilla's zombie neck plug
// (`meshes\creatures\zombie\neckplug.nif`: two shapes, flesh and bone, skinned
// to one bone, "Bip01 Head", at the neck's end of the head) loaded by the
// engine's model loader (0x00439EB0 on 0x00B33A1C, thiscall, ret 0x10),
// cloned (0x00700900 - the loader's copy is shared), its bone pointed at
// `bone`, hung under the actor's root (vt+0x84) and updated (0x00707610,
// 0x00707370) - ../OBNG/knowledge/skinned-part-attach.md. Once on the head
// (it caps the head's underside) and once on the stump's own bone (capSide:
// the head's place and turn at the cut, following the neck). 0 when refused.
constexpr UInt32 kModelLoader = 0x00B33A1C;
constexpr UInt32 kLoadModel = 0x00439EB0;
constexpr UInt32 kCloneObject = 0x00700900;
constexpr UInt32 kUpdateUpward = 0x00707610;
constexpr UInt32 kUpdateDownward = 0x00707370;
constexpr UInt32 kNodeAttachSlot = 0x84;
constexpr UInt16 kAppCulled = 0x0001;
const char* const kCapModel = "Creatures\\Zombie\\NeckPlug.nif";
const UInt8 kLoadModelBytes[] = {0x6A, 0xFF, 0x68, 0x08, 0xC9, 0x9A, 0x00, 0x64};
const UInt8 kCloneBytes[] = {0x6A, 0xFF, 0x68, 0xE8, 0x93, 0x9C, 0x00, 0x64};
const UInt8 kUpdateUpwardBytes[] = {0x6A, 0xFF, 0x68, 0x7B, 0x96, 0x9C, 0x00, 0x64};
const UInt8 kUpdateDownwardBytes[] = {0xD9, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xF1, 0x8B};

using LoadModelFn = UInt32(__fastcall*)(void* loader, void* edx, const char* path, UInt32 a, UInt32 b, UInt32 c);
using CloneFn = UInt32(__fastcall*)(void* source, void* edx);
using AttachChildFn = void(__fastcall*)(void* node, void* edx, void* child, UInt32 firstAvailable);
using UpdateUpwardFn = void(__fastcall*)(void* node, void* edx);
using UpdateDownwardFn = void(__fastcall*)(void* node, void* edx, float time, UInt32 flags);

bool CapsVerified() {
	static int verified = 0;
	if (verified == 0) {
		verified = mem::Verify(kLoadModel, kLoadModelBytes, sizeof(kLoadModelBytes)) &&
		                   mem::Verify(kCloneObject, kCloneBytes, sizeof(kCloneBytes)) &&
		                   mem::Verify(kUpdateUpward, kUpdateUpwardBytes, sizeof(kUpdateUpwardBytes)) &&
		                   mem::Verify(kUpdateDownward, kUpdateDownwardBytes, sizeof(kUpdateDownwardBytes))
		               ? 1
		               : -1;
		OBVR_LOG("Dismember: the model loader, clone and update %s", verified > 0 ? "as read - the cuts get caps"
		                                                                             : "NOT as read - no caps");
	}
	return verified > 0;
}

// Every skinned shape under `node`: its bones pointed at `bone`. The shapes counted.
UInt32 PointCapBones(const NiAVObject* node, UInt32 bone, UInt32 depth) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node)) || depth > 8) {
		return 0;
	}
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		UInt32 shapes = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			shapes += PointCapBones(children[i], bone, depth + 1);
		}
		return shapes;
	}
	if (className == nullptr ||
	    (std::strncmp(className, "NiTriShape@", 11) != 0 && std::strncmp(className, "NiTriStrips@", 12) != 0)) {
		return 0;
	}
	const UInt32 skin = Read(reinterpret_cast<UInt32>(node) + kGeometrySkinInstance);
	const UInt32 data = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
	const UInt32 bones = LooksLikeObject(skin) ? Read(skin + kSkinInstanceBones) : 0;
	if (!LooksLikeObject(data) || !LooksLikeObject(bones)) {
		return 0;
	}
	const UInt32 count = Read(data + kSkinDataBoneCount);
	if (count == 0 || count > kMaxBones) {
		return 0;
	}
	for (UInt32 i = 0; i < count; ++i) {
		reinterpret_cast<UInt32*>(bones)[i] = bone;
	}
	return 1;
}

UInt32 AttachCap(UInt32 root, const NiAVObject* bone, char* why, UInt32 whySize) {
	const UInt32 loader = Read(kModelLoader);
	if (!CapsVerified() || !LooksLikeObject(loader) || !LooksLikeObject(root)) {
		std::snprintf(why, whySize, "no loader or root");
		return 0;
	}
	const UInt32 shared =
		reinterpret_cast<LoadModelFn>(kLoadModel)(reinterpret_cast<void*>(loader), nullptr, kCapModel, 0, 0, 1);
	if (!LooksLikeObject(shared)) {
		std::snprintf(why, whySize, "%s not loaded", kCapModel);
		return 0;
	}
	const UInt32 cap = reinterpret_cast<CloneFn>(kCloneObject)(reinterpret_cast<void*>(shared), nullptr);
	if (!LooksLikeObject(cap)) {
		std::snprintf(why, whySize, "not cloned");
		return 0;
	}
	const UInt32 shapes = PointCapBones(reinterpret_cast<const NiAVObject*>(cap), reinterpret_cast<UInt32>(bone), 0);
	if (shapes == 0) {
		std::snprintf(why, whySize, "no skinned shape in it");
		return 0;
	}
	const UInt32 attach = Read(Read(root) + kNodeAttachSlot);
	reinterpret_cast<AttachChildFn>(attach)(reinterpret_cast<void*>(root), nullptr, reinterpret_cast<void*>(cap), 1);
	reinterpret_cast<UpdateUpwardFn>(kUpdateUpward)(reinterpret_cast<void*>(cap), nullptr);
	reinterpret_cast<UpdateDownwardFn>(kUpdateDownward)(reinterpret_cast<void*>(cap), nullptr, 0.0f, 0);
	std::snprintf(why, whySize, "%u shape(s)", shapes);
	return cap;
}

// Which way a cap faces, in its bone's space: its shapes' vertices and
// normals (NiGeometryData +0x20, BodyView.cpp) under their skin-to-bone.
CapFacing ReadCapFacing(UInt32 cap) {
	CapFacing f;
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(reinterpret_cast<const NiAVObject*>(cap), shapes, 8);
	for (const ShapeUnder& s : shapes) {
		SkinnedShape shape;
		char why[64] = "";
		if (ReadSkinnedShape(s.geometry, shape, why, sizeof(why)) != SkinReadResult::Skinned || shape.skinToBone.empty()) {
			continue;
		}
		const UInt32 normals = Read(shape.data + 0x20);
		if (normals < 0x10000) {
			continue;
		}
		AddCapFacing(f, shape.bind, reinterpret_cast<const NiPoint3*>(normals), shape.vertexCount, shape.skinToBone[0]);
	}
	return f;
}

// A head's cap: read which way it faces against the head bone's way down the
// neck; facing up into the head it is turned over (Place, FlippedCap).
void FaceHeadCapDown(Cut& c, UInt32 stumpCap, UInt32 headCap) {
	const NiPoint3 toNeck = c.neck->worldTransform.pos - c.head->worldTransform.pos;
	const float len = math::Sqrt(toNeck.LengthSquared());
	if (!(len > 1e-3f)) {
		return;
	}
	c.capDown = InverseMatrix(c.head->worldTransform.rot) * (toNeck * (1.0f / len));
	const CapFacing head = ReadCapFacing(headCap);
	const CapFacing stump = ReadCapFacing(stumpCap);
	const float headDown = CapFacingAlong(head, c.capDown);
	const float stumpUp = CapFacingAlong(stump, c.capDown * -1.0f);
	c.capCentre = CapCentre(head);
	c.capFlip = head.unitNormals > 0 && headDown < 0.0f;
	Place(c);
	// Measured after the placing: the cap's mean normal in the world, by the
	// node it now follows, against the head's way down the neck in the world.
	const float normalLen = math::Sqrt(head.normal.LengthSquared());
	const NiPoint3 normalWorld =
		AsNode(c.partCapSide)->worldTransform.rot * (head.normal * (normalLen > 1e-6f ? 1.0f / normalLen : 0.0f));
	const NiPoint3 downWorld = c.head->worldTransform.rot * c.capDown;
	const float placed = normalWorld.x * downWorld.x + normalWorld.y * downWorld.y + normalWorld.z * downWorld.z;
	OBVR_LOG("Dismember: the caps' facing read - the stump's %.2f up the neck (%u of %u normals), the head's %.2f "
	         "down it (%u of %u)%s; placed, the head's %.2f down the neck",
	         static_cast<double>(stumpUp), stump.unitNormals, stump.count, static_cast<double>(headDown), head.unitNormals,
	         head.count, c.capFlip ? " - turned over" : "", static_cast<double>(placed));
}

// The caps of a cut forgotten while its root still stands: culled, so a
// stump cap never reads a node slot given to another cut.
void CullCaps(Cut& c, UInt32 rootNow) {
	for (UInt32& cap : c.caps) {
		if (cap != 0 && rootNow == c.root && LooksLikeObject(cap)) {
			reinterpret_cast<NiAVObject*>(cap)->flags |= kAppCulled;
		}
		cap = 0;
	}
}

// The blood at the cut (game/Blood.h): spurts from the body's neck outward, the
// first with one from the head the other way and a stain under the neck.
void Bleed(Cut& c, const CutSettings& s, float dtSeconds) {
	if (BloodPulseDue(s, c.age, c.spurts)) {
		const NiPoint3 at = AsNode(c.bodySide)->worldTransform.pos;
		const NiPoint3 out = SprayDirection(c.neck->worldTransform.pos, at);
		SprayBlood(c.actor, at, out);
		if (c.spurts == 0) {
			SprayBlood(c.actor, c.head->worldTransform.pos, out * -1.0f);
			StainBlood(c.actor, at, NiPoint3{0.0f, 0.0f, -1.0f});
		}
		++c.spurts;
	}
	if (dtSeconds > 0.0f && dtSeconds < 1.0f) {
		c.age += dtSeconds;
	}
}

// Room for one more cut under [Hands] DismemberMostCuts (CutRoom).
bool RoomForACut() {
	UInt32 standing = 0;
	for (const Cut& c : g_cuts) {
		standing += c.actor != 0 ? 1u : 0u;
	}
	return CutRoom(standing, g_wanted.limit.most);
}

// A slot no cut holds, or null when all kCutsMost do.
Cut* FreeSlot() {
	for (Cut& c : g_cuts) {
		if (c.actor == 0) {
			return &c;
		}
	}
	return nullptr;
}

Cut* Find(UInt32 actor, CutPart part) {
	for (Cut& c : g_cuts) {
		if (c.actor == actor && Live(c) && c.part == part) {
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

bool ActorIsEssential(UInt32 actor) {
	const UInt32 base = LooksLikeObject(actor) ? Read(actor + addr::kRefBaseFormOffset) : 0;
	return LooksLikeObject(base) && ActorBaseFlagsEssential(Read(base + kEssentialFlagsOffset));
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
		for (UInt32 p = 0; p < kCutPartCount; ++p) {
			const CutPartBones bones = BonesOfCut(static_cast<CutPart>(p));
			NiAVObject* const joint = IsLimbCut(static_cast<CutPart>(p)) ? Named(root, bones.part) : nullptr;
			NiAVObject* const next = joint != nullptr ? Named(root, bones.next) : nullptr;
			k.hasLimb[p] = next != nullptr;
			if (k.hasLimb[p]) {
				k.limbJoint[p] = joint->worldTransform.pos;
				k.limbNext[p] = next->worldTransform.pos;
			}
		}
		for (UInt32 side = 0; side < 2; ++side) {
			NiAVObject* const forearm = Named(root, side == 0 ? "Bip01 L Forearm" : "Bip01 R Forearm");
			NiAVObject* const hand = Named(root, side == 0 ? "Bip01 L Hand" : "Bip01 R Hand");
			k.hasWrist[side] = forearm != nullptr && hand != nullptr;
			if (k.hasWrist[side]) {
				k.wristLow[side] = forearm->worldTransform.pos;
				k.wristHigh[side] = hand->worldTransform.pos;
			}
		}
	}
	return found;
}

bool HeadIsCut(UInt32 actor) { return Find(actor, CutPart::Head) != nullptr; }

bool PartIsCut(UInt32 actor, CutPart part) { return Find(actor, part) != nullptr; }

CutMemory& RememberedCuts() { return g_memory; }

void ForgetRememberedCuts() {
	g_memory = CutMemory{};
	for (Waiting& w : g_waiting) {
		w = Waiting{};
	}
}

bool CutHead(UInt32 actor, const NiPoint3& pushUnits, char* why, UInt32 whySize, bool redo) {
	return CutPartOff(actor, CutPart::Head, pushUnits, why, whySize, redo);
}

bool CutPartOff(UInt32 actor, CutPart part, const NiPoint3& pushUnits, char* why, UInt32 whySize, bool redo) {
	auto say = [&](const char* text) {
		std::snprintf(why, whySize, "%s", text);
		return false;
	};
	if (!Verified()) {
		return say("the engine's functions are not as read");
	}
	if (Find(actor, part) != nullptr) {
		return say("cut already");
	}
	for (const Cut& other : g_cuts) {
		if (other.actor == actor && Live(other) && CutPartsClash(part, other.part)) {
			return say("a part of that limb is cut already");
		}
	}
	const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
	// The part's bones (variables named for the head, the first part): the
	// part, the body its joint holds it to, the bone the stump follows
	// (DismemberLogic.h BonesOfCut).
	const bool isHead = part == CutPart::Head;
	const CutPartBones bones = BonesOfCut(part);
	NiAVObject* const head = Named(root, bones.part);
	NiAVObject* const spine2 = Named(root, bones.joint);
	NiAVObject* neck = Named(root, bones.stump);
	if (head == nullptr || spine2 == nullptr) {
		return say("the part's bone or its joint's body is not in its skeleton");
	}
	if (neck == nullptr) {
		neck = spine2;
	}
	if (!RoomForACut()) {
		return say("DismemberMostCuts cuts standing - no room");
	}
	Cut* const slot = FreeSlot();
	if (slot == nullptr) {
		return say("all cut slots taken - no room");
	}
	const UInt32 joints = ReleaseNeckJoints(head, spine2);
	Cut& c = *slot;
	c.actor = actor;
	c.world = CurrentWorld();
	c.formId = Read(actor + addr::kFormIdOffset);
	c.part = part;
	c.root = root;
	c.head = head;
	c.neck = neck;
	c.neckTop = isHead ? Named(root, "Bip01 Neck1") : nullptr;
	c.neckBase = isHead ? Named(root, "Bip01 Neck") : nullptr;
	c.other = bones.other != nullptr ? Named(root, bones.other) : nullptr;
	const NiTransform& nw = neck->worldTransform;
	const NiPoint3 rel = head->worldTransform.pos - nw.pos;
	const float inv = nw.scale > 0.0f ? 1.0f / nw.scale : 1.0f;
	c.cutInNeck = InverseMatrix(nw.rot) * rel * inv;
	MakeFrom(c.bodySide, head);
	MakeFrom(c.headSide, head);
	c.headRotInNeck = InverseMatrix(nw.rot) * head->worldTransform.rot;
	c.headScale = head->worldTransform.scale > 0.0f ? head->worldTransform.scale : 1.0f;
	MakeFrom(c.capSide, head);
	MakeFrom(c.partCapSide, head);
	c.capScale = CapScaleFor(part);
	Place(c);
	SwapCount n;
	SwapSkins(reinterpret_cast<const NiAVObject*>(root), false, c, spine2->worldTransform.pos, n, 0);
	const NiPoint3 push = CutPush(pushUnits);
	const bool pushed = ThrowBodyOfNode(reinterpret_cast<UInt32>(head), push);
	OBVR_LOG("Dismember: %08X's %s cut - %u %s joint(s) released, %u skinned shape(s) redirected (%u body-side "
	         "bone pointers to the stump, %u part-side to the part: %s; to the part: %s), the part %s at %.0f %.0f %.0f units/s",
	         actor, CutPartName(part), joints, CutJointName(part), n.shapes, n.bodyBones, n.headBones, n.names, n.parted, pushed ? "woken and sent off" : "NOT woken",
	         static_cast<double>(push.x), static_cast<double>(push.y), static_cast<double>(push.z));
	SoundCut(actor, head->worldTransform.pos, redo);
	if (g_wanted.caps) {
		char capWhy[64] = "";
		char headWhy[64] = "";
		c.caps[0] = AttachCap(root, AsNode(c.capSide), capWhy, sizeof(capWhy));
		// The part's cap on a node following it at its scale (the head's at
		// 1), turned over when it faces into the head.
		c.capFlip = false;
		c.caps[1] = AttachCap(root, AsNode(c.partCapSide), headWhy, sizeof(headWhy));
		if (isHead && c.caps[0] != 0 && c.caps[1] != 0) {
			FaceHeadCapDown(c, c.caps[0], c.caps[1]);
		}
		OBVR_LOG("Dismember: %08X's wound caps - the stump %s (%s), the %s %s (%s)", actor,
		         c.caps[0] != 0 ? "capped" : "NOT capped", capWhy, CutPartName(part), c.caps[1] != 0 ? "capped" : "NOT capped", headWhy);
	}
	if (redo) {
		// A cut done again after a load or a reload of the cell: no blood,
		// no message - both were the first cut's.
		c.spurts = 0xFFFFFFFFu;
	} else {
		RememberCut(g_memory, Read(actor + addr::kFormIdOffset), part);
		// Oblivion Walkers ends a walker whose head is off (its spec 4.17).
		if (isHead) {
			SendWalkersHead(Read(actor + addr::kFormIdOffset), kHeadCut);
		}
	}
	std::snprintf(why, whySize, "cut");
	return true;
}

namespace {

using LookupFn = UInt32(__cdecl*)(UInt32 id);
constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl, as HandBones.cpp

bool CutStands(UInt32 formId, CutPart part) {
	for (const Cut& c : g_cuts) {
		if (Live(c) && c.part == part && Read(c.actor + addr::kFormIdOffset) == formId) {
			return true;
		}
	}
	return false;
}

// An actor raised again alive (RedoFor's Forget): its remembered cuts go,
// and the waits that followed them start afresh.
void ForgetRaised(UInt32 id) {
	const UInt32 gone = ForgetCuts(g_memory, id);
	for (Waiting& w : g_waiting) {
		w = Waiting{};
	}
	OBVR_LOG("Dismember: %08X raised again alive - its %u remembered cut(s) forgotten, its 3D whole", id, gone);
}

// The heads to stay off (g_memory) whose actor's 3D is there and not cut:
// cut again once it has stood kRedoAfterSeconds (the ragdoll built).
void RedoCuts(float dtSeconds) {
	for (UInt32 i = 0; i < g_memory.count && i < kCutMemoryMost; ++i) {
		const UInt32 id = g_memory.ids[i];
		const CutPart part = static_cast<CutPart>(g_memory.parts[i]);
		Waiting& w = g_waiting[i];
		if (w.id != id) {
			w = Waiting{};
			w.id = id;
		}
		if (CutStands(id, part)) {
			w.root = 0;
			w.seconds = 0.0f;
			w.tried = false;
			continue;
		}
		const UInt32 actor = reinterpret_cast<LookupFn>(kLookupFormById)(id);
		const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
		if (!LooksLikeObject(root)) {
			w.root = 0;
			w.seconds = 0.0f;
			w.tried = false;
			continue;
		}
		if (root != w.root) {
			w.root = root;
			w.seconds = 0.0f;
			w.tried = false;
		}
		if (w.tried) {
			continue;
		}
		if (dtSeconds > 0.0f && dtSeconds < 1.0f) {
			w.seconds += dtSeconds;
		}
		bool dead = false;
		const bool loaded = ActorInWorld(actor, dead);
		const RedoStep step = RedoFor(loaded, dead, w.seconds);
		if (step == RedoStep::Forget) {
			ForgetRaised(id);
			return;  // the memory moved under the loop: the rest next frame
		}
		if (step != RedoStep::Redo) {
			continue;
		}
		w.tried = true;
		char why[96] = "";
		const bool cut = CutPartOff(actor, part, NiPoint3{0.0f, 0.0f, 0.0f}, why, sizeof(why), true);
		if (g_redoLines > 0) {
			--g_redoLines;
			OBVR_LOG("Dismember: %08X's %s cut again after its 3D came back (a load, the cell again) - %s", id, CutPartName(part),
			         cut ? "off again" : why);
		}
	}
}

// After a load (CutWorldFor): each cut whose actor the form table still
// holds is taken into this world, read from the next frame on; the others
// are dropped unread - the load freed them and their 3D, the caps with it -
// and the waits start afresh, so the co-save's memory cuts the parts again
// once their 3D stands.
void RecheckCutsAfterLoad() {
	const UInt32 world = CurrentWorld();
	UInt32 kept = 0;
	UInt32 dropped = 0;
	for (Cut& c : g_cuts) {
		if (c.actor == 0 || CutInWorld(c.world, world)) {
			continue;
		}
		const UInt32 byId = c.formId != 0 ? reinterpret_cast<LookupFn>(kLookupFormById)(c.formId) : 0;
		if (CutWorldFor(c.world, world, c.actor, byId) == CutWorldStep::Recheck) {
			c.world = world;
			++kept;
			continue;
		}
		c = Cut{};
		++dropped;
	}
	if (dropped != 0) {
		for (Waiting& w : g_waiting) {
			w = Waiting{};
		}
	}
	// Said once a load for the cuts kept (each of its frames rechecks them),
	// and for every drop, from a budget of its own.
	static UInt32 s_lines = 16;
	static bool s_keptSaid = false;
	if (dropped != 0 && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Dismember: a load since - %u cut(s) dropped unread, their actor gone from the form table (world %u)",
		         dropped, world);
	}
	if (kept != 0 && !s_keptSaid && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Dismember: a load since - %u cut(s) still on their actor, read again once the load is done (world %u)",
		         kept, world);
	}
	s_keptSaid = kept != 0;
}

}  // namespace

UInt32 StandingCuts() {
	UInt32 n = 0;
	for (const Cut& c : g_cuts) {
		n += Live(c) ? 1 : 0;
	}
	return n;
}

void StepCuts(const CutSettings& settings, float dtSeconds) {
	g_wanted.caps = settings.caps;
	g_wanted.sound = settings.sound;
	const CutLimit limit = CutLimitFor(settings.most);
	if (limit.most != g_wanted.limit.most || limit.clamped != g_wanted.limit.clamped) {
		OBVR_LOG("Dismember: at most %u cuts at once ([Hands] DismemberMostCuts=%d%s)", limit.most, settings.most,
		         limit.clamped ? ", kept within 16 and the slots there are" : "");
	}
	g_wanted.limit = limit;
	RecheckCutsAfterLoad();
	for (Cut& c : g_cuts) {
		if (!Live(c)) {
			continue;  // none, or rechecked this frame: read from the next
		}
		const UInt32 root = LooksLikeObject(c.actor) ? Read(c.actor + addr::kRefNiNodeOffset) : 0;
		const bool skinsRebuilt =
			c.checkSlot != 0 && (!RefBytesReadable(c.checkSlot, 4) || Read(c.checkSlot) != c.checkValue);
		if (root != c.root || skinsRebuilt || !LooksLikeObject(reinterpret_cast<UInt32>(c.head)) ||
		    !LooksLikeObject(reinterpret_cast<UInt32>(c.neck))) {
			OBVR_LOG("Dismember: %08X's 3D changed - its cut forgotten (%s %s: root %08X -> %08X, skins %s)", c.actor,
			         CutPartName(c.part), CutJointName(c.part), c.root, root, skinsRebuilt ? "rebuilt" : "kept");
			CullCaps(c, root);
			c = Cut{};
			continue;
		}
		Place(c);
		Bleed(c, settings, dtSeconds);
	}
	if (settings.enabled) {
		RedoCuts(dtSeconds);
	}
}

void LogCuts(const char* mark) {
	for (Cut& c : g_cuts) {
		if (!Live(c)) {
			continue;
		}
		const NiPoint3 at = AsNode(c.bodySide)->worldTransform.pos;
		const NiPoint3 head = c.head->worldTransform.pos;
		OBVR_LOG("Dismember: at the mark \"%s\" %08X's %s at %.1f %.1f %.1f, the cut at %.1f %.1f %.1f - %.1f "
		         "units apart",
		         mark, c.actor, CutPartName(c.part), static_cast<double>(head.x), static_cast<double>(head.y), static_cast<double>(head.z),
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

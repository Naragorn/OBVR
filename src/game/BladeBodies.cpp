#include "game/BladeBodies.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

struct ListNode {
	void* data;
	ListNode* next;
};

using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
using NodeFn = void*(__fastcall*)(void* self, void* edx);
using DeadFn = UInt8(__fastcall*)(void* self, void* edx, UInt32 arg);
using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

bool IsLivingActor(UInt32 actor, UInt32 player) {
	if (actor == 0 || actor == player || !LooksLikeObject(actor)) {
		return false;
	}
	const UInt32 vtable = Read(actor);
	if (vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) {
		return false;
	}
	const UInt32 dead = Slot(actor, addr::kActorVtableIsDeadOffset);
	return dead != 0 && (reinterpret_cast<DeadFn>(dead)(reinterpret_cast<void*>(actor), nullptr, 0) & 1) == 0;
}

bool IsBone(const NiAVObject* node) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
		return false;
	}
	const char* const name = node->name;
	if (!LooksLikeObject(reinterpret_cast<UInt32>(name) & ~3u)) {
		return false;
	}
	return name[0] == 'B' && name[1] == 'i' && name[2] == 'p' && name[3] == '0' && name[4] == '1';
}

// The bones kept per actor and root: a new root (a load, a new 3D) is read
// anew.
struct Kept {
	UInt32 actor = 0;
	UInt32 root = 0;
	const NiAVObject* bone[kBladeBoneCount] = {};
	UInt32 lastFrame = 0;
};

constexpr UInt32 kKeptMax = 16;
Kept g_kept[kKeptMax];
UInt32 g_frame = 0;
UInt32 g_readLines = 8;

Kept& KeptFor(UInt32 actor, UInt32 root) {
	Kept* oldest = &g_kept[0];
	for (Kept& k : g_kept) {
		if (k.actor == actor && k.root == root) {
			k.lastFrame = g_frame;
			return k;
		}
		if (k.actor == actor || k.lastFrame < oldest->lastFrame) {
			oldest = &k;
			if (k.actor == actor) {
				break;
			}
		}
	}
	Kept& k = *oldest;
	k = Kept{};
	k.actor = actor;
	k.root = root;
	k.lastFrame = g_frame;
	const UInt32 getObject = Slot(root, addr::kNiAVObjectGetObjectVtableOffset);
	UInt32 found = 0;
	for (UInt32 b = 0; b < kBladeBoneCount && getObject != 0; ++b) {
		auto* const node = static_cast<const NiAVObject*>(
			reinterpret_cast<GetObjectFn>(getObject)(reinterpret_cast<void*>(root), nullptr, BladeBoneName(b)));
		if (IsBone(node)) {
			k.bone[b] = node;
			++found;
		}
	}
	if (g_readLines > 0) {
		--g_readLines;
		char missing[160] = {};
		UInt32 at = 0;
		for (UInt32 b = 0; b < kBladeBoneCount; ++b) {
			if (k.bone[b] != nullptr) {
				continue;
			}
			const char* name = BladeBoneName(b) + 6;  // past "Bip01 "
			for (; *name != '\0' && at + 2 < sizeof(missing); ++name) {
				missing[at++] = *name;
			}
			if (at + 2 < sizeof(missing)) {
				missing[at++] = ',';
			}
		}
		missing[at > 0 ? at - 1 : 0] = '\0';
		OBVR_LOG("Contact: %08X's skeleton read - %u of %u bones%s%s%s", actor, found,
		         static_cast<UInt32>(kBladeBoneCount), found < kBladeBoneCount ? " (none for " : "", missing,
		         found < kBladeBoneCount ? ")" : "");
	}
	return k;
}

}  // namespace

UInt32 CollectBladeBodies(const NiPoint3& around, float withinUnits, BladeBodies& out) {
	++g_frame;
	out.count = 0;
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	UInt32 people = 0;
	for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
		if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
			break;
		}
		const UInt32 actor = reinterpret_cast<UInt32>(node->data);
		node = node->next;
		if (!IsLivingActor(actor, player)) {
			continue;
		}
		const UInt32 getNode = Slot(actor, addr::kActorVtableGetNiNodeOffset);
		if (getNode == 0) {
			continue;
		}
		const UInt32 root =
			reinterpret_cast<UInt32>(reinterpret_cast<NodeFn>(getNode)(reinterpret_cast<void*>(actor), nullptr));
		if (!LooksLikeObject(root)) {
			continue;
		}
		const auto* const rootNode = reinterpret_cast<const NiAVObject*>(root);
		const NiBound bound = rootNode->worldBound;
		if (!(bound.radius > 0.0f && bound.radius < 4096.0f)) {
			continue;
		}
		const NiPoint3 off = bound.center - around;
		const float reach = withinUnits + bound.radius;
		if (!(off.LengthSquared() <= reach * reach)) {
			continue;
		}
		Kept& k = KeptFor(actor, root);
		NiPoint3 at[kBladeBoneCount];
		bool have[kBladeBoneCount];
		for (UInt32 b = 0; b < kBladeBoneCount; ++b) {
			have[b] = k.bone[b] != nullptr && IsBone(k.bone[b]);
			at[b] = have[b] ? k.bone[b]->worldTransform.pos : NiPoint3{0.0f, 0.0f, 0.0f};
		}
		if (BodyCapsulesFromBones(at, have, rootNode->worldTransform.scale, actor, out) == 0 &&
		    !BodyColumnFromBound(bound.center, bound.radius, actor, out)) {
			continue;
		}
		++people;
	}
	return people;
}

void ForgetBladeBodies() {
	for (Kept& k : g_kept) {
		k = Kept{};
	}
}

}  // namespace obvr::game

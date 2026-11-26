#include "game/CutJoints.h"

#include <windows.h>

#include "core/Log.h"
#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/CutSkeletonLogic.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"

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
constexpr UInt32 kMaxDepth = 48;
// The bodies under a part: an arm's upper arm, forearm and hand, a weapon's
// or a shield's own body on them; a head's one.
constexpr UInt32 kPartBodiesMost = 32;

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

struct PartBodies {
	UInt32 body[kPartBodiesMost] = {};
	UInt32 count = 0;
	bool Has(UInt32 b) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (body[i] == b) {
				return true;
			}
		}
		return false;
	}
};

// The bodies of `node` and every node under it.
void CollectBodies(const NiAVObject* node, PartBodies& out, UInt32 depth) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node)) || depth > kMaxDepth || out.count >= kPartBodiesMost) {
		return;
	}
	if (!NiClassIsNode(NiClassNameOf(node))) {
		return;
	}
	const UInt32 body = CutBodyOf(node);
	if (body != 0 && !out.Has(body)) {
		out.body[out.count++] = body;
	}
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(node, count);
	for (UInt32 i = 0; children != nullptr && i < count; ++i) {
		if (children[i] != nullptr) {
			CollectBodies(children[i], out, depth + 1);
		}
	}
}

// The wrapper of a joint's entity (its +0x0C), 0 when it has none.
UInt32 WrapperOfEntity(UInt32 entity) { return LooksLikeObject(entity) ? Read(entity + kEntityWrapper) : 0; }

}  // namespace

bool CutJointsVerified() {
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

UInt32 CutBodyOf(const NiAVObject* node) {
	using CollisionOfFn = UInt32(__cdecl*)(const NiAVObject* node);
	const UInt32 collision = reinterpret_cast<CollisionOfFn>(kCollisionObjectOf)(node);
	return LooksLikeObject(collision) ? Read(collision + kCollisionBody) : 0;
}

UInt32 ReleasePartJoints(const NiAVObject* part) {
	const UInt32 wrapper = CutBodyOf(part);
	if (!LooksLikeObject(wrapper)) {
		return 0;
	}
	PartBodies inPart;
	CollectBodies(part, inPart, 0);
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
					const UInt32 aw = WrapperOfEntity(Read(instance + kInstanceEntityA));
					const UInt32 bw = WrapperOfEntity(Read(instance + kInstanceEntityB));
					if (ReleasesPartJoint(wrapper, aw, bw, inPart.Has(aw), inPart.Has(bw))) {
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

}  // namespace obvr::game

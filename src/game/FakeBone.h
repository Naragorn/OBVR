#pragma once

#include <cstring>

#include "core/Types.h"
#include "game/GameTypes.h"

namespace obvr::game {

// A bone OBVR owns, put into an engine skin's bone list in place of a real
// one (game/ArmStump.cpp: the forearm stumps and the gloves; game/Dismember.cpp:
// the cuts' sides and caps). Never in a tree: a real bone's first bytes (its
// vtable and name, for anything that looks), a reference count nothing brings
// to zero, and a world transform OBVR writes.
//
// With a parent of its own. The engine takes a bone's parent for granted:
// 0x007E4960 (called from 0x007E53E0, under the scene walk at 0x0070AC00)
// picks one bone of a skin at random (rand() % count over NiSkinInstance
// +0x14) and reads the world position of its parent ([bone+0x1C], then
// +0x88..+0x90 of it - NiAVObject's worldTransform.pos). A stand-in with no
// parent crashed it: reading 00000088 at Oblivion.exe+3E4A11, the bone at
// esi inside OBVR.dll's own data, edi the player's "Arms" shape whose skin the
// stump had swapped (Quest 2 tester, 2026-11-18 23:30 and 2026-11-19 03:26,
// twice in a guard's arrest talk; CobbCrashLogger and OBVR-0326-crash.log;
// the disassembly 0x007E49DA..0x007E4A11 is the tester's, not yet read in
// rea-all). A real bone always has a parent, so the engine never checks.
//
// The parent is a second stand-in beside the bone: a copy of the same
// bone's first bytes, itself without a parent - the way the root of every
// real tree ends - and at the bone's own place (SyncFakeBoneParent after each
// write of the bone's world transform). Not the real bone's parent: the
// stand-ins outlive the skeleton they were made from (a load rebuilds it), and
// a pointer into a freed skeleton would be worse than none.
struct alignas(16) FakeBone {
	UInt8 bytes[0xC0];
	UInt8 parentBytes[0xC0];
};

inline NiAVObject* FakeBoneNode(FakeBone& fake) { return reinterpret_cast<NiAVObject*>(fake.bytes); }
inline NiAVObject* FakeBoneParent(FakeBone& fake) { return reinterpret_cast<NiAVObject*>(fake.parentBytes); }

// One stand-in node, copied from `real`: no parent, no controller, no extra
// data, a reference count nothing brings to zero.
inline void MakeStandInNode(UInt8 (&bytes)[0xC0], const NiAVObject* real) {
	std::memset(bytes, 0, sizeof(bytes));
	std::memcpy(bytes, real, sizeof(NiAVObject));
	NiAVObject* const node = reinterpret_cast<NiAVObject*>(bytes);
	node->parent = nullptr;
	node->refCount = 0x40000000u;
	node->controller = nullptr;
	node->extraDataList = nullptr;
	node->extraDataListLen = 0;
	node->extraDataListCapacity = 0;
}

// The parent at the bone's world transform: after every write of it.
inline void SyncFakeBoneParent(FakeBone& fake) {
	FakeBoneParent(fake)->worldTransform = FakeBoneNode(fake)->worldTransform;
}

// A stand-in bone (FakeBoneNode of a FakeBone) to `world`, its parent with it.
inline void PlaceFakeBone(NiAVObject* bone, const NiTransform& world) {
	bone->worldTransform = world;
	if (bone->parent != nullptr) {
		bone->parent->worldTransform = world;
	}
}

inline void MakeFakeBone(FakeBone& fake, const NiAVObject* real) {
	MakeStandInNode(fake.bytes, real);
	MakeStandInNode(fake.parentBytes, real);
	FakeBoneNode(fake)->parent = FakeBoneParent(fake);
	SyncFakeBoneParent(fake);
}

}  // namespace obvr::game

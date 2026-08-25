#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

// An excerpt from Oblivion's Gamebryo/NetImmerse scene graph.
//
// OBVR deliberately does not include the full xOBSE or TES-Reloaded headers.
// The hook needs exactly one object type, and its layout is verified against
// Oblivion.exe 1.2.0.416 (see GameAddresses.h).
//
// The pure arithmetic types live in NiMath.h.

// <cstddef> is not available in the freestanding cross build.
#if defined(__clang__) || defined(__GNUC__)
#define OBVR_OFFSETOF(type, member) __builtin_offsetof(type, member)
#else
#include <cstddef>
#define OBVR_OFFSETOF(type, member) offsetof(type, member)
#endif

namespace obvr {

// Modelled out only as far as the camera hook needs. Everything past
// worldTransform stays opaque because OBVR does not touch it.
struct NiAVObject {
	void* vtable;                     // 0x00
	UInt32 refCount;                  // 0x04
	const char* name;                 // 0x08
	void* controller;                 // 0x0C
	void** extraDataList;             // 0x10
	UInt16 extraDataListLen;          // 0x14
	UInt16 extraDataListCapacity;     // 0x16
	UInt16 flags;                     // 0x18
	UInt8 pad1A[2];                   // 0x1A
	NiAVObject* parent;               // 0x1C
	NiBound worldBound;               // 0x20
	NiTransform localTransform;       // 0x30
	NiTransform worldTransform;       // 0x64
};

// These offsets are the core of the hook's assumption. Oblivion.exe writes to
// [eax+0x54/0x58/0x5C] at 0x0066BE3D..0x0066BE47 and copies nine DWORDs to
// [eax+0x30] at 0x0066BE60. That matches localTransform.pos = 0x30 + 0x24 =
// 0x54 and localTransform.rot = 0x30 exactly.
//
// The check only applies to 32-bit builds: the layout depends on pointer
// size, and that is the only place the DLL is loaded into. Natively built
// tests (64 bit) only use the types from NiMath.h anyway.
#if defined(OBVR_TARGET_32BIT)
static_assert(sizeof(void*) == 4, "OBVR_TARGET_32BIT requires 32-bit pointers");
static_assert(OBVR_OFFSETOF(NiAVObject, parent) == 0x1C, "parent must sit at 0x1C");
static_assert(OBVR_OFFSETOF(NiAVObject, localTransform) == 0x30, "localTransform must sit at 0x30");
static_assert(OBVR_OFFSETOF(NiAVObject, worldTransform) == 0x64, "worldTransform must sit at 0x64");
static_assert(sizeof(NiAVObject) == 0x98, "NiAVObject must be 0x98 bytes up to worldTransform");
#endif

}  // namespace obvr

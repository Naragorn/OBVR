#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

// Ausschnitt aus Oblivions Gamebryo/NetImmerse-Szenengraph.
//
// OBVR bindet bewusst nicht die vollstaendigen xOBSE- oder TES-Reloaded-Header
// ein. Der Hook braucht genau einen Objekttyp, und dessen Layout ist gegen
// Oblivion.exe 1.2.0.416 verifiziert (siehe GameAddresses.h).
//
// Die reinen Rechentypen stehen in NiMath.h.

// <cstddef> ist im freestanding Cross-Build nicht verfuegbar.
#if defined(__clang__) || defined(__GNUC__)
#define OBVR_OFFSETOF(type, member) __builtin_offsetof(type, member)
#else
#include <cstddef>
#define OBVR_OFFSETOF(type, member) offsetof(type, member)
#endif

namespace obvr {

// Nur so weit ausmodelliert, wie der Kamera-Hook es braucht. Alles hinter
// worldTransform bleibt undurchsichtig, weil OBVR es nicht anfasst.
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

// Die Offsets sind der Kern der Hook-Annahme. Oblivion.exe schreibt bei
// 0x0066BE3D..0x0066BE47 nach [eax+0x54/0x58/0x5C] und kopiert bei
// 0x0066BE60 neun DWORDs nach [eax+0x30]. Das deckt sich exakt mit
// localTransform.pos = 0x30 + 0x24 = 0x54 und localTransform.rot = 0x30.
//
// Die Pruefung gilt nur fuer 32-Bit-Builds: das Layout haengt an der
// Zeigergroesse, und nur dorthin wird die DLL geladen. Nativ gebaute Tests
// (64 Bit) benutzen ohnehin nur die Typen aus NiMath.h.
#if defined(OBVR_TARGET_32BIT)
static_assert(sizeof(void*) == 4, "OBVR_TARGET_32BIT verlangt 32-Bit-Zeiger");
static_assert(OBVR_OFFSETOF(NiAVObject, parent) == 0x1C, "parent muss bei 0x1C liegen");
static_assert(OBVR_OFFSETOF(NiAVObject, localTransform) == 0x30, "localTransform muss bei 0x30 liegen");
static_assert(OBVR_OFFSETOF(NiAVObject, worldTransform) == 0x64, "worldTransform muss bei 0x64 liegen");
static_assert(sizeof(NiAVObject) == 0x98, "NiAVObject muss bis worldTransform 0x98 gross sein");
#endif

}  // namespace obvr

#pragma once

#include "core/Types.h"

namespace obvr::platform {

// Replaces one entry in a module's import address table.
//
// What this is for: Oblivion decides the size of its frame when it creates its
// Direct3D device, and nothing afterwards can change it without destroying
// every resource the game holds. So the only place to change it is before -
// inside the call that makes the device, which means being in the way of that
// call before it happens.
//
// The detour that was there instead wrote iSize into Oblivion.ini and let the
// game read it next time. That is a workaround: it goes around the place the
// decision is made rather than to it, it takes two restarts, and it edits a
// file that belongs to the user rather than to OBVR.
//
// Why the import table rather than patching the function itself. A byte patch
// has to decode whatever instructions happen to be at the entry point and
// relocate them, and those bytes belong to DXVK here, which is under no
// obligation to keep them recognisable. An import entry is a single aligned
// pointer that the loader filled in and that x86 writes atomically. There is
// nothing to decode and nothing to get wrong about a calling convention.
//
// The limit is that it only catches calls made THROUGH the table. Code that
// resolved the address itself with GetProcAddress is unaffected, and so is any
// call already made - which is the real risk here, since a plugin loads at
// some point during startup and Direct3DCreate9 may already be behind it.
// Whether it was is reported rather than assumed.

// Swaps the entry for one function imported from one module, and hands back
// what was there. False when the module has no such import, which is not an
// error - it means the game reaches that function some other way.
//
// moduleBase is where the importing module is loaded: 0x00400000 for
// Oblivion.exe. importedFrom is the DLL name as the import table spells it,
// compared without regard to case. functionName is exact.
bool ReplaceImport(UInt32 moduleBase, const char* importedFrom, const char* functionName,
                   void* replacement, void** previous);

}  // namespace obvr::platform

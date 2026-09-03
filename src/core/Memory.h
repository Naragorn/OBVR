#pragma once

#include "platform/Win32Min.h"

namespace obvr::mem {

// Writes into Oblivion's code segment and restores the original protection.
// Returns false when VirtualProtect fails.
bool SafeWrite(UInt32 address, const void* data, UInt32 size);

// Reads bytes from the process. Used to confirm, before patching, that the
// expected instruction really sits at the target address.
bool Verify(UInt32 address, const UInt8* expected, UInt32 size);

// Reserves executable memory for a trampoline.
void* AllocExecutable(UInt32 size);

}  // namespace obvr::mem

namespace obvr::mem {

// Says who was there first. Called at a site whose bytes failed Verify:
// reads what sits there, decodes a relative jump or call if that is what it
// is, names the module the target lies in, and writes one log line under the
// given subsystem prefix. Another plugin's detour at one of OBVR's sites is
// the documented shape of a mod conflict (Oblivion Reloaded plants one at
// the scene render entry), and a log that names the DLL turns "OBVR does
// nothing" into "these two cannot share this function".
void ReportForeignCode(const char* subsystem, UInt32 address);

}  // namespace obvr::mem

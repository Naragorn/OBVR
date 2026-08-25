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

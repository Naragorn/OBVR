#pragma once

#include "core/Types.h"

namespace obvr::camera {

// Generating the hook code, kept apart from installing the hook.
//
// The split has a practical reason: the byte sequence is the part where a
// mistake reliably crashes the game, and at the same time the only part that
// can be checked without a running Oblivion. Both functions are therefore
// free of Windows and process dependencies, and tests/ exercises them
// directly.

// The eight bytes that must sit at addr::kHookCameraUpdate:
//   cmp word ptr [ebx+0xB6], 0
extern const UInt8 kOriginalBytes[8];

// Writes the trampoline into buffer. trampolineAddress is where it will
// later live; callbackAddress points at OBVR_OnCameraUpdated.
//
// Returns the number of bytes written, or 0 when the capacity is not enough.
UInt32 BuildTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                       UInt32 callbackAddress);

// Writes the patch that goes to the hook site: a jump to the trampoline,
// padded out to the length of the overwritten original instruction.
//
// Returns the number of bytes written, or 0 when the capacity is too small.
UInt32 BuildPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                  UInt32 trampolineAddress);

}  // namespace obvr::camera

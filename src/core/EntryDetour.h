#pragma once

#include "core/Types.h"

namespace obvr::mem {

// Byte generation for a detour at the entry of a whole function, kept apart
// from installing it for the same reason the camera trampoline is: the bytes
// are the part where a mistake reliably crashes the game, and the only part
// that can be checked without a running Oblivion.
//
// The shape differs from the camera trampoline. That one interrupts the
// middle of a function and has to preserve every register and rebuild the
// control flow it displaced. An entry detour replaces the function's first
// instructions with a jump to a replacement that has the same calling
// convention, so there is nothing to preserve - the replacement is simply
// called instead. What remains is the way back in: the displaced entry
// instructions, followed by a jump to the first byte the patch did not touch.
// Calling that buffer is calling the original function.
//
// The displaced bytes are passed in rather than read from the process, so the
// generation stays free of process dependencies and a test can hand it any
// entry it likes. They must be whole instructions with nothing relative in
// them - a displaced rel32 would point 4 gigabytes minus a few bytes away
// from its target, and nothing here can tell.

// Writes entryBytes followed by a jump to entryAddress + entryLength.
// Returns the number of bytes written, or 0 when the capacity is too small
// or entryLength is shorter than the five bytes the patch needs.
UInt32 BuildEntryTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                            UInt32 entryAddress, const UInt8* entryBytes,
                            UInt32 entryLength);

// Writes the patch for the function's entry: a jump to the replacement,
// padded with nops out to entryLength so no torn instruction survives.
// Returns the number of bytes written - always entryLength - or 0 when the
// capacity is too small or entryLength is shorter than five bytes.
UInt32 BuildEntryPatch(UInt8* buffer, UInt32 capacity, UInt32 entryAddress,
                       UInt32 replacementAddress, UInt32 entryLength);

}  // namespace obvr::mem

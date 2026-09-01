#pragma once

#include "core/Types.h"

namespace obvr::camera {

// The hook that sits on MagicCaster::CastMagicItem, built the same way the
// camera's is: bytes here, installation elsewhere.
//
// The split is the same one CameraTrampoline explains - the byte sequence is
// where a mistake reliably crashes the game and is also the only part that can
// be checked without a running Oblivion, so it is kept free of Windows and
// exercised by tests/ directly.
//
// WHAT IT IS FOR. A spell goes where the caster's heading points, and so does
// walking, so turning the heading to aim makes the character walk that way for
// as long as the turn stands. Watching the engine's action field cut that from
// 1.32 seconds to 0.9 and could go no further, because nothing outside the
// engine knows when inside the cast animation the spell is made. This function
// is that moment. See addr::kHookMagicCastItem for the two sources that place
// it and for the shape of its prologue.

// The seven bytes that must sit at addr::kHookMagicCastItem:
//   push ebx / push esi / push edi / mov edi, [esp+0x10]
extern const UInt8 kCastOriginalBytes[7];

// Writes the trampoline into buffer. trampolineAddress is where it will live;
// callbackAddress points at OBVR_OnMagicCastItem.
//
// The callback is handed ecx, which holds `this` - the MagicCaster. It is
// pushed AFTER pushad and pushfd, which is safe because neither disturbs the
// register itself, only copies it.
//
// Returns the number of bytes written, or 0 when the capacity is not enough.
UInt32 BuildCastTrampoline(UInt8* buffer, UInt32 capacity, UInt32 trampolineAddress,
                           UInt32 callbackAddress);

// The patch for the hook site: a jump to the trampoline, padded to seven bytes
// so no half instruction is left behind.
//
// Returns the number of bytes written, or 0 when the capacity is too small.
UInt32 BuildCastPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                      UInt32 trampolineAddress);

}  // namespace obvr::camera

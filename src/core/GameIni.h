#pragma once

#include "core/Types.h"

namespace obvr::core {

// Oblivion's own INI, and the one setting in it that a headset needs changed.
//
// The render resolution belongs to the game rather than to OBVR, and it is the
// one thing OBVR cannot supply for it: a headset's view is nearly square, and
// spreading a 16:9 frame over it stretches the pixels vertically however
// correct the geometry is. The frustum can be written every frame; the frame's
// shape cannot, because Direct3D fixes it when the device is made.
//
// So this writes it, and the game picks it up on the next start. That delay is
// stated in the log rather than left to be discovered.

// Finds Oblivion.ini under the user's Documents. False when it cannot be
// located, which is not a failure worth stopping for - it only means the
// resolution stays whatever the game already had.
bool FindOblivionIni(char* out, UInt32 outSize);

// Reads the resolution currently written there. False if either value is
// missing or not a number.
bool ReadRenderSize(const char* path, UInt32& width, UInt32& height);

// Writes it. False when the file cannot be written - which happens when the
// game is running and holding it, so this is meant for startup.
bool WriteRenderSize(const char* path, UInt32 width, UInt32 height);

}  // namespace obvr::core

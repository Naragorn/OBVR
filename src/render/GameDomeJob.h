#pragma once

#include "core/Types.h"

namespace obvr::render {

// The sky dome (stage/DomeLook.h) made from the game's own textures and kept
// in OBVR_Stage\sky beside OBVR.dll. Called once a frame while the sky is
// the look asked for: the first call looks whether the kept dome still fits
// the game's textures (stage/GameDomeKey.h) and, when not, starts its making
// on a worker thread of low priority - about 2.5 s for the 8192 x 4096 atlas
// (measured 2026-11-15 outside the game, tools/make-loading-stage.cpp). True
// once the dome is there, its model's full path in modelPath. A call costs
// one atomic read while it is made and after.
//
// The making fails without a word to the player when a texture cannot be
// found or read (a texture mod in a format this does not decode, a missing
// archive): the reason goes to the log once and the dark dome stays.
bool GameDomeReady(char* modelPath, UInt32 modelPathSize);

}  // namespace obvr::render

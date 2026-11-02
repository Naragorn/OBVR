#pragma once

// What a hand script's picture is of ([Debug] HandScript, test/HandScript.h):
// the state on the frame of each mark, to the log - where the player stands
// and faces, the hands' bodies, the mode's state (the world, the menu, the
// view, the head, the controllers, the camera, the teleport, the weapon, the
// laser, the cursor), the flat picture's anchor, the tile under the cursor
// with its traits and the menu's vtable, the inventory's item under the
// pointer, the map's drag point, the nearest item and actor (the console-at
// lines' {near} and {actor}), the actors loaded, what the nearest wear
// (Debug.WornShapes), and the items near a hand and the eyes with what is
// held and what the crosshair is on. Diagnostics only: nothing of the game's
// state is changed here.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18e): the code as it was, no
// state of its own. Every scenario with a mark reads its lines; the grab and
// stow scenarios the items line, the roster scenarios the actors line.

#include "camera/hands/HandStep.h"
#include "game/NearbyItems.h"

namespace obvr::camera::hands {

// The picture on a mark's frame; nothing on any other.
void LogHandScriptPicture(const HandStep& step, const HandShared& shared, const vr::HandModeFrame& frame,
                          const game::NearItem& nearItem);

}  // namespace obvr::camera::hands

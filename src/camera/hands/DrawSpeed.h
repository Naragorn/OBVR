#pragma once

// Drawing and sheathing without the animation's second ([Hands]
// WeaponDrawSpeed, game/WeaponDrawSpeed.h): the Equip and Unequip sequences'
// time runs that many times faster, stepped once a frame, each hastening,
// shortening and release said the first several times.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18g): the code as it was, its
// one static now DrawSpeedState's member. Harness: every scenario that draws
// a weapon reads "the draw animation's time runs" (holster.txt,
// swing-lengths.txt).

#include "camera/hands/HandStep.h"

namespace obvr::camera::hands {

struct DrawSpeedState {
	UInt32 drawLinesLeft = 60;
};

void StepDrawSpeed(DrawSpeedState& state, const HandStep& step);

}  // namespace obvr::camera::hands

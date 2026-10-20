#pragma once

namespace obvr::game {

// At a hand script's mark: the five animation slots of the player's
// third-person animation data (the process's +0x17C) and first-person one
// (the player's +0x5CC) - each slot's sequence state (1 animating) and its
// group's code - and the engine's own answer whether a special idle plays on
// each (0x00472EA0 on the animation data, which Enhanced Camera reads as
// "not al, and 1"). For a scripted idle against the body IK (idle-play.txt,
// 2026-10-20): which body the idle lands on, and whether the engine counts
// it as playing while OBVR's writes stand.
void LogPlayerAnimationAtMark();

}  // namespace obvr::game

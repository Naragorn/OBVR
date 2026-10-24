#pragma once

#include "core/Types.h"

namespace obvr::game {

// Debug.FlameProbe: at every hand script mark, every object under the
// first-person "Torch" node and under the body's "Torch" node with its class,
// name and flags, and every time controller on it or on its properties with
// the controller's raw words - the flags at +0x08, the floats at +0x0C to
// +0x28 (frequency, phase, the key window, the start, last, weighted-last and
// scaled times by Gamebryo's layout) and the words at +0x2C to +0x54 (the
// target, the next controller, an interpolator, a flip controller's textures
// and index). For a flame that stands still (a Quest 3 tester's torch video,
// 2026-10-18, and the harness after it): the marks two seconds apart say
// whether the controllers' times move, whether they are active, and whether
// the body's own torch - updated with the world - differs from the
// first-person one. Bounded: 40 marks, 120 lines each.
void ProbeFlames(const char* mark);

// The first-person flame's particle emitter and update controllers' last,
// weighted-last and scaled times, logged for four frames after each mark at
// the moment named (before, between and after the world renders) - which
// render moves them where. `frameEnds` counts the frame.
void ProbeFlameTimes(const char* when, bool frameEnds);

}  // namespace obvr::game

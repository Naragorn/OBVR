#pragma once

#include "game/NiMath.h"

namespace obvr::camera {

// At a hand script's mark: where the eyes are and which way they look, as the
// picture is about to be built, beside the player's own angles - so a run's
// pictures can be read against numbers (the body's first harness run,
// 2026-10-11, looked at the floor with the head level) - and the body's head
// against them (a Quest 3 tester, beta 8: after a load, a dismount, a feed, a
// chair or a knockdown "the camera sometimes shifts to the side of the
// character's head, only returning to the correct position after a short
// delay"; view-offset-events.txt).
void LogEyesAtMark(const NiTransform& eyes);

}  // namespace obvr::camera

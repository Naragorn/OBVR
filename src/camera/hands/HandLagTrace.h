#pragma once

// [Debug] HandLagTrace=<frames>: for that many frames after each hand-script
// mark, one log line a frame with where the controllers put the hands against
// where the hands and the drawn weapon were drawn - after every pin of the
// frame, so what the eyes are shown. A sweep of the hands at an even speed
// then reads as how far, and so how long, the hands and the weapon trail the
// controllers, at whatever frame rate the run has (the hand script's `load`
// and `hitch`). The tester's half-swording "very sluggish" below 60 fps
// (free-weapons-spec, open bugs of 2026-11-06) is what it was made to
// measure.
//
// The line, world units:
//   Lag trace: <mark> <n> dt <ms> steps <count> <ms> | R <controller> <pin>
//   <bone> | L <controller> <bone> | W <weapon node> | lag <gap m> <gap deg>
//   | O <off hand drawn> | off lag <gap m> <gap deg>
// R's controller is where the right controller alone puts the hand bone, its
// pin where the pin was asked to put it (the weapon's weight and a held blade
// in it), its bone where the bone is; L's controller is the left controller's
// own place for its bone. W is the Weapon node, 0 0 0 without one. The lag is
// the weapon's weight's own gap (vr/WeaponWeight.h); O and off lag the off
// hand's as drawn and its gap (vr/HeldItemWeight.h). Steps are the physics
// planner's this frame.

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

void TraceHandLag(const vr::HandModeResult& hand, const vr::HandSettings& hands, const NiMatrix33& cameraRot,
                  const NiPoint3& cameraPos, const NiPoint3& sharedGripMetres, float perMetre, float dtSeconds,
                  bool mirrored);

}  // namespace obvr::camera::hands

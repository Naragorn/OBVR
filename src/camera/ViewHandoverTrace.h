#pragma once

// The camera's side of the menu view's terms (camera/MenuViewTerms.h) and of
// the view handover's measurement (camera/ViewHandover.h): the owners of what
// the last world frame built on and of this frame's build, and the log.

#include "camera/MenuViewTerms.h"
#include "game/NiMath.h"

namespace obvr::camera {

// What the last world frame laid on the engine's eye, for a menu frame.
const WorldViewTerms& LastWorldViewTerms();

// The camera hook, before the head's offset goes on: the terms, the engine's
// eye they were laid on, the head's offset in the world and the real-height
// lift. Kept for the menu frames and as this frame's build.
void NoteWorldView(const WorldViewTerms& terms, const NiPoint3& engineEye, const NiPoint3& head, float realLift);

// The camera hook, after the eye step: the final rotation, the step to the
// first eye and the shift to the second.
void NoteViewSteps(const NiMatrix33& view, const NiPoint3& firstEyeStep, const NiPoint3& shift);

// The menu stand-in (PlaceMenuCamera): the base as placed (terms included
// when termsUsed), the height with the real-height lift and the lift, the
// head's offset, the step to the first eye, the shift and the final rotation.
void NoteMenuView(const NiPoint3& placedBase, bool termsUsed, float height, float realLift, const NiPoint3& head,
                  const NiPoint3& firstEyeStep, const NiPoint3& shift, const NiMatrix33& view);

// At the world render's entry: the first eye's world place as drawn and the
// cyclopean camera the hands and the body used. Logs at a change between the
// hook and the stand-in and a few frames after (StepViewHandover); nothing
// per frame but a copy.
void TraceViewAsDrawn(const NiPoint3& eyeDrawn, const NiPoint3& cyclopean);

}  // namespace obvr::camera

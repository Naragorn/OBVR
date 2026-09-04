#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The hands on the controllers, bone by bone.
//
// The arms follow the right controller as a whole (FirstPersonArms), which
// is right until the animation moves the hand within the arm: an attack
// swings the hand and the weapon in it away from where the controller is,
// and the player watches a sword leave their hand. So the hand BONES are
// written each frame, after the animation and the arm placement: the bone's
// local transform is set so its world pose is the controller's, and the
// engine's update pass carries that into the weapon node under it and the
// skinned hand mesh over it. The animation still runs, on arms that are
// hidden (FirstPersonHide) and on hands that are overwritten before they are
// drawn - which is what makes a swing of the controller the swing that is
// seen, while the engine keeps deciding what a swing hits.
//
// Where a bone is written is its parent's space; the arithmetic is in
// BonePin.h and tested there.

// Writes one hand bone, found by name under the first-person root. The
// relative rotation and offset are the controller's relative to the head,
// in the game's axes and units, the way HandMode answers them; the
// calibration is the fixed turn between controller and bone axes. Answers
// whether the bone was found and written.
bool PinHandBone(bool rightHand, const char* boneName, const NiMatrix33& relativeRot,
                 const NiPoint3& offsetUnits, const NiMatrix33& calibration);

// Forgets the bones found, so a new model is searched afresh.
void ForgetHandBones();

}  // namespace obvr::game

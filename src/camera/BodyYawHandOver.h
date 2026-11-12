#pragma once

// A heading written into the player for something other than the camera
// pass's own turns (the aim's, the walk's) - the mod's slap facing the body
// to the slapped one (camera/hands/SlapTap.h) - handed over the way those
// turns are: the step goes into the body offset the camera takes back out
// (AimCameraShare, AimYawRemaining), so the body turns and the picture holds
// still.
//
// Written plainly, as the slap did until 2026-11-12, the camera is built
// from the turned heading next frame and nothing takes the turn back out:
// the view jumped by the whole turn (a slap in yield.txt turned the eyes
// about 120 degrees off the bandit, 796e5fb's note in physical-combat-spec.md).
//
// Steps are in the offset's sense (FrameLogic.h PlayerYawForGaze: the
// heading written is the engine's minus the step).

#include "core/MathFns.h"

namespace obvr::camera {

// The body offset and the landing check's record (CameraHook.cpp's
// g_aimBodyOffset, g_aimYawWrote, g_aimYawStepTaken, g_aimYawPending).
struct BodyYawBook {
	float offset = 0.0f;
	float wrote = 0.0f;
	float stepTaken = 0.0f;
	bool pending = false;
};

// The step from the heading in the player to the heading wanted, in the
// offset's sense: the engine's heading minus the wanted one, the short way
// round.
inline float HandOverStep(float engineYaw, float wantedYaw) {
	return math::WrapAngle(engineYaw - wantedYaw);
}

// Whether the camera takes a written heading back out. With a headset in
// first person the view is the head's, and a body turn must not move it;
// without one, or in third person, the view is the body's (or the chase
// camera's, which follows the body), and the old plain write stays.
inline bool HandOverKeepsView(bool headsetConnected, bool thirdPerson) {
	return headsetConnected && !thirdPerson;
}

// A write that landed in the player field, booked. A step already waiting
// for its landing check this frame (the aim's or the walk's, written in the
// camera pass before the slap's at Present) stays in the record: the check
// next frame compares the field with the last heading written and takes the
// whole of the frame's steps back out if it moved, so the steps add up.
inline void BookHandOver(BodyYawBook& book, float wroteYaw, float step) {
	book.offset = math::WrapAngle(book.offset + step);
	book.stepTaken = book.pending ? math::WrapAngle(book.stepTaken + step) : step;
	book.wrote = wroteYaw;
	book.pending = true;
}

// The write itself (BodyYawTurn.cpp): the player's heading set to the one
// wanted and, with the view kept, the step booked; without, a plain write.
// False when the heading could not be written.
bool WriteBodyYawKeepingView(float wantedYaw, bool keepView, BodyYawBook& book);

}  // namespace obvr::camera

#pragma once

// The view's height over the feet on top of the game's eye: the sneak's drop
// taken back (camera/SneakEyeHold.h) and the player's own height ([Head]
// RealHeight, vr/RealHeight.h). Taken out of CameraHook.cpp; the camera hook
// adds both to the camera's height each frame.

#include "core/Config.h"
#include "camera/SneakEyeHold.h"
#include "vr/HeadTracker.h"

namespace obvr::camera {

// What the camera hook knows this frame.
struct EyeHeightFrame {
	bool thirdPerson = false;
	bool deathViewHeld = false;
	bool headset = false;           // a headset connected
	bool playerValid = false;       // the player's position read
	float eyeAboveFeet = 0.0f;      // the game's eye over the player's feet, units
	bool crouched = false;          // the hand mode's real crouch, a frame old
	float dtSeconds = 0.0f;
	UInt32 frameCount = 0;
};

// The sneak's drop taken back this frame, units, with its log lines.
float StepSneakEyeLift(const Config& config, const EyeHeightFrame& f);

// The real height's lift taken (after a recenter, after each load) and said;
// at a hand script's mark the eye heights logged. `sneakLift` this frame's.
void StepRealHeight(vr::HeadTracker& tracker, const Config& config, const EyeHeightFrame& f, float sneakLift);

// The lift this frame: the setting on, the lift taken, first person on foot,
// out of the death view.
float RealHeightLiftNow(const vr::HeadTracker& tracker, const Config& config, bool thirdPerson, bool deathViewHeld);

// At a hand script's mark: the camera's eye over the player's feet as placed,
// the head's move and every lift in it.
void NoteViewEyeAtMark(bool playerValid, float viewEyeAboveFeet);

// The shown body's scale for this frame's lift (it is shown in first person).
float RealHeightBodyScaleNow(const vr::HeadTracker& tracker, const Config& config, bool deathViewHeld);

}  // namespace obvr::camera

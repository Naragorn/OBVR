#pragma once

#include "game/NiMath.h"

namespace obvr::game {

// The player's own body, shown in first person.
//
// The third-person skeleton is unhidden, its root is moved every frame so the
// head bone stands under the headset, and the head and both clavicles are
// collapsed to nothing for the draw so the face and the animated arms do not
// hang in the picture - the first-person arms and weapon stay where they were.
// After Enhanced Camera 1.4b; the arithmetic is in BodyPlacement.h, the
// engine sites in GameAddresses.h.
//
// Runs from the scene render's first callback, after animation has settled
// the frame and before anything is drawn - the only point where a skeleton
// write reaches the picture (see BeforeFirstScenePass).

struct BodyFrame {
	// The headset between the eyes, in the world.
	NiPoint3 cameraWorld{0.0f, 0.0f, 0.0f};
	// The eyes relative to Bip01 Head, in the root's axes: forward and up.
	NiPoint3 eyeOffsetUnits{0.0f, 14.0f, 6.0f};
	bool hideHead = true;
	bool hideArms = true;
};

// Shows and places the body for this frame. False when the skeleton could
// not be reached; the log says why, once.
bool ShowPlayerBody(const BodyFrame& frame);

// Puts the engine's hidden bit back on the third-person root if this code
// took it off - only while the player is still in first person, where the
// engine wants it hidden; in third person the engine shows the body itself
// and the bit is left alone. Safe to call every frame the body is not wanted.
void ReleasePlayerBody(bool isThirdPerson);

// The two POV-switch branches that skip the third-person body's setup in
// first person, replaced with NOPs the way Enhanced Camera does it. Verified
// before writing; false, with a log line, when the bytes are not the
// expected jumps.
bool InstallPlayerBodyPatches();

}  // namespace obvr::game

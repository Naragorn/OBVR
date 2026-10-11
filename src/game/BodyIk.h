#pragma once

#include "core/Types.h"
#include "game/BodyIkLogic.h"
#include "game/GameTypes.h"
#include "game/NiMath.h"

namespace obvr::game {

// The body IK's engine side: the third-person skeleton's bones found, put
// back, turned and written; BodyIkLogic.h has every decision, and why.
// game/PlayerBody.cpp calls the torso, the root's position and the legs
// around the body's placement; the camera hook calls the arms once the hands
// are pinned.
//
// Every bone written is booked: what it held before the write (the
// animation's pose) and what was written. At the start of the next frame a
// bone that still holds the write - the engine left it, as it does for any
// bone the playing animation does not key - is put back first, so nothing
// is ever computed from last frame's answer (a stretched forearm grown
// again on top of its stretch, a turn turned again), and a body given back
// to the engine (third person, a menu, the switch off) is given back as the
// animation had it. The log says once how many bones the engine left
// ("Body IK: of the n bones written ...").

struct BodyIkSettings {
	bool arms = false;   // [Body] IK and IKArms: each arm reaches its pinned hand
	bool torso = false;  // IK and IKTorso: the body turns with the head, the chest's own twist out
	bool legs = false;   // IK and IKLegs: the knees bend for a crouch
	TorsoYawSettings torsoYaw;
	float armStretch = 1.15f;  // ArmStretch: how much longer an arm may grow to reach its hand
};

// The start of a frame of the shown body (PlayerBody.cpp, before anything
// is read off it): the skeleton's bones found under `root`, and the ones
// written last frame put back where they still hold the write. False when
// the skeleton is not there - then the IK does nothing this frame.
bool BeginBodyIk(NiAVObject* root);

// The torso: the root turned so the body faces the head's yaw in the world
// plus the torso's yaw relative to the head (StepTorsoYaw, kept in the
// headset's tracking space), then the chest's own twist taken out over the
// three spine bones. Answers the body's world yaw, HeadingOf's sense - the
// root's own when the torso part is off or the head unread.
float StepBodyTorso(const BodyIkSettings& s, bool headValid, float headWorldYaw, float headTrackingYaw,
                    float dtSeconds);

// The root's local position as the placement wants it (BodyRootLocalPos),
// written through the same booking.
void WriteBodyRootPosition(const NiPoint3& localPos);

// The knees: the body was let down `loweredUnits` to put its eyes at the
// headset; the feet are lifted back (FeetLiftUnits) by bending each leg.
// Answers the lift.
float StepBodyLegs(const BodyIkSettings& s, float loweredUnits);

// After the pins, once a frame: each arm to its hand where it follows
// (ArmFollowsHand), collapsed with hideArms where it does not; while an arm
// follows, the body's own hands, what they hold and what hangs on a forearm
// hidden, and its arm shapes kept in view round the camera; and the
// first-person arm laid onto the body's - the same bones at the same places
// - so the hand's cuff meets the body's sleeve or skin.
struct BodyArmsFrame {
	BodyIkSettings ik;
	bool hideArms = true;      // [Body] HideArms, for an arm that does not follow
	bool rightPinned = false;  // the first-person right hand bone written this frame
	bool leftPinned = false;
	bool rolesSwapped = false;  // left-handed: the hands on each other's controllers
	NiPoint3 cameraWorld{0.0f, 0.0f, 0.0f};
	bool logState = false;      // a hand script's mark: the state line
};

// Answers which arms follow: bit 0 the right, bit 1 the left.
UInt32 StepBodyArms(const BodyArmsFrame& f);

// For a frame the body is not shown: every bone written put back where it
// still holds the write, under the body root the engine has now (none: only
// forgotten), and the hidden nodes shown again.
void ReleaseBodyIk(NiAVObject* rootNow);

// Whether anything is booked or hidden that ReleaseBodyIk would give back -
// so the root is looked for only then.
bool BodyIkHolds();

// Before the pins, every frame: the first-person arm bones the IK laid onto
// the body's put back the same way (the pins read the hand's own local
// transform), and - with `lookUp` - this frame's first-person arm bones
// found for the arms' step.
void RestoreFirstPersonArmWrites(bool lookUp);

}  // namespace obvr::game

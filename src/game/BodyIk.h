#pragma once

#include "core/Types.h"
#include "game/BodyIkLogic.h"
#include "game/GameTypes.h"
#include "game/KickLogic.h"
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
	bool lean = false;   // IK and IKLean: a lean bends the body at the hips, the feet staying (BodyLeanLogic.h)
	TorsoYawSettings torsoYaw;
	float armStretch = 1.15f;  // ArmStretch: how much longer an arm may grow to reach its hand
};

// The start of a frame of the shown body (PlayerBody.cpp, before anything
// is read off it): the skeleton's bones found under `root`, and the ones
// written last frame put back where they still hold the write. False when
// the skeleton is not there - then the IK does nothing this frame.
bool BeginBodyIk(NiAVObject* root);

// The body's arm bones as they read this frame, in world units: the upper
// arm (shoulder to elbow) and the forearm (elbow to wrist), for the
// skeleton's check (BodyPlacement.h, JudgeSkeleton). False, both left as
// they were, when the frame or the bones are not there.
bool ReadBodyArmUnits(UInt32 side, float& upperUnits, float& foreUnits);

// The frame given up after BeginBodyIk: nothing more is written to the
// skeleton this frame (the placement found it reads like no body).
void StandDownBodyIk();

// The torso: the root turned so the body faces the head's yaw in the world
// plus the torso's yaw relative to the head (StepTorsoYaw, kept in the
// headset's tracking space); the trunk - the chain from the root to the head
// - held at the standing idle's pose, taken once the player has stood idle
// for half a second (BodyIkLogic.h, "The trunk"), so no animation moves the
// body under the headset; then the chest's own twist taken out over the
// three spine bones. The feet are noted where the animation put them,
// before the trunk is held, for the legs. Answers the body's world yaw,
// HeadingOf's sense - the root's own when the torso part is off or the head
// unread.
struct BodyTorsoFrame {
	bool headValid = false;
	float headWorldYaw = 0.0f;
	float headTrackingYaw = 0.0f;
	float dtSeconds = 0.0f;
	bool standsIdle = false;  // PlayerStandsIdle this frame, for taking the trunk's pose
};
float StepBodyTorso(const BodyIkSettings& s, const BodyTorsoFrame& t);

// The root's local position as the placement wants it (BodyRootLocalPos),
// written through the same booking.
void WriteBodyRootPosition(const NiPoint3& localPos);

// The knees: the body was let down `loweredUnits` to put its eyes at the
// headset, and its trunk held where the animation had it lower or higher;
// each foot is solved back to where the animation put it on the ground
// (FootSolveWanted), by bending or straightening the leg. Answers the
// larger lift.
// The lean (BodyLeanLogic.h): after the root is placed, before the legs -
// the hips held at their anchor, the trunk turned at Bip01 Spine so the head
// still lands under the headset. Answers how far the root went up (the
// let-down the bend explains, and what the turn could not reach), to take
// off the legs' let-down.
float StepBodyLean(const BodyIkSettings& s, float loweredUnits, float dtSeconds);

float StepBodyLegs(const BodyIkSettings& s, float loweredUnits);

// The kick (game/KickLogic.h): this frame the weapon side's leg is driven
// from where the animation has its foot toward the kick's peak by `weight`,
// along `facing` (world, taken level) - StepBodyLegs solves it, the knee to
// a pole ahead and up. Set before the legs each frame; weight 0 gives the
// leg back to the animation. Nothing is drawn with IKLegs off or no body.
struct BodyKickPose {
	float weight = 0.0f;
	bool rightLeg = true;  // left-handed: the left
	NiPoint3 facing{0.0f, 1.0f, 0.0f};
	KickSettings settings;
};
void SetBodyKick(const BodyKickPose& pose);

// Where the kicking foot was put last frame, world - false when the kick
// moved no leg then (no kick, no body, IKLegs off).
bool BodyKickFoot(NiPoint3& out);

// After the pins, once a frame: each arm to its hand where it follows
// (ArmFollowsHand), collapsed with hideArms where it does not; while an arm
// follows, the body's own hands, what they hold and what hangs on a forearm
// hidden, and its arm shapes kept in view round the camera; and the
// first-person arm laid onto the body's - the same bones at the same places
// - so the hand's cuff meets the body's sleeve or skin.
struct BodyArmsFrame {
	BodyIkSettings ik;
	bool hideArms = true;      // [Body] HideArms, for an arm that does not follow
	bool keepInView = true;    // [Body] KeepInView: every shape of the body kept in view round the camera
	bool rightPinned = false;  // the first-person right hand bone written this frame
	bool leftPinned = false;
	bool rolesSwapped = false;  // left-handed: the hands on each other's controllers
	// Left-handed with the first-person skeleton drawn mirrored (BonePin.h
	// "The mirror"): each body arm reaches the first-person hand of the
	// OTHER side (the right arm the "Bip01 L Hand" bone, on the right
	// controller), and that side's first-person arm is laid onto it through
	// the skin's bind-pose partners (BoneMirror.h).
	bool mirrored = false;
	NiPoint3 cameraWorld{0.0f, 0.0f, 0.0f};
	float dtSeconds = 0.0f;     // for the elbow's ease
	bool logState = false;      // a hand script's mark: the state line
	// Debug.BodyTrace: this frame's "Body trace:" line, numbered.
	bool trace = false;
	UInt32 traceFrame = 0;
};

// Answers which arms follow: bit 0 the right, bit 1 the left.
UInt32 StepBodyArms(const BodyArmsFrame& f);

// What the body's placement had this frame, for the trace (PlayerBody.cpp
// notes it; the arms' step writes the line).
struct BodyPlacementTrace {
	NiPoint3 cameraWorld{0.0f, 0.0f, 0.0f};
	NiPoint3 parentWorld{0.0f, 0.0f, 0.0f};    // the skeleton's parent ("Player"), as the engine has it
	NiPoint3 headOffsetRaw{0.0f, 0.0f, 0.0f};  // the head bone from the root, as the animation left it
	NiPoint3 headOffsetUsed{0.0f, 0.0f, 0.0f};  // and as the placement took it
	float lowered = 0.0f;
};
void NoteBodyPlacement(const BodyPlacementTrace& t);

// After the world is drawn: how many of this frame's writes the engine left
// through the draw - the trace's "held" count. A bone the engine rewrote
// between the write and the draw never showed the write.
void NoteBodyRenderDone();

// The sheathed weapons' nodes on the body - "SideWeapon" (the hip),
// "BackWeapon" (the back), "Quiver" - mirrored across the body's middle,
// each when asked (vr/SheathMirror.h decides which: a left-hander's places,
// Nadi 2026-10-21), in its parent's frame (MirroredAcrossLocalZ). Booked like
// every bone the IK writes: put back at the next frame's start, given back
// by ReleaseBodyIk. A node whose parent's z is not the body's left-right
// (LateralAxisAgrees against the thighs or the clavicles, 30 degrees) is
// left where the game hangs it, said once. After BeginBodyIk, before the
// body is placed.
void StepBodySheaths(bool side, bool back, bool quiver);

// For a frame the body is not shown: every bone written put back where it
// still holds the write, under the body root the engine has now (none: only
// forgotten), and the hidden nodes shown again.
void ReleaseBodyIk(NiAVObject* rootNow);

// The soft push (SoftPushLogic.h): each palm near the body's BBB breast
// bones presses them away from itself, and they ease back when it leaves.
// Booked like every bone the IK writes, given back by ReleaseBodyIk. After
// the arms, with the hands pinned; a skeleton without the bones is left
// alone, said once per skeleton.
struct BodySoftPushFrame {
	bool enabled = false;          // [Body] SoftPush
	float reachUnits = 13.0f;      // SoftPushReach: how near a palm presses
	float mostUnits = 6.0f;        // SoftPushMost: how far the bones give at most
	float returnPerSecond = 8.0f;  // how fast they ease back
	NiPoint3 palm[2] = {};         // the right, the left, world
	bool palmValid[2] = {false, false};
	float dtSeconds = 0.0f;
	bool logState = false;  // a hand script's mark: the state line
};
void StepBodySoftPush(const BodySoftPushFrame& f);

// At a hand script's mark: every shape of the shown body with its world
// bound against the camera ("Body bounds:" lines) - the engine culls a shape
// by that bound, which follows the shape's node, not the bones it is
// skinned to.
void LogBodyBounds(const NiPoint3& cameraWorld, const char* mark);

// Whether anything is booked or hidden that ReleaseBodyIk would give back -
// so the root is looked for only then.
bool BodyIkHolds();

// Before the pins, every frame: the first-person arm bones the IK laid onto
// the body's put back the same way (the pins read the hand's own local
// transform), and - with `lookUp` - this frame's first-person arm bones
// found for the arms' step.
void RestoreFirstPersonArmWrites(bool lookUp);

}  // namespace obvr::game

#pragma once

// Two hands on a two-hander and half-swording (vr/TwoHandLogic.h,
// vr/HalfSwordLogic.h): the left grip closed on the handle puts the left hand
// on it the way the game's animation holds it, moving there over a moment;
// the weapon stays with the right hand - or, half-swording, the hands on the
// blade carry it. And the drawn weapon held in the hand as it rests, through
// the attack animations (HoldWeaponAtRest). Decided before the hands are
// pinned, in the camera pass (BeforeFirstScenePass): the order there matters -
// PoseHalfSwordWeapon after the right pin and before the left one,
// LogHalfSwordArrival after both, HoldWeaponAtRest standing back while the
// right hand is off the handle.
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.10): the functions as they were, the 26 file globals they
// shared now TwoHandState's members under the same names (the rest of OBVR
// reads them as g_twoHands.<name>); their own function statics stay inside
// them. Harness: two-hand-grip.txt, two-hand-half-sword(-off).txt,
// half-sword-right-hand.txt, half-sword-hilt-hard.txt.

#include "game/BonePin.h"
#include "game/GameTypes.h"
#include "vr/HalfSwordLogic.h"
#include "vr/HandMode.h"
#include "vr/TwoHandLogic.h"

namespace obvr::camera::hands {

// The left hand's share of the handle's pose (0: its controller's; above 0,
// PinLeftHandOnHandle places it), and where along the handle.
struct TwoHandPins {
	float handleWeight = 0.0f;
	float handAxial = 0.0f;
};

// The drawn weapon held in the hand as it rests, through the attack
// animations. The game's attack animations turn the Weapon node in the hand
// (_1stperson\twohandattackleft.kf keys it up to 17 degrees from the idle's
// pose and back; the idle, the block and the one-handed idle all hold it at
// the same pose, read with pyffi 2026-09-30). The pose it has while the
// player does nothing (action -1) is kept per weapon model and written back
// while any action plays - before the hands are pinned, so they carry it.
// Not for the bow, whose draw the animation is.
struct WeaponRest {
	const NiAVObject* node = nullptr;
	bool have = false;
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	float largestDegrees = 0.0f;
};

struct TwoHandState {
	// Two hands on a two-hander (StepTwoHands, vr/TwoHandLogic.h).
	vr::TwoHandState twoHand;
	bool twoHandLeftGripWas = false;
	vr::GripHoldState twoHandHold;  // the other hand's hold on the handle (vr::TwoHandHoldSettings)
	// The left hand's way from its controller to the handle (vr::StepTwoHandBlend),
	// how far the approach had shaped it when the grip closed (vr::HeldWeight),
	// and last frame's approach (vr::PreshapeWeight).
	float twoHandBlend = 0.0f;
	float twoHandFloor = 0.0f;
	float twoHandPreshape = 0.0f;
	// The drawn two-hander's pommel end along its axis, in the Weapon node's frame
	// (game::AxialExtentOf), measured once per model.
	// Keyed by the weapon's form: the "Weapon" node is the skeleton's own, the
	// same for every weapon, and keyed by it a staff kept a longsword's handle
	// all session (2026-09-30: grips 14, 36 and 48 units down the staff refused).
	const UInt8* twoHandMeasured = nullptr;
	bool twoHandPommelValid = false;
	float twoHandPommel = 0.0f;
	float twoHandTip = 0.0f;  // the far end, for a staff's shaft above the right hand
	// Where the right palm is along the Weapon node's axis on this weapon, measured with
	// its handle: a staff hangs further down the hand than the file's grip has it.
	float twoHandRightPalm = 0.0f;
	// Where the shaft lies across the Weapon node's axis at the right hand, in the
	// node's frame (y 0): a staff's is off it (vr::ShaftOffset).
	NiPoint3 twoHandShaft{0.0f, 0.0f, 0.0f};
	// Half-swording, the right hand on the blade (vr/HalfSwordLogic.h, step 2):
	// its hold, this frame's input (the weapon's release reads it), the hold the
	// weapon's blend comes from, and what changed this frame for the weapon's pose
	// (PoseHalfSwordWeapon). The weapon's pose against the left hand while that
	// hand carries it, against the right hand's parent node on the blade, the
	// game's own (rest) against that node, and the weapon as last placed, against
	// the camera (the lines of the next frame's grips).
	vr::RightHoldState rightHold;
	vr::RightHoldInput rightHoldIn;
	vr::RightHold rightHoldBefore = vr::RightHold::Handle;
	vr::RightHoldEvent rightHoldPending = vr::RightHoldEvent::None;
	bool rightHoldWas = false;
	vr::WeaponPose freeRel;
	vr::WeaponPose bladeLocal;
	vr::WeaponPose restLocal;
	// How far the free right hand has been drawn to the sword (its fingers take
	// the grip's shape as far, game::StepHandFingers' towardHandle).
	float rightApproach = 0.0f;
	bool rightApproachSaid = false;  // its line said once while free
	bool restLocalValid = false;
	vr::WeaponPose weaponInCamera;
	bool weaponInCameraValid = false;
	// Both hands have just come onto the blade: measured after the pins.
	bool halfSwordArrival = false;
	// The game's own left hand on the two-hander (vr::VanillaGrip), read from the
	// animation while the left hand is not held.
	vr::VanillaGrip vanillaGrip = vr::GameGripFromFiles();
	// The left grip belongs to the weapon's handle (StepTwoHands): the hands'
	// frame and the lead leave it alone.
	bool leftGripOnHandle = false;
	WeaponRest weaponRest;
};

// What the two-hander's steps read of the frame.
struct TwoHandEnv {
	const vr::HandModeResult& hand;  // the hand mode's result (g_hand)
	float deltaSeconds;              // the frame's (g_deltaSeconds)
	bool mirrored;                   // the first-person skeleton drawn mirrored (left-handed)
	// Where the drawn weapon's head stands out of its haft, along the Weapon
	// node's axis from the node (game::WeaponShape's headA); not a number
	// without a head read (choke-up, vr::WithHaft).
	float headAxial;
	// The other hand's controller as it reads (the grip's pull steps its way
	// onto the handle, vr::ApproachWeight).
	const vr::HandPose& other;
};

// A hand's palm, in the world: halfway from its bone to its middle finger's
// base (game::PalmCentre). False when either node is missing. Stateless.
bool PalmOf(bool right, NiPoint3& out);

void HoldWeaponAtRest(TwoHandState& s, const TwoHandEnv& env);
TwoHandPins StepTwoHands(TwoHandState& s, const TwoHandEnv& env, const vr::HandSettings& hands,
                         const NiMatrix33& cameraRot, const NiPoint3& cameraPos);
void PinLeftHandOnHandle(TwoHandState& s, const TwoHandEnv& env, const vr::HandSettings& hands, float weight,
                         float handAxial, const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                         const NiPoint3& sharedGripMetres, float perMetre);
void PoseHalfSwordWeapon(TwoHandState& s, const TwoHandEnv& env, const vr::HandSettings& hands,
                         const NiMatrix33& cameraRot, const NiPoint3& cameraPos, const NiPoint3& sharedGripMetres,
                         float perMetre);
void LogHalfSwordArrival(TwoHandState& s, const TwoHandEnv& env);

}  // namespace obvr::camera::hands

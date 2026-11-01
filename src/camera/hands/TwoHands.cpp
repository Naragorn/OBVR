#include "camera/hands/TwoHands.h"

#include <cstdio>

#include "core/Config.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/HandBones.h"
#include "game/MeleeHits.h"
#include "game/NearbyItems.h"
#include "game/PlayerAim.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {


// A hand's palm, in the world: halfway from its bone to its middle finger's
// base (game::PalmCentre, as the casting hand's effect). False when either
// node is missing.
bool PalmOf(bool right, NiPoint3& out) {
	const NiAVObject* const wrist = game::FindFirstPersonNode(right ? "Bip01 R Hand" : "Bip01 L Hand");
	const NiAVObject* const knuckle = game::FindFirstPersonNode(right ? "Bip01 R Finger2" : "Bip01 L Finger2");
	if (wrist == nullptr || knuckle == nullptr) {
		return false;
	}
	out = game::PalmCentre(wrist->worldTransform.pos, knuckle->worldTransform.pos);
	return true;
}

// The game's own left hand on the drawn two-hander, from this frame's
// animation, before anything is pinned (vr::VanillaGripFrom). Kept from the
// last frame it held the handle.
void ReadVanillaGrip(TwoHandState& s, const TwoHandEnv& /*env*/, const vr::HandSettings& hands, const NiAVObject* weapon) {
	static UInt32 s_lines = 8;
	const NiAVObject* const left = game::FindFirstPersonNode(hands.leftHandBone);
	NiPoint3 leftPalm{};
	NiPoint3 rightPalm{};
	if (left == nullptr || !PalmOf(false, leftPalm) || !PalmOf(true, rightPalm)) {
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Hands: the game's left hand on the two-hander not read - the hand or finger nodes are missing");
		}
		return;
	}
	const vr::VanillaGrip grip =
		vr::VanillaGripFrom(weapon->worldTransform.rot, weapon->worldTransform.pos, left->worldTransform.rot,
		                    left->worldTransform.pos, leftPalm, rightPalm, vr::kGamePalmOnHandleUnits);
	static bool s_readValid = false;
	if (grip.valid != s_readValid && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Hands: the game's left hand on the two-hander %s - its palm %.1f units %s the right palm, %.1f "
		         "from the weapon's axis; its bone at (%.1f %.1f %.1f) in the weapon's frame",
		         grip.valid ? "read" : "NOT on the handle in this animation (the last one kept)",
		         static_cast<double>(grip.Below() < 0.0f ? -grip.Below() : grip.Below()),
		         grip.Below() < 0.0f ? "below" : "above", static_cast<double>(grip.leftPalmLateral),
		         static_cast<double>(grip.pos.x), static_cast<double>(grip.pos.y), static_cast<double>(grip.pos.z));
	}
	s_readValid = grip.valid;
	if (grip.valid) {
		s.vanillaGrip = grip;
	}
}

// The drawn weapon held in the hand as it rests, through the attack
// animations. The game's attack animations turn the Weapon node in the hand
// (_1stperson\twohandattackleft.kf keys it up to 17 degrees from the idle's
// pose and back; the idle, the block and the one-handed idle all hold it at
// the same pose, read with pyffi 2026-09-30). In Full VR the hand is where the
// controller is, and so should the weapon be: a strike by motion presses
// attack, the animation then twisted the blade in the hand, and a left hand on
// its handle with it. The pose it has while the player does nothing (action
// -1) is kept per weapon model and written back while any action plays -
// before the hands are pinned, so they carry it. Not for the bow, whose draw
// the animation is.

void HoldWeaponAtRest(TwoHandState& s, const TwoHandEnv& /*env*/) {
	// Half-swording places the sword itself while the right hand is off the
	// handle (PoseHalfSwordWeapon); its pose then is no rest.
	if (s.rightHold.hold != vr::RightHold::Handle || s.rightHold.blend < 1.0f) {
		return;
	}
	SInt32 weaponType = -1;
	const bool drawn = game::EquippedWeaponForm(&weaponType) != nullptr &&
	                   game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
	const vr::EquippedKind kind = vr::KindOfWeaponType(weaponType);
	NiAVObject* const weapon = game::FindFirstPersonNode("Weapon");
	if (!drawn || weapon == nullptr || (kind != vr::EquippedKind::OneHand && kind != vr::EquippedKind::TwoHand)) {
		return;
	}
	if (weapon != s.weaponRest.node) {
		s.weaponRest = WeaponRest{};
		s.weaponRest.node = weapon;
	}
	if (game::ReadPlayerAction() < 0) {
		s.weaponRest.rot = weapon->localTransform.rot;
		s.weaponRest.pos = weapon->localTransform.pos;
		s.weaponRest.have = true;
		return;
	}
	if (!s.weaponRest.have) {
		return;
	}
	// How far the animation had turned it, for the log: the angle of the turn
	// from the rest to now.
	const NiMatrix33 turn = vr::Transposed(s.weaponRest.rot) * weapon->localTransform.rot;
	float cosine = (turn.data[0][0] + turn.data[1][1] + turn.data[2][2] - 1.0f) * 0.5f;
	cosine = cosine > 1.0f ? 1.0f : (cosine < -1.0f ? -1.0f : cosine);
	const float degrees = math::Atan2(math::Sqrt(1.0f - cosine * cosine), cosine) * math::kRadiansToDegrees;
	static UInt32 s_lines = 6;
	if (degrees > s.weaponRest.largestDegrees + 5.0f && s_lines > 0) {
		--s_lines;
		s.weaponRest.largestDegrees = degrees;
		OBVR_LOG("Hands: an animation turned the weapon %.1f degrees in the hand (player action %d) - held at its "
		         "rest",
		         static_cast<double>(degrees), static_cast<int>(game::ReadPlayerAction()));
	}
	weapon->localTransform.rot = s.weaponRest.rot;
	weapon->localTransform.pos = s.weaponRest.pos;
	game::UpdateNodeTransforms(weapon);
}

TwoHandPins StepTwoHands(TwoHandState& s, const TwoHandEnv& env, const vr::HandSettings& hands, const NiMatrix33& cameraRot, const NiPoint3& cameraPos) {
	TwoHandPins out;
	// The left hand's hold (vr/GripHold.h): the fingers closed round the
	// handle at an ordinary grip take it, as a weapon's grip and the holsters
	// do - not only the force click of a full squeeze (Nadi, 2026-10-27: "ab
	// normaler griff Stärke ... nicht nur auf 100 Prozent mit press so wie
	// B&S"). With [Hands] GripByPull 0 or no pull read, the click as before.
	const bool leftGrip = env.hand.leftHold;
	const bool closedNow = leftGrip && !s.twoHandLeftGripWas;
	s.twoHandLeftGripWas = leftGrip;
	s.leftGripOnHandle = false;

	// A two-hander, or a one-hander when both hands may hold those too
	// (vr::TwoHandWeapon). The game's own grip is read from the two-handed
	// animation only; a one-hander's keeps the left hand at the side.
	SInt32 weaponType = -1;
	const UInt8* const weaponForm = game::EquippedWeaponForm(&weaponType);
	const bool drawn = env.hand.rightHandValid && env.hand.leftHandValid && weaponForm != nullptr &&
	                   game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
	const vr::EquippedKind kind = vr::KindOfWeaponType(weaponType);
	const bool twoHander = drawn && kind == vr::EquippedKind::TwoHand;
	// The left hand with a torch in it (kept with a two-hander drawn,
	// [Hands] TorchWithTwoHander) does not take the handle.
	const bool torchInLeft =
		drawn && game::WornTorchForm() != nullptr && game::HandHoldsItem(false, hands.leftHandBone);
	const bool twoHanded =
		drawn && !torchInLeft && vr::TwoHandWeapon(hands.twoHand, twoHander, kind == vr::EquippedKind::OneHand);
	const NiAVObject* const handBone = game::FindFirstPersonNode(hands.rightHandBone);
	const NiAVObject* const weapon = game::FindFirstPersonNode("Weapon");
	if (handBone == nullptr || weapon == nullptr) {
		s.twoHand = vr::TwoHandState{};
		s.twoHandBlend = 0.0f;
		return out;
	}
	// The blade in the hand bone's frame (fixed by the skeleton), then where
	// the pinned hand will put it this frame.
	const NiPoint3 bladeInHand = vr::Transposed(handBone->worldTransform.rot) *
	                             (weapon->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f});
	const NiMatrix33 calibration = game::MirroredCalibration(
		game::HandCalibration(hands.rightHandRoll, hands.rightHandPitch, hands.rightHandYaw),
		env.mirrored);
	// The weapon hand as it is drawn, behind the controller by the weapon's
	// weight (vr/WeaponWeight.h): the left hand holds the handle where it is
	// seen.
	const NiPoint3 blade = cameraRot * (env.hand.weaponHandRotation * (calibration * bladeInHand));
	const NiPoint3 right = cameraPos + cameraRot * env.hand.weaponHandOffsetUnits;
	const NiPoint3 left = cameraPos + cameraRot * env.hand.leftHandOffsetUnits;

	// Only while the player does nothing (action -1): the tester's run of
	// 2026-09-30 read the game's hand at 14.6 and 16.4 units below the right
	// palm, frames of an attack animation that passed the check - the idle
	// holds it at 7.0.
	// A blade or blunt two-hander's idle only: the staff's own animation holds
	// the left hand elsewhere and passed the check - read from it, the grip
	// read "the game's own hand at 18.0", then at 1.5 on the one-hander drawn
	// next, and the left hand on both was put off the handle (the tester,
	// 2026-09-30 15:53: "greift iwo in den leeren raum").
	if (twoHander && !vr::IsStaffWeaponType(weaponType) && !s.twoHand.active && game::ReadPlayerAction() < 0) {
		ReadVanillaGrip(s, env, hands, weapon);
	}
	// The handle from the weapon's own model: its pommel end along the weapon's
	// axis, measured once per model, then taken from the right palm, so the
	// left hand takes hold on the handle, not in the air. A model that could
	// not be read is tried again about once a second: the first try can come
	// while the draw is still putting the weapon together (2026-09-30: "not
	// readable" on the first frame, and never again, so no handle all session).
	static UInt32 s_measureWait = 0;
	if (s_measureWait > 0) {
		--s_measureWait;
	}
	// Measured once the draw is over (action -1): during it the node can still
	// carry the model it had, or none.
	if (twoHanded && game::ReadPlayerAction() < 0 &&
	    (weaponForm != s.twoHandMeasured || (!s.twoHandPommelValid && s_measureWait == 0))) {
		// Logged once per model: its first try, and its measurement. The
		// retries used the lines up before (a staff's never showed).
		const bool firstTry = weaponForm != s.twoHandMeasured;
		s.twoHandMeasured = weaponForm;
		float tip = 0.0f;
		const NiPoint3 axis = weapon->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f};
		const NiMatrix33 toNode = vr::Transposed(weapon->worldTransform.rot);
		// The right palm along the node's axis first: the shaft is looked for
		// where that hand holds it.
		NiPoint3 rightPalm = weapon->worldTransform.pos;
		float palmLateral = 0.0f;
		s.twoHandRightPalm = s.vanillaGrip.rightPalmAxial;
		if (PalmOf(true, rightPalm)) {
			vr::AxialLateral(rightPalm, weapon->worldTransform.pos, axis, s.twoHandRightPalm, palmLateral);
		}
		const float band[2] = {s.twoHandRightPalm - vr::kShaftBandUnits, s.twoHandRightPalm + vr::kShaftBandUnits};
		NiPoint3 shaftCentre{};
		UInt32 shaftVertices = 0;
		s.twoHandPommelValid = game::AxialExtentOf(weapon, weapon->worldTransform.pos, axis, s.twoHandPommel, tip,
		                                           band, &shaftCentre, &shaftVertices);
		s.twoHandTip = tip;
		// A long plain shaft has its vertices far apart along it: none within
		// the band of the tester's second staff (2026-09-30 16:23, "0 vertices",
		// the hand "etwas versetzt") - then the wider band. A staff's only: a
		// sword's would take in its guard, and the two-hander is fine as it is.
		if (s.twoHandPommelValid && shaftVertices < vr::kShaftMinVertices && vr::IsStaffWeaponType(weaponType)) {
			const float wide[2] = {s.twoHandRightPalm - vr::kShaftWideBandUnits,
			                       s.twoHandRightPalm + vr::kShaftWideBandUnits};
			float wideLow = 0.0f;
			float wideHigh = 0.0f;
			game::AxialExtentOf(weapon, weapon->worldTransform.pos, axis, wideLow, wideHigh, wide, &shaftCentre,
			                    &shaftVertices);
		}
		// Where the shaft lies across the node's axis, in the node's frame.
		s.twoHandShaft = vr::ShaftOffset(s.twoHandPommelValid, shaftVertices,
		                                 toNode * (shaftCentre - weapon->worldTransform.pos));
		const NiPoint3 palmInNode = toNode * (rightPalm - weapon->worldTransform.pos);
		s_measureWait = s.twoHandPommelValid ? 0 : 90;
		static UInt32 s_handleLines = 32;
		if (s_handleLines > 0 && (s.twoHandPommelValid || firstTry)) {
			--s_handleLines;
			const float fromRight = s.twoHandPommel - s.twoHandRightPalm;
			OBVR_LOG("Hands: the %s's model (weapon type %d) - %s; pommel %.1f units %s the right palm, tip %.0f "
			         "above the weapon node, the right palm at (%.1f %.1f %.1f) in its frame; the shaft there %.1f "
			         "%.1f off its axis (%u vertices)",
			         twoHander ? "two-hander" : "one-hander", static_cast<int>(weaponType),
			         s.twoHandPommelValid ? "measured" : "not readable yet",
			         static_cast<double>(fromRight < 0.0f ? -fromRight : fromRight),
			         fromRight < 0.0f ? "below" : "above", static_cast<double>(tip), static_cast<double>(palmInNode.x),
			         static_cast<double>(palmInNode.y), static_cast<double>(palmInNode.z),
			         static_cast<double>(s.twoHandShaft.x), static_cast<double>(s.twoHandShaft.z), shaftVertices);
		}
	}
	vr::HandleSpan handle =
		weaponForm == s.twoHandMeasured
			? vr::HandleSpanFor(hands.twoHand, s.twoHandPommelValid,
			                    s.twoHandPommel - s.twoHandRightPalm, s.vanillaGrip.Below(),
			                    vr::IsStaffWeaponType(weaponType), s.twoHandTip - s.twoHandRightPalm)
			: vr::HandleSpan{};
	// A two-handed sword's blade is a place for the left hand too
	// (vr/HalfSwordLogic.h): above the right hand, past the guard.
	const bool halfSword = twoHander && vr::HalfSwordWeapon(GetConfig().hands.halfSword, weaponType);
	if (halfSword) {
		handle = vr::WithBlade(GetConfig().hands.halfSword, handle, s.twoHandTip - s.twoHandRightPalm);
	}
	float axial = 0.0f;
	float lateral = 0.0f;
	// While the right hand is off the handle (half-swording), the weapon's line
	// is where it was last placed, not along the right controller.
	NiPoint3 lineOrigin = right;
	NiPoint3 lineAxis = blade;
	if (s.rightHold.hold != vr::RightHold::Handle && s.weaponInCameraValid) {
		const vr::WeaponPose w = vr::Compose(vr::WeaponPose{cameraRot, cameraPos}, s.weaponInCamera);
		lineAxis = w.rot * NiPoint3{0.0f, 1.0f, 0.0f};
		lineOrigin = w.pos + w.rot * NiPoint3{0.0f, s.twoHandRightPalm, 0.0f};
	}
	vr::AxialLateral(left, lineOrigin, lineAxis, axial, lateral);
	// The handle's point nearest the left hand, and how far off it is.
	const vr::HandleReach reach = vr::NearestOnHandle(handle, s.vanillaGrip.Below(), axial, lateral);
	// The open hand coming near the handle already takes the grip's shape
	// (vr::PreshapeWeight).
	float preshape = 0.0f;
	if (twoHanded && !s.twoHand.active && !leftGrip) {
		preshape = vr::PreshapeWeight(hands.twoHand, reach.distance);
	}

	if (!s.twoHand.active) {
		if (vr::TwoHandTakes(hands.twoHand, twoHanded, closedNow, axial, lateral, handle)) {
			const float held = vr::LeftHandAxial(handle, axial, s.vanillaGrip.Below());
			vr::StartTwoHand(s.twoHand, axial, held);
			// From where the approach had shaped the hand, the rest of the way.
			s.twoHandFloor = s.twoHandBlend > 0.0f ? 0.0f : s.twoHandPreshape;
			OBVR_LOG("Hands: the left hand took the %s %.1f units %s the right palm (closed at %.0f, %.0f "
			         "from the line; the game's own hand at %.1f; the handle %s; already shaped %.2f) - held with "
			         "both hands, the game's grip",
			         halfSword && held > 0.0f ? "two-handed sword's blade (half-swording)" : vr::IsStaffWeaponType(weaponType) ? "staff's shaft" : twoHander ? "two-hander's handle" : "one-hander's handle",
			         static_cast<double>(held < 0.0f ? -held : held), held < 0.0f ? "below" : "above",
			         static_cast<double>(axial),
			         static_cast<double>(lateral), static_cast<double>(-s.vanillaGrip.Below()),
			         handle.valid ? "measured" : "not measured", static_cast<double>(s.twoHandFloor));
		} else if (closedNow && twoHanded) {
			static UInt32 s_missLines = 6;
			if (s_missLines > 0) {
				--s_missLines;
				OBVR_LOG("Hands: the left grip closed off the handle - %.0f units along the weapon, %.0f from its "
				         "line",
				         static_cast<double>(axial), static_cast<double>(lateral));
			}
		}
	} else {
		const float openBefore = s.twoHand.openSeconds;
		if (!vr::TwoHandHolds(s.twoHand, hands.twoHand, twoHanded, leftGrip, reach.distance, env.deltaSeconds)) {
			OBVR_LOG("Hands: the left hand let go of the handle (%s)",
			         !twoHanded ? "the weapon went"
			                    : !leftGrip ? "the grip opened"
			                                : "the hand left the handle");
		} else {
			// Held on through a grip that read open for a moment: how often, and
			// how long - whether the controller's grip flickers.
			static UInt32 s_flickerLines = 8;
			if (leftGrip && openBefore > 0.0f && s_flickerLines > 0) {
				--s_flickerLines;
				OBVR_LOG("Hands: the left grip read open for %.3f s and closed again - held on to the handle",
				         static_cast<double>(openBefore));
			}
			// The hand slides along the handle with its controller while the
			// trigger is held, and stays put when it is let go.
			static bool s_slideLogged = false;
			if (vr::TwoHandSlides(hands.twoHand, env.hand.leftTriggerDown) && s.rightHold.hold == vr::RightHold::Handle) {
				// On its own side of the right hand (a staff's shaft is both).
				const float slid = vr::SlideAxial(handle, s.twoHand.handAxial, axial, s.vanillaGrip.Below());
				if (!s_slideLogged && slid != s.twoHand.handAxial) {
					s_slideLogged = true;
					OBVR_LOG("Hands: the left hand slides along the handle with the trigger held - from %.1f to %.1f "
					         "units from the right palm",
					         static_cast<double>(s.twoHand.handAxial), static_cast<double>(slid));
				}
				s.twoHand.handAxial = slid;
			}
		}
	}
	// The right hand on the blade too (vr/HalfSwordLogic.h, step 2): letting
	// go while the left holds the blade, closing again on the blade above it
	// or on the handle. The weapon's pose follows after the right hand's pin
	// (PoseHalfSwordWeapon).
	{
		const vr::RightHold before = s.rightHold.hold;
		const bool rightHold = env.hand.rightHold;
		vr::RightHoldInput& rin = s.rightHoldIn;
		rin = vr::RightHoldInput{};
		rin.halfSwordDrawn = halfSword && drawn;
		rin.leftHolds = s.twoHand.active;
		rin.leftAxial = s.twoHand.handAxial;
		rin.rightHold = rightHold;
		rin.rightClosedNow = rightHold && !s.rightHoldWas;
		s.rightHoldWas = rightHold;
		vr::AxialLateral(right, lineOrigin, lineAxis, rin.rightAxial, rin.rightLateral);
		rin.handle = handle;
		rin.dt = env.deltaSeconds;
		const vr::RightHoldEvent e = vr::StepRightHold(s.rightHold, GetConfig().hands.halfSword, hands.twoHand, rin);
		if (e != vr::RightHoldEvent::None && e != vr::RightHoldEvent::Missed) {
			s.rightHoldBefore = before;
			s.rightHoldPending = e;
		}
		static UInt32 s_lines = 40;
		if (e != vr::RightHoldEvent::None && s_lines > 0) {
			--s_lines;
			OBVR_LOG("Half-sword: the right hand %s (it at %.1f units along the sword from the handle's palm, %.1f "
			         "from its line; the left hand at %.1f)",
			         e == vr::RightHoldEvent::LetGo      ? "let go - the left hand carries the sword"
			         : e == vr::RightHoldEvent::TookBlade  ? "took the blade - the sword moves into it"
			         : e == vr::RightHoldEvent::TookHandle ? "took the handle again - the game's hold"
			         : e == vr::RightHoldEvent::Missed     ? "closed on neither the blade above the left hand nor the handle"
			         : e == vr::RightHoldEvent::Lost       ? "and the left both let go - nobody holds the sword"
			                                               : "is back on the handle - the sword went or was sheathed",
			         static_cast<double>(rin.rightAxial), static_cast<double>(rin.rightLateral),
			         static_cast<double>(rin.leftAxial));
		}
	}
	// To the handle while held, back to the controller after; at once when the
	// weapon is no longer one both hands hold.
	s.twoHandBlend = twoHanded
	                     ? vr::StepTwoHandBlend(s.twoHandBlend, s.twoHand.active, env.deltaSeconds, hands.twoHand)
	                     : 0.0f;
	if (!s.twoHand.active && s.twoHandBlend <= 0.0f) {
		s.twoHandFloor = 0.0f;
	}
	s.twoHandPreshape = preshape;
	// The left grip is the handle's while held, and while the open hand is near
	// enough that closing it would take the handle: it does not grab
	// (HandFrame::leftGripOnHandle, next frame).
	s.leftGripOnHandle = s.twoHand.active || preshape > 0.5f;
	// Held (or on the way back) or approaching, whichever shapes the hand more.
	const float heldWeight = s.twoHandBlend > 0.0f ? vr::HeldWeight(s.twoHandFloor, s.twoHandBlend) : 0.0f;
	if (heldWeight >= preshape) {
		out.handleWeight = heldWeight;
		out.handAxial = s.twoHand.handAxial;
	} else {
		out.handleWeight = preshape;
		out.handAxial = reach.axial;
	}
	static bool s_preshapeLogged = false;
	if (!s_preshapeLogged && preshape > 0.5f) {
		s_preshapeLogged = true;
		OBVR_LOG("Hands: the open left hand near the %s's handle takes its shape - %.2f of it, %.1f units "
		         "below the right palm",
		         twoHander ? "two-hander" : "one-hander", static_cast<double>(preshape),
		         static_cast<double>(-reach.axial));
	}
	return out;
}

// The left hand on the handle, after the right hand is pinned: the game's own
// grip against the weapon where the pinned right hand now holds it, moved
// along the handle to where it took hold (vr::LeftHandOnHandle), and on the
// way there or back blended with where its controller would put it by
// `weight`. Through the same pin as a held hand (an exact world pose, no
// calibration or grip).
void PinLeftHandOnHandle(TwoHandState& s, const TwoHandEnv& env, const vr::HandSettings& hands, float weight, float handAxial,
                         const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                         const NiPoint3& sharedGripMetres, float perMetre) {
	const NiAVObject* const weapon = game::FindFirstPersonNode("Weapon");
	if (weapon == nullptr) {
		return;
	}
	// Where the controller alone would put the hand: its turn is kept on the
	// handle (vr::LeftHandOnHandle), and the way there starts from it.
	const bool mirrored = env.mirrored;
	const NiPoint3 grip = game::MirroredGrip(
		(sharedGripMetres + NiPoint3{hands.leftHandGripX, hands.leftHandGripY, hands.leftHandGripZ}) * perMetre,
		mirrored);
	const game::BonePose own = game::HandBoneWorld(
		cameraRot, cameraPos, env.hand.leftHandRotation, env.hand.leftHandOffsetUnits,
		game::MirroredCalibration(game::HandCalibration(hands.leftHandRoll, hands.leftHandPitch, hands.leftHandYaw),
	                              mirrored),
		grip);
	// The right palm where this weapon has it, this frame (the pinned hand
	// carries the weapon: where the palm lies along it does not change).
	float rightPalmAxial = s.vanillaGrip.rightPalmAxial;
	NiPoint3 rightPalmNow{};
	// Not while the right hand is off the handle (half-swording): the left hand
	// keeps its place on the sword, wherever the right hand is.
	if (s.rightHold.hold == vr::RightHold::Handle && PalmOf(true, rightPalmNow)) {
		float offLine = 0.0f;
		vr::AxialLateral(rightPalmNow, weapon->worldTransform.pos,
		                 weapon->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f}, rightPalmAxial, offLine);
	}
	// The handle's line: the node's axis, moved across to the model's shaft.
	const NiPoint3 lineOrigin = weapon->worldTransform.pos + weapon->worldTransform.rot * s.twoHandShaft;
	vr::LeftHandPose pose = vr::LeftHandOnHandle(s.vanillaGrip, handAxial, weapon->worldTransform.rot, lineOrigin,
	                                             own.rot, rightPalmAxial);
	if (weight < 1.0f && env.hand.leftHandValid) {
		pose.rot = game::BlendRotation(own.rot, pose.rot, weight);
		pose.pos = own.pos + (pose.pos - own.pos) * weight;
	}
	const NiMatrix33 inverseCamera = InverseRotation(cameraRot);
	game::PinHandBone(false, hands.leftHandBone, inverseCamera * pose.rot, inverseCamera * (pose.pos - cameraPos),
	                  NiMatrix33::Identity(), cameraRot, cameraPos, NiPoint3{0.0f, 0.0f, 0.0f});
	// Once per hold, the hand arrived: where its palm is against the handle's
	// line and against the right palm - both palms equally far off the line is
	// both hands on the same shaft.
	static UInt32 s_lines = 24;
	static float s_loggedAxial = 1e9f;
	if (weight < 1.0f) {
		if (weight <= 0.0f) {
			s_loggedAxial = 1e9f;
		}
		return;
	}
	NiPoint3 leftPalm{};
	NiPoint3 rightPalm{};
	if (s_lines > 0 && s_loggedAxial != handAxial && PalmOf(false, leftPalm) && PalmOf(true, rightPalm)) {
		--s_lines;
		s_loggedAxial = handAxial;
		float axial = 0.0f;
		float lateral = 0.0f;
		float leftAlong = 0.0f;
		float leftOff = 0.0f;
		float rightAlong = 0.0f;
		float rightOff = 0.0f;
		const NiPoint3 axis = weapon->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f};
		vr::AxialLateral(leftPalm, rightPalm, axis, axial, lateral);
		vr::AxialLateral(leftPalm, lineOrigin, axis, leftAlong, leftOff);
		vr::AxialLateral(rightPalm, lineOrigin, axis, rightAlong, rightOff);
		OBVR_LOG("Hands: the left hand drawn on the handle - its palm %.1f units along the weapon from the right palm "
		         "(wanted %.1f), %.1f from the right palm's line; off the handle's line the left palm %.1f, the right "
		         "%.1f",
		         static_cast<double>(axial), static_cast<double>(handAxial), static_cast<double>(lateral),
		         static_cast<double>(leftOff), static_cast<double>(rightOff));
	}
}

// Where the left controller alone would put the left hand bone (world): its
// pose without the handle, as PinLeftHandOnHandle starts from.
game::BonePose LeftHandOwnPose(TwoHandState& /*s*/, const TwoHandEnv& env, const vr::HandSettings& hands, const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                               const NiPoint3& sharedGripMetres, float perMetre) {
	const bool mirrored = env.mirrored;
	const NiPoint3 grip = game::MirroredGrip(
		(sharedGripMetres + NiPoint3{hands.leftHandGripX, hands.leftHandGripY, hands.leftHandGripZ}) * perMetre,
		mirrored);
	return game::HandBoneWorld(
		cameraRot, cameraPos, env.hand.leftHandRotation, env.hand.leftHandOffsetUnits,
		game::MirroredCalibration(game::HandCalibration(hands.leftHandRoll, hands.leftHandPitch, hands.leftHandYaw),
	                              mirrored),
		grip);
}

// The sword placed for the right hand's hold (vr/HalfSwordLogic.h, step 2),
// after the right hand is pinned and before the left hand is put on it: on the
// handle the game's own pose (not written once arrived); free, carried by the
// left hand as it was when the right let go; on the blade, moved in the right
// hand so the fist holds the blade where it closed - the least turn onto the
// fist, either way round. A change of hold moves it over the blend. Written
// as the Weapon node's local transform, as HoldWeaponAtRest writes it.
void PoseHalfSwordWeapon(TwoHandState& s, const TwoHandEnv& env, const vr::HandSettings& hands, const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                         const NiPoint3& sharedGripMetres, float perMetre) {
	NiAVObject* const weapon = game::FindFirstPersonNode("Weapon");
	const vr::WeaponPose camera{cameraRot, cameraPos};
	if (weapon == nullptr || weapon->parent == nullptr) {
		s.weaponInCameraValid = false;
		s.rightHoldPending = vr::RightHoldEvent::None;
		return;
	}
	const NiTransform& pw = weapon->parent->worldTransform;
	const float scale = pw.scale > 1e-3f ? pw.scale : 1.0f;
	const vr::WeaponPose parent{pw.rot, pw.pos};
	// A local pose carried into the world by the parent, and back.
	const auto toWorld = [&](const vr::WeaponPose& local) {
		vr::WeaponPose w;
		w.rot = parent.rot * local.rot;
		w.pos = parent.pos + parent.rot * (local.pos * scale);
		return w;
	};
	const auto toLocal = [&](const vr::WeaponPose& world) {
		vr::WeaponPose l = vr::RelativeTo(parent, world);
		l.pos = l.pos * (1.0f / scale);
		return l;
	};
	const vr::WeaponPose current{weapon->worldTransform.rot, weapon->worldTransform.pos};
	const bool gameHold = s.rightHold.hold == vr::RightHold::Handle && s.rightHold.blend >= 1.0f &&
	                      s.rightHoldPending == vr::RightHoldEvent::None;
	if (gameHold || !s.restLocalValid) {
		// The game's own hold: kept as the rest to come back to.
		s.restLocal = vr::WeaponPose{weapon->localTransform.rot, weapon->localTransform.pos};
		s.restLocalValid = true;
	}
	if (gameHold) {
		s.weaponInCamera = vr::RelativeTo(camera, current);
		s.weaponInCameraValid = true;
		return;
	}
	const game::BonePose ownLeft = LeftHandOwnPose(s, env, hands, cameraRot, cameraPos, sharedGripMetres, perMetre);
	const vr::WeaponPose left{ownLeft.rot, ownLeft.pos};
	const auto holdPose = [&](vr::RightHold h) {
		switch (h) {
		case vr::RightHold::Free: return vr::Compose(left, s.freeRel);
		case vr::RightHold::Blade: return toWorld(s.bladeLocal);
		default: return toWorld(s.restLocal);
		}
	};
	static UInt32 s_lines = 24;
	static bool s_arrivalSaid = true;
	const vr::RightHoldEvent pending = s.rightHoldPending;
	s.rightHoldPending = vr::RightHoldEvent::None;
	if (pending == vr::RightHoldEvent::LetGo) {
		// Carried by the left hand from where the sword is now.
		s.freeRel = vr::RelativeTo(left, holdPose(s.rightHoldBefore));
	} else if (pending == vr::RightHoldEvent::TookBlade) {
		// The fist's line and its point on it, where the game's hold has the
		// handle in this hand.
		const vr::WeaponPose rest = toWorld(s.restLocal);
		const NiPoint3 fistAxis = rest.rot * NiPoint3{0.0f, 1.0f, 0.0f};
		const NiPoint3 fistPoint = rest.pos + rest.rot * (s.twoHandShaft + NiPoint3{0.0f, s.twoHandRightPalm, 0.0f});
		bool reversed = false;
		const vr::WeaponPose bladeWorld = vr::BladeHoldPose(holdPose(vr::RightHold::Free), fistAxis, fistPoint,
		                                                    s.twoHandRightPalm + s.rightHold.gripAxial, &reversed);
		s.bladeLocal = toLocal(bladeWorld);
		s_arrivalSaid = false;
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Half-sword: the right hand holds the blade %.1f units above the handle's palm - %s",
			         static_cast<double>(s.rightHold.gripAxial),
			         reversed ? "the other way round, the hilt ahead of the little finger"
			                  : "the tip ahead of the thumb, as the handle is held");
		}
	} else if (pending == vr::RightHoldEvent::Lost || pending == vr::RightHoldEvent::Reset) {
		// The game's own hold back at once (a drop or a sheathe follows).
		weapon->localTransform.rot = s.restLocal.rot;
		weapon->localTransform.pos = s.restLocal.pos;
		game::UpdateNodeTransforms(weapon);
		s.weaponInCameraValid = false;
		return;
	}
	const vr::WeaponPose target = holdPose(s.rightHold.hold);
	const vr::WeaponPose placed =
		s.rightHold.blend < 1.0f
			? vr::BlendPose(holdPose(s.rightHoldBefore), target, vr::TwoHandBlendWeight(s.rightHold.blend))
			: target;
	const vr::WeaponPose local = toLocal(placed);
	weapon->localTransform.rot = local.rot;
	weapon->localTransform.pos = local.pos;
	game::UpdateNodeTransforms(weapon);
	s.weaponInCamera = vr::RelativeTo(camera, placed);
	s.weaponInCameraValid = true;
	// Once the sword has come into the right hand on the blade, the palms are
	// measured after the left hand is put on it (LogHalfSwordArrival).
	if (!s_arrivalSaid && s.rightHold.hold == vr::RightHold::Blade && s.rightHold.blend >= 1.0f) {
		s_arrivalSaid = true;
		s.halfSwordArrival = true;
	}
}

// Where the two palms are along the sword once both hands hold its blade,
// against where they were wanted - after both hands are pinned.
void LogHalfSwordArrival(TwoHandState& s, const TwoHandEnv& /*env*/) {
	if (!s.halfSwordArrival) {
		return;
	}
	s.halfSwordArrival = false;
	static UInt32 s_lines = 12;
	const NiAVObject* const weapon = game::FindFirstPersonNode("Weapon");
	NiPoint3 rightPalm{};
	NiPoint3 leftPalm{};
	if (s_lines == 0 || weapon == nullptr || !PalmOf(true, rightPalm) || !PalmOf(false, leftPalm)) {
		return;
	}
	--s_lines;
	const NiPoint3 axis = weapon->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f};
	const NiPoint3 origin = weapon->worldTransform.pos + axis * s.twoHandRightPalm;
	float ra = 0.0f;
	float rl = 0.0f;
	float la = 0.0f;
	float ll = 0.0f;
	vr::AxialLateral(rightPalm, origin, axis, ra, rl);
	vr::AxialLateral(leftPalm, origin, axis, la, ll);
	OBVR_LOG("Half-sword: both hands on the blade - the right palm %.1f units along it from the handle's palm "
	         "(wanted %.1f), %.1f off its line; the left palm %.1f (wanted %.1f), %.1f off",
	         static_cast<double>(ra), static_cast<double>(s.rightHold.gripAxial), static_cast<double>(rl),
	         static_cast<double>(la), static_cast<double>(s.twoHand.handAxial), static_cast<double>(ll));
}

}  // namespace obvr::camera::hands

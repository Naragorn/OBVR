#include "camera/hands/Swim.h"

#include "core/Log.h"
#include "game/KickLogic.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/AimAtSource.h"
#include "game/Shove.h"
#include "game/SwimStep.h"
#include "test/HandScriptRuntime.h"
#include "vr/HeadOffset.h"
#include "vr/Holster.h"
#include "vr/RoomFollowLogic.h"

namespace obvr::camera::hands {

namespace {

// The buzz in a hand that pulls through the water, seconds between pulses.
constexpr float kBuzzEverySeconds = 0.1f;

// What each hand holds or reaches for, from the game and the hand mode's
// result (vr::SwimHandBusy decides).
vr::SwimBusyInput BusyNow(const vr::HandModeResult& hand) {
	vr::SwimBusyInput in;
	SInt32 type = -1;
	game::EquippedWeaponForm(&type);
	const vr::EquippedKind kind = vr::KindOfWeaponType(type);
	in.weaponDrawn = game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
	in.twoHanderOrBow = kind == vr::EquippedKind::TwoHand || kind == vr::EquippedKind::Bow;
	in.torchHeld = game::WornTorchForm() != nullptr;
	in.grabHolds = game::PlayerHoldsGrab();
	in.grabWithLeft = hand.grabWithLeftHand;
	in.claimed[0] = hand.holster.rightClaimed;
	in.claimed[1] = hand.holster.leftClaimed;
	return in;
}

// A hand's velocity against the head, in the world: its velocity in the head's frame less the
// head's own (differenced from the pose), turned by the camera's rotation.
vr::SwimHand HandNow(bool valid, bool grip, const NiPoint3& velocityInHeadFrame, const NiPoint3& headInHeadFrame,
                     const NiMatrix33& cameraRotation, bool busy) {
	vr::SwimHand h;
	h.tracked = valid;
	h.grip = grip;
	h.busy = busy;
	h.velocity = cameraRotation * vr::SwimRelativeVelocity(velocityInHeadFrame, headInHeadFrame);
	return h;
}

// The game sees a swimmer: a tiny push in the stroke's direction (the horizontal part of
// the velocity in the player's frame, else ahead) as the stick's, and its keys.
void ShowGameTheDirection(const NiPoint3& velocity, game::AnalogWalkRequest& request,
                          vr::HandControlsWanted& controls) {
	float x = 0.0f;
	float y = 1.0f;
	float rotZ = 0.0f;
	if (game::PlayerHeadingRadians(rotZ)) {
		vr::SwimNudgeDirection(velocity, rotZ, x, y);
	}
	request.active = true;
	request.stick = vr::AnalogStick{kSwimNudgeAmount, x, y};
	request.running = false;
	request.forcedRun = false;
	request.sprinting = false;
	controls.move = vr::AnalogStickKeys(request.stick);
}

void Buzz(const HandStep& step, bool rightRole, bool rolesSwapped, float amplitude, float seconds) {
	step.backend.Pulse(vr::HandDeviceForRole(rightRole, rolesSwapped), seconds, 120.0f, amplitude);
}

void LogSwim(SwimArmsState& state, const vr::SwimVerdict& verdict, bool swimming, float swimSpeed, SInt32 stateCode) {
	if (swimming != state.wasSwimming && state.lines > 0) {
		--state.lines;
		OBVR_LOG("Swim: %s (the game's swim speed %.1f units a second)",
		         swimming ? "the player swims - the arms' strokes are on" : "out of the water - the strokes are off",
		         static_cast<double>(swimSpeed));
	}
	if (verdict.holding != state.wasHolding && state.lines > 0) {
		--state.lines;
		OBVR_LOG("Swim: %s", verdict.holding ? "a hand holds the water" : "the water let go");
	}
	if (test::HandScriptMarkedThisFrame()) {
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		game::ReadPlayerFeet(feet);
		OBVR_LOG("Swim: at the mark - %s (the controller's state %d), %s, the swimmer at %.1f units a second (%.1f %.1f "
		         "%.1f), %.0f units carried by the arms, the feet at %.1f %.1f %.1f",
		         swimming ? "swimming" : "not swimming", stateCode,
		         verdict.holding ? (verdict.pulling ? "pulling" : "holding the water") : "no hand holds the water",
		         static_cast<double>(math::Sqrt(verdict.velocity.LengthSquared())), static_cast<double>(verdict.velocity.x),
		         static_cast<double>(verdict.velocity.y), static_cast<double>(verdict.velocity.z),
		         static_cast<double>(state.swum), static_cast<double>(feet.x), static_cast<double>(feet.y),
		         static_cast<double>(feet.z));
	}
}

}  // namespace

void StepSwimArms(SwimArmsState& state, const HandStep& step, const HandShared& shared, bool flying,
                  bool rolesSwapped, game::AnalogWalkRequest& request, vr::HandControlsWanted& controls) {
	const vr::SwimSettings& settings = step.config.swim;
	const float deltaSeconds = step.dt;
	const vr::HandModeResult& hand = shared.hand;
	const SInt32 controllerState = game::PlayerCharacterState();
	vr::SwimGate gate;
	gate.fullVr = step.active;
	gate.inWorld = game::PlayerInWorld();
	gate.menuUp = step.menuIsUp;
	gate.riding = game::PlayerRiding();
	gate.flyCam = flying;
	gate.cameraKnown = shared.cyclopeanCameraValid;
	gate.gameSwimming = game::KickSwimming(controllerState);
	const bool swimming = vr::SwimAllowed(settings, gate);
	// The head's own velocity, differenced from its pose in tracking space, in the head's frame.
	NiPoint3 headInHeadFrame{0.0f, 0.0f, 0.0f};
	vr::Quaternion headRotation = vr::Quaternion::Identity();
	NiPoint3 headAt{0.0f, 0.0f, 0.0f};
	if (step.backend.ReadHeadPose(headRotation, headAt)) {
		if (state.headLastValid && deltaSeconds > 0.0f) {
			headInHeadFrame = vr::OffsetFromPose(headRotation, (headAt - state.headLast) * (1.0f / deltaSeconds),
			                                     NiPoint3{0.0f, 0.0f, 0.0f}, 1.0f);
		}
		state.headLast = headAt;
		state.headLastValid = true;
	} else {
		state.headLastValid = false;
	}
	vr::SwimInput in;
	in.swimming = swimming;
	in.dt = deltaSeconds;
	vr::SwimVerdict verdict;
	float swimSpeed = 0.0f;
	if (swimming) {
		swimSpeed = game::PlayerSwimSpeed();
		in.swimSpeed = swimSpeed;
		const vr::SwimBusyInput busy = BusyNow(hand);
		const NiMatrix33& camera = shared.cyclopeanCamera.rot;
		in.hand[0] = HandNow(hand.rightHandValid, hand.rightHold, hand.rightVelocity, headInHeadFrame, camera,
		                     vr::SwimHandBusy(busy, false));
		in.hand[1] = HandNow(hand.leftHandValid, hand.leftHold, hand.leftVelocity, headInHeadFrame, camera,
		                     vr::SwimHandBusy(busy, true));
	}
	verdict = vr::StepSwim(state.stroke, settings, in);
	game::RequestSwimVelocity(swimming ? verdict.velocity : NiPoint3{0.0f, 0.0f, 0.0f});
	state.sinceHolding = verdict.holding ? 0.0f : state.sinceHolding + deltaSeconds;
	if (swimming) {
		state.swum += math::Sqrt(verdict.velocity.LengthSquared()) * deltaSeconds;
		const float speed = math::Sqrt(verdict.velocity.LengthSquared());
		// A stick pushed walks as before; idle, the arms' glide shows the game a swimmer.
		if (!request.active && speed > vr::kSwimNudgeShowsAbove) {
			ShowGameTheDirection(verdict.velocity, request, controls);
		}
		// A closed hand holding the water is no fist raised to fight.
		if (vr::SwimDropsReadyClick(state.sinceHolding)) {
			controls.readyWeapon = false;
		}
		for (int i = 0; i < 2; ++i) {
			if (verdict.caughtNow[i]) {
				Buzz(step, i == 0, rolesSwapped, 0.5f, 0.03f);
			}
		}
		state.buzzClock -= deltaSeconds;
		if (verdict.pulling && state.buzzClock <= 0.0f) {
			state.buzzClock = kBuzzEverySeconds;
			for (int i = 0; i < 2; ++i) {
				if (in.hand[i].tracked && in.hand[i].grip && !in.hand[i].busy) {
					Buzz(step, i == 0, rolesSwapped, 0.1f + 0.3f * verdict.effort, kBuzzEverySeconds);
				}
			}
		}
	} else {
		state.swum = 0.0f;
	}
	LogSwim(state, verdict, swimming, swimSpeed, controllerState);
	state.wasSwimming = swimming;
	state.wasHolding = verdict.holding;
}

}  // namespace obvr::camera::hands

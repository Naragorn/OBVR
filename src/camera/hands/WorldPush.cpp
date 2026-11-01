#include "camera/hands/WorldPush.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FirstPersonHide.h"
#include "game/HandBodies.h"
#include "game/HandBodyLogic.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/PlayerCapsule.h"
#include "game/PlayerTeleport.h"
#include "game/WorldPush.h"
#include "test/HandScriptRuntime.h"
#include "vr/Fist.h"

namespace obvr::camera::hands {

WorldPushOutput StepWorldPush(WorldPushState& state, const HandStep& step, const HandShared& shared,
                   const vr::HandModeFrame& frame, const WorldPushLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	const bool inPlace =
		active && !menuIsUp && frame.inWorld && frame.firstPerson && shared.cyclopeanCameraValid;
	const bool pushing = inPlace && config.hands.pushWorld;
	game::PushFrame push;
	push.dtSeconds = links.deltaSeconds;
	if (inPlace && (config.hands.pushWorld || config.hands.bodyCollision)) {
		const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
		const NiPoint3& camPos = shared.cyclopeanCamera.pos;
		const int weapon = static_cast<int>(game::Pusher::WeaponHand);
		const int other = static_cast<int>(game::Pusher::OtherHand);
		if (hand.rightHandValid) {
			// The drawn hand (vr/WeaponWeight.h): the same as the controller's
			// unless a weapon's weight holds it back.
			const NiPoint3 grip = camPos + camRot * hand.weaponHandOffsetUnits;
			const NiPoint3 forward = camRot * (hand.weaponHandRotation * NiPoint3{0.0f, 1.0f, 0.0f});
			const bool blade = game::MeleeInHand(nullptr) &&
			                   game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
			float length = game::kPushFallbackBladeUnits;
			if (blade) {
				const NiAVObject* const node = game::FindFirstPersonNode("Weapon");
				length = game::BladeLengthFromBound(node != nullptr ? node->worldBound.radius : 0.0f);
			}
			push.valid[weapon] = true;
			push.blade[weapon] = blade;
			push.segment[weapon] = blade ? game::BladeSegment(grip, forward, length)
			                             : game::HandSegment(grip, forward);
		}
		if (hand.leftHandValid) {
			const NiPoint3 grip = camPos + camRot * hand.leftHandOffsetUnits;
			const NiPoint3 forward = camRot * (hand.leftHandRotation * NiPoint3{0.0f, 1.0f, 0.0f});
			push.valid[other] = true;
			push.segment[other] = game::HandSegment(grip, forward);
		}
	}
	// Whether each hand is a fist - for its body (no pushing people with a
	// fist) and for the off hand's blow (a fist punches, an open hand
	// slaps). Every frame, the bodies on or off: decided inside their
	// block until 2026-10-12, it never changed with BodyCollision off. A
	// finger whose sensor has never read is left out (vr/FingersRead.h:
	// the tester's left little finger read 0.00 all that day's run).
	vr::NoteFingerCurls(state.rightFingersRead, hand.rightCurlValid, hand.rightCurl);
	vr::NoteFingerCurls(state.leftFingersRead, hand.leftCurlValid, hand.leftCurl);
	{
		const float closeCurl = config.hands.fist.closeCurl;
		const float openLimit = vr::FistOpenLimit(config.hands.fist);
		const bool wasLeftFist = state.leftBodyFist;
		const bool wasRightFist = state.rightBodyFist;
		state.rightBodyFist = game::HandBodyFist(state.rightBodyFist, hand.rightCurlValid, hand.rightCurl, closeCurl,
		                                     openLimit, &state.rightFingersRead, hand.rightGripForce,
		                                     config.hands.fist.gripForce);
		state.leftBodyFist = game::HandBodyFist(state.leftBodyFist, hand.leftCurlValid, hand.leftCurl, closeCurl,
		                                    openLimit, &state.leftFingersRead, hand.leftGripForce,
		                                    config.hands.fist.gripForce);
		// A fist the grip's force made, the fingers reading open: said a
		// few times per hand, with the force, for the threshold's tuning.
		for (int side = 0; side < 2; ++side) {
			const bool fist = side == 0 ? state.rightBodyFist : state.leftBodyFist;
			const bool was = side == 0 ? wasRightFist : wasLeftFist;
			if (!fist || was || state.gripFistLines[side] == 0) {
				continue;
			}
			const float* curl = side == 0 ? hand.rightCurl : hand.leftCurl;
			float lowest = 0.0f;
			float highest = 0.0f;
			vr::FistCurlSpan(curl, side == 0 ? &state.rightFingersRead : &state.leftFingersRead, lowest, highest);
			if (lowest >= closeCurl) {
				continue;  // the fingers made it
			}
			--state.gripFistLines[side];
			OBVR_LOG("Hands: the %s hand's fist by the grip's force %.2f (FistGripForce %.2f; the fingers' lowest "
			         "curl %.2f, the close limit %.2f)",
			         side == 0 ? "right" : "left",
			         static_cast<double>(side == 0 ? hand.rightGripForce : hand.leftGripForce),
			         static_cast<double>(config.hands.fist.gripForce), static_cast<double>(lowest),
			         static_cast<double>(closeCurl));
		}
		for (int side = 0; side < 2; ++side) {
			const vr::FingersRead& read = side == 0 ? state.rightFingersRead : state.leftFingersRead;
			bool& said = state.fingerLeftOutLogged[side];
			if (!said && vr::FistFingerCount(&read) < 4) {
				said = true;
				OBVR_LOG("Hands: the %s hand's fist goes by %d fingers - one never read (index %s, middle %s, ring "
				         "%s, little %s; the highest curls %.2f %.2f %.2f %.2f)",
				         side == 0 ? "right" : "left", vr::FistFingerCount(&read),
				         vr::FingerHasRead(read, 1) ? "read" : "never", vr::FingerHasRead(read, 2) ? "read" : "never",
				         vr::FingerHasRead(read, 3) ? "read" : "never", vr::FingerHasRead(read, 4) ? "read" : "never",
				         static_cast<double>(read.most[1]), static_cast<double>(read.most[2]),
				         static_cast<double>(read.most[3]), static_cast<double>(read.most[4]));
			}
		}
		if (state.leftBodyFist != wasLeftFist && state.fistChangeLines > 0) {
			--state.fistChangeLines;
			OBVR_LOG("Hands: the left hand %s (curls %.2f %.2f %.2f %.2f, index to little)",
			         state.leftBodyFist ? "made a fist" : "opened", static_cast<double>(hand.leftCurl[1]),
			         static_cast<double>(hand.leftCurl[2]), static_cast<double>(hand.leftCurl[3]),
			         static_cast<double>(hand.leftCurl[4]));
		}
	}
	// The hands and the drawn melee weapon as Havok bodies
	// (game/HandBodies.h); the rays below then only cover what has none.
	game::HandBodyFrame bodies;
	bodies.enabled = inPlace && config.hands.bodyCollision;
	// Below 0: the headset's own rate (game/HandBodyLogic.h).
	bodies.physicsRate = game::ResolvePhysicsRate(
		config.hands.physicsRate,
		config.hands.physicsRate < 0.0f ? backend.DisplayFrequencyHz() : 0.0f);
	bodies.dtSeconds = links.deltaSeconds;
	bool bodyBlade = false;
	if (bodies.enabled) {
		const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
		const NiPoint3& camPos = shared.cyclopeanCamera.pos;
		if (hand.rightHandValid) {
			bodyBlade = push.blade[static_cast<int>(game::Pusher::WeaponHand)];
			const int slot = static_cast<int>(bodyBlade ? game::HandBodySlot::Weapon : game::HandBodySlot::RightHand);
			bodies.valid[slot] = true;
			bodies.rot[slot] = camRot * hand.weaponHandRotation;
			bodies.pos[slot] = camPos + camRot * hand.weaponHandOffsetUnits;
			if (bodyBlade) {
				const game::PushSegment& s = push.segment[static_cast<int>(game::Pusher::WeaponHand)];
				bodies.bladeUnits = math::Sqrt((s.b - s.a).LengthSquared());
			}
		}
		if (hand.leftHandValid) {
			const int slot = static_cast<int>(game::HandBodySlot::LeftHand);
			bodies.valid[slot] = true;
			bodies.rot[slot] = camRot * hand.leftHandRotation;
			bodies.pos[slot] = camPos + camRot * hand.leftHandOffsetUnits;
		}
		// Each body's span from what is drawn (game::HandSpanFromBones,
		// BladeSpanFromNode), in the body's own frame.
		for (int slot = 0; slot < static_cast<int>(game::HandBodySlot::Count); ++slot) {
			if (!bodies.valid[slot]) {
				continue;
			}
			const NiMatrix33& r = bodies.rot[slot];
			const NiPoint3& p = bodies.pos[slot];
			if (slot == static_cast<int>(game::HandBodySlot::Weapon)) {
				const NiAVObject* const node = game::FindFirstPersonNode("Weapon");
				if (node != nullptr) {
					const NiMatrix33& w = node->worldTransform.rot;
					const NiPoint3 axis{w.data[0][1], w.data[1][1], w.data[2][1]};
					bodies.span[slot] = game::BladeSpanFromNode(
						game::ToBodyFrame(r, p, node->worldTransform.pos),
						game::ToBodyFrame(r, NiPoint3{0.0f, 0.0f, 0.0f}, axis),
						game::ToBodyFrame(r, p, node->worldBound.center), node->worldBound.radius);
				}
				continue;
			}
			const bool left = slot == static_cast<int>(game::HandBodySlot::LeftHand);
			const NiAVObject* const wrist = game::FindFirstPersonNode(left ? "Bip01 L Hand" : "Bip01 R Hand");
			const NiAVObject* const knuckle =
				game::FindFirstPersonNode(left ? "Bip01 L Finger2" : "Bip01 R Finger2");
			if (wrist != nullptr && knuckle != nullptr) {
				bodies.span[slot] = game::HandSpanFromBones(game::ToBodyFrame(r, p, wrist->worldTransform.pos),
				                                            game::ToBodyFrame(r, p, knuckle->worldTransform.pos));
			}
		}
		// People are pushed out of combat only, and not by a hand made a fist
		// (game::HandBodyPushesActors; the tester, 2026-09-29).
		const bool inCombat = game::PlayerInCombat();
		bodies.pushesActors[static_cast<int>(game::HandBodySlot::RightHand)] =
			game::HandBodyPushesActors(config.hands.pushPeople, true, inCombat, state.rightBodyFist);
		bodies.pushesActors[static_cast<int>(game::HandBodySlot::LeftHand)] =
			game::HandBodyPushesActors(config.hands.pushPeople, true, inCombat, state.leftBodyFist);
		bodies.pushesActors[static_cast<int>(game::HandBodySlot::Weapon)] =
			game::HandBodyPushesActors(config.hands.pushPeople, false, inCombat, false);
	}
	// While a dropped weapon waits for its body, the hands' bodies stand out
	// of the world: the body can come in and be stepped by Havok before
	// this frame's check sees it and puts it into the player's group
	// (PassDroppedBody), and a hand moving on would knock it away in that
	// step (weapon-drop-sweep.txt, 2026-10-25: 148 to 290 units/s with the
	// group alone). Two or three frames; back in once it passes through.
	if (links.droppedWeaponRef != 0) {
		for (int slot = 0; slot < static_cast<int>(game::HandBodySlot::Count); ++slot) {
			bodies.valid[slot] = false;
		}
	}
	const game::HandBodyReport live = game::StepHandBodies(bodies);
	if (frame.inWorld && !menuIsUp) {
		game::StepPlayerCapsule(config.hands.bodyRadiusScale);
	}
	if (bodies.enabled) {
		links.measureBodies(bodies, bodyBlade, test::HandScriptMarkedThisFrame());
	}
	if (live.live[static_cast<int>(bodyBlade ? game::HandBodySlot::Weapon : game::HandBodySlot::RightHand)]) {
		push.valid[static_cast<int>(game::Pusher::WeaponHand)] = false;
	}
	if (live.live[static_cast<int>(game::HandBodySlot::LeftHand)]) {
		push.valid[static_cast<int>(game::Pusher::OtherHand)] = false;
	}
	game::StepWorldPush(pushing, push);
	return WorldPushOutput{inPlace};
}

}  // namespace obvr::camera::hands

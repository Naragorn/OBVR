#include "camera/hands/Kick.h"

#include <cstdio>

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "core/Rotation.h"
#include "game/BodyIk.h"
#include "game/GameAddresses.h"
#include "game/GameSound.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/Shove.h"
#include "platform/PluginPath.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

namespace {

// A kick landed: felt in both hands, with the contact buzz on
// ([Hands] ContactHaptics).
void PulseKickLanded(vr::OpenVRBackend& backend, const Config& config) {
	if (config.hands.contactHaptics) {
		backend.Pulse(true, 0.08f, 80.0f, 0.9f);
		backend.Pulse(false, 0.08f, 80.0f, 0.9f);
	}
}

// Where the foot sits in the view at its peak, once a kick (Nadi,
// 2026-11-03: "auch wenn ich gerade schaue sehe ich keinen kick").
void LogFootInView(KickHoldState& state, const HandShared& shared, float weight, const NiPoint3& facing) {
	NiPoint3 foot{0.0f, 0.0f, 0.0f};
	if (weight < 1.0f || state.viewSaid == state.kick.serial || state.viewLines == 0 ||
	    !shared.cyclopeanCameraValid || !game::BodyKickFoot(foot)) {
		return;
	}
	state.viewSaid = state.kick.serial;
	--state.viewLines;
	const game::KickFootView view = game::KickFootViewFor(shared.cyclopeanCamera.pos, facing, foot);
	OBVR_LOG("Kick: %u - the foot at its peak %.1f degrees under the eyes' level, %.1f under the look",
	         state.kick.serial, static_cast<double>(view.belowLevelDegrees),
	         static_cast<double>(view.belowLookDegrees));
}

// The leg's pose for the body IK: the look's facing, and the eyes, so a look
// down past the foot brings the foot under it.
game::BodyKickPose KickPoseFor(const KickHoldState& state, float weight, const KickLinks& links,
                               const HandShared& shared, const game::KickSettings& settings) {
	game::BodyKickPose pose;
	pose.weight = weight;
	pose.rightLeg = !links.rolesSwapped;
	pose.facing = ForwardOf(shared.cyclopeanCamera.rot);
	pose.eye = shared.cyclopeanCamera.pos;
	pose.eyeValid = shared.cyclopeanCameraValid;
	pose.aim = state.aim;
	pose.aimValid = state.aimValid;
	pose.settings = settings;
	return pose;
}

// At a kick's start: whom the look meets within KickAimUnits, and where -
// the foot goes there for the whole kick (game::KickLookTarget).
void AimKick(KickHoldState& state, const HandShared& shared, const game::KickSettings& settings) {
	state.aimValid = false;
	if (!shared.cyclopeanCameraValid || !(settings.aimUnits > 0.0f)) {
		return;
	}
	const NiPoint3 eye = shared.cyclopeanCamera.pos;
	game::CollectBladeBodies(eye, settings.aimUnits + 60.0f, state.kickBodies);
	const game::KickLookHit hit =
		game::KickLookTarget(state.kickBodies, eye, ForwardOf(shared.cyclopeanCamera.rot), settings.aimUnits);
	state.aimValid = hit.hit;
	state.aim = hit.point;
	if (state.aimLines > 0) {
		--state.aimLines;
		if (hit.hit) {
			OBVR_LOG("Kick: %u aimed at %08X's %s, %.1f units from the eyes", state.kick.serial, hit.actor,
			         game::BodyPartName(hit.part), static_cast<double>(hit.units));
		} else {
			OBVR_LOG("Kick: %u aimed at no one (%u capsules within %.0f units) - by the look", state.kick.serial,
			         state.kickBodies.count, static_cast<double>(settings.aimUnits + 60.0f));
		}
	}
}

// The ring of a held button (OBVR_Controls.ini; by default the left B's
// toward the pause menu), over that button's controller, and its lines.
void StepHoldRing(KickHoldState& state, vr::OpenVRBackend& backend, const vr::HandModeResult& hand,
                  const vr::HandModeFrame& frame, bool shown, float holdSeconds) {
	const char* holdButton = vr::HoldButtonPhrase(hand.menuHoldButton);
	const vr::HandPose& ringHand = hand.menuHoldRight ? frame.right : frame.left;
	if (hand.menuHoldCancelled && state.kickLines > 0) {
		--state.kickLines;
		OBVR_LOG("Hands: %s let go with the ring %.0f %% full - called off", holdButton,
		         static_cast<double>(state.menuHoldView.fill * 100.0f));
	}
	if (shown && hand.menuHoldRing && ringHand.valid && frame.headValid) {
		const float level = static_cast<float>(static_cast<int>(hand.menuHoldFill * 48.0f)) / 48.0f;
		if (level != state.menuHoldView.fill) {
			state.menuHoldView.fill = level;
			++state.menuHoldRevision;
		}
		const NiPoint3 above{ringHand.position.x, ringHand.position.y + kMenuHoldAboveMetres, ringHand.position.z};
		const bool wasVisible = state.menuHoldLayer.IsVisible();
		state.menuHoldLayer.Show(backend, render::GetGameDevice(), StowSpotPose(above, frame.headPosition),
		                         ui::kHoldRingMetres, state.menuHoldRevision, ui::PaintHoldRingFor, &state.menuHoldView);
		if (!wasVisible && state.menuHoldLayer.IsVisible() && state.kickLines > 0) {
			--state.kickLines;
			OBVR_LOG("Hands: %s held - %s's ring shown over the controller", holdButton,
			         vr::ControlActionPhrase(hand.menuHoldAction));
		}
	} else {
		state.menuHoldLayer.Hide(backend);
	}
	if (hand.holdReached != vr::HoldButton::Count && shown && state.kickLines > 0) {
		--state.kickLines;
		OBVR_LOG("Hands: %s held %.2f s - %s", vr::HoldButtonPhrase(hand.holdReached), static_cast<double>(holdSeconds),
		         vr::ControlActionPhrase(hand.holdReachedAction));
	}
	if (test::HandScriptMarkedThisFrame() && state.menuHoldLayer.IsVisible()) {
		char name[96];
		char path[512];
		std::snprintf(name, sizeof(name), "OBVR-MenuHold-%s.bmp", test::HandScriptMarkName());
		if (platform::BuildGamePath(name, path, sizeof(path)) && state.menuHoldLayer.SaveBmp(path)) {
			OBVR_LOG("HandScript: the menu hold's ring saved as %s (%.0f %% full)", name,
			         static_cast<double>(state.menuHoldView.fill * 100.0f));
		}
	}
}

}  // namespace

void StepKick(KickHoldState& state, const HandStep& step, const HandShared& shared, const vr::HandModeFrame& frame,
              const KickLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	game::KickGate gate;
	gate.inWorld = frame.inWorld && active;
	gate.menuUp = menuIsUp;
	gate.mounted = game::PlayerMountedHorse() != 0;
	gate.knockedDown = game::PlayerKnockState() > 0;
	gate.exhausted = links.exhausted;
	const SInt32 bodyState = game::PlayerCharacterState();
	gate.swimming = game::KickSwimming(bodyState);
	if (bodyState != state.bodyStateWas && frame.inWorld && state.bodyStateLines > 0) {
		--state.bodyStateLines;
		OBVR_LOG("Kick: the player's character controller's state %d (0 ground, 1 jumping, 2 in the air, 5 "
		         "swimming)",
		         bodyState);
	}
	state.bodyStateWas = bodyState;
	const bool allowed = game::KickAllowed(config.hands.kick, gate);
	const bool pressed = hand.controls.kick;
	const float kickSeconds = dt == dt && dt > 0.0f ? dt : 0.0f;
	const game::KickFrame kick =
		game::StepKick(state.kick, config.hands.kick, pressed, allowed, kickSeconds, hand.controls.kickHeld);
	if (pressed && !kick.started && state.kickLines > 0) {
		--state.kickLines;
		OBVR_LOG("Kick: the right B pressed - no kick (%s)",
		         !allowed ? (gate.mounted       ? "on a horse"
		                     : gate.knockedDown ? "knocked down"
		                     : gate.exhausted   ? "exhausted"
		                     : gate.swimming    ? "swimming"
		                     : gate.menuUp      ? "a menu up"
		                                        : "not in the world")
		                  : "the last one not over yet");
	}
	if (kick.started) {
		AimKick(state, shared, config.hands.kick);
		NiPoint3 at{0.0f, 0.0f, 0.0f};
		const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
		const bool swish = game::PlayerWorldPosition(at) && game::PlaySoundFormAt(game::kSoundSwishHand, player, at);
		if (state.kickLines > 0) {
			--state.kickLines;
			OBVR_LOG("Kick: %u - the %s leg out%s%s%s", state.kick.serial, links.rolesSwapped ? "left" : "right",
			         hand.controls.run && vr::SticksMovePlayer(hand.controls) ? " (running)" : "",
			         game::KickAirborne(bodyState) ? " (in a jump)" : "", swish ? ", the swish played" : "");
		}
	}
	const game::BodyKickPose pose = KickPoseFor(state, kick.weight, links, shared, config.hands.kick);
	game::SetBodyKick(pose);
	// The hit (decision 5, the cleave): the foot's ball swept from the
	// frame before against the living's capsules while it strikes, each
	// person once a kick - staggered and pushed standing, knocked down
	// running, told as a hit either way. Where the IK drew no foot (no
	// body, IKLegs off) the foot is where the IK would have put it, from
	// the player's feet.
	NiPoint3 strike{0.0f, 0.0f, 0.0f};
	bool strikeValid = false;
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
	const bool feetKnown = game::PlayerWorldPosition(feet);
	if (kick.striking) {
		strikeValid = game::BodyKickFoot(strike);
		if (!strikeValid && feetKnown) {
			const float leg = 61.0f;  // thigh 33.67 and calf 27.33 (physical-combat-spec 11)
			const NiPoint3 hip{feet.x, feet.y, feet.z + leg};
			strike = game::KickFootTarget(feet, game::KickPeak(hip, pose.facing, leg, config.hands.kick, pose.eyeValid ? &pose.eye : nullptr,
			                                                             pose.aimValid ? &pose.aim : nullptr),
			                              kick.weight);
			strikeValid = true;
		}
	}
	if (strikeValid) {
		const NiPoint3 from = state.footBeforeValid ? state.footBefore : strike;
		const UInt32 peopleNear = game::CollectBladeBodies(strike, 120.0f, state.kickBodies);
		if (state.nearSaid != state.kick.serial && state.kickLines > 0) {
			state.nearSaid = state.kick.serial;
			--state.kickLines;
			OBVR_LOG("Kick: %u striking - %u people within 120 units of the foot (%u capsules)", state.kick.serial, peopleNear,
			         state.kickBodies.count);
		}
		UInt32 met[game::kKickMetMax] = {};
		const UInt32 n = game::KickTouches(state.kickBodies, from, strike, config.hands.kick.footUnits, state.kickMet,
		                                   state.kick.serial, met, game::kKickMetMax);
		const bool running = hand.controls.run && vr::SticksMovePlayer(hand.controls);
		const game::ShoveKind kind = game::KickKindFor(config.hands.kick, running, game::KickAirborne(bodyState));
	
		const NiPoint3 hipNow = feetKnown ? NiPoint3{feet.x, feet.y, strike.z} : from;
		for (UInt32 i = 0; i < n; ++i) {
			void* const actor = reinterpret_cast<void*>(met[i]);
			// The damage first, the engine's fist blow (game::KickStrike),
			// then the stagger or the knockdown on top.
			const bool engineHit = game::KickStrike(actor, kind == game::ShoveKind::Hard);
			const game::ShoveSettings shove =
				game::KickShoveSettings(config.hands.shove, config.hands.kick, engineHit);
			const bool done = game::ShoveActor(actor, kind, hipNow, strike, shove, false);
			const UInt32 sound = game::KickHitSound(kind, engineHit);
			const bool sounded = done && sound != 0 && game::PlaySoundFormAt(sound, met[i], strike);
			if (done && state.kickLanded != state.kick.serial) {
				state.kickLanded = state.kick.serial;
				game::SpendPlayerFatigue(config.hands.kick.fatigue, vr::FatigueCostKind::Shove);
				PulseKickLanded(backend, config);
			}
			if (state.kickLines > 0) {
				--state.kickLines;
				OBVR_LOG("Kick: %u met %08X - %s%s%s%s", state.kick.serial, met[i],
				         !done                             ? "refused"
				         : kind == game::ShoveKind::Hard ? "knocked down (running or in a jump)"
				                                         : "staggered",
				         engineHit ? ", the engine's fist blow" : (done ? ", told as a hit" : ""),
				         sounded ? ", its sound played" : "", engineHit ? "" : " (no damage)");
			}
		}
	}
	state.footBefore = strike;
	state.footBeforeValid = strikeValid;
	LogFootInView(state, shared, kick.weight, pose.facing);
	NiPoint3 foot{0.0f, 0.0f, 0.0f};
	if (test::HandScriptMarkedThisFrame() && game::BodyKickFoot(foot)) {
		OBVR_LOG("Kick: at the mark the foot is %.1f %.1f %.1f, %.1f units above the player's feet (weight %.2f)",
		         static_cast<double>(foot.x), static_cast<double>(foot.y), static_cast<double>(foot.z),
		         feetKnown ? static_cast<double>(foot.z - feet.z) : 0.0, static_cast<double>(kick.weight));
	}
	StepHoldRing(state, backend, hand, frame, active && !menuIsUp, config.hands.menuHoldSeconds);
}

}  // namespace obvr::camera::hands

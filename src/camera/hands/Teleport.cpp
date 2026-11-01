#include "camera/hands/Teleport.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FatigueCost.h"
#include "game/GameSound.h"
#include "game/MeleeHits.h"
#include "game/PlayerTeleport.h"
#include "game/TeleportNoise.h"
#include "game/WorldPickHook.h"
#include "render/VignetteLayer.h"
#include "vr/LaserGeometry.h"

namespace obvr::camera::hands {
namespace {

// Where the arc goes and what it lands on this frame.
struct TeleportAim {
	NiPoint3 points[vr::kArcMaxPoints];
	UInt32 count = 0;
	bool landed = false;
	NiPoint3 landing{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};
	vr::TeleportRefusal refusal = vr::TeleportRefusal::NoGround;
	float cost = 0.0f;
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
};

// Without Blink the glide must not pass through anything: the straight line
// at waist height from here to the landing has to be clear.
constexpr float kTeleportWaistUnits = 60.0f;
// Set down a hair above the ground, so the capsule does not start in it.
constexpr float kTeleportLiftUnits = 2.0f;
// How long a teleport is heard at least, with TeleportMakesNoise: about
// the time of a step.
constexpr float kTeleportNoiseSeconds = 0.5f;

TeleportAim AimTeleport(const Config& config, const vr::LaserWorldRay& ray) {
	const vr::TeleportSettings& tp = config.hands.teleport;
	const float perMetre = config.tracker.unitsPerMetre;
	const float rangeUnits = tp.rangeMetres * perMetre;
	const float gravity = vr::kArcGravityMetres * perMetre;
	const float speed = vr::ArcSpeed(rangeUnits, gravity);
	const float step = vr::ArcTimeStep(rangeUnits, gravity);
	TeleportAim aim;
	aim.points[0] = ray.origin;
	aim.count = 1;
	bool worldAsked = true;
	for (UInt32 i = 1; i < vr::kArcMaxPoints && worldAsked; ++i) {
		const NiPoint3 from = aim.points[i - 1];
		const NiPoint3 to =
			vr::ArcPoint(ray.origin, ray.direction, speed, gravity, step * static_cast<float>(i));
		game::WorldPick pick;
		worldAsked = game::PickWorldSegment(from, to, pick);
		if (worldAsked && pick.hit) {
			aim.points[aim.count++] = pick.point;
			aim.landed = true;
			aim.landing = pick.point;
			aim.normal = pick.normal;
			break;
		}
		aim.points[aim.count++] = to;
	}
	vr::LandingQuery q;
	q.hit = aim.landed;
	q.point = aim.landing;
	const float normalLength = math::Sqrt(aim.normal.LengthSquared());
	q.normalUp = normalLength > 1.0e-6f ? aim.normal.z / normalLength : 0.0f;
	q.allowedNow = game::ReadPlayerFeet(aim.feet);
	q.feet = aim.feet;
	q.rangeUnits = rangeUnits;
	q.jumpUnits = game::PlayerJumpUnits();
	q.blink = tp.blink;
	if (aim.landed && !tp.blink) {
		const NiPoint3 up{0.0f, 0.0f, kTeleportWaistUnits};
		game::WorldPick between;
		q.pathClear =
			game::PickWorldSegment(aim.feet + up, aim.landing + up, between) && !between.hit;
	}
	const float rise = (aim.landing.z - aim.feet.z) / (perMetre > 0.0f ? perMetre : 1.0f);
	aim.cost = vr::TeleportFatigueCost(game::PlayerDodgeFatigueCost(), rise, tp);
	float fatigueBase = 0.0f;
	q.fatigueCost = aim.cost;
	if (!game::ReadPlayerFatigue(q.fatigueNow, fatigueBase)) {
		q.fatigueNow = aim.cost;  // unreadable: do not lock the player out
	}
	aim.refusal = vr::JudgeLanding(q);
	return aim;
}

}  // namespace

bool TeleportAllowedNow(const TeleportState& state, const Config& config, bool menuIsUp, bool inWorld,
                        bool thirdPerson) {
	const vr::TeleportSettings& tp = config.hands.teleport;
	if (!tp.enabled || menuIsUp || !inWorld || thirdPerson || game::PlayerIsDead() ||
	    state.move.phase != vr::TeleportPhase::Idle) {
		return false;
	}
	if (game::PlayerRiding()) {
		return false;
	}
	return tp.inCombat || !game::PlayerInCombat();
}

bool TeleportFading(const TeleportState& state) {
	return state.move.phase != vr::TeleportPhase::Idle && state.move.fade;
}

void StepTeleport(TeleportState& state, const HandStep& frame, const HandShared& shared, bool active) {
	const Config& config = frame.config;
	vr::OpenVRBackend& backend = frame.backend;
	const bool menuIsUp = frame.menuIsUp;
	const float dtSeconds = frame.dt;
	const vr::HandModeResult& hand = shared.hand;
	const vr::TeleportSettings& tp = config.hands.teleport;
	const float perMetre = config.tracker.unitsPerMetre;
	bool arcShown = false;
	if (state.move.phase != vr::TeleportPhase::Idle && (!active || menuIsUp)) {
		// A menu opened mid-move (a notice, a tutorial message), or the mode
		// stopped: the move ends at once, at its target, the view back from
		// black and the player hurtable again, so nothing of the teleport is
		// left standing while the menu waits for its click. Pausing it there
		// instead was when one message box could not be clicked away
		// (2026-09-27); that the pause was the cause is not proven.
		if (state.move.fade) {
			backend.FadeToColor(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		}
		const bool placed = active && game::PlacePlayerAt(state.move.to);
		if (state.linesLeft > 0) {
			--state.linesLeft;
			OBVR_LOG("Teleport: %s mid-move - ended at once%s",
			         active ? "a menu opened" : "the mode stopped",
			         placed ? ", placed at the target" : "");
		}
		state.move = vr::TeleportMove{};
	}
	if (active && !menuIsUp && (hand.teleportAiming || hand.teleportCommit) &&
	    hand.rightHandValid && shared.cyclopeanCameraValid) {
		const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
			shared.cyclopeanCamera.rot, shared.cyclopeanCamera.pos,
			hand.rightHandRotation, hand.rightHandOffsetUnits, config.hands.laserPitchDegrees,
			config.hands.laserYawDegrees, config.hands.laserOriginMetres, perMetre,
			vr::LaserOffsetLocal(config.hands.laserOffsetRightMetres, config.hands.laserOffsetUpMetres, false));
		const TeleportAim aim = AimTeleport(config, ray);
		const bool valid = aim.refusal == vr::TeleportRefusal::None;
		vr::openvr::HmdMatrix34 head{};
		if (hand.teleportAiming && backend.GetRenderPoseMatrix(head)) {
			const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
			const NiPoint3& camPos = shared.cyclopeanCamera.pos;
			NiPoint3 tracked[vr::kArcMaxPoints];
			for (UInt32 i = 0; i < aim.count; ++i) {
				tracked[i] = vr::WorldPointInTracking(head, camRot, camPos, aim.points[i], perMetre);
			}
			const NiPoint3 ringAt =
				vr::WorldPointInTracking(head, camRot, camPos, aim.landing, perMetre);
			const NiPoint3 normalTip = vr::WorldPointInTracking(
				head, camRot, camPos, aim.landing + aim.normal * perMetre, perMetre);
			const NiPoint3 eyes{head.m[0][3], head.m[1][3], head.m[2][3]};
			state.arc.Submit(backend, true, tracked, aim.count, eyes, aim.landed, ringAt,
			                     normalTip - ringAt, valid);
			arcShown = true;
		}
		if (hand.teleportCommit) {
			if (valid) {
				// The price first.
				float fatigueBefore = 0.0f;
				float fatigueBase = 0.0f;
				const bool fatigueKnown = game::ReadPlayerFatigue(fatigueBefore, fatigueBase);
				game::SpendPlayerFatigue(aim.cost, vr::FatigueCostKind::Teleport);
				float fatiguePaid = 0.0f;
				if (fatigueKnown && game::ReadPlayerFatigue(fatiguePaid, fatigueBase) && state.linesLeft > 0) {
					OBVR_LOG("Teleport: fatigue %.1f -> %.1f of %.1f (the price %.1f)", static_cast<double>(fatigueBefore),
					         static_cast<double>(fatiguePaid), static_cast<double>(fatigueBase),
					         static_cast<double>(aim.cost));
				}
				const NiPoint3 target = aim.landing + NiPoint3{0.0f, 0.0f, kTeleportLiftUnits};
				state.move = vr::StartTeleport(aim.feet, target, tp, perMetre);
				state.noiseLeft = state.move.duration > kTeleportNoiseSeconds
				                          ? state.move.duration
				                          : kTeleportNoiseSeconds;
				if (!tp.instant && tp.vignette) {
					shared.vignette.Trigger();
				}
			}
			if (state.linesLeft > 0) {
				--state.linesLeft;
				const NiPoint3 d = aim.landing - aim.feet;
				OBVR_LOG("Teleport: %s - %.2f m away, %.2f m up, cost %.1f fatigue, %s (%u arc "
				         "points)",
				         valid ? "going" : "refused",
				         static_cast<double>(math::Sqrt(d.LengthSquared()) / perMetre),
				         static_cast<double>(d.z / perMetre), static_cast<double>(aim.cost),
				         valid ? (tp.instant ? "instant" : "glide")
				               : vr::TeleportRefusalName(aim.refusal),
				         aim.count);
			}
		}
	}
	if (!arcShown) {
		state.arc.Submit(backend, false, nullptr, 0, NiPoint3{0.0f, 0.0f, 0.0f}, false,
		                     NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, false);
	}

	// The move, once it runs (a menu has ended it above).
	bool teleportFallSafe = false;
	if (state.move.phase != vr::TeleportPhase::Idle) {
		const vr::TeleportMoveStep step = vr::StepTeleportMove(state.move, dtSeconds);
		teleportFallSafe = step.fallSafe;
		if (step.fadeOut) {
			backend.FadeToColor(tp.fadeSeconds, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (step.place && !game::PlacePlayerAt(step.at)) {
			if (state.linesLeft > 0) {
				--state.linesLeft;
				OBVR_LOG("Teleport: the player could not be placed - the move stops");
			}
			if (state.move.fade) {
				backend.FadeToColor(tp.fadeSeconds, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			state.move = vr::TeleportMove{};
		}
		if (step.fadeIn) {
			backend.FadeToColor(tp.fadeSeconds, 0.0f, 0.0f, 0.0f, 0.0f);
		}
		if (vr::TeleportSoundDue(tp.makesNoise, step.finished)) {
			// The landing heard ([Locomotion] TeleportSound).
			const UInt32 form = vr::TeleportSoundForm(tp.sound);
			const bool played = form != 0 ? game::PlaySoundForm(form) : game::PlayPlayerLandingSound();
			if (state.soundLines > 0) {
				--state.soundLines;
				OBVR_LOG("Teleport: the landing sound %s (%08X) %s",
				         vr::kTeleportSoundNames[static_cast<UInt32>(tp.sound)], form,
				         played ? "played" : "NOT played");
			}
		}
		if (step.finished) {
			NiPoint3 feet{0.0f, 0.0f, 0.0f};
			if (state.arrivedLines > 0 && game::ReadPlayerFeet(feet)) {
				--state.arrivedLines;
				const NiPoint3 miss = feet - state.move.to;
				float fatigueNow = 0.0f;
				float fatigueBase = 0.0f;
				const bool fatigueKnown = game::ReadPlayerFatigue(fatigueNow, fatigueBase);
				OBVR_LOG("Teleport: arrived, %.1f units from the target, fatigue %.1f of %.1f",
				         static_cast<double>(math::Sqrt(miss.LengthSquared())),
				         fatigueKnown ? static_cast<double>(fatigueNow) : -1.0, static_cast<double>(fatigueBase));
				state.landedFeet = feet;
				state.driftCheckLeft = 1.0f;
			}
		}
	}
	// No fall counted while the move runs (vr::TeleportMoveStep::fallSafe; not
	// the god mode, which refilled the fatigue).
	if (active && teleportFallSafe) {
		game::ClearPlayerFall();
	}
	// Heard like walking ([Locomotion] TeleportMakesNoise): for as long as
	// the move lasts, and at least kTeleportNoiseSeconds - a glide shorter
	// than a frame (the hand-script runs at a few frames a second) would
	// otherwise be over before the engine ever read the flags.
	if (state.noiseLeft > 0.0f) {
		state.noiseLeft -= dtSeconds;
	}
	if (state.driftCheckLeft > 0.0f) {
		state.driftCheckLeft -= dtSeconds;
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		if (state.driftCheckLeft <= 0.0f && state.driftLines > 0 && game::ReadPlayerFeet(feet)) {
			--state.driftLines;
			const NiPoint3 moved = feet - state.landedFeet;
			OBVR_LOG("Teleport: a second after the landing the player stands %.1f units from it "
			         "(%.1f across the ground; the left stick %s)",
			         static_cast<double>(math::Sqrt(moved.LengthSquared())),
			         static_cast<double>(math::Sqrt(moved.x * moved.x + moved.y * moved.y)),
			         hand.controls.move.forward || hand.controls.move.back || hand.controls.move.left ||
			                 hand.controls.move.right
			             ? "walking"
			             : "still");
		}
	}
	game::SetTeleportNoise(active && config.hands.teleport.makesNoise &&
	                       (state.move.phase != vr::TeleportPhase::Idle ||
	                        state.noiseLeft > 0.0f));
}

}  // namespace obvr::camera::hands

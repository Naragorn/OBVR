#include "camera/hands/Locomotion.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/AnalogWalk.h"
#include "game/FatigueCost.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/StaffLine.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

void LogArmSwing(LocomotionState& state, const HandStep& step, const HandShared& shared) {
	const Config& config = step.config;
	const bool active = step.active;
	const vr::HandModeResult& hand = shared.hand;
	const bool swinging = active && hand.armSwinging;
	if (swinging != state.swingWas && state.swingLines > 0) {
		--state.swingLines;
		OBVR_LOG("Arm swing: %s (the arms %s against each other at %.1f m/s or faster)",
		         swinging ? "running" : "walking again", swinging ? "swing" : "stopped swinging",
		         static_cast<double>(config.hands.armSwing.speedMetres));
	}
	state.swingWas = swinging;
}

namespace {

// "Hands: <what> - " and the walk's state at that moment.
void LogWithWalk(const char* what, const vr::HandModeResult& hand, const vr::HandModeFrame& frame) {
	const float left = frame.left.valid ? math::Sqrt(frame.left.velocity.LengthSquared()) : -1.0f;
	const float right = frame.right.valid ? math::Sqrt(frame.right.velocity.LengthSquared()) : -1.0f;
	OBVR_LOG("Hands: %s - the stick pushed %.2f (its run %d), a run asked %d, the arms running %d; the hands at "
	         "%.1f and %.1f m/s (left, right; -1 untracked)",
	         what, static_cast<double>(hand.analogStick.amount), hand.analogRunning ? 1 : 0,
	         hand.analogForcedRun ? 1 : 0, hand.armSwinging ? 1 : 0, static_cast<double>(left),
	         static_cast<double>(right));
}

}  // namespace

void LogBlockChange(LocomotionState& state, const vr::HandModeResult& hand, const vr::HandModeFrame& frame) {
	if (hand.blocking != state.blockingWas) {
		state.blockingWas = hand.blocking;
		if (!hand.blocking) {
			OBVR_LOG("Hands: the left hand is down - block released");
		} else {
			LogWithWalk(hand.bowGuard    ? "the bow is held upright - blocking"
			            : hand.fistGuard ? "both fists are up - blocking"
			                             : "the left hand is up - blocking",
			            hand, frame);
		}
	}
	const bool raised = hand.shieldPose.byContact && hand.shieldPose.raised;
	if (raised != state.shieldRaisedWas && state.shieldRaisedLines > 0) {
		--state.shieldRaisedLines;
		LogWithWalk(raised ? "the shield hand is up - no block held, the shield blocks what it is in the way of"
		                   : "the shield hand is down - the shield still blocks what it is in the way of",
		            hand, frame);
	}
	state.shieldRaisedWas = raised;
	const bool guardUp = hand.weaponPose.byContact && hand.weaponPose.raised;
	if (guardUp != state.weaponRaisedWas && state.weaponRaisedLines > 0) {
		--state.weaponRaisedLines;
		LogWithWalk(guardUp ? "the weapon's guard is up - no block held, the blade blocks what it is in the way of"
		                    : "the weapon's guard is down - the blade still blocks what it is in the way of",
		            hand, frame);
		if (game::StaffLineRead()) {
			// The staff's line as read (game/StaffLine.h): its node's axis taken
			// as a blade's - the first log tells whether that holds.
			NiPoint3 a;
			NiPoint3 b;
			game::LastStaffLine(a, b);
			const NiPoint3 d = b - a;
			const float length = math::Sqrt(d.LengthSquared());
			const float k = length > 0.0f ? 1.0f / length : 0.0f;
			OBVR_LOG("Hands: the staff's line - %.0f units long, along %.2f %.2f %.2f (the world)",
			         static_cast<double>(length), static_cast<double>(d.x * k), static_cast<double>(d.y * k),
			         static_cast<double>(d.z * k));
		}
	}
	state.weaponRaisedWas = guardUp;
	const bool torchUp = hand.torchPose.byContact && hand.torchPose.raised;
	if (torchUp != state.torchRaisedWas && state.torchRaisedLines > 0) {
		--state.torchRaisedLines;
		LogWithWalk(torchUp ? "the torch is up - no block held, its shaft blocks what it is in the way of"
		                    : "the torch is down - its shaft still blocks what it is in the way of",
		            hand, frame);
	}
	state.torchRaisedWas = torchUp;
	const bool fistsUp = hand.fistPose.byContact && hand.fistPose.raised;
	if (fistsUp != state.fistsRaisedWas && state.fistsRaisedLines > 0) {
		--state.fistsRaisedLines;
		LogWithWalk(fistsUp ? "the fists are up - no block held, a forearm blocks an unarmed blow in its way"
		                    : "the fists are down - a forearm still blocks an unarmed blow in its way",
		            hand, frame);
	}
	state.fistsRaisedWas = fistsUp;
}

void StepAnalogWalk(LocomotionState& state, const HandStep& step, const HandShared& shared, bool flying,
                    float deltaSeconds, bool rolesSwapped, vr::HandControlsWanted& controls) {
	const Config& config = step.config;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	vr::AnalogWalkGate gate;
	gate.fullVr = config.fullVrMode;
	gate.inWorld = game::PlayerInWorld() && (!menuIsUp || hand.walkingUnderMenu);
	gate.riding = game::PlayerRiding();
	gate.flyCam = flying;
	game::AnalogWalkRequest request;
	request.active = vr::AnalogWalkAllowed(config.hands.analogWalk, gate) && hand.analogWalking;
	request.stick = hand.analogStick;
	request.running = hand.analogRunning;
	request.forcedRun = hand.analogForcedRun;
	request.runFrom = hand.analogRunFrom;
	request.sprintFactor = config.hands.analogWalk.sprintFactor;
	// The run asked otherwise is the sprint (vr::StepSprint): with
	// fatigue it sprints and pays for it; spent, or the fatigue unread,
	// the push decides the gait. SprintFactor 1: the old full run.
	float fatigue = 0.0f;
	float fatigueMost = 0.0f;
	const bool fatigueKnown = game::ReadPlayerFatigue(fatigue, fatigueMost);
	const bool sprintAsked = request.active && request.forcedRun;
	request.sprinting = vr::StepSprint(state.sprint, sprintAsked, fatigueKnown, fatigue, request.sprintFactor);
	if (sprintAsked && !request.sprinting && request.sprintFactor > 1.0f) {
		request.forcedRun = false;
	}
	if (request.sprinting) {
		game::SpendPlayerFatigue(
			vr::SprintFatigueCost(config.hands.analogWalk.sprintFatiguePerSecond, deltaSeconds, fatigue),
			vr::FatigueCostKind::Sprint);
	}
	if (state.sprint.spent != state.spentWas && state.spentLines > 0) {
		--state.spentLines;
		OBVR_LOG("AnalogWalk: %s (fatigue %.1f)",
		         state.sprint.spent ? "the sprint spent - running by the push until the fatigue is back"
		                        : "the sprint ready again",
		         static_cast<double>(fatigue));
	}
	state.spentWas = state.sprint.spent;
	// Swimming with the arms: the swimmer's step for the player's move, and the game shown a direction.
	StepSwimArms(state.swim, step, shared, flying, rolesSwapped, request, controls);
	float pace = 1.0f;
	game::StepAnalogWalk(request, pace);
	if (request.active) {
		controls.run = vr::AnalogRunKey(request.running || request.forcedRun || request.sprinting,
		                                game::PlayerAlwaysRuns());
	}
	if (test::HandScriptMarkedThisFrame()) {
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		game::ReadPlayerFeet(feet);
		OBVR_LOG("AnalogWalk: at the mark - %s, push %.2f (%.2f %.2f), %s, the pace factor %.2f, Always Run %s, fatigue %.1f, the "
		         "feet at %.1f %.1f %.1f",
		         request.active ? "walking analog" : "not walking analog",
		         static_cast<double>(request.stick.amount), static_cast<double>(request.stick.x),
		         static_cast<double>(request.stick.y),
		         request.sprinting   ? "sprinting"
		         : request.forcedRun ? "a run asked"
		         : request.running   ? "running"
		                             : "walking",
		         static_cast<double>(pace), game::PlayerAlwaysRuns() ? "on" : "off", static_cast<double>(fatigue),
		         static_cast<double>(feet.x),
		         static_cast<double>(feet.y), static_cast<double>(feet.z));
	}
}

}  // namespace obvr::camera::hands

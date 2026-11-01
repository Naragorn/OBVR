#include "camera/hands/Locomotion.h"

#include "core/Log.h"
#include "game/AnalogWalk.h"
#include "game/FatigueCost.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
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

void StepAnalogWalk(LocomotionState& state, const HandStep& step, const HandShared& shared, bool flying,
                    float deltaSeconds, vr::HandControlsWanted& controls) {
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

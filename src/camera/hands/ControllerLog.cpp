#include "camera/hands/ControllerLog.h"

#include "core/Config.h"
#include "core/Log.h"
#include "vr/ControllerActions.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

void LogControllers(ControllerLogState& state, const vr::HandModeFrame& frame, float deltaSeconds) {
// What the controllers report, the first few times a button mask changes:
// which bits an Index puts its buttons on under the legacy path is what
// the gamepad layout rests on, and a wrong bit shows here as a mask.
if (state.maskLinesLeft > 0) {
	for (int side = 0; side < 2; ++side) {
		const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
		UInt64& last = side == 0 ? state.lastRightMask : state.lastLeftMask;
		if (hand.valid && hand.buttonsPressed != last) {
			last = hand.buttonsPressed;
			--state.maskLinesLeft;
			OBVR_LOG("Hands: %s buttons=%08X%08X trigger=%.2f stick=%.2f,%.2f (%s) grip=%.2f",
			         side == 0 ? "right" : "left",
			         static_cast<UInt32>(hand.buttonsPressed >> 32),
			         static_cast<UInt32>(hand.buttonsPressed), static_cast<double>(hand.trigger),
			         static_cast<double>(hand.thumbX), static_cast<double>(hand.thumbY),
			         hand.thumbFromJoystickAxis ? "axis 3" : "axis 0",
			         static_cast<double>(hand.gripForce));
		}
	}
}

// The grips and which actions SteamVR has bound, whenever either changes:
// the 2026-09-25 evening run closed a grip on an object with nothing to
// show for it, and the mask lines above had run out by then.
{
	for (int side = 0; side < 2; ++side) {
		const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
		if (!hand.valid) {
			continue;
		}
		const bool grip = vr::GripDown(hand.buttonsPressed);
		if (grip != state.grip[side] || hand.actionActiveMask != state.active[side]) {
			state.grip[side] = grip;
			state.active[side] = hand.actionActiveMask;
			OBVR_LOG("Hands: %s grip %s, actions %s, bound mask %02X (stick click, A, B, grip, "
			         "trackpad from bit 0)",
			         side == 0 ? "right" : "left", grip ? "CLOSED" : "open",
			         hand.actionInput ? "in use" : "not in use (legacy input)",
			         hand.actionActiveMask);
		}
	}
}

// The grip's signals for a measurement ([Debug] GripTrace, vr/GripTrace.h):
// the pull, the force and the click of each hand, at its first tracked
// frame, every half second, and at once when the click changes.
{
	vr::GripTraceHand hands[2];
	for (int side = 0; side < 2; ++side) {
		const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
		hands[side].valid = hand.valid;
		hands[side].click = hand.valid && vr::GripDown(hand.buttonsPressed);
	}
	const vr::GripTraceVerdict trace =
		vr::StepGripTrace(state.gripTrace, hands, GetConfig().hands.gripTrace, deltaSeconds);
	for (int side = 0; side < 2; ++side) {
		if (trace.why[side] == vr::GripTraceWhy::Nothing) {
			continue;
		}
		const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
		OBVR_LOG("Grip trace: %s pull %.2f%s force %.2f%s click %d (%s)", side == 0 ? "right" : "left",
		         static_cast<double>(hand.gripPull),
		         vr::GripSignalNote(hand.actionInput, hand.actionActiveMask, vr::input::GripPull),
		         static_cast<double>(hand.gripForce),
		         vr::GripSignalNote(hand.actionInput, hand.actionActiveMask, vr::input::GripForce),
		         hands[side].click ? 1 : 0, vr::GripTraceWhyText(trace.why[side]));
	}
}

// How long a hand's actions stay dead (vr/ActionsWatch.h): the mask 00
// a tester's log of 2026-10-17 read four times, on the change alone -
// whether his sprint and jump went with it wants the seconds.
{
	for (int side = 0; side < 2; ++side) {
		const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
		const vr::ActionsInactiveVerdict dead = vr::StepActionsInactive(
			state.actionsWatch[side], hand.valid, hand.actionInput, hand.actionActiveMask, deltaSeconds);
		if ((dead.reportInactive || dead.reportBack) && state.actionsLinesLeft > 0) {
			--state.actionsLinesLeft;
			if (dead.reportInactive) {
				OBVR_LOG("Hands: the %s hand's actions have been inactive for %.0f s (bound mask 00: SteamVR "
				         "gives the input to something else - its dashboard, an overlay, the headset put down); "
				         "its sticks and buttons do nothing meanwhile",
				         side == 0 ? "right" : "left", static_cast<double>(dead.seconds));
			} else {
				OBVR_LOG("Hands: the %s hand's actions are back after %.1f s", side == 0 ? "right" : "left",
				         static_cast<double>(dead.seconds));
			}
		}
	}
}

if (frame.right.valid != state.rightTracked || frame.left.valid != state.leftTracked) {
	state.rightTracked = frame.right.valid;
	state.leftTracked = frame.left.valid;
	OBVR_LOG("Hands: right controller %s, left controller %s",
	         frame.right.valid ? "tracked" : "not tracked",
	         frame.left.valid ? "tracked" : "not tracked");
}
}

}  // namespace obvr::camera::hands

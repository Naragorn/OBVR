#pragma once

#include "vr/ScrollGesture.h"

namespace obvr::vr::menu {

enum class ScrollPhase {
	Inactive, PoseDetected, Grabbed, Unrolling, Open, HeldOpen,
	Interacting, RollingUp, Closing, AwaitRelease
};
enum class EngineMenu { Closed, Owned, Foreign };
enum class ScrollCommand { None, Open, Close, CancelPendingOpen };

struct ScrollSessionFrame {
	GestureFrame gesture;
	ScrollOrientation orientation = ScrollOrientation::Off;
	bool gripKnown[2]{false, false}; // Missing action data is never a release.
	bool grip[2]{false, false}; // left, right
	bool bridgeAvailable = false;
	bool contextValid = false; // focus, world, Full VR, unchanged tracking origin
	bool cancel = false;
	bool freeEndReached = false; // geometric re-grab test, not just a grip press
	EngineMenu menu = EngineMenu::Closed;
	UInt32 menuGeneration = 0; // engine-observed lifecycle, never a pointer alone
};

struct ScrollSessionState {
	ScrollPhase phase = ScrollPhase::Inactive;
	ReadyState readiness;
	ScrollOrientation orientation = ScrollOrientation::Off;
	bool lastGrip[2]{false, false};
	bool lastGripKnown[2]{false, false};
	int support = -1;
	float amount = 0;
	float previousDistance = 0;
	float pendingSeconds = 0;
	UInt32 menuGeneration = 0;
	bool openPending = false;
};

struct ScrollSessionResult {
	ScrollPhase phase = ScrollPhase::Inactive;
	ScrollCommand command = ScrollCommand::None;
	float amount = 0;
	int support = -1;
	bool visible = false;
	bool ownsInput = false;
	bool interactive = false;
	bool readyCue = false;
	bool fullCue = false;
	bool closedCue = false;
	bool captureSupport = false;
	bool releaseSelection = false;
};

inline bool Manipulating(ScrollPhase p) {
	return p != ScrollPhase::Inactive && p != ScrollPhase::PoseDetected &&
	       p != ScrollPhase::AwaitRelease && p != ScrollPhase::Closing;
}

// Pure transition function. Commands are requests; the engine bridge supplies
// acknowledgements on later frames. No clock-driven transition changes amount.
// Closing is bounded: if the engine cannot close, its ordinary menu remains
// usable through the legacy input path after a neutral controller sample.
inline ScrollSessionResult StepScroll(ScrollSessionState& s, const ScrollSessionFrame& f,
                                      const ScrollLimits& limits = {}) {
	ScrollSessionResult r;
	const auto before = s.phase;
	const bool dtValid = Finite(f.gesture.dt) && f.gesture.dt > 0 && f.gesture.dt <= 0.1f;
	const bool tracked = Valid(f.gesture.head) && Valid(f.gesture.left) && Valid(f.gesture.right);
	const bool known = f.gripKnown[0] && f.gripKnown[1];
	const bool neutral = known && !f.grip[0] && !f.grip[1] && f.gesture.controlsNeutral;
	const float distance = tracked
	    ? math::Sqrt((f.gesture.right.position-f.gesture.left.position).LengthSquared()) : -1;
	const Opening opening = MapOpening(distance, limits);
	const bool enabled = f.orientation == ScrollOrientation::Horizontal ||
	                     f.orientation == ScrollOrientation::Vertical;
	const bool down[2] = {
		known && s.lastGripKnown[0] && f.grip[0] && !s.lastGrip[0],
		known && s.lastGripKnown[1] && f.grip[1] && !s.lastGrip[1]};
	const bool up[2] = {
		known && s.lastGripKnown[0] && !f.grip[0] && s.lastGrip[0],
		known && s.lastGripKnown[1] && !f.grip[1] && s.lastGrip[1]};
	const bool sameMenu = f.menu == EngineMenu::Owned && s.menuGeneration != 0 &&
	                      f.menuGeneration == s.menuGeneration;
	const bool valid = enabled && Valid(limits) && tracked && known && dtValid &&
	                   opening.valid && f.contextValid && f.bridgeAvailable;

	auto close = [&]() {
		r.releaseSelection = true;
		if (sameMenu) r.command = ScrollCommand::Close;
		else if (s.openPending) r.command = ScrollCommand::CancelPendingOpen;
		s.openPending = false;
		s.pendingSeconds = 0;
		s.phase = ScrollPhase::Closing;
	};

	if (Manipulating(s.phase) && (!valid || f.cancel || f.orientation != s.orientation ||
	                            f.menu == EngineMenu::Foreign)) {
		close();
	} else switch (s.phase) {
	case ScrollPhase::Inactive:
	case ScrollPhase::PoseDetected: {
		if (!valid || f.menu != EngineMenu::Closed || f.cancel) {
			s.readiness = {}; s.phase = ScrollPhase::Inactive;
			break;
		}
		// A grip press consumes an already completed neutral dwell. Checking
		// it before StepReady avoids resetting the dwell on this intended press.
		if (s.readiness.ready && (down[0] || down[1]) && ScrollPose(f.gesture, f.orientation, limits)) {
			s.phase = ScrollPhase::Grabbed; s.orientation = f.orientation;
			s.amount = opening.amount; s.previousDistance = distance;
			s.menuGeneration = 0; s.support = -1; s.readiness = {};
		} else {
			auto readyFrame = f.gesture;
			readyFrame.controlsNeutral = neutral;
			const auto ready = StepReady(s.readiness, readyFrame, f.orientation, limits);
			r.readyCue = ready.entered;
			s.phase = s.readiness.dwell > 0 ? ScrollPhase::PoseDetected : ScrollPhase::Inactive;
		}
		break;
	}
	case ScrollPhase::Grabbed:
	case ScrollPhase::Unrolling:
	case ScrollPhase::RollingUp:
	case ScrollPhase::Open: {
		if ((!f.grip[0] && !f.grip[1]) ||
		    (s.menuGeneration != 0 && !sameMenu) ||
		    (s.menuGeneration == 0 && f.menu != EngineMenu::Closed)) {
			close(); break;
		}
		s.amount = opening.amount;
		// A release on the first full-distance sample cannot also activate.
		// Both hands must have held the full-open pose in an earlier sample.
		if (before == ScrollPhase::Open && opening.fullyOpen &&
		    ((up[0] && f.grip[1]) || (up[1] && f.grip[0]))) {
			s.support = up[0] ? 1 : 0;
			r.captureSupport = true;
			if (sameMenu) s.phase = ScrollPhase::Interacting;
			else {
				s.phase = ScrollPhase::HeldOpen; s.openPending = true;
				s.pendingSeconds = 0; r.command = ScrollCommand::Open;
			}
		} else if (opening.fullyOpen && f.grip[0] && f.grip[1]) {
			s.phase = ScrollPhase::Open;
			r.fullCue = before != ScrollPhase::Open;
		} else if (distance <= limits.closedDistance + 0.000001f && before != ScrollPhase::Grabbed) {
			// One micrometre absorbs cancellation error in translated tracking
			// coordinates. This epsilon never applies to the full-open gate.
			r.closedCue = true; close();
		} else {
			s.phase = distance < s.previousDistance ? ScrollPhase::RollingUp : ScrollPhase::Unrolling;
		}
		s.previousDistance = distance;
		break;
	}
	case ScrollPhase::HeldOpen:
	case ScrollPhase::Interacting: {
		if (s.support < 0 || s.support > 1 || !f.grip[s.support]) { close(); break; }
		if (s.phase == ScrollPhase::HeldOpen) {
			s.pendingSeconds += f.gesture.dt;
			if (f.menu == EngineMenu::Owned && f.menuGeneration != 0) {
				s.menuGeneration = f.menuGeneration; s.openPending = false;
				s.phase = ScrollPhase::Interacting;
			} else if (s.pendingSeconds >= 1.0f) { close(); break; }
		} else if (!sameMenu) { close(); break; }
		const int free = 1-s.support;
		if (s.phase == ScrollPhase::Interacting && down[free] && f.freeEndReached && opening.fullyOpen) {
			// Start with both physical ends at their real full separation. A
			// press away from that end never snaps the surface to the other hand.
			r.releaseSelection = true; s.phase = ScrollPhase::Open;
			s.previousDistance = distance;
		}
		break;
	}
	case ScrollPhase::Closing:
		s.pendingSeconds += dtValid ? f.gesture.dt : 0.1f;
		if (!sameMenu || s.pendingSeconds >= 1.0f || !f.bridgeAvailable)
			s.phase = ScrollPhase::AwaitRelease;
		break;
	case ScrollPhase::AwaitRelease:
		if (tracked && neutral) s = {};
		break;
	}

	for (int hand = 0; hand < 2; ++hand) {
		s.lastGrip[hand] = f.grip[hand]; s.lastGripKnown[hand] = f.gripKnown[hand];
	}
	r.phase = s.phase; r.amount = s.amount; r.support = s.support;
	r.visible = Manipulating(s.phase) || (s.phase == ScrollPhase::PoseDetected && s.readiness.ready);
	r.ownsInput = s.phase != ScrollPhase::Inactive && s.phase != ScrollPhase::PoseDetected;
	r.interactive = s.phase == ScrollPhase::Interacting && valid &&
	                f.menu == EngineMenu::Owned && f.menuGeneration == s.menuGeneration;
	r.releaseSelection = r.releaseSelection || (before == ScrollPhase::Interacting && !r.interactive);
	return r;
}

} // namespace obvr::vr::menu

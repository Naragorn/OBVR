#pragma once

// The grip's signals, logged for a measurement ([Debug] GripTrace;
// docs/free-weapons-spec.md section 5.4). The grips to come - the holsters,
// the weapon taken from the world by its grip, the second hand, the drop -
// are to close on a realistic hold rather than on the binding's force click
// (the tester, 2026-10-23), and where that hold lies on the Index's two grip
// signals nobody has measured: the pull (/input/grip "value", the capacitive
// sensing, which reads 1.0 at a light touch - godotengine/godot issue 74023)
// and the force (/input/grip "force", the force sensor, which the shipped
// binding clicks at 0.3). So while the trace is on, a line per hand says
// what the pull, the force and the click read: at the hand's first tracked
// frame, every half second after, and at once when the click changes - the
// moment the tester's hand closed or opened, with that frame's values. A
// hand that loses tracking is forgotten and gets a first line again. Pure,
// covered by grip_trace_test; CameraHook writes the lines.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr float kGripTracePeriodSeconds = 0.5f;

struct GripTraceHand {
	bool valid = false;  // tracked this frame
	bool click = false;  // the grip's click (GripDown)
};

struct GripTraceState {
	bool seen[2] = {false, false};   // a line has been written for the hand
	bool click[2] = {false, false};  // the click as last written
	float since[2] = {0.0f, 0.0f};   // seconds since the last line
};

enum class GripTraceWhy : unsigned char { Nothing, First, Period, Click };

struct GripTraceVerdict {
	GripTraceWhy why[2] = {GripTraceWhy::Nothing, GripTraceWhy::Nothing};  // right, left
};

inline GripTraceVerdict StepGripTrace(GripTraceState& s, const GripTraceHand hands[2], bool enabled,
                                      float dt) {
	GripTraceVerdict v;
	if (!enabled) {
		s = GripTraceState{};
		return v;
	}
	const float step = dt == dt && dt > 0.0f ? dt : 0.0f;
	for (int side = 0; side < 2; ++side) {
		const GripTraceHand& hand = hands[side];
		if (!hand.valid) {
			s.seen[side] = false;
			s.since[side] = 0.0f;
			continue;
		}
		GripTraceWhy why = GripTraceWhy::Nothing;
		if (!s.seen[side]) {
			why = GripTraceWhy::First;
		} else if (hand.click != s.click[side]) {
			why = GripTraceWhy::Click;
		} else {
			s.since[side] += step;
			if (s.since[side] >= kGripTracePeriodSeconds) {
				why = GripTraceWhy::Period;
			}
		}
		if (why != GripTraceWhy::Nothing) {
			s.seen[side] = true;
			s.click[side] = hand.click;
			s.since[side] = 0.0f;
			v.why[side] = why;
		}
	}
	return v;
}

inline const char* GripTraceWhyText(GripTraceWhy why) {
	switch (why) {
		case GripTraceWhy::First:
			return "first";
		case GripTraceWhy::Period:
			return "every 0.5 s";
		case GripTraceWhy::Click:
			return "click changed";
		default:
			return "";
	}
}

// What to say after a signal's value: nothing when its action is bound and
// active; " (not bound)" when the action path has it inactive - a binding
// without it, a controller without the sensor; " (legacy)" on the legacy
// path, which reads no pull and takes the force from axis 2.
inline const char* GripSignalNote(bool actionInput, UInt32 activeMask, unsigned actionBit) {
	if (!actionInput) {
		return " (legacy)";
	}
	return (activeMask & (1u << actionBit)) != 0 ? "" : " (not bound)";
}

}  // namespace obvr::vr

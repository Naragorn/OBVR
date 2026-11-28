#pragma once

// The finger on the tablet as on a touch screen (docs/inventory-tablet-spec.md
// 4.3, stage 2): a press decides on the way out, not the moment the tip
// reaches the glass.
// - down and up again without moving: a click, where the finger went down
//   (the cursor is held there from the touch to the click);
// - moved up or down past the slop: the list scrolls with the finger by wheel
//   notches, one per kTabletTouchNotchMetres, nothing is clicked; let go while
//   moving, it coasts on (the laser's coasting, vr::StepLaserPress);
// - swiped sideways past kTabletTouchSwipeMetres: the next or the previous
//   page, once per touch - the finger moved left turns to the page on the
//   right, as a page is turned on a tablet; a book read alone turns, a
//   letter only scrolls (TabletTouchInput::turns and scrolls);
// - held still for kTabletTouchHoldSeconds, or put down on a scroll bar's
//   knob: the button goes down and stays until the finger leaves (sliders,
//   the quantity box, the bar's knob);
// - moved past the slop and let go without a scroll or a page: nothing;
// - the finger gone from the face - off its edge, far through it, curled, or
//   the hand lost - ends the touch with nothing clicked.
//
// Where the touch begins and ends is the contact's (StepTabletContact): the
// glass gives no resistance, so the tip goes through it without the player
// knowing, and the touch ends as the tip is pulled back from its deepest
// point - not only once it is back in front of the glass (section 14, the
// headset run of 2026-11-20: nine touches, no tap).
// Distances are metres on the glass, so the feel does not depend on the
// tablet's size or the layer's pixels. The numbers are guesses for the
// headset. Pure; covered by tablet_touch_test and menu_place_test.

#include "vr/FingerTip.h"
#include "vr/HandInput.h"
#include "vr/TouchHandover.h"

namespace obvr::vr {

inline constexpr float kTabletTouchSlopMetres = 0.02f;    // a move, not a finger's wobble or its way in
inline constexpr float kTabletTouchSwipeMetres = 0.04f;   // sideways this far: a page
inline constexpr float kTabletTouchNotchMetres = 0.02f;   // the finger's travel per wheel notch
inline constexpr float kTabletTouchHoldSeconds = 0.5f;    // held still this long: the button stays down
inline constexpr float kTabletTouchStillMetres = 0.01f;   // a tap's wobble (the headset's taps: 0.2-0.9 cm at their release)

// ------------------------------------------------------------- The contact

// How deep counts, metres in front of the glass (negative: through it).
struct TabletContactLimits {
	float press = 0.015f;       // the tip this near in front of the glass: the touch begins
	float release = 0.04f;      // back out past this: lifted, whatever came before
	float begin = 0.06f;        // a touch begins no further behind the glass than this
	float deep = 0.25f;         // a tip further through than this is a hand behind the panel
	float pullBackTap = 0.02f;  // pulled back this far from its deepest point: let go (a tap's way out)
	float pullBackMove = 0.05f; // the same while the finger scrolls or drags, whose depth wanders
	// The same for a touch that has kept still on the glass (TabletTouchStill):
	// the finger turned back, so the tap's click goes out now, not 2 cm later
	// (Nadi, Windows 2026-11-27: the touch came a few frames late).
	float pullBackStill = 0.006f;
};

// How a touch ended, for the log.
enum class TabletContactEnd : UInt8 { None, Lifted, PulledBack, OffEdge, TooDeep, FingerIn };

struct TabletContactState {
	// Ready: a tip at the glass begins a touch. Released: the last touch ended
	// behind the glass - the next begins when the tip goes back in by
	// pullBackTap, or after it came out in front. Behind: the hand went far
	// through - only from the front again.
	enum class Arm : UInt8 { Ready, Released, Behind };
	bool touching = false;
	Arm arm = Arm::Ready;
	float deepest = 0.0f;  // of the running touch
	float back = 0.0f;     // the furthest out since the release
};

struct TabletContact {
	bool onGlass = false;   // the finger out, over the face, not far through it
	bool touching = false;
	TabletContactEnd end = TabletContactEnd::None;  // the frame a touch ended, how
	float deepest = 0.0f;   // the touch's deepest point so far (or as it ended)
};

// `fingerOut`: the index out and the trigger let go (vr::FingerPokes).
// `inside`: the tip's foot within the face. `moving`: the touch scrolls,
// drags or turned a page. `still`: it has not moved along the glass past a
// tap's wobble (TabletTouchStill), so it lets go as soon as it turns back.
inline TabletContact StepTabletContact(TabletContactState& s, bool fingerOut, bool inside, float depth, bool moving,
                                       const TabletContactLimits& l = TabletContactLimits{}, bool still = false) {
	using Arm = TabletContactState::Arm;
	TabletContact c;
	if (!fingerOut || !inside) {
		c.end = !s.touching ? TabletContactEnd::None : (fingerOut ? TabletContactEnd::OffEdge : TabletContactEnd::FingerIn);
		c.deepest = s.deepest;
		s = TabletContactState{};
		return c;
	}
	if (s.touching) {
		s.deepest = depth < s.deepest ? depth : s.deepest;
		c.deepest = s.deepest;
		if (depth < -l.deep) {
			c.end = TabletContactEnd::TooDeep;
			s = TabletContactState{};
			s.arm = Arm::Behind;
			return c;
		}
		c.onGlass = true;
		const float pullBack = moving ? l.pullBackMove : (still ? l.pullBackStill : l.pullBackTap);
		if (depth > l.release) {
			c.end = TabletContactEnd::Lifted;
		} else if (depth - s.deepest >= pullBack) {
			c.end = TabletContactEnd::PulledBack;
		} else {
			c.touching = true;
			return c;
		}
		s.touching = false;
		s.arm = depth > l.press ? Arm::Ready : Arm::Released;
		s.back = depth;
		return c;
	}
	c.onGlass = depth >= -l.begin;
	bool again = false;
	if (depth > l.press) {
		s.arm = Arm::Ready;  // in front of the glass: the next touch is an ordinary one
	} else if (s.arm == Arm::Released) {
		s.back = depth > s.back ? depth : s.back;
		again = s.back - depth >= l.pullBackTap && depth >= -l.deep;
	}
	if (again || (s.arm == Arm::Ready && depth <= l.press && depth >= -l.begin)) {
		s.touching = true;
		s.arm = Arm::Ready;
		s.deepest = depth;
		c.touching = c.onGlass = true;
		c.deepest = depth;
	}
	return c;
}

// --------------------------------------------------------------- The touch

enum class TabletTouchPhase : UInt8 { Idle, Down, Scrolling, Turned, Dragging };

// What a frame changed, for the log. Moved: let go after a move that was
// neither a scroll nor a page.
enum class TabletTouchEvent : UInt8 { None, Tap, ScrollStart, PageTurn, HoldStart, Cancelled, Moved };

struct TabletTouchInput {
	bool onGlass = false;   // the finger out, over the face, not far through it (the contact's)
	bool touching = false;  // at the glass (the contact's)
	float glassX = 0.0f;    // metres on the face from its centre, to the right
	float glassY = 0.0f;    // and down
	float pixelX = 0.0f;    // the layer's pixel under the tip
	float pixelY = 0.0f;
	bool onScrollBar = false;  // a scroll bar's knob or a drag surface under the cursor
	// What a move may do here (vr/TabletBook.h): a book turns but does not
	// scroll, a letter scrolls but does not turn. A move it may not do is let
	// go as one that was neither.
	bool turns = true;
	bool scrolls = true;
};

struct TabletTouchVerdict {
	bool mouseDown = false;
	int wheel = 0;         // notches, positive up
	int pageStep = 0;      // +1 the page to the right, -1 the one to the left
	bool active = false;   // a touch running or its click going out: the finger owns the cursor
	bool pinned = false;   // the cursor held where the finger went down
	float pinX = 0.0f;
	float pinY = 0.0f;
	TabletTouchEvent event = TabletTouchEvent::None;
	float seconds = 0.0f;  // the touch so far: on the glass this long,
	float travel = 0.0f;   // and at most this far from where it went down
};

struct TabletTouchState {
	TabletTouchPhase phase = TabletTouchPhase::Idle;
	float startX = 0.0f;  // on the glass
	float startY = 0.0f;
	float startPixelX = 0.0f;
	float startPixelY = 0.0f;
	float seconds = 0.0f;
	float travel = 0.0f;        // the furthest from the start, along either axis
	bool clickPending = false;  // the tap's click went down; up next frame
	LaserPressState scroll;     // the scroll and its coasting, in metres
	TabletContactState contact; // where the touch begins and ends (StepTabletContact)
	TouchHandoverState handover;  // the finger or the laser leads (vr/TouchHandover.h)
};

// What the hand mode hands on of a frame's touch, for the log: the event, how
// a touch ended, and the touch's measures (the headset's numbers to tune by).
struct TabletTouchReport {
	TabletTouchEvent event = TabletTouchEvent::None;
	TabletContactEnd end = TabletContactEnd::None;
	float deepestMetres = 0.0f;  // in front of the glass; negative through it
	float travelMetres = 0.0f;
	float seconds = 0.0f;
	// The laser yielding to the finger (vr/TouchHandover.h): the beam's alpha,
	// whether the finger took or gave the pointer this frame (and the tip's
	// distance then), and the touch mark where the tip would land - in the
	// layer's pixels, grown 0 to 1.
	float laserAlpha = 1.0f;
	bool leadChanged = false;
	float leadDepthMetres = 0.0f;
	bool mark = false;
	float markShare = 0.0f;
	float markX = 0.0f;
	float markY = 0.0f;
};

// StepLaserPress measures in shares of a layer height of at least one unit:
// the finger's way goes in as millimetres, and this height makes one of its
// notches the finger's notch.
inline constexpr float kTabletTouchUnits = 1000.0f;
inline constexpr float kTabletTouchScrollHeight = kTabletTouchNotchMetres * kTabletTouchUnits / kLaserPixelsPerNotch;

// The finger down: a tap on the release, or what its move or its rest makes
// of it.
inline void StepTabletTouchDown(TabletTouchState& s, const TabletTouchInput& in, float dtSeconds,
                                TabletTouchVerdict& v) {
	const float dx = in.glassX - s.startX;
	const float dy = in.glassY - s.startY;
	const float ax = dx < 0.0f ? -dx : dx;
	const float ay = dy < 0.0f ? -dy : dy;
	const float furthest = ax > ay ? ax : ay;
	s.travel = furthest > s.travel ? furthest : s.travel;
	if (!in.touching) {
		if (s.travel < kTabletTouchSlopMetres) {
			v.mouseDown = true;
			v.event = TabletTouchEvent::Tap;
			s.clickPending = true;
		} else {
			v.event = TabletTouchEvent::Moved;
			s.phase = TabletTouchPhase::Idle;
		}
		return;
	}
	s.seconds += dtSeconds > 0.0f ? dtSeconds : 0.0f;
	if (in.onScrollBar || (s.travel < kTabletTouchSlopMetres && s.seconds >= kTabletTouchHoldSeconds)) {
		s.phase = TabletTouchPhase::Dragging;
		v.event = TabletTouchEvent::HoldStart;
	} else if (in.scrolls && ay >= kTabletTouchSlopMetres && ay >= ax) {
		s.phase = TabletTouchPhase::Scrolling;
		s.scroll.phase = LaserPressPhase::Scrolling;
		s.scroll.startX = s.startX * kTabletTouchUnits;
		s.scroll.startY = s.scroll.lastY = s.startY * kTabletTouchUnits;
		v.event = TabletTouchEvent::ScrollStart;
	} else if (in.turns && ax >= kTabletTouchSwipeMetres && ax > ay) {
		s.phase = TabletTouchPhase::Turned;
		v.pageStep = dx < 0.0f ? 1 : -1;
		v.event = TabletTouchEvent::PageTurn;
	}
}

// The touch has kept still on the glass: down, not moved past a tap's wobble.
// Its contact lets go as the finger turns back (pullBackStill); a touch that
// has begun to move keeps the longer way out, so a drag's start that drifts
// out a little is no tap.
inline bool TabletTouchStill(const TabletTouchState& s) {
	return s.phase == TabletTouchPhase::Down && s.travel < kTabletTouchStillMetres;
}

inline TabletTouchVerdict StepTabletTouch(TabletTouchState& s, const TabletTouchInput& in, float dtSeconds) {
	TabletTouchVerdict v;
	v.seconds = s.seconds;
	v.travel = s.travel;
	if (s.clickPending) {
		// The tap's click went down last frame, held where the finger went down.
		s.clickPending = false;
		s.phase = TabletTouchPhase::Idle;
		v.active = v.pinned = true;
		v.pinX = s.startPixelX;
		v.pinY = s.startPixelY;
		return v;
	}
	const bool running = s.phase != TabletTouchPhase::Idle;
	if (running && !in.onGlass) {
		v.event = TabletTouchEvent::Cancelled;
		s.phase = TabletTouchPhase::Idle;
		s.scroll = LaserPressState{};
		return v;
	}
	switch (s.phase) {
	case TabletTouchPhase::Idle:
		if (!in.touching || !in.onGlass) {
			// A flicked list coasting on.
			v.wheel = StepLaserPress(s.scroll, false, false, 0.0f, 0.0f, kTabletTouchScrollHeight, dtSeconds).wheel;
			return v;
		}
		s.scroll = LaserPressState{};  // a new touch stops a coasting list
		s.startX = in.glassX;
		s.startY = in.glassY;
		s.startPixelX = in.pixelX;
		s.startPixelY = in.pixelY;
		s.seconds = 0.0f;
		s.travel = 0.0f;
		s.phase = TabletTouchPhase::Down;
		if (in.onScrollBar) {
			s.phase = TabletTouchPhase::Dragging;
			v.event = TabletTouchEvent::HoldStart;
		}
		break;
	case TabletTouchPhase::Down:
		StepTabletTouchDown(s, in, dtSeconds, v);
		break;
	case TabletTouchPhase::Scrolling:
	case TabletTouchPhase::Turned:
	case TabletTouchPhase::Dragging:
		s.seconds += dtSeconds > 0.0f ? dtSeconds : 0.0f;
		break;
	}
	if (s.phase == TabletTouchPhase::Scrolling) {
		v.wheel = StepLaserPress(s.scroll, in.touching, true, in.glassX * kTabletTouchUnits,
		                         in.glassY * kTabletTouchUnits, kTabletTouchScrollHeight, dtSeconds)
		              .wheel;
	}
	if (s.phase == TabletTouchPhase::Dragging) {
		v.mouseDown = in.touching;
	}
	if (s.phase != TabletTouchPhase::Down && !s.clickPending && !in.touching) {
		s.phase = TabletTouchPhase::Idle;  // the finger left: the touch is over
	}
	v.active = s.phase != TabletTouchPhase::Idle || s.clickPending;
	v.pinned = s.phase == TabletTouchPhase::Down || s.clickPending;
	v.pinX = s.startPixelX;
	v.pinY = s.startPixelY;
	v.seconds = s.seconds;
	v.travel = s.travel;
	return v;
}

}  // namespace obvr::vr

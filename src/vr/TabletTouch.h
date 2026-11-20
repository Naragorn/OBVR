#pragma once

// The finger on the tablet as on a touch screen (docs/inventory-tablet-spec.md
// 4.3, stage 2): a press decides on the way out, not the moment the tip
// reaches the glass.
// - down and up again without moving: a click, where the finger went down
//   (the cursor is held there from the touch to the click);
// - moved up or down past the slop: the list scrolls with the finger by wheel
//   notches, one per kTabletTouchNotchMetres, nothing is clicked; let go while
//   moving, it coasts on (the laser's coasting, vr::StepLaserPress);
// - moved sideways past the slop: the next or the previous page, once per
//   touch - the finger moved left turns to the page on the right, as a page
//   is turned on a tablet;
// - held still for kTabletTouchHoldSeconds, or put down on a scroll bar's
//   knob: the button goes down and stays until the finger leaves (sliders,
//   the quantity box, the bar's knob);
// - the finger gone from the face - off its edge, through it from behind,
//   curled, or the hand lost - ends the touch with nothing clicked.
// Distances are metres on the glass, so the feel does not depend on the
// tablet's size or the layer's pixels. The numbers are first guesses for the
// headset. Pure; covered by tablet_touch_test and menu_place_test.

#include "vr/FingerTip.h"
#include "vr/HandInput.h"

namespace obvr::vr {

inline constexpr float kTabletTouchSlopMetres = 0.012f;   // a move, not a finger's wobble
inline constexpr float kTabletTouchNotchMetres = 0.02f;   // the finger's travel per wheel notch
inline constexpr float kTabletTouchHoldSeconds = 0.5f;    // held still this long: the button stays down

enum class TabletTouchPhase : UInt8 { Idle, Down, Scrolling, Turned, Dragging };

// What a frame changed, for the log.
enum class TabletTouchEvent : UInt8 { None, Tap, ScrollStart, PageTurn, HoldStart, Cancelled };

struct TabletTouchInput {
	bool onGlass = false;   // the finger out, over the face, not through it from behind
	bool touching = false;  // at the glass (vr::StepPoke's held, with its release margin)
	float glassX = 0.0f;    // metres on the face from its centre, to the right
	float glassY = 0.0f;    // and down
	float pixelX = 0.0f;    // the layer's pixel under the tip
	float pixelY = 0.0f;
	bool onScrollBar = false;  // a scroll bar's knob or a drag surface under the cursor
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
};

struct TabletTouchState {
	TabletTouchPhase phase = TabletTouchPhase::Idle;
	float startX = 0.0f;  // on the glass
	float startY = 0.0f;
	float startPixelX = 0.0f;
	float startPixelY = 0.0f;
	float seconds = 0.0f;
	bool clickPending = false;  // the tap's click went down; up next frame
	LaserPressState scroll;     // the scroll and its coasting, in metres
};

// StepLaserPress measures in shares of a layer height of at least one unit:
// the finger's way goes in as millimetres, and this height makes one of its
// notches the finger's notch.
inline constexpr float kTabletTouchUnits = 1000.0f;
inline constexpr float kTabletTouchScrollHeight = kTabletTouchNotchMetres * kTabletTouchUnits / kLaserPixelsPerNotch;

inline TabletTouchVerdict StepTabletTouch(TabletTouchState& s, const TabletTouchInput& in, float dtSeconds) {
	TabletTouchVerdict v;
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
		s.phase = TabletTouchPhase::Down;
		if (in.onScrollBar) {
			s.phase = TabletTouchPhase::Dragging;
			v.event = TabletTouchEvent::HoldStart;
		}
		break;
	case TabletTouchPhase::Down: {
		if (!in.touching) {
			v.mouseDown = true;
			v.event = TabletTouchEvent::Tap;
			s.clickPending = true;
			break;
		}
		s.seconds += dtSeconds > 0.0f ? dtSeconds : 0.0f;
		const float dx = in.glassX - s.startX;
		const float dy = in.glassY - s.startY;
		const float ax = dx < 0.0f ? -dx : dx;
		const float ay = dy < 0.0f ? -dy : dy;
		if (in.onScrollBar || (ax < kTabletTouchSlopMetres && ay < kTabletTouchSlopMetres &&
		                       s.seconds >= kTabletTouchHoldSeconds)) {
			s.phase = TabletTouchPhase::Dragging;
			v.event = TabletTouchEvent::HoldStart;
		} else if (ay >= kTabletTouchSlopMetres && ay >= ax) {
			s.phase = TabletTouchPhase::Scrolling;
			s.scroll.phase = LaserPressPhase::Scrolling;
			s.scroll.startX = s.startX * kTabletTouchUnits;
			s.scroll.startY = s.scroll.lastY = s.startY * kTabletTouchUnits;
			v.event = TabletTouchEvent::ScrollStart;
		} else if (ax >= kTabletTouchSlopMetres) {
			s.phase = TabletTouchPhase::Turned;
			v.pageStep = dx < 0.0f ? 1 : -1;
			v.event = TabletTouchEvent::PageTurn;
		}
		break;
	}
	case TabletTouchPhase::Scrolling:
	case TabletTouchPhase::Turned:
	case TabletTouchPhase::Dragging:
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
	return v;
}

}  // namespace obvr::vr

#pragma once

// The lock's minigame under the controllers ([Hands] LockpickByHand; a
// tester, 2026-11-06: "lock picking is very finicky when you increase the
// lock picking UI"; Nadi: the beam chooses, the sticks step and push "wie
// erwartet").
//
// The minigame is the mouse's (measured 2026-11-06, docs/controls-spec.md,
// tools/hand-scripts/lockpick-trace.txt). The pick stands under the tumbler
// nearest to a place that moves three tenths as far as the cursor does from
// the screen's middle - a band of 170 menu units of cursor for each tumbler,
// which is drawn 51 wide - so a beam on the fifth tumbler left the pick under
// the third or fourth. And the push is an upward move of the cursor, not a
// place: each rise threw the pick up, so a beam drifting upward pushed.
//
// So while the LockPickMenu is on top the game is not given the beam's pixel
// but a cursor made here:
// - across, the middle of the band of the tumbler chosen. The beam chooses
//   the tumbler it is on (the nearest when it is beside them) each time it
//   comes onto another one; a stick flicked left or right - either hand's -
//   steps one tumbler from there, as the gamepad's does;
// - in height, a rest place a little over the buttons' row that the beam's
//   drift does not move, and for a few frames after a stick is pushed up a
//   push's step above it: the upward move the game takes for the push.
//   Coming back down does nothing in the game (measured). The cursor goes
//   down to its rest at once and up to it a pixel a frame - a rise that
//   slow pushed nothing in the run, and the cursor a click on Close or on
//   the tutorial's Continue left low threw the pick up when it was taken to
//   the middle at once (run 144333: the pick stuck under the pushed tumbler
//   for two seconds).
// The trigger stays the click that secures the tumbler - with no beam on
// the menu either hand's, so the sticks and a trigger are enough. On the
// buttons' row (Auto Attempt, Close) the beam's own pixel goes to the game,
// so they are clicked as on any menu.
//
// The places come from the menu's own tiles each frame (the background, the
// five tumbler covers, the exit button), so another layer size or a moved
// menu follows; the band's width is the game's own number for its own
// lockpick_menu.xml and a UI mod's may differ. Pure; lockpick_logic_test.

#include "core/Types.h"

namespace obvr::vr {

// The cursor's band for one tumbler, in menu units: the 51 units between two
// tumblers over the three tenths the pick's place moves for the cursor's.
inline constexpr float kPickBandUnits = 170.0f;
// The upward step that is the push, in menu units (18 pushed in the run) and
// for how many frames it is held (the engine's cursor is walked there by
// mouse steps, half the way a frame).
inline constexpr float kPickPushUnits = 24.0f;
inline constexpr UInt32 kPickPushFrames = 6;
// A stick's flick: past this it steps or pushes, and it has to come back
// under the rest mark before the next.
inline constexpr float kLockStickFlick = 0.6f;
inline constexpr float kLockStickRest = 0.3f;
// How far over the buttons their row begins, in menu units, and how far
// over the row the cursor rests.
inline constexpr float kLockButtonsMarginUnits = 30.0f;
inline constexpr float kLockRestMarginUnits = 20.0f;
// The most the cursor comes up to its rest in a frame, in layer pixels.
inline constexpr float kLockCreepPixels = 1.0f;

// The minigame's tiles as read from the tile tree, in menu units: the
// background on the screen, the tumbler covers and the exit button under it.
struct LockTiles {
	bool background = false;
	float backgroundX = 0.0f;
	float backgroundY = 0.0f;
	float backgroundWidth = 0.0f;
	float backgroundHeight = 0.0f;
	bool cover[5] = {false, false, false, false, false};
	float coverX[5] = {};
	float coverWidth[5] = {};
	bool buttons = false;
	float buttonsY = 0.0f;
};

// The same in the layer's pixels, the cursor's own.
struct LockGeometry {
	bool valid = false;
	float unit = 0.0f;     // layer pixels a menu unit
	float middleX = 0.0f;  // the screen's middle
	float tumblerLeft[5] = {};
	float tumblerRight[5] = {};
	float buttonsTop = 0.0f;  // from here down the beam is on the buttons' row
	float restY = 0.0f;       // the cursor's rest height
};

inline bool LockNumber(float v) { return v == v && v > -1.0e6f && v < 1.0e6f; }

// The places from the tiles: known only with the background, all five
// tumblers and a layer width. The menu is centred on the screen, so its
// screen is the background's width and twice its x wide.
inline LockGeometry LockGeometryFrom(const LockTiles& t, float layerPixelsWidth) {
	LockGeometry g;
	if (!t.background || !LockNumber(t.backgroundX) || !LockNumber(t.backgroundY) || !(t.backgroundWidth > 0.0f) ||
	    !LockNumber(t.backgroundWidth) || !(t.backgroundHeight > 0.0f) || !LockNumber(t.backgroundHeight) ||
	    !(layerPixelsWidth > 0.0f) || !LockNumber(layerPixelsWidth)) {
		return g;
	}
	const float screenUnits = 2.0f * t.backgroundX + t.backgroundWidth;
	if (!(screenUnits > 0.0f)) {
		return g;
	}
	g.unit = layerPixelsWidth / screenUnits;
	g.middleX = 0.5f * layerPixelsWidth;
	for (UInt32 i = 0; i < 5; ++i) {
		if (!t.cover[i] || !LockNumber(t.coverX[i]) || !(t.coverWidth[i] > 0.0f) || !LockNumber(t.coverWidth[i])) {
			return g;
		}
		g.tumblerLeft[i] = (t.backgroundX + t.coverX[i]) * g.unit;
		g.tumblerRight[i] = g.tumblerLeft[i] + t.coverWidth[i] * g.unit;
	}
	const float buttonsY = t.buttons && LockNumber(t.buttonsY) ? t.buttonsY : 0.9f * t.backgroundHeight;
	g.buttonsTop = (t.backgroundY + buttonsY - kLockButtonsMarginUnits) * g.unit;
	g.restY = g.buttonsTop - kLockRestMarginUnits * g.unit;
	g.valid = true;
	return g;
}

// The tumbler the beam is on, 1 to 5: the one whose cover it is in, the
// nearest when it is beside them or between two. 0 for no number or places
// not known.
inline UInt32 TumblerUnderBeam(const LockGeometry& g, float beamX) {
	if (!g.valid || !LockNumber(beamX)) {
		return 0;
	}
	UInt32 best = 0;
	float least = 0.0f;
	for (UInt32 i = 0; i < 5; ++i) {
		const float off = beamX < g.tumblerLeft[i] ? g.tumblerLeft[i] - beamX
		                                            : (beamX > g.tumblerRight[i] ? beamX - g.tumblerRight[i] : 0.0f);
		if (best == 0 || off < least) {
			best = i + 1;
			least = off;
		}
	}
	return best;
}

// One axis of the two sticks: the one further from rest; a stick that reads
// as no number is at rest.
inline float LockStick(float left, float right) {
	const float l = LockNumber(left) ? left : 0.0f;
	const float r = LockNumber(right) ? right : 0.0f;
	return (l < 0.0f ? -l : l) > (r < 0.0f ? -r : r) ? l : r;
}

// The trigger with no beam on the menu (the sticks alone at work): either
// hand's pulled past this is the click that secures. With a beam on the menu
// the hand mode's own click stands.
inline constexpr float kLockTriggerPull = 0.7f;

inline bool LockTriggerClicks(bool beamOnMenu, float rightTrigger, float leftTrigger) {
	return !beamOnMenu && (rightTrigger > kLockTriggerPull || leftTrigger > kLockTriggerPull);
}

struct LockpickFrame {
	bool lockOnTop = false;  // the LockPickMenu is the menu on top
	LockGeometry geometry;
	bool beamValid = false;  // a laser is on the menu this frame
	float beamX = 0.0f;      // its pixel
	float beamY = 0.0f;
	float stickX = 0.0f;  // LockStick of the two hands' sticks, right positive
	float stickY = 0.0f;  // up positive
	UInt32 pickTumbler = 0;  // the tumbler the pick stands under now, 0 unread
	bool cursorValid = false;  // the engine's own cursor is known
	float cursorY = 0.0f;      // its height
};

struct LockpickState {
	UInt32 selected = 0;     // the tumbler chosen, 0 none yet
	UInt32 beamTumbler = 0;  // the tumbler the beam was on last, 0 off the tumblers
	bool flickLatched = false;  // a stick is out past the flick and has not come back to rest
	UInt32 pushFrames = 0;      // frames the push's step is still held
	bool heightKnown = false;   // the cursor's height below is one given before
	float height = 0.0f;        // the cursor's height without a push, on its way to the rest
};

struct LockpickCursor {
	bool active = false;  // give the game x and y instead of the beam's pixel
	float x = 0.0f;
	float y = 0.0f;
	bool beamChose = false;  // the beam came onto another tumbler this frame
	bool stepped = false;    // a stick's flick stepped a tumbler
	bool pushed = false;     // a stick's push began
	// The cursor is still on the buttons' row on its way up: a click now
	// would land on a button.
	bool settling = false;
};

inline LockpickCursor StepLockpick(LockpickState& s, const LockpickFrame& f) {
	LockpickCursor c;
	if (!f.lockOnTop || !f.geometry.valid) {
		s = LockpickState{};
		return c;
	}
	const LockGeometry& g = f.geometry;
	if (s.selected == 0) {
		s.selected = f.pickTumbler >= 1 && f.pickTumbler <= 5 ? f.pickTumbler : 3;
	}
	// The beam: on the buttons' row it is the game's cursor as on any menu;
	// over it, it chooses the tumbler it comes onto.
	const bool beamOnButtons = f.beamValid && LockNumber(f.beamY) && f.beamY >= g.buttonsTop;
	const UInt32 under = f.beamValid && !beamOnButtons ? TumblerUnderBeam(g, f.beamX) : 0;
	if (under != 0 && under != s.beamTumbler) {
		s.selected = under;
		c.beamChose = true;
	}
	s.beamTumbler = under;
	// The sticks: the axis pushed further decides between a step and a push;
	// one flick, one of either, until the stick is back at rest.
	const float x = LockNumber(f.stickX) ? f.stickX : 0.0f;
	const float y = LockNumber(f.stickY) ? f.stickY : 0.0f;
	const float sideways = x < 0.0f ? -x : x;
	const float upward = y < 0.0f ? -y : y;
	if (s.flickLatched) {
		s.flickLatched = sideways >= kLockStickRest || upward >= kLockStickRest;
	} else if (sideways > kLockStickFlick && sideways >= upward) {
		s.flickLatched = true;
		const UInt32 next = x > 0.0f ? (s.selected < 5 ? s.selected + 1 : 5) : (s.selected > 1 ? s.selected - 1 : 1);
		c.stepped = next != s.selected;
		s.selected = next;
	} else if (upward > kLockStickFlick) {
		s.flickLatched = true;
		if (y > 0.0f) {
			s.pushFrames = kPickPushFrames;
			c.pushed = true;
		}
	}
	const bool raised = s.pushFrames > 0;
	if (raised) {
		--s.pushFrames;
	}
	if (beamOnButtons) {
		// The beam's own pixel; the cursor goes up from there afterwards.
		s.heightKnown = true;
		s.height = f.beamY;
		return c;
	}
	// The height: from where the cursor is, down to the rest at once, up to
	// it a pixel a frame.
	if (!s.heightKnown) {
		s.heightKnown = true;
		s.height = f.cursorValid && LockNumber(f.cursorY) ? f.cursorY : g.restY;
	}
	s.height = s.height > g.restY + kLockCreepPixels ? s.height - kLockCreepPixels : g.restY;
	c.active = true;
	c.settling = s.height >= g.buttonsTop;
	c.x = g.middleX + (static_cast<float>(s.selected) - 3.0f) * kPickBandUnits * g.unit;
	c.y = s.height - (raised ? kPickPushUnits * g.unit : 0.0f);
	return c;
}

}  // namespace obvr::vr

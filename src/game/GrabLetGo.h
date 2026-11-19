#pragma once

#include "core/Types.h"

namespace obvr::game {

// A grab that outlives its key: the hand opened, the grab key went up, and
// the engine's grab still holds what the hand took. The Quest 2 tester's
// dagger (2026-11-19, OBVR-grab-bug-0505.log): "grab - grip open, the
// object let go" logged, but no "let go of" after it - the release
// (game::StepGrabPhysics) waits for the engine's grabbed body to change, and
// it never did. The dagger hung upright in the air a metre before the hand,
// the engine's spring following the aim, and a new grip found nothing to
// take. With Put it in its Place - Enhanced Grabbing loaded, whose grab is a
// toggle (tap to hold, tap to let go; its Nexus page), a key held by a grip
// is not the grab's end. What exactly kept it is not read (the mod's grab
// script, zzPiiiPGrabQS, is not read for this); the remedy holds either way.
//
// The remedy: a short tap of the grab key after a grace. A tap lets go of
// both: the vanilla grab ends on the key's up after the down, a toggled one
// on the tap itself. Only for a grab OBVR's key held (the reference the
// engine held while the key was down), never while the hand closes again,
// never with a living one under the crosshair (game::GrabMayTakeTarget: a
// tap there is the mod's slap), and at most kLetGoTaps times a release
// before it is given up and said.
inline constexpr float kLetGoGraceSeconds = 0.25f;
inline constexpr UInt32 kLetGoTapFrames = 3;
inline constexpr UInt32 kLetGoTaps = 2;

enum class LetGoPhase : UInt8 { Idle, Waiting, Tapping };

struct LetGoState {
	LetGoPhase phase = LetGoPhase::Idle;
	UInt32 ref = 0;         // what the engine held when the key went up
	float wait = 0.0f;      // the grace left before the next tap
	UInt32 tapFrames = 0;   // frames of the current tap still to press
	UInt32 taps = 0;        // taps sent for this release
	bool keyWas = false;    // the hand's grab key the frame before
	UInt32 heldWhileDown = 0;  // the engine's grabbed reference while the key was down
};

struct LetGoInput {
	bool keyDown = false;        // the grab key as the hand wants it (the grip, the slap's tap)
	UInt32 engineHolds = 0;      // what the engine's grab holds now, 0 nothing
	bool livingUnderCrosshair = false;
	float dt = 0.0f;
};

struct LetGoVerdict {
	bool press = false;      // the grab key down this frame for the tap
	bool tapStarted = false; // a tap began this frame (for the log)
	bool released = false;   // the engine let go while watched
	bool gaveUp = false;     // the taps did not end it
};

inline LetGoVerdict StepLetGo(LetGoState& s, const LetGoInput& in) {
	LetGoVerdict v;
	const bool keyUpEdge = s.keyWas && !in.keyDown;
	if (in.keyDown) {
		s.heldWhileDown = in.engineHolds != 0 ? in.engineHolds : s.heldWhileDown;
	}
	s.keyWas = in.keyDown;
	if (in.keyDown) {
		// The hand closed again: the grab is the hand's once more.
		s.phase = LetGoPhase::Idle;
		s.taps = 0;
		return v;
	}
	if (keyUpEdge) {
		const UInt32 held = s.heldWhileDown;
		s.heldWhileDown = 0;
		if (held != 0 && in.engineHolds == held) {
			s.phase = LetGoPhase::Waiting;
			s.ref = held;
			s.wait = kLetGoGraceSeconds;
			s.taps = 0;
		}
		return v;
	}
	if (s.phase == LetGoPhase::Idle) {
		return v;
	}
	if (in.engineHolds != s.ref) {
		// Let go (or something else taken by another way): not ours to end.
		v.released = in.engineHolds == 0;
		s.phase = LetGoPhase::Idle;
		return v;
	}
	if (s.phase == LetGoPhase::Tapping) {
		if (s.tapFrames > 0) {
			--s.tapFrames;
			v.press = true;
			return v;
		}
		s.phase = LetGoPhase::Waiting;
		s.wait = kLetGoGraceSeconds;
		return v;
	}
	s.wait -= in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (s.wait > 0.0f || in.livingUnderCrosshair) {
		return v;
	}
	if (s.taps >= kLetGoTaps) {
		v.gaveUp = true;
		s.phase = LetGoPhase::Idle;
		return v;
	}
	++s.taps;
	s.phase = LetGoPhase::Tapping;
	s.tapFrames = kLetGoTapFrames - 1;
	v.press = true;
	v.tapStarted = true;
	return v;
}

}  // namespace obvr::game

#pragma once

#include "core/Types.h"

namespace obvr::vr {

// ------------------------------------------------- The A and B held together
//
// The other hand's A and B held together toward the quick save (Nadi,
// 2026-11-13: "a + b halten mit kreis"; on an Oculus Touch X and Y): a ring
// over the controller from RingSeconds on, filling to full at HoldSeconds,
// then the quick save - the pause menu's ring (vr::StepButtonHold) for two
// buttons. Let go of either before it is full and the chord is called off.
//
// When the two count as one chord: B pressed while A is held (however long A
// was down), or A pressed while B is held and B went down no more than
// WindowSeconds before - a B held longer is on its way to its own tap or
// hold (the Tab menus, the pause menu) and an A then is only an A. From the
// chord on the two buttons are the chord's until both are let go: B's own tap
// and hold are nothing (the caller blocks them), and a chord reached or called
// off starts again only from a fresh press of both.
//
// Pure, covered by chord_hold_test.

struct ChordHoldState {
	bool aDown = false;
	bool bDown = false;
	float bHeldSeconds = 0.0f;  // since B went down, while it is held
	bool active = false;        // the two are the chord's until both are up
	float seconds = 0.0f;       // since the chord began
	bool done = false;          // reached or called off: nothing until both are up
};

struct ChordHoldVerdict {
	bool active = false;     // the chord owns both buttons this frame
	bool began = false;      // the frame it began
	bool ringShown = false;  // the ring is up
	float fill = 0.0f;       // 0..1 of the ring, while it is up
	bool reached = false;    // the frame it was held long enough
	bool cancelled = false;  // let go before full, the ring up
};

inline ChordHoldVerdict StepChordHold(ChordHoldState& s, bool aDown, bool bDown, float holdSeconds,
                                      float ringSeconds, float windowSeconds, float dtSeconds) {
	ChordHoldVerdict v;
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	const bool aPressed = aDown && !s.aDown;
	const bool bPressed = bDown && !s.bDown;
	const float bBefore = s.bHeldSeconds;
	s.bHeldSeconds = bDown ? (bPressed ? 0.0f : s.bHeldSeconds + dt) : 0.0f;

	if (!s.active && aDown && bDown && (aPressed || bPressed)) {
		// B onto a held A, or both in the same frame; A onto a B that went
		// down within the window.
		const bool bOntoA = bPressed && (s.aDown || aPressed);
		const bool aOntoFreshB = aPressed && !bPressed && bBefore + dt <= windowSeconds;
		if (bOntoA || aOntoFreshB) {
			s.active = true;
			s.done = false;
			s.seconds = 0.0f;
			v.began = true;
		}
	}

	if (s.active) {
		v.active = true;
		if (!aDown && !bDown) {
			// Both let go in one frame: called off like one let go, if it was
			// not over already.
			v.cancelled = !s.done && s.seconds >= ringSeconds;
			s.active = false;
			s.done = false;
			s.seconds = 0.0f;
		} else if (!s.done) {
			if (!(aDown && bDown)) {
				// One let go before full: called off; the other stays the chord's
				// until it is let go too.
				v.cancelled = s.seconds >= ringSeconds;
				s.done = true;
			} else {
				s.seconds += v.began ? 0.0f : dt;
				if (s.seconds >= holdSeconds) {
					v.reached = true;
					s.done = true;
				} else if (s.seconds >= ringSeconds) {
					v.ringShown = true;
					v.fill = holdSeconds > ringSeconds ? (s.seconds - ringSeconds) / (holdSeconds - ringSeconds) : 1.0f;
				}
			}
		}
	}

	s.aDown = aDown;
	s.bDown = bDown;
	return v;
}

}  // namespace obvr::vr

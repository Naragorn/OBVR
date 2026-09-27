#pragma once

// Stowing a held object by bringing it to the body (docs/controls-spec.md
// 4.6, the tester, 2026-09-27): an item held in the hand, brought to the
// chest or the belly and let go there, goes into the inventory. Full VR only;
// [Hands] StowAtBody, on by default.
//
// It is taken the way activating it takes it, so the game decides everything
// activating decides: the item's own OnActivate script, and the crime - an
// owned item is stolen, and seen, it is a crime (game::TakeIntoInventory).
// Only item types are taken; a body or anything else held is let go as
// usual. The take waits until the engine has let go of the object itself,
// so its grab spring never holds a reference that has gone into the pack.
//
// [Hands] TakeOnlyByHand (off by default): the activate button no longer
// takes loose items; activating a book still opens it to read. It needs
// stowing on - with that off, activating takes items as ever, or they could
// not be taken at all.
//
// The zone is in the body's frame (vr::BodyRelative): metres from the eyes
// along the head's heading, x right, y forward, z up. An upright cylinder
// round the torso, below the chin - the mouth is left for eating.
//
// Pure, covered by stow_test.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct StowSettings {
	bool enabled = true;
	bool takeOnlyByHand = false;
	// The cylinder's axis, from the eyes: a little behind them, where the
	// chest's middle is.
	float centreRight = 0.0f;
	float centreForward = -0.05f;
	float radius = 0.28f;
	float top = -0.17f;     // below the chin
	float bottom = -0.80f;  // down to the hips
	// How long the engine may take to let go before the take is given up.
	float waitSeconds = 1.0f;
};

inline bool InStowZone(const NiPoint3& relative, const StowSettings& s) {
	const float dx = relative.x - s.centreRight;
	const float dy = relative.y - s.centreForward;
	return dx * dx + dy * dy <= s.radius * s.radius && relative.z <= s.top &&
	       relative.z >= s.bottom;
}

struct StowInput {
	bool allowed = false;     // Full VR, in the world, no menu
	bool keyDown = false;     // the grab key OBVR holds down, as it is now
	UInt32 heldRef = 0;       // what the engine holds (player+0x578), 0 for nothing
	bool heldIsItem = false;  // heldRef's base form is an item that goes into a pack
	bool handValid = false;   // the hand that holds it
	NiPoint3 handRelative{0.0f, 0.0f, 0.0f};  // BodyRelative
	float dt = 0.0f;
};

struct StowState {
	bool keyWas = false;
	UInt32 ref = 0;         // held while the key was down
	bool refIsItem = false;
	bool atBody = false;    // the hand at the body on the last frame it held
	UInt32 pending = 0;     // let go at the body: waiting for the engine to let go
	float wait = 0.0f;
};

struct StowVerdict {
	bool atBody = false;    // holding something at the body now: letting go stows it
	UInt32 take = 0;        // take this reference into the inventory now
	bool waiting = false;   // a take waits for the engine: no throw
	bool notItem = false;   // let go at the body, but it is not an item: dropped
	bool gaveUp = false;    // the engine kept holding it: nothing taken
};

inline StowVerdict StepStow(StowState& s, const StowInput& in, const StowSettings& settings) {
	StowVerdict v;
	const bool release = s.keyWas && !in.keyDown;
	s.keyWas = in.keyDown;
	if (!settings.enabled || !in.allowed) {
		s.ref = 0;
		s.atBody = false;
		s.pending = 0;
		return v;
	}
	if (in.keyDown && in.heldRef != 0) {
		s.ref = in.heldRef;
		s.refIsItem = in.heldIsItem;
		s.atBody = in.handValid && InStowZone(in.handRelative, settings);
		v.atBody = s.atBody && s.refIsItem;
	}
	if (release) {
		if (s.ref != 0 && s.atBody) {
			if (s.refIsItem) {
				s.pending = s.ref;
				s.wait = settings.waitSeconds;
			} else {
				v.notItem = true;
			}
		}
		s.ref = 0;
		s.atBody = false;
	}
	if (s.pending != 0) {
		if (in.heldRef != s.pending) {
			v.take = s.pending;
			s.pending = 0;
		} else {
			s.wait -= in.dt;
			if (s.wait <= 0.0f) {
				v.gaveUp = true;
				s.pending = 0;
			} else {
				v.waiting = true;
			}
		}
	}
	return v;
}

// Whether the activate button is kept from the game this frame: taking
// loose items only by hand, and it points at one. A book is let through - it
// opens to read.
inline bool ActivateWithheld(const StowSettings& settings, bool haveRef, bool isItem, bool isBook) {
	return settings.enabled && settings.takeOnlyByHand && haveRef && isItem && !isBook;
}

}  // namespace obvr::vr

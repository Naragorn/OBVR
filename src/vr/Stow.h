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
// [Hands] TakeOnlyByHand (on by default since 2026-10-07, the tester: "aufheben
// nur mit an körper ran führen auch default an"): the activate button no longer
// takes loose items; activating a book still opens it to read. It needs
// stowing on - with that off, activating takes items as ever, or they could
// not be taken at all.
//
// The zone is in the body's frame (vr::BodyRelative): metres from the eyes
// along the head's heading, x right, y forward, z up.
//
// Since the second headset test it is a spot the player sees: a gold ring at
// the chest, shown while an item is held, lit while the hand is in it (the
// tester, 2026-09-27: the wide zone left no way to drop an item in front of
// oneself - "einen oblivion farbenen kreis an der brust rendern und wenn man
// in diese legt gehts ins inventar"). A sphere round that point; anywhere
// else, letting go drops the item as the game does.
//
// Placed from the first headset test (2026-09-27, OBVR.log.prev): a hand
// brought "to the chest" read 0.19 to 0.23 m ahead of the eyes and 0.17 to
// 0.30 m below them - the controller is held in front of the body, not on
// it - so the first zone, round a point 5 cm behind the eyes with 28 cm
// reach, missed a sword let go 0.23 m ahead. And the hand moves as it
// opens: releases came a frame after the hand had left the zone, so a
// release still stows for graceSeconds after the hand was last in it.
//
// Pure, covered by stow_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

struct StowSettings {
	bool enabled = true;
	bool takeOnlyByHand = true;
	// The spot, from the eyes: where a hand held against the chest was
	// measured (0.19 to 0.23 m ahead, 0.17 to 0.30 m below), and how far
	// round it the hand counts as in it.
	float centreRight = 0.0f;
	float centreForward = 0.17f;
	float centreUp = -0.30f;
	float radius = 0.16f;
	// The ring drawn while an item is held. Off by default since the tester
	// placed it where wanted once (vr/StowPlace.h, 2026-09-28): the spot
	// stows whether it is seen or not.
	bool spotVisible = false;
	// A release this soon after the hand left the zone still stows.
	float graceSeconds = 0.3f;
	// How long the engine may take to let go before the take is given up.
	float waitSeconds = 1.0f;
};

inline bool InStowZone(const NiPoint3& relative, const StowSettings& s) {
	const float dx = relative.x - s.centreRight;
	const float dy = relative.y - s.centreForward;
	const float dz = relative.z - s.centreUp;
	return dx * dx + dy * dy + dz * dz <= s.radius * s.radius;
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
	float sinceAtBody = 1e6f;  // seconds since the hand holding it was last there
	UInt32 pending = 0;     // let go at the body: waiting for the engine to let go
	float wait = 0.0f;
};

struct StowVerdict {
	bool atBody = false;    // holding something at the body now: letting go stows it
	bool showSpot = false;  // an item is held: the spot is shown (lit when atBody)
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
		s.sinceAtBody = 1e6f;
		s.pending = 0;
		return v;
	}
	if (in.keyDown && in.heldRef != 0) {
		if (in.heldRef != s.ref) {
			s.sinceAtBody = 1e6f;  // another object: its own time at the body
		}
		s.ref = in.heldRef;
		s.refIsItem = in.heldIsItem;
		s.atBody = in.handValid && InStowZone(in.handRelative, settings);
		s.sinceAtBody = s.atBody ? 0.0f : s.sinceAtBody + in.dt;
		v.atBody = s.atBody && s.refIsItem;
		v.showSpot = s.refIsItem;
	}
	if (release) {
		const bool recently = s.atBody || s.sinceAtBody <= settings.graceSeconds;
		s.sinceAtBody = 1e6f;
		if (s.ref != 0 && recently) {
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

// The spot in tracking space (OpenVR's axes: x right, y up, z back), for
// drawing it: the inverse of vr::BodyRelative - the eyes, plus the spot's
// offsets along the head's level heading, its right, and up.
inline NiPoint3 StowSpotInTracking(const Quaternion& head, const NiPoint3& headPosition,
                                   const StowSettings& s) {
	const NiPoint3 forward = ToMatrix(head) * NiPoint3{0.0f, 0.0f, -1.0f};
	float fx = forward.x;
	float fz = forward.z;
	const float length = math::Sqrt(fx * fx + fz * fz);
	if (length < 1e-4f) {
		fx = 0.0f;
		fz = -1.0f;
	} else {
		fx /= length;
		fz /= length;
	}
	// BodyRelative's right is (-fz, fx) on the level, its ahead (fx, fz).
	return NiPoint3{headPosition.x + s.centreRight * -fz + s.centreForward * fx,
	                headPosition.y + s.centreUp,
	                headPosition.z + s.centreRight * fx + s.centreForward * fz};
}

// Whether the activate button is kept from the game this frame: taking
// loose items only by hand, and it points at one. A book is let through - it
// opens to read.
inline bool ActivateWithheld(const StowSettings& settings, bool haveRef, bool isItem, bool isBook) {
	return settings.enabled && settings.takeOnlyByHand && haveRef && isItem && !isBook;
}

}  // namespace obvr::vr

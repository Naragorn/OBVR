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
// Eating at the mouth (Nadi, Windows, 2026-10-24: "Essen und Tränke an den
// Mund ist Essen" - instead of taking the item, opening the inventory and
// using it there): an ingredient or a potion - food is either in Oblivion -
// let go at the mouth is taken the same way and then used as the inventory
// uses it, eaten or drunk (game::EatItem). The release decides, as at the
// chest - HIGGS, Skyrim VR's hands, eats "by dropping them at the mouth"
// too, so nothing is eaten by passing the face. The mouth is a point below
// and ahead of the eyes in the head's own frame, so it goes with the head's
// pitch; where its sphere meets the chest's, the mouth wins for something
// edible. A poison is not edible: from the inventory the game puts it on the
// weapon. Needs StowAtBody, as it is the stow's release.
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
	float centreUp = -0.33f;  // the chest (the tester, 2026-10-07: "by default in der brust"),
	                          // still over where items were held in 2026-09-27's log
	float radius = 0.16f;
	// The ring drawn while an item is held. Off by default from 2026-09-28,
	// when the tester had placed it where wanted (vr/StowPlace.h) and knew
	// the spot; on again since 2026-10-18 for the beta - two testers of
	// 2026-10-17 let items go at the chest without knowing where the spot
	// was ("cant open doors not sure how to pick up items", "letting go at
	// the chest drops it"). The spot stows whether it is seen or not.
	bool spotVisible = true;
	// A release this soon after the hand left the zone still stows.
	float graceSeconds = 0.3f;
	// How long the engine may take to let go before the take is given up.
	float waitSeconds = 1.0f;
	// [Hands] EatAtMouth and the mouth, metres from the eyes in the head's
	// frame: below them and ahead, and how far round it the drawn hand counts
	// as at it. Guessed from a face's build (the mouth some 9 cm under the
	// eyes, the lips ahead of them), wide enough for the hand that holds a
	// bottle to the lips; the headset's to settle.
	bool eatAtMouth = true;
	float mouthDown = 0.09f;     // [Hands] MouthDownMetres
	float mouthForward = 0.05f;  // [Hands] MouthForwardMetres
	float mouthRadius = 0.13f;   // [Hands] MouthReachMetres
};

inline bool InStowZone(const NiPoint3& relative, const StowSettings& s) {
	const float dx = relative.x - s.centreRight;
	const float dy = relative.y - s.centreForward;
	const float dz = relative.z - s.centreUp;
	return dx * dx + dy * dy + dz * dz <= s.radius * s.radius;
}

// The mouth in tracking space (OpenVR's axes: x right, y up, z back): the
// eyes plus the head's own rotation of (0, -down, -forward).
inline NiPoint3 MouthInTracking(const Quaternion& head, const NiPoint3& headPosition, const StowSettings& s) {
	return headPosition + Rotate(head, NiPoint3{0.0f, -s.mouthDown, -s.mouthForward});
}

inline bool AtMouth(const NiPoint3& hand, const NiPoint3& mouth, const StowSettings& s) {
	if (!s.eatAtMouth) {
		return false;
	}
	const NiPoint3 d = hand - mouth;
	return d.x * d.x + d.y * d.y + d.z * d.z <= s.mouthRadius * s.mouthRadius;
}

// Where the hand is drawn, in tracking space, from its controller: the
// controller's tracked origin plus the grip the hand sits at - the shared
// HandGripForwardMetres and HandGripUpMetres with the hand's own
// HandGripX/Y/Z, metres along the controller's right, forward and up, the
// axes BonePin.h moves the hand bone by. The stow zone is measured here
// since 2026-10-18, not at the controller: a tester (Quest 3, 2026-10-17)
// had adjusted his hands to HandGripForwardMetres -0.23, so the drawn hand
// touched his chest while the controller stood 0.40 m out - outside the
// 0.16 m round the spot - and the item he let go there dropped. OpenVR's
// controller axes are x right, y up, z back, so the grip's forward is -z
// and its up is +y (vr::FromOpenXR's change of basis, taken back).
inline NiPoint3 DrawnHandInTracking(const Quaternion& orientation, const NiPoint3& position,
                                    const NiPoint3& gripMetres) {
	return position + Rotate(orientation, NiPoint3{gripMetres.x, gripMetres.z, -gripMetres.y});
}

struct StowInput {
	bool allowed = false;     // Full VR, in the world, no menu
	bool keyDown = false;     // the grab key OBVR holds down, as it is now
	UInt32 heldRef = 0;       // what the engine holds (player+0x578), 0 for nothing
	bool heldIsItem = false;  // heldRef's base form is an item that goes into a pack
	bool heldIsEdible = false;  // and one to eat or drink (game::RefIsEdible)
	bool handAtMouth = false;   // the drawn hand at the mouth (AtMouth)
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
	bool refIsEdible = false;
	bool atMouth = false;      // an edible thing at the mouth on the last frame it held
	float sinceAtMouth = 1e6f;
	UInt32 pending = 0;     // let go at the body: waiting for the engine to let go
	bool pendingEat = false;   // and to be eaten once taken
	float wait = 0.0f;
};

struct StowVerdict {
	bool atBody = false;    // holding something at the body now: letting go stows it
	bool atMouth = false;   // holding something edible at the mouth: letting go eats it
	bool eat = false;       // take: the reference is to be eaten once taken
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
		s.atMouth = false;
		s.sinceAtMouth = 1e6f;
		s.pending = 0;
		s.pendingEat = false;
		return v;
	}
	if (in.keyDown && in.heldRef != 0) {
		if (in.heldRef != s.ref) {
			s.sinceAtBody = 1e6f;  // another object: its own time at the body
			s.sinceAtMouth = 1e6f;
		}
		s.ref = in.heldRef;
		s.refIsItem = in.heldIsItem;
		s.refIsEdible = in.heldIsItem && in.heldIsEdible;
		s.atBody = in.handValid && InStowZone(in.handRelative, settings);
		s.sinceAtBody = s.atBody ? 0.0f : s.sinceAtBody + in.dt;
		s.atMouth = s.refIsEdible && settings.eatAtMouth && in.handValid && in.handAtMouth;
		s.sinceAtMouth = s.atMouth ? 0.0f : s.sinceAtMouth + in.dt;
		v.atMouth = s.atMouth;
		v.atBody = s.atBody && s.refIsItem && !s.atMouth;
		v.showSpot = s.refIsItem;
	}
	if (release) {
		const bool recently = s.atBody || s.sinceAtBody <= settings.graceSeconds;
		const bool mouth = s.refIsEdible && (s.atMouth || s.sinceAtMouth <= settings.graceSeconds);
		s.sinceAtBody = 1e6f;
		s.sinceAtMouth = 1e6f;
		if (s.ref != 0 && (recently || mouth)) {
			if (s.refIsItem) {
				s.pending = s.ref;
				s.pendingEat = mouth;
				s.wait = settings.waitSeconds;
			} else {
				v.notItem = true;
			}
		}
		s.ref = 0;
		s.atBody = false;
		s.atMouth = false;
	}
	if (s.pending != 0) {
		if (in.heldRef != s.pending) {
			v.take = s.pending;
			v.eat = s.pendingEat;
			s.pending = 0;
			s.pendingEat = false;
		} else {
			s.wait -= in.dt;
			if (s.wait <= 0.0f) {
				v.gaveUp = true;
				s.pending = 0;
				s.pendingEat = false;
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

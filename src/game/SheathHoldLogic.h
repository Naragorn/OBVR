#pragma once

// The body's sheath nodes kept still through the draw and the sheathe (Nadi,
// Windows 2026-11-26: "beim wegstecken vom schwert flackert es mehrere frames
// her und dahin statt direkt dort holstered zu sein").
//
// [M] Harness h2-sheathe-katana (20261126-164256-slot4, the sheath trace's
// "hip local turns"): the Unequip group, run ten times faster
// ([Hands] WeaponDrawSpeed), turns the hip's sheath node "SideWeapon" in its
// parent's frame by 11.5, 38.9, 16.8, 22.4 and 19.8 degrees in the five frames
// after it begins and then not at all; the sheathed weapon, hung under the
// node with no turn of its own (the engine's hang 0x00656270 sets its local to
// the identity), turns with it. Filmed at both hips, mirrored or not: the
// katana's hilt swings up and back for about ten frames.
// [R] The engine hangs the weapon on the sheath at the group's Detach key
// (onehandunequip.kf 0.467 s of 0.867 s), after that turn: what the turn moves
// in the game is the empty scabbard. OBVR hangs it as the Unequip begins
// (game/WeaponRelease.h StepSheathHang, so it leaves the hand at once), and
// with that it rides the turn.
//
// The rule: while the player's action is the draw or the sheathe, each sheath
// node keeps the local it had in the last frame before (no game animation moves
// the body's sheaths, as none moves its trunk - game/BodyIk.cpp StepTrunk);
// when the action ends it goes back to the animation's, eased over
// kSheathEaseSeconds when the two differ. A node seen first in the middle of
// an action (a new 3D) is held from its first local.
//
// Pure, covered by sheath_hold_test; game/BodySheaths.cpp applies it before the
// left-handed mirror.

#include "core/Types.h"
#include "game/HandGrip.h"
#include "game/NiMath.h"

namespace obvr::game {

// The player's action codes of the draw and the sheathe (vr/HandInput.h
// kPlayerActionEquipWeapon, kPlayerActionUnequipWeapon).
inline constexpr SInt32 kSheathActionEquip = 0;
inline constexpr SInt32 kSheathActionUnequip = 1;
inline constexpr float kSheathEaseSeconds = 0.15f;
// The sheath trace's verdict: a sheath that turned less than this a frame in
// its parent through a sheathe held still.
inline constexpr float kSheathStillDegrees = 1.0f;

inline bool SheathStillFor(SInt32 playerAction) {
	return playerAction == kSheathActionEquip || playerAction == kSheathActionUnequip;
}

inline bool SheathTiltStill(float mostDegreesAFrame) {
	return mostDegreesAFrame == mostDegreesAFrame && mostDegreesAFrame < kSheathStillDegrees;
}

struct SheathHoldState {
	UInt32 node = 0;         // the node the state is about; another one starts afresh
	bool have = false;       // `last` holds a local of that node
	NiTransform last{};      // the local handed out last
	bool wasStill = false;   // the draw or the sheathe ran last frame
	bool easing = false;     // back to the animation's after an action
	NiTransform easeFrom{};  // where the ease began
	float eased = 0.0f;      // seconds of it so far
};

inline NiTransform BlendTransform(const NiTransform& a, const NiTransform& b, float t) {
	NiTransform out{};
	out.rot = BlendRotation(a.rot, b.rot, t);
	out.pos = a.pos + (b.pos - a.pos) * t;
	out.scale = a.scale + (b.scale - a.scale) * t;
	return out;
}

inline bool SameLocal(const NiTransform& a, const NiTransform& b) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (a.rot.data[r][c] != b.rot.data[r][c]) {
				return false;
			}
		}
	}
	return a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z && a.scale == b.scale;
}

// The local a sheath node gets this frame: `animated` as the animation left
// it, `still` the draw or the sheathe running (SheathStillFor), `node` the
// node's identity (0: none, passed through and nothing kept).
inline NiTransform StepSheathHold(SheathHoldState& s, UInt32 node, const NiTransform& animated, bool still, float dt) {
	if (node == 0) {
		return animated;
	}
	if (s.node != node) {
		s = SheathHoldState{};
		s.node = node;
	}
	// The action just over: from the held local back to the animation's.
	if (!still && s.wasStill && s.have && !SameLocal(s.last, animated)) {
		s.easing = true;
		s.easeFrom = s.last;
		s.eased = 0.0f;
	}
	s.wasStill = still;
	NiTransform out = animated;
	if (still) {
		out = s.have ? s.last : animated;  // where an ease ran, the local it had reached
		s.easing = false;
	} else if (s.easing) {
		s.eased += dt == dt && dt > 0.0f ? dt : 0.0f;
		const float t = s.eased >= kSheathEaseSeconds ? 1.0f : s.eased / kSheathEaseSeconds;
		out = t >= 1.0f ? animated : BlendTransform(s.easeFrom, animated, t);
		s.easing = t < 1.0f;
	}
	s.last = out;
	s.have = true;
	return out;
}

}  // namespace obvr::game

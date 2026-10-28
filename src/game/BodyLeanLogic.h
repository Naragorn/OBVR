#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The lean: bending forward (or aside, or back) bends the body at the hips
// instead of carrying it along under the head (Nadi, 2026-10-28: "nach vorne
// beugen ... nun geht der ganze körper bischen vor").
//
// The placement (BodyPlacement.h) stands the body so its head bone is under
// the headset - every move of the head carries the whole body, feet and all.
// Measured with the harness (body-lean-keep1, 2026-10-28): a head 30 cm
// forward and 20 cm lower took the pelvis the same 30 cm forward. A real
// body leaning keeps its hips where it stands and bends the trunk.
//
// So the hips keep an anchor on the ground. How far the head may lean from it
// follows from how far the head went down: a trunk of length l bent until its
// top is d lower reaches sqrt(l^2 - (l - d)^2) forward. A head further off
// than that (and a little slack for sway) is a step, and drags the anchor
// along; a head upright draws it back under the head over a moment, so a
// step in the room does not leave the body bent. The trunk is then turned at
// the spine's first bone so the head lands where the headset is, the pivot
// raised by as much of the let-down as the bend explains (the knees need not
// bend for a lean), and whatever the turn cannot reach taken up by moving
// the body, as before.
//
// Plain values only; game/BodyIk.cpp reads and writes the bones.

inline constexpr float kLeanSlackUnits = 3.0f;        // sway the anchor ignores, about 4 cm
inline constexpr float kLeanUprightUnits = 2.0f;      // let down less than this: standing upright
inline constexpr float kLeanReturnPerSecond = 2.0f;   // the anchor's way back under an upright head

namespace lean_detail {
inline bool Finite(float v) { return v == v && v - v == 0.0f; }
inline bool Finite(const NiPoint3& p) { return Finite(p.x) && Finite(p.y) && Finite(p.z); }
inline float Length(const NiPoint3& p) { return math::Sqrt(p.LengthSquared()); }
inline float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline NiPoint3 Cross(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
}  // namespace lean_detail

// How far, level, a trunk `trunkUnits` long reaches from its hips when its
// top is `dropUnits` lower than standing; plus the slack. A drop of the whole
// trunk or more reaches the whole trunk; a drop that is not a number, or
// none, only the slack.
inline float LeanAllowedUnits(float dropUnits, float trunkUnits, float slackUnits) {
	using lean_detail::Finite;
	const float slack = Finite(slackUnits) && slackUnits > 0.0f ? slackUnits : 0.0f;
	if (!Finite(dropUnits) || !Finite(trunkUnits) || !(dropUnits > 0.0f) || !(trunkUnits > 0.0f)) {
		return slack;
	}
	const float d = dropUnits < trunkUnits ? dropUnits : trunkUnits;
	const float rest = trunkUnits - d;
	return math::Sqrt(trunkUnits * trunkUnits - rest * rest) + slack;
}

inline constexpr float kLeanStandingFollowSeconds = 30.0f;

struct LeanAnchor {
	bool valid = false;
	float x = 0.0f;
	float y = 0.0f;
	// The let-down while standing upright: a player shorter than the
	// character has the body let down all the time, and that is no lean. The
	// least let-down seen, followed slowly upwards (a player who settles lower
	// over minutes) - HandInput.h's crouch does the same with the head.
	bool haveStanding = false;
	float standingLowered = 0.0f;
};

// How far the head is below standing: the let-down against the standing
// let-down, which this frame keeps.
inline float StepLeanDrop(LeanAnchor& a, float loweredUnits, float dtSeconds) {
	using lean_detail::Finite;
	if (!Finite(loweredUnits)) {
		return 0.0f;
	}
	if (!a.haveStanding || !Finite(a.standingLowered) || loweredUnits < a.standingLowered) {
		a.haveStanding = true;
		a.standingLowered = loweredUnits;
		return 0.0f;
	}
	if (Finite(dtSeconds) && dtSeconds > 0.0f) {
		float share = dtSeconds / kLeanStandingFollowSeconds;
		share = share < 1.0f ? share : 1.0f;
		a.standingLowered += (loweredUnits - a.standingLowered) * share;
	}
	return loweredUnits - a.standingLowered;
}

// The anchor's step: where the hips stay, level, given where the placement
// would put them this frame (`hipsX/Y`, under the head) and how far the head
// is below standing (`dropUnits`, StepLeanDrop). Answers the lean, the level
// way from the anchor to those hips - what the trunk's turn has to make up.
// A first frame, or numbers that are not numbers, start the anchor at the
// hips: no lean.
inline NiPoint3 StepLeanAnchor(LeanAnchor& a, float hipsX, float hipsY, float dropUnits, float trunkUnits,
                               float dtSeconds) {
	using lean_detail::Finite;
	if (!Finite(hipsX) || !Finite(hipsY)) {
		a.valid = false;
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	if (!a.valid || !Finite(a.x) || !Finite(a.y)) {
		a.valid = true;
		a.x = hipsX;
		a.y = hipsY;
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	// Upright: drawn back under the head.
	if (!(dropUnits >= kLeanUprightUnits) && Finite(dtSeconds) && dtSeconds > 0.0f) {
		float share = dtSeconds * kLeanReturnPerSecond;
		share = share < 1.0f ? share : 1.0f;
		a.x += (hipsX - a.x) * share;
		a.y += (hipsY - a.y) * share;
	}
	const float allowed = LeanAllowedUnits(dropUnits, trunkUnits, kLeanSlackUnits);
	float dx = hipsX - a.x;
	float dy = hipsY - a.y;
	const float off = math::Sqrt(dx * dx + dy * dy);
	if (off > allowed) {
		// A step: the anchor dragged to the edge of the reach.
		const float keep = allowed / off;
		dx *= keep;
		dy *= keep;
		a.x = hipsX - dx;
		a.y = hipsY - dy;
	}
	return NiPoint3{dx, dy, 0.0f};
}

// How far to raise the trunk's pivot, from where the hips stand held back by
// the lean, so the pivot is a trunk's length from the head: `toHead` is the
// way from the pivot to the head as the placement has it (before the hips are
// held back), `lean` the level way the hips are held back, `trunkUnits` the
// pivot-to-head length. Never more than `mostUnits` (the let-down the body
// took), never below zero; a head out of the trunk's reach raises nothing.
inline float LeanPivotRaise(const NiPoint3& toHead, const NiPoint3& lean, float trunkUnits, float mostUnits) {
	using lean_detail::Dot;
	using lean_detail::Finite;
	if (!Finite(toHead) || !Finite(lean) || !Finite(trunkUnits) || !Finite(mostUnits) || !(mostUnits > 0.0f)) {
		return 0.0f;
	}
	// The head from the held-back pivot raised by r: w - r*up, w = toHead + lean.
	const NiPoint3 w = toHead + lean;
	const float c = w.LengthSquared() - trunkUnits * trunkUnits;
	if (!(c > 0.0f)) {
		return 0.0f;  // already within reach without raising
	}
	const float disc = w.z * w.z - c;
	if (!(disc >= 0.0f)) {
		return 0.0f;
	}
	const float r = w.z - math::Sqrt(disc);
	if (!(r > 0.0f)) {
		return 0.0f;
	}
	return r < mostUnits ? r : mostUnits;
}

// The rotation that turns direction `from` onto direction `to` the short way
// (row-major, applied as R * v). The identity for either being no direction
// or numbers that are not numbers; half a turn about any square axis for
// opposite ones.
inline NiMatrix33 RotationBetween(const NiPoint3& from, const NiPoint3& to) {
	using lean_detail::Cross;
	using lean_detail::Dot;
	using lean_detail::Finite;
	using lean_detail::Length;
	const float lf = Length(from);
	const float lt = Length(to);
	if (!Finite(from) || !Finite(to) || !(lf > 1.0e-6f) || !(lt > 1.0e-6f)) {
		return NiMatrix33::Identity();
	}
	const NiPoint3 a = from * (1.0f / lf);
	const NiPoint3 b = to * (1.0f / lt);
	const float c = Dot(a, b);
	NiPoint3 k = Cross(a, b);
	float s = Length(k);
	if (s < 1.0e-6f) {
		if (c > 0.0f) {
			return NiMatrix33::Identity();
		}
		k = Cross(a, NiPoint3{1.0f, 0.0f, 0.0f});
		if (Length(k) < 1.0e-3f) {
			k = Cross(a, NiPoint3{0.0f, 1.0f, 0.0f});
		}
		s = 0.0f;
	}
	const float kl = Length(k);
	k = k * (1.0f / kl);
	const float t = 1.0f - c;
	NiMatrix33 r{};
	r.data[0][0] = c + k.x * k.x * t;
	r.data[0][1] = k.x * k.y * t - k.z * s;
	r.data[0][2] = k.x * k.z * t + k.y * s;
	r.data[1][0] = k.y * k.x * t + k.z * s;
	r.data[1][1] = c + k.y * k.y * t;
	r.data[1][2] = k.y * k.z * t - k.x * s;
	r.data[2][0] = k.z * k.x * t - k.y * s;
	r.data[2][1] = k.z * k.y * t + k.x * s;
	r.data[2][2] = c + k.z * k.z * t;
	return r;
}

}  // namespace obvr::game

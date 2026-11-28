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

// Aside, the trunk bends less and the hips come back (Nadi, 2026-11-28: the
// body always leaned to the left or the right, never straight down).
//
// Until 2026-11-28 the reach was the trunk's for the whole drop, and aside the
// same as forward. A trunk's reach grows fastest from upright: a trunk of 38
// units (Bip01 Spine to the head) let down 2 reaches 12 forward, 3 reaches 15.
// So at the upright line the reach jumped from the slack's 3 to 15 units, and a
// head 2 or 3 units below its standing height - a player's posture, the
// camera's own rise with a look down (2.5 units, player-body-spec round 2),
// a standing height taken from one frame's spike - held the hips wherever they
// were: a weight moved from one foot to the other bent the trunk up to 20
// degrees aside, and it stayed until the head came back above the line.
//
// Now the reach is the trunk's for the drop past the upright line only, so it
// grows from the slack and does not jump; aside it is held to a spine's own
// bend aside (about 30 degrees: a third of the bend forward); and a lean aside
// is drawn back under the head over a couple of seconds even while leaning -
// a body held aside moves its hips under it, it does not stay bent. Forward
// and back are kept as they were (Nadi, 2026-10-29: "das nach vorne beugen
// geht nun wie erwartet").
inline constexpr float kLeanSideMostSine = 0.5f;          // aside at most 30 degrees of the trunk
inline constexpr float kLeanSideReturnPerSecond = 0.5f;   // the hips' way back under a head held aside

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

// The player moved by the game (the left stick's walk, a slope, a push) since
// the last frame: the anchor goes along, so only the head's own move against
// the player counts as a lean (Nadi, 2026-10-28: the body leaned "mit dem
// linken stick auch statt kopf" - the anchor stood in the world, the walk
// carried the hips away from it). A jump no walk makes in a frame (a load, a
// door, a teleport) forgets the anchor; it starts again at the hips.
inline constexpr float kLeanCarryJumpUnits = 200.0f;

inline void CarryLeanAnchor(LeanAnchor& a, float dx, float dy) {
	using lean_detail::Finite;
	if (!a.valid || !Finite(dx) || !Finite(dy)) {
		return;
	}
	if (dx * dx + dy * dy > kLeanCarryJumpUnits * kLeanCarryJumpUnits) {
		a.valid = false;
		return;
	}
	a.x += dx;
	a.y += dy;
}

// The carry from the player node's two poses: the anchor held where it was in
// the player's own frame, the player's turn (a snap turn) with it. `wasAt` and
// `wasRot` last frame's, `at` and `rot` this frame's.
inline void CarryLeanAnchorWithPlayer(LeanAnchor& a, const NiPoint3& wasAt, const NiMatrix33& wasRot,
                                      const NiPoint3& at, const NiMatrix33& rot) {
	if (!a.valid) {
		return;
	}
	const NiPoint3 was{a.x, a.y, wasAt.z};
	const NiPoint3 now = rot * (InverseMatrix(wasRot) * (was - wasAt)) + at;
	CarryLeanAnchor(a, now.x - was.x, now.y - was.y);
}

// A head higher than standing is the new standing over this time, not at
// once: one frame's spike (a tracking glitch) took it for good until
// 2026-11-28, and every frame after read as let down by the spike.
inline constexpr float kLeanStandingRiseSeconds = 0.2f;

// How far the head is below standing: the let-down against the standing
// let-down, which this frame keeps.
inline float StepLeanDrop(LeanAnchor& a, float loweredUnits, float dtSeconds) {
	using lean_detail::Finite;
	if (!Finite(loweredUnits)) {
		return 0.0f;
	}
	if (!a.haveStanding || !Finite(a.standingLowered)) {
		a.haveStanding = true;
		a.standingLowered = loweredUnits;
		return 0.0f;
	}
	if (loweredUnits < a.standingLowered) {
		float share = Finite(dtSeconds) && dtSeconds > 0.0f ? dtSeconds / kLeanStandingRiseSeconds : 1.0f;
		share = share < 1.0f ? share : 1.0f;
		a.standingLowered += (loweredUnits - a.standingLowered) * share;
		return 0.0f;
	}
	if (Finite(dtSeconds) && dtSeconds > 0.0f) {
		float share = dtSeconds / kLeanStandingFollowSeconds;
		share = share < 1.0f ? share : 1.0f;
		a.standingLowered += (loweredUnits - a.standingLowered) * share;
	}
	return loweredUnits - a.standingLowered;
}

// How far the hips may stay from under the head, forward and aside, for a head
// `dropUnits` below standing: the trunk's reach for the drop past the upright
// line, aside no more than a spine bends aside; the slack on each.
struct LeanReach {
	float forward = kLeanSlackUnits;
	float aside = kLeanSlackUnits;
};

inline LeanReach LeanReachFor(float dropUnits, float trunkUnits) {
	const float reach = LeanAllowedUnits(dropUnits - kLeanUprightUnits, trunkUnits, 0.0f);
	const float sideMost = lean_detail::Finite(trunkUnits) && trunkUnits > 0.0f ? trunkUnits * kLeanSideMostSine : 0.0f;
	return LeanReach{reach + kLeanSlackUnits, (reach < sideMost ? reach : sideMost) + kLeanSlackUnits};
}

// The anchor's step: where the hips stay, level, given where the placement
// would put them this frame (`hipsX/Y`, under the head), how far the head is
// below standing (`dropUnits`, StepLeanDrop) and the way the body faces
// (`bodyYaw`, HeadingOf's sense: the yaw of its right). Answers the lean, the
// level way from the anchor to those hips - what the trunk's turn has to make
// up. Upright, the anchor is drawn back under the head; leaning, only its part
// aside, slower. Past the reach - an ellipse, LeanReachFor's forward and aside
// - it is a step, and the anchor is dragged to the edge. A first frame, or
// hips that are not numbers, start the anchor at the hips: no lean. A yaw that
// is not a number reaches as far aside as forward.
inline NiPoint3 StepLeanAnchor(LeanAnchor& a, float hipsX, float hipsY, float dropUnits, float trunkUnits,
                               float dtSeconds, float bodyYaw) {
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
	const bool split = Finite(bodyYaw);
	const float rx = split ? math::Cos(bodyYaw) : 1.0f;  // the body's right, level
	const float ry = split ? math::Sin(bodyYaw) : 0.0f;
	const bool upright = !(dropUnits >= kLeanUprightUnits);
	if (Finite(dtSeconds) && dtSeconds > 0.0f && (upright || split)) {
		float share = dtSeconds * (upright ? kLeanReturnPerSecond : kLeanSideReturnPerSecond);
		share = share < 1.0f ? share : 1.0f;
		const float dx = hipsX - a.x;
		const float dy = hipsY - a.y;
		const float side = dx * rx + dy * ry;
		a.x += upright ? dx * share : rx * side * share;
		a.y += upright ? dy * share : ry * side * share;
	}
	LeanReach reach = LeanReachFor(dropUnits, trunkUnits);
	reach.aside = split ? reach.aside : reach.forward;
	float dx = hipsX - a.x;
	float dy = hipsY - a.y;
	const float side = (dx * rx + dy * ry) / reach.aside;
	const float ahead = (dy * rx - dx * ry) / reach.forward;
	const float past = side * side + ahead * ahead;
	if (past > 1.0f) {
		// A step: the anchor dragged to the edge of the reach.
		const float keep = 1.0f / math::Sqrt(past);
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

// The angle between two directions, in degrees; 0 for either being no
// direction.
inline float DegreesBetween(const NiPoint3& a, const NiPoint3& b) {
	const float lengths = lean_detail::Length(a) * lean_detail::Length(b);
	if (!lean_detail::Finite(lengths) || !(lengths > 1.0e-6f)) {
		return 0.0f;
	}
	float c = lean_detail::Dot(a, b) / lengths;
	c = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
	return math::Atan2(math::Sqrt(1.0f - c * c), c) * math::kRadiansToDegrees;
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

#pragma once

// The player's own body following the tracked player (docs/player-body-spec.md;
// the tester, 2026-10-11: "mach als nächstes das ik vom body. es gibt bereits
// eine stting für was ähnliches. das als grundlage verwendn" - the setting is
// [Body] Visible, the third-person skeleton shown under the headset,
// game/PlayerBody.cpp).
//
// Three parts, each on its own switch:
// - the arms reach the hands: each arm of the body solved so its wrist is
//   where the first-person hand is pinned (the controller), the clavicle
//   turned towards a far or high hand, the elbow placed by a hint from the
//   hand and the body, the twist bones taking half the roll;
// - the torso turns with the player: the body's yaw follows the head's, a
//   quick look aside leaves it, a held one brings it round; the animation's
//   own twist of the chest (an attack's swing) is taken out, so the shoulders
//   the arms hang from are where the player's are;
// - the knees bend for a crouch: the body is let down to put its eyes at the
//   headset, and its feet are lifted back to where the animation had them.
//
// The skeleton, read with Debug.BodyTreeProbe in the harness (2026-10-11,
// "Body bones:" lines): every Bip01 bone runs along its own +x (the upper arm
// 21.85 units to the forearm, the forearm 14.78 to the hand, the thigh 33.67
// to the calf, the calf 27.33 to the foot); the forearm's local rotation
// under the upper arm and the calf's under the thigh are pure turns about
// z (the idle pose: -32.5 and -18 degrees for the forearms, -17 and -14 for
// the calves) - the elbow and the knee are hinges about the upper bone's z,
// and the lower bone folds towards the upper bone's -y. So +y of an upper
// bone points out of its joint: the elbow's back, the knee's front.
//
// Pure, over plain values; game/BodyIk.cpp reads and writes the bones.
// body_ik_test covers every flow.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

namespace ik {

inline float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline NiPoint3 Cross(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float Length(const NiPoint3& a) { return math::Sqrt(a.LengthSquared()); }

// The unit along `a`, or false (and `out` untouched) for one too short or not
// a number.
inline bool Unit(const NiPoint3& a, NiPoint3& out, float shortest = 1e-5f) {
	const float l = Length(a);
	if (!(l > shortest)) {
		return false;
	}
	out = a * (1.0f / l);
	return true;
}

inline bool Finite(float v) { return v == v && v < 3.0e38f && v > -3.0e38f; }

inline bool Finite(const NiPoint3& p) { return Finite(p.x) && Finite(p.y) && Finite(p.z); }

// A rotation from its three columns: where the local x, y and z point in
// the parent's space (NiMatrix33 * v carries a local vector out).
inline NiMatrix33 FromColumns(const NiPoint3& x, const NiPoint3& y, const NiPoint3& z) {
	NiMatrix33 m{};
	m.data[0][0] = x.x;
	m.data[1][0] = x.y;
	m.data[2][0] = x.z;
	m.data[0][1] = y.x;
	m.data[1][1] = y.y;
	m.data[2][1] = y.z;
	m.data[0][2] = z.x;
	m.data[1][2] = z.y;
	m.data[2][2] = z.z;
	return m;
}

inline NiPoint3 Column(const NiMatrix33& m, int c) { return NiPoint3{m.data[0][c], m.data[1][c], m.data[2][c]}; }

inline NiMatrix33 Transposed(const NiMatrix33& m) {
	NiMatrix33 t{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			t.data[r][c] = m.data[c][r];
		}
	}
	return t;
}

// A turn of `angle` radians about the unit `axis` (Rodrigues; positive is
// counter-clockwise looking down the axis).
inline NiMatrix33 TurnAbout(const NiPoint3& k, float angle) {
	const float c = math::Cos(angle);
	const float s = math::Sin(angle);
	const float t = 1.0f - c;
	NiMatrix33 m{};
	m.data[0][0] = c + k.x * k.x * t;
	m.data[0][1] = k.x * k.y * t - k.z * s;
	m.data[0][2] = k.x * k.z * t + k.y * s;
	m.data[1][0] = k.y * k.x * t + k.z * s;
	m.data[1][1] = c + k.y * k.y * t;
	m.data[1][2] = k.y * k.z * t - k.x * s;
	m.data[2][0] = k.z * k.x * t - k.y * s;
	m.data[2][1] = k.z * k.y * t + k.x * s;
	m.data[2][2] = c + k.z * k.z * t;
	return m;
}

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// The inverse cosine, out of the atan2 that is there (MathFns.h keeps the
// build's imports from msvcrt short: no acos).
inline float Acos(float c) {
	const float k = Clamp(c, -1.0f, 1.0f);
	return math::Atan2(math::Sqrt(1.0f - k * k), k);
}

// 0 below `edge0`, 1 above `edge1`, smooth between.
inline float Smoothstep(float edge0, float edge1, float x) {
	if (!(edge1 > edge0)) {
		return x >= edge1 ? 1.0f : 0.0f;
	}
	const float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

// The yaw of a horizontal direction the way HeadingOf reads a rotation's
// right axis: atan2 of its y over its x, counter-clockwise from +x.
inline float YawOf(float x, float y) { return math::Atan2(y, x); }

}  // namespace ik

// ------------------------------------------------------------ Two bones
//
// A limb of two bones from `root` (the shoulder, the hip) to its end (the
// wrist, the ankle) through the joint (the elbow, the knee), the joint
// pointing towards `pole` across the root-target line. A target too far is
// reached by growing the two bones, at most `maxStretch` times their length
// (the player's arm and the body's need not be one length: the body's
// shoulder-to-wrist is 36.6 units, 52 cm, a grown man's about 55-60); past
// that the limb points straight at it and ends short. A target closer than
// the bones can fold leaves the limb folded as far as it goes. The joint is
// never quite straight (0.1 % short), so its hinge stays defined.
//
// The last share of the reach is softened (SoftReach): with the end at the
// limb's full length the joint would be straight, and there its angle
// swings degrees for a millimetre of the end - the cosine rule's slope runs
// to infinity at the straight limb (worked out for the body's arm, 21.85
// and 14.78 units: 68 degrees of elbow a unit in the last tenth of a
// percent, 16 at 2 % short, 8 at 8 %, 5 at 20 %) - so the head's own sway
// under the shoulder and the controller's tremor made a hand held out at
// arm's length twitch at the elbow (the tester, 2026-10-12: "zuckelt").
// From (1 - kLimbSoftShare) of the length on, the distance solved with
// saturates towards the length, d' = L - sL exp(-(d - (1 - s)L) / sL), and
// the bones are grown by d / d' so the end still lands on the target: the
// stretch curve of VRIK's kind in one number. With s = 0.08 the slope never
// exceeds its value at the zone's start, 8 degrees a unit; at the full
// length the elbow keeps 28 degrees of bend and the bones have grown 3 %
// (the zone's own growth, allowed whatever maxStretch says), a tenth past
// it 16 degrees and 11 % (with maxStretch allowing it).
inline constexpr float kLimbSoftShare = 0.08f;

inline float SoftReach(float d, float length, float share = kLimbSoftShare) {
	const float soft = length * share;
	const float start = length - soft;
	if (!(soft > 0.0f) || !(d > start) || !ik::Finite(soft)) {
		return d;
	}
	return length - soft * math::Exp(-(d - start) / soft);
}

struct LimbInput {
	NiPoint3 root{0.0f, 0.0f, 0.0f};
	float upperLength = 0.0f;
	float lowerLength = 0.0f;
	NiPoint3 target{0.0f, 0.0f, 0.0f};
	NiPoint3 pole{0.0f, 0.0f, -1.0f};
	float maxStretch = 1.0f;
	// The softened share of the reach (SoftReach); 0 solves the triangle
	// exactly - the legs, whose targets are the animation's own feet and
	// tremble no more than it does.
	float softShare = kLimbSoftShare;
};

struct LimbSolve {
	bool valid = false;
	NiPoint3 joint{0.0f, 0.0f, 0.0f};
	NiPoint3 end{0.0f, 0.0f, 0.0f};  // the target, or short of it
	NiMatrix33 upperRot = NiMatrix33::Identity();  // world: x to the joint, z the hinge
	NiMatrix33 lowerRot = NiMatrix33::Identity();  // world: x to the end, z the same hinge
	float stretch = 1.0f;   // the growth used
	bool reached = false;   // the end at the target
	float shortUnits = 0.0f;  // how far short of it, when not
};

inline constexpr float kLimbStraightShare = 0.999f;

inline LimbSolve SolveLimb(const LimbInput& in) {
	using namespace ik;
	LimbSolve s;
	if (!Finite(in.root) || !Finite(in.target) || !Finite(in.pole) || !(in.upperLength > 1e-3f) ||
	    !(in.lowerLength > 1e-3f) || !Finite(in.upperLength) || !Finite(in.lowerLength)) {
		return s;
	}
	const NiPoint3 toTarget = in.target - in.root;
	NiPoint3 u;
	if (!Unit(toTarget, u, 1e-3f)) {
		return s;  // the target on the root: no direction to reach in
	}
	const float d = Length(toTarget);
	// The reach softened, and the growth that lands the end on the target
	// all the same (a triangle grown keeps its angles).
	const float dSoft = SoftReach(d, in.upperLength + in.lowerLength, in.softShare);
	float k = 1.0f;
	if (d > dSoft) {
		// The soft zone's own growth - what lands the end at the full length,
		// 1 / (1 - s/e) - is always allowed, or a hand at arm's length would
		// be missed by that much with no growth allowed; past it the limit.
		float most = in.maxStretch > 1.0f && Finite(in.maxStretch) ? in.maxStretch : 1.0f;
		const float softMost =
			in.softShare > 0.0f && Finite(in.softShare) ? 1.0f / (1.0f - in.softShare * 0.36788f) : 1.0f;
		most = most > softMost ? most : softMost;
		k = Clamp(d / dSoft, 1.0f, most);
	}
	const float a = in.upperLength * k;
	const float b = in.lowerLength * k;
	const float maxD = (a + b) * kLimbStraightShare;
	const float folded = a > b ? a - b : b - a;
	const float minD = folded + 0.001f * (a + b);
	const float dc = Clamp(dSoft * k, minD, maxD);
	// The bend plane: the pole's part across the line, or - a pole on the
	// line - any direction across it.
	NiPoint3 v;
	if (!Unit(in.pole - u * Dot(in.pole, u), v, 1e-4f)) {
		const NiPoint3 helper = (u.z < 0.9f && u.z > -0.9f) ? NiPoint3{0.0f, 0.0f, 1.0f} : NiPoint3{1.0f, 0.0f, 0.0f};
		Unit(Cross(u, helper), v);
		v = Cross(v, u);
	}
	const float cosA = Clamp((a * a + dc * dc - b * b) / (2.0f * a * dc), -1.0f, 1.0f);
	const float sinA = math::Sqrt(1.0f - cosA * cosA);
	const NiPoint3 xu = u * cosA + v * sinA;
	const NiPoint3 yu = v * cosA - u * sinA;
	const NiPoint3 zu = Cross(xu, yu);
	s.joint = in.root + xu * a;
	s.end = in.root + u * dc;
	NiPoint3 xl;
	if (!Unit(s.end - s.joint, xl)) {
		return LimbSolve{};
	}
	const NiPoint3 yl = Cross(zu, xl);
	s.upperRot = FromColumns(xu, yu, zu);
	s.lowerRot = FromColumns(xl, yl, zu);
	s.stretch = k;
	s.shortUnits = d - dc;
	s.reached = s.shortUnits < 0.01f && s.shortUnits > -0.01f;
	s.valid = true;
	return s;
}

// ------------------------------------------------------------ The elbow
//
// Which way an elbow points: nothing tracks it, so it is guessed from two
// things (VRIK and Parger et al., "Human Upper-Body Inverse Kinematics for
// Increased Embodiment in Consumer-Grade VR", VRST 2018, both guess it from
// the hand's place relative to the shoulder and its orientation; the numbers
// here are this file's own, not theirs). The body's own habit: down, a
// little out and back. And the hand: the forearm runs on behind the wrist
// along the hand's own axis (a Bip01 hand bone runs along its +x, wrist to
// fingers), so the elbow is where the forearm would be with the wrist
// straight. Each taken across the shoulder-wrist line and the two added,
// each weighing what it has across the line (the hand's in forearm
// lengths): a hand pointing along the arm says nothing about the elbow, and
// a direction made unit out of a sliver across the line swung with every
// tremor of the hand (the first version, 2026-10-11). The sum made unit;
// down when neither has anything across the line. `side` is +1 for the
// right arm, -1 for the left.
inline constexpr float kElbowOut = 0.45f;
inline constexpr float kElbowBack = 0.35f;
inline constexpr float kElbowHandShare = 0.5f;

inline NiPoint3 ElbowPole(float side, const NiPoint3& right, const NiPoint3& forward, const NiPoint3& up,
                          const NiPoint3& shoulder, const NiPoint3& wrist, const NiPoint3& handAlong,
                          float forearmLength) {
	using namespace ik;
	NiPoint3 u;
	if (!Unit(wrist - shoulder, u, 1e-3f)) {
		return up * -1.0f;
	}
	const NiPoint3 body = right * (kElbowOut * side) - forward * kElbowBack - up;
	const NiPoint3 hint = (wrist - handAlong * forearmLength) - shoulder;
	const NiPoint3 bodyAcross = body - u * Dot(body, u);
	const NiPoint3 hintAcross =
		(hint - u * Dot(hint, u)) * (forearmLength > 1e-3f && Finite(forearmLength) ? 1.0f / forearmLength : 0.0f);
	NiPoint3 pole;
	if (!Unit(bodyAcross * (1.0f - kElbowHandShare) + hintAcross * kElbowHandShare, pole, 1e-3f)) {
		return up * -1.0f;
	}
	return pole;
}

// A direction eased towards another by `share` of the way: the two blended
// and the blend made unit; opposite directions, whose blend has no length,
// take the new one whole. The elbow's pole is eased so (kElbowEaseSeconds):
// the guess above and the torso's keep-out below move it in small steps
// where the hand's own motion is smooth, and the elbow swings instead of
// twitching; the hand itself is never eased.
inline constexpr float kElbowEaseSeconds = 0.05f;

inline NiPoint3 EasedDirection(const NiPoint3& last, const NiPoint3& wanted, float share) {
	const float s = ik::Clamp(share, 0.0f, 1.0f);
	NiPoint3 out;
	if (ik::Finite(last) && ik::Unit(last * (1.0f - s) + wanted * s, out, 1e-3f)) {
		return out;
	}
	return wanted;
}

// The share of the way to go in one frame for a first-order ease with the
// time constant `tauSeconds`: all of it with no time or no constant, or
// after a stall of a second and more.
inline float EaseShare(float dtSeconds, float tauSeconds) {
	if (!(dtSeconds > 0.0f) || !(dtSeconds < 1.0f) || !(tauSeconds > 0.0f)) {
		return 1.0f;
	}
	return 1.0f - math::Exp(-dtSeconds / tauSeconds);
}

// Never in the torso. The hand's hint can send the elbow there: a right hand
// reaching across to the left shoulder with its axis forward and left puts
// "the forearm straight behind the wrist" inside the chest, and the elbow
// went behind the shoulders' plane, past the middle (the harness,
// 2026-10-11, body-ik "across": the upper arm through the neck). Parger et
// al. keep their elbow's swivel to the outer half (13-175 degrees, "the
// elbow should always point away from the body center"); a swivel measured
// from "up" loses its sense for an arm pointing down and out, where "up"
// across the arm is already outwards (the next run: a natural elbow back
// taken for one inwards). So the rule here is the place itself: the elbow
// may not be in the torso - between the torso's back and a little in front
// of the shoulders' plane, inwards of a shoulder by more than
// kTorsoInsideUnits, between the pelvis and just above the shoulders - and
// kept kElbowClearUnits clear of it. If the elbow the pole gives is not that
// clear, the joint's circle round the shoulder-wrist line is searched from
// it both ways, kElbowSearchStep at a time, for the first place that is, and
// the crossing refined between the last step short of it and that one, so
// the turn grows and shrinks with the hand instead of jumping a step (the
// first version took whole steps of 10 degrees: at the chest the elbow
// popped with every crossing, 2026-10-12); the way round it went last
// frame is kept while the other way is not kElbowSideHoldRadians shorter.
// In the body's own measure (the probe of 2026-10-11): the shoulder joints
// 10.4 units either side of the middle, the spine bones in their plane, the
// pelvis 37 below them.
struct TorsoKeepOut {
	NiPoint3 mid{0.0f, 0.0f, 0.0f};      // between the two shoulder joints
	NiPoint3 right{1.0f, 0.0f, 0.0f};    // the body's right, across the ground
	NiPoint3 forward{0.0f, 1.0f, 0.0f};  // and its forward
	float shoulderLateral = 10.4f;       // a shoulder joint from the middle
	float shoulderZ = 0.0f;
	float pelvisZ = -37.0f;
};

inline constexpr float kTorsoBackUnits = 7.0f;    // the torso's back behind the shoulders' plane
inline constexpr float kTorsoFrontUnits = 2.0f;   // in front of it the chest's surface still bars the elbow
inline constexpr float kTorsoInsideUnits = 3.0f;  // an elbow this far inwards of its shoulder is in the side
inline constexpr float kTorsoAboveUnits = 10.0f;  // the shoulders' caps and the trapezius above them
inline constexpr float kElbowSearchStep = 10.0f * math::kDegreesToRadians;
inline constexpr float kElbowClearUnits = 2.0f;
inline constexpr float kElbowSideHoldRadians = 30.0f * math::kDegreesToRadians;

// How far an elbow is outside the torso's box: positive the distance past
// its nearest face (the largest excess over any one bound), negative the
// depth inside to the nearest face. Continuous across the faces.
inline float TorsoClearance(const NiPoint3& elbow, float side, const TorsoKeepOut& t) {
	const NiPoint3 d = elbow - t.mid;
	const float lateral = side * ik::Dot(d, t.right);  // out on its own side positive
	const float depth = ik::Dot(d, t.forward);
	float c = lateral - (t.shoulderLateral - kTorsoInsideUnits);
	const float others[4] = {-kTorsoBackUnits - depth, depth - kTorsoFrontUnits, t.pelvisZ - elbow.z,
	                         elbow.z - (t.shoulderZ + kTorsoAboveUnits)};
	for (const float o : others) {
		c = o > c ? o : c;
	}
	return c;
}

inline bool ElbowInsideTorso(const NiPoint3& elbow, float side, const TorsoKeepOut& t) {
	return TorsoClearance(elbow, side, t) < 0.0f;
}

// The limb's pole moved off the torso where it has to be: the pole to solve
// with, whether it was moved, which way round (+1 or -1) and how far. A
// limb that does not solve, or one whose every elbow is in the torso (the
// hand inside it), keeps its own. `preferSign` is the way it went last
// frame (0 for none).
struct ElbowClear {
	NiPoint3 pole{0.0f, 0.0f, -1.0f};
	bool moved = false;
	float sign = 0.0f;
	float angle = 0.0f;
};

inline ElbowClear ElbowClearOfTorso(const LimbInput& limb, float side, const TorsoKeepOut& t, float preferSign) {
	using namespace ik;
	ElbowClear r;
	r.pole = limb.pole;
	const LimbSolve first = SolveLimb(limb);
	if (!first.valid || TorsoClearance(first.joint, side, t) >= kElbowClearUnits) {
		return r;
	}
	NiPoint3 u;
	NiPoint3 p;
	if (!Unit(first.end - limb.root, u, 1e-3f) || !Unit(first.joint - limb.root - u * Dot(first.joint - limb.root, u), p)) {
		return r;
	}
	const NiPoint3 q = Cross(u, p);
	LimbInput tried = limb;
	// The elbow's clearance with the pole turned `angle` round the line.
	const auto clearanceAt = [&](float angle) {
		tried.pole = p * math::Cos(angle) + q * math::Sin(angle);
		const LimbSolve s = SolveLimb(tried);
		return s.valid ? TorsoClearance(s.joint, side, t) : -1.0e9f;
	};
	// Each way round: the first step past the clearance, then the crossing
	// between the step short of it and that one, to a sixty-fourth of a step.
	float found[2] = {-1.0f, -1.0f};
	for (int way = 0; way < 2; ++way) {
		const float sign = way == 0 ? 1.0f : -1.0f;
		float below = 0.0f;
		for (int step = 1; static_cast<float>(step) * kElbowSearchStep <= math::kPi + 1e-4f; ++step) {
			const float at = static_cast<float>(step) * kElbowSearchStep;
			if (clearanceAt(sign * at) >= kElbowClearUnits) {
				float lo = below;
				float hi = at;
				for (int i = 0; i < 6; ++i) {
					const float mid = 0.5f * (lo + hi);
					if (clearanceAt(sign * mid) >= kElbowClearUnits) {
						hi = mid;
					} else {
						lo = mid;
					}
				}
				found[way] = hi;
				break;
			}
			below = at;
		}
	}
	if (found[0] < 0.0f && found[1] < 0.0f) {
		return r;
	}
	int pick = 0;
	if (found[0] < 0.0f) {
		pick = 1;
	} else if (found[1] >= 0.0f) {
		pick = found[0] <= found[1] ? 0 : 1;
		const int preferred = preferSign > 0.0f ? 0 : (preferSign < 0.0f ? 1 : -1);
		if (preferred >= 0 && preferred != pick && found[preferred] - found[pick] < kElbowSideHoldRadians) {
			pick = preferred;
		}
	}
	r.sign = pick == 0 ? 1.0f : -1.0f;
	r.angle = found[pick];
	r.pole = p * math::Cos(r.sign * r.angle) + q * math::Sin(r.sign * r.angle);
	r.moved = true;
	return r;
}

// ------------------------------------------------------------ The shield
//
// What hangs on a forearm's twist bone - the shield - is carried with the
// hand where the pins left it, not with the forearm the arm is laid onto. A
// shield is held by its grip: in the hand it stays put, whichever way the
// elbow goes. And the contact step that holds it at walls reads its disc in
// the hand's frame (HandContactLogic.h) - carried by the IK's forearm
// instead, the shield left the hand as the elbow moved, the disc read again
// each second jumped, and the shield-wall harness (2026-10-11, body IK on)
// met the floor three times as a "knock" and never held it, where without
// the IK it held and let go 22.7 units on. (Holding the forearm itself
// still instead - the wrist locked - was tried the same day and left the
// forearm 19-46 degrees off the body's arm in every pose of the harness:
// the animation's wrist is not a reachable one for the body's upper arm.)
//
// The child's local transform under its parent's new world pose that keeps
// the world pose it had under the old: L' = W'^-1 W L, scale included.
inline NiTransform CarriedLocal(const NiTransform& oldParentWorld, const NiTransform& newParentWorld,
                                const NiTransform& childLocal) {
	// The world the child had: W * L.
	NiTransform world;
	world.rot = oldParentWorld.rot * childLocal.rot;
	world.pos = oldParentWorld.pos + oldParentWorld.rot * (childLocal.pos * oldParentWorld.scale);
	world.scale = oldParentWorld.scale * childLocal.scale;
	// Taken back under the new parent.
	const float scale = newParentWorld.scale > 0.0f ? newParentWorld.scale : 1.0f;
	const NiMatrix33 inverse = InverseMatrix(newParentWorld.rot);  // not the transpose: a race's height sits in these matrices
	NiTransform local;
	local.rot = inverse * world.rot;
	local.pos = (inverse * (world.pos - newParentWorld.pos)) * (1.0f / scale);
	local.scale = world.scale / scale;
	return local;
}

// ------------------------------------------------------------ The clavicle
//
// A far hand draws the shoulder after it, a high one lifts it: the clavicle
// turns towards the hand by a share of the angle between it and the hand,
// at most kClavicleMostRadians. The share grows from nothing at two thirds of
// the arm's reach to half at its full length, and to 0.6 with the hand four
// fifths of an arm above the shoulder; the larger of the two.
inline constexpr float kClavicleMostRadians = 40.0f * math::kDegreesToRadians;

inline float ClavicleShare(float reachRatio, float liftRatio) {
	const float reach = 0.5f * ik::Smoothstep(0.65f, 1.05f, reachRatio);
	const float lift = 0.6f * ik::Smoothstep(0.0f, 0.8f, liftRatio);
	return reach > lift ? reach : lift;
}

// The clavicle's turn: about `axis`, by `angle` radians (0 for none).
struct ClavicleTurn {
	NiPoint3 axis{0.0f, 0.0f, 1.0f};
	float angle = 0.0f;
};

inline ClavicleTurn TurnClavicle(const NiPoint3& clavicle, const NiPoint3& clavicleAlong, const NiPoint3& shoulder,
                                 const NiPoint3& target, float armLength, const NiPoint3& up) {
	using namespace ik;
	ClavicleTurn t;
	NiPoint3 along;
	NiPoint3 towards;
	if (!(armLength > 1e-3f) || !Unit(clavicleAlong, along) || !Unit(target - clavicle, towards, 1e-3f)) {
		return t;
	}
	const NiPoint3 fromShoulder = target - shoulder;
	const float share = ClavicleShare(Length(fromShoulder) / armLength, Dot(fromShoulder, up) / armLength);
	if (!(share > 0.0f)) {
		return t;
	}
	NiPoint3 axis;
	if (!Unit(Cross(along, towards), axis, 1e-4f)) {
		return t;
	}
	const float between = Acos(Dot(along, towards));
	t.axis = axis;
	t.angle = share * (between < kClavicleMostRadians ? between : kClavicleMostRadians);
	return t;
}

// ------------------------------------------------------------ The twist
//
// How far a rotation turns about its own x axis: the twist of its
// swing-twist split about x (the quaternion's w and x alone), in radians,
// -pi to pi. For a forearm's child it is how far the hand is rolled
// (pronation); the twist bones take half of a change in it, so the skin
// between a joint and the next does not wring at one end.
inline float TwistAboutX(const NiMatrix33& m) {
	const float trace = m.data[0][0] + m.data[1][1] + m.data[2][2];
	float w = 0.0f;
	float x = 0.0f;
	if (trace > 0.0f) {
		const float s = math::Sqrt(trace + 1.0f) * 2.0f;
		w = 0.25f * s;
		x = (m.data[2][1] - m.data[1][2]) / s;
	} else if (m.data[0][0] > m.data[1][1] && m.data[0][0] > m.data[2][2]) {
		const float s = math::Sqrt(1.0f + m.data[0][0] - m.data[1][1] - m.data[2][2]) * 2.0f;
		w = (m.data[2][1] - m.data[1][2]) / s;
		x = 0.25f * s;
	} else if (m.data[1][1] > m.data[2][2]) {
		const float s = math::Sqrt(1.0f + m.data[1][1] - m.data[0][0] - m.data[2][2]) * 2.0f;
		w = (m.data[0][2] - m.data[2][0]) / s;
		x = (m.data[0][1] + m.data[1][0]) / s;
	} else {
		const float s = math::Sqrt(1.0f + m.data[2][2] - m.data[0][0] - m.data[1][1]) * 2.0f;
		w = (m.data[1][0] - m.data[0][1]) / s;
		x = (m.data[0][2] + m.data[2][0]) / s;
	}
	if (!(w * w + x * x > 1e-12f)) {
		return 0.0f;  // a half turn about an axis across x: no twist to speak of
	}
	return math::WrapAngle(2.0f * math::Atan2(x, w));
}

// A turn about x, the twist bones' share.
inline NiMatrix33 TurnAboutX(float angle) { return ik::TurnAbout(NiPoint3{1.0f, 0.0f, 0.0f}, angle); }

// Half a change of roll, the change wrapped to a half turn either way and
// held within kTwistMostRadians, so a hand rolled past a half turn does not
// flip its twist bone round.
inline constexpr float kTwistMostRadians = 150.0f * math::kDegreesToRadians;

inline float HalfRoll(float before, float after) {
	const float change = math::WrapAngle(after - before);
	return 0.5f * ik::Clamp(change, -kTwistMostRadians, kTwistMostRadians);
}

// An angle continued from last frame's: the step from it taken the shorter
// way round, so a roll that runs on past a half turn keeps counting up
// (held within a half turn and a bit by the caller's clamp) instead of
// flipping to the other side - a twist bone swung round by 150 degrees in
// one frame.
inline float UnwrapAngle(float last, float raw) { return last + math::WrapAngle(raw - last); }

// ------------------------------------------------------------ The torso
//
// The body faces where the player does. Nothing tracks the chest, so it is
// taken from the head: the head may turn kTorsoFreeRadians either way before
// it drags the torso along, and a turn held is followed, the torso closing
// on the head with a time constant of `followSeconds` (a glance aside stays
// a glance, a look held turns the body). Kept in the headset's own tracking
// space - its yaw from the recenter - so a snap turn, which turns the world
// and not the head, and the game turning the player under a still view (the
// walk direction) leave the torso where it is relative to the head. A jump
// of the head's yaw no head can make in one frame (a recenter, a tracking
// glitch) starts the torso afresh on the head.
inline constexpr float kTorsoJumpRadians = 30.0f * math::kDegreesToRadians;

struct TorsoYawSettings {
	float freeRadians = 40.0f * math::kDegreesToRadians;
	float followSeconds = 1.0f;
};

// [Body] TorsoFollowDegrees and TorsoFollowSeconds, held to sense: 0-120
// degrees, 0 (no lag) to 10 s; out of range or not a number, the defaults.
inline TorsoYawSettings TorsoYawFrom(float freeDegrees, float followSeconds) {
	TorsoYawSettings s;
	if (freeDegrees >= 0.0f && freeDegrees <= 120.0f) {
		s.freeRadians = freeDegrees * math::kDegreesToRadians;
	}
	if (followSeconds >= 0.0f && followSeconds <= 10.0f) {
		s.followSeconds = followSeconds;
	}
	return s;
}

struct TorsoYawState {
	bool valid = false;
	float torso = 0.0f;     // the torso's yaw in tracking space
	float lastHead = 0.0f;  // the head's, last frame
};

// One frame: the head's yaw in tracking space (radians) and the frame's
// time. Answers the torso's yaw relative to the head's: what is added to the
// head's yaw in the world to face the body.
inline float StepTorsoYaw(TorsoYawState& s, float headYaw, float dtSeconds, const TorsoYawSettings& set) {
	if (!ik::Finite(headYaw)) {
		return s.valid ? math::WrapAngle(s.torso - s.lastHead) : 0.0f;
	}
	const float jump = math::WrapAngle(headYaw - s.lastHead);
	if (!s.valid || jump > kTorsoJumpRadians || jump < -kTorsoJumpRadians) {
		s.valid = true;
		s.torso = headYaw;
	}
	s.lastHead = headYaw;
	const float behind = math::WrapAngle(headYaw - s.torso);
	if (behind > set.freeRadians) {
		s.torso = math::WrapAngle(headYaw - set.freeRadians);
	} else if (behind < -set.freeRadians) {
		s.torso = math::WrapAngle(headYaw + set.freeRadians);
	}
	if (dtSeconds > 0.0f && dtSeconds < 1.0f) {
		const float share = set.followSeconds > 0.0f ? 1.0f - math::Exp(-dtSeconds / set.followSeconds) : 1.0f;
		s.torso = math::WrapAngle(s.torso + math::WrapAngle(headYaw - s.torso) * share);
	}
	return math::WrapAngle(s.torso - headYaw);
}

// Which way a chest faces, from its two shoulder joints: the yaw of the line
// from the left one to the right one (the chest's right axis, HeadingOf's
// sense). False for two shoulders less than a unit apart across the ground.
inline bool ChestYaw(const NiPoint3& leftShoulder, const NiPoint3& rightShoulder, float& yaw) {
	const float dx = rightShoulder.x - leftShoulder.x;
	const float dy = rightShoulder.y - leftShoulder.y;
	if (!(dx * dx + dy * dy > 1.0f)) {
		return false;
	}
	yaw = ik::YawOf(dx, dy);
	return true;
}

// The chest's turn back to the torso's yaw - the animation's own twist (an
// attack's swing of the shoulders) taken out - held within a quarter turn
// and a bit either way: more is not a twist but shoulders read wrongly.
inline constexpr float kChestMostRadians = 75.0f * math::kDegreesToRadians;

inline float ChestCorrection(float torsoYaw, float chestYaw) {
	return ik::Clamp(math::WrapAngle(torsoYaw - chestYaw), -kChestMostRadians, kChestMostRadians);
}

// ------------------------------------------------------------ The knees
//
// The body is let down to put its eyes at the headset; crouching, that is
// more than its legs' own height allows and the feet go into the ground. The
// feet are lifted back by what it was let down, less kFeetFreeUnits - so a
// headset standing a little lower than the animation's eyes, which is
// calibration and not a crouch, bends no knee (the floor hides a sole a few
// units under it).
inline constexpr float kFeetFreeUnits = 3.0f;

inline float FeetLiftUnits(float loweredUnits, float freeUnits = kFeetFreeUnits) {
	return loweredUnits > freeUnits && ik::Finite(loweredUnits) ? loweredUnits - freeUnits : 0.0f;
}

// Whether a foot is solved back to where the animation put it: `delta` is
// that place less where the foot is now (the body let down, its trunk held
// higher or lower than the animation's). Sunk into the floor by no more than
// the free units it is left (the floor hides it); floating above by more
// than a quarter unit, sunk further, or off sideways by more than half a
// unit, it is solved.
inline bool FootSolveWanted(const NiPoint3& delta, float freeBelow = kFeetFreeUnits) {
	if (!ik::Finite(delta)) {
		return false;
	}
	return delta.z > freeBelow || delta.z < -0.25f || delta.x * delta.x + delta.y * delta.y > 0.25f;
}

// Half a roll, held within kTwistMostRadians either way - for a roll
// continued from frame to frame (UnwrapAngle), which may stand past a half
// turn.
inline float HalfOfRoll(float roll) { return 0.5f * ik::Clamp(roll, -kTwistMostRadians, kTwistMostRadians); }

// The roll memory held within a half turn and a bit past the clamp, so a
// controller spun round and round does not run it away: the next unwrap
// still takes the short step from it.
inline constexpr float kRollMemoryMostRadians = kTwistMostRadians + 0.5f;

inline float HoldRollMemory(float roll) { return ik::Clamp(roll, -kRollMemoryMostRadians, kRollMemoryMostRadians); }

// The forearm's roll spread (the tester, 2026-10-12: "arme wirken super, nur
// wenn ich die hände voll drehe sodass die senkrecht nach aussen schauen
// (also linke hand daumen runter handfläche nach links, rechts vice versa)
// dann verkleinert sich das handgelenk zu sehr").
//
// The hand's roll on the forearm (the controller's hand against the solved
// forearm, less the animation's own hand on its forearm) is spread over the
// three places the skin blends it: the forearm bone itself (the elbow's skin,
// blended upper arm to forearm), its twist bone (the forearm's skin, blended
// forearm to twist up the forearm) and the hand (the wrist's ring, blended
// forearm, twist and hand). Read from the game's own meshes
// (meshes\characters\_male\upperbody.nif "Arms" and hand.nif, pyffi,
// 2026-10-12, the weights along the right arm in the bind pose): the elbow
// 0.42 upper arm and 0.51 forearm; the forearm's middle 0.46 forearm and 0.54
// twist; the wrist's ring 0.17 forearm, 0.41 twist and 0.41 hand. The
// forearm's twist bone sits at the elbow and is a gradient up the forearm,
// not a wrist bone.
//
// Linear blend skinning pinches a vertex blended between bones turned apart.
// Until 2026-10-12 the forearm took none of the roll, its twist bone half of
// it held to 75 degrees, the hand the whole: at a roll of 180 degrees the
// wrist's ring kept 0.48 of its width (0.17 at 0, 0.41 at 90 and 0.41 at 180
// degrees, summed as turns), and past 150 the twist bone stood while the hand
// went on - the pinch the tester saw. Now the forearm takes kForearmRollShare,
// its twist bone kForeTwistRollShare (in the world: its local turn is the
// difference), the hand the whole, all of it up to kForeRollMostRadians
// either way: at 180 degrees the elbow keeps 0.87 of its width, the forearm
// 0.87 and the wrist's ring 0.74 (the weights above, derived, not measured on
// screen; the harness's eye dumps show the wrists full).
//
// The roll the harness measured at rest is not 0 - the right hand 65 to 74
// degrees, the left -108 to -113, the controllers held pointing ahead
// (body-look-wrist, 2026-10-12) - so the range is wide enough for a half turn
// from rest either way. Also tried that day and left out: the elbow swung
// round the shoulder-wrist line by the roll past 60 degrees (VRArmIK's
// rotateElbowWithHandRight, github.com/dabeschte/VRArmIK): at rest the left
// arm's -110 already swung its elbow 36 degrees, and a hand turned half round
// in one step swung both elbows across the chest - the way round a half turn
// is not known from one frame. docs/player-body-spec.md, round 3.
inline constexpr float kForearmRollShare = 1.0f / 3.0f;
inline constexpr float kForeTwistRollShare = 2.0f / 3.0f;
inline constexpr float kForeRollMostRadians = 270.0f * math::kDegreesToRadians;

struct ForearmRolls {
	float forearm = 0.0f;  // the forearm bone about its own length, in the world
	float twist = 0.0f;    // its twist bone, in the world
};

inline ForearmRolls ForearmRollsFor(float roll) {
	ForearmRolls r;
	if (!(roll == roll)) {
		return r;
	}
	const float held = ik::Clamp(roll, -kForeRollMostRadians, kForeRollMostRadians);
	r.forearm = held * kForearmRollShare;
	r.twist = held * kForeTwistRollShare;
	return r;
}

// The forearm roll's memory held a little past that range, as the upper
// arm's is past its own (HoldRollMemory).
inline constexpr float kForeRollMemoryMostRadians = kForeRollMostRadians + 0.5f;

inline float HoldForeRollMemory(float roll) {
	return ik::Clamp(roll, -kForeRollMemoryMostRadians, kForeRollMemoryMostRadians);
}

// The arms traced over the first seconds the IK runs after a load (the
// tester, 2026-10-14: the wrists "verdreht und verkleinert beim
// spielstart. erst nach paar sekunden sehen die normal aus"; the harness,
// its hands held still from the start, saw rolls steady from the first
// mark): every kStartTraceEverySeconds for kStartTraceSeconds, the arm
// lines as at a mark, so a player's own start shows what changes.
inline constexpr float kStartTraceSeconds = 8.0f;
inline constexpr float kStartTraceEverySeconds = 0.25f;

struct StartTrace {
	float elapsed = 0.0f;
	float nextAt = 0.0f;
};

inline bool StepStartTrace(StartTrace& s, float dtSeconds) {
	if (dtSeconds > 0.0f && dtSeconds < 1.0e6f) {
		s.elapsed += dtSeconds;
	}
	if (s.elapsed > kStartTraceSeconds || s.elapsed < s.nextAt) {
		return false;
	}
	s.nextAt = s.elapsed + kStartTraceEverySeconds;
	return true;
}

// Which way a knee points: forward, a little out.
inline NiPoint3 KneePole(float side, const NiPoint3& right, const NiPoint3& forward) {
	return forward + right * (0.15f * side);
}

// ------------------------------------------------------------ The trunk
//
// The body stands steady under the head: the chain from the skeleton's root
// to the head (NonAccum, pelvis, the three spine bones, the neck, the head)
// is held at one pose - the standing idle's, taken once - instead of the
// animation's. The body is stood so its head's eyes land on the headset, and
// with the animation's own chain every pose moved the whole body under the
// still headset: the walk bobbed it 3 units a step and leaned it 6 back, the
// sneak dropped its head 40 units in one frame and the body jumped 44 with
// it (the harness, 2026-10-12; the tester: "ruckelt und zuckelt ... alles
// auf einmal"). A real walk bobs the eyes with the body; a headset does not
// bob, so the body may not either (VRIK's way round: the pelvis hangs from
// the head target). The legs keep the animation; their feet are solved back
// to where it put them on the ground (the knees).
//
// The pose is taken from a frame the player stands idle: not sneaking, no
// action, no step asked of the legs (the movement flags' four direction
// bits; xOBSE GameProcess.h: forward 0x1, backward 0x2, left 0x4, right
// 0x8), held for kStandingSettleSeconds so a landing or a blend is not
// taken - and the head's offset from the root has to read as a standing
// one.
inline constexpr UInt16 kMovementStepBits = 0x000F;
inline constexpr float kStandingSettleSeconds = 0.5f;

inline bool PlayerStandsIdle(bool sneaking, SInt32 action, UInt16 movementFlags, SInt32 actionNone) {
	return !sneaking && action == actionNone && (movementFlags & kMovementStepBits) == 0;
}

// Whether a head offset from the root (the root's own axes, scale one) is a
// standing one: the head 95 to 130 units up (the idle's 112), within 12
// units of the root across the ground.
inline bool HeadOffsetStands(const NiPoint3& headFromRoot) {
	return ik::Finite(headFromRoot) && headFromRoot.z > 95.0f && headFromRoot.z < 130.0f &&
	       headFromRoot.x * headFromRoot.x + headFromRoot.y * headFromRoot.y < 144.0f;
}

// Seconds the player has stood idle, carried from frame to frame: zero the
// moment the stance breaks. Answers whether the pose may be taken now.
inline bool StandingSettled(float& standingSeconds, bool standsIdle, float dtSeconds) {
	if (!standsIdle || !(dtSeconds >= 0.0f) || !(dtSeconds < 1.0f)) {
		standingSeconds = 0.0f;
		return false;
	}
	standingSeconds += dtSeconds;
	return standingSeconds >= kStandingSettleSeconds;
}

// ------------------------------------------------------------ The gates
//
// Whether a side's arm follows its hand this frame: the arms part on, the
// body shown, that hand pinned this frame, and the hands in their own sides
// (left-handed play puts the right hand's bone on the left controller - the
// body's right arm would reach across the chest to it; left to the body's
// own arms for now).
inline bool ArmFollowsHand(bool armsOn, bool bodyShown, bool handPinned, bool rolesSwapped) {
	return armsOn && bodyShown && handPinned && !rolesSwapped;
}

}  // namespace obvr::game

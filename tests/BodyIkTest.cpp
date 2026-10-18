// Checks the body IK's decisions (game/BodyIkLogic.h): the two-bone limb
// against the rig's own hinge, reachable, too far (grown, then short), too
// near, and with nothing to go by, and its softened reach; the elbow's hint
// for the poses an arm takes and its fade along the arm, its ease, the rolls
// continued; the torso keep-out, its refined turn and the way round kept;
// the clavicle's share and turn; the twist split; the torso after the head,
// its free turn, its follow, its fresh start; the chest's yaw and its
// correction; the knees' lift and the feet put back; the trunk's pose (the
// idle stance, its settle, a standing head offset); the gate of an arm.

#include <cstdio>
#include <limits>

#include "game/BodyIkLogic.h"

namespace {

using obvr::NiMatrix33;
using obvr::NiPoint3;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 0.01f) { return a == a && a >= b - within && a <= b + within; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float within = 0.01f) {
	return Near(a.x, b.x, within) && Near(a.y, b.y, within) && Near(a.z, b.z, within);
}

float Distance(const NiPoint3& a, const NiPoint3& b) { return ik::Length(a - b); }

constexpr float kDeg = obvr::math::kDegreesToRadians;

// Orthonormal with determinant +1.
bool IsRotation(const NiMatrix33& m) {
	const NiPoint3 x = ik::Column(m, 0);
	const NiPoint3 y = ik::Column(m, 1);
	const NiPoint3 z = ik::Column(m, 2);
	return Near(ik::Dot(x, x), 1.0f, 1e-3f) && Near(ik::Dot(y, y), 1.0f, 1e-3f) && Near(ik::Dot(z, z), 1.0f, 1e-3f) &&
	       Near(ik::Dot(x, y), 0.0f, 1e-3f) && Near(ik::Dot(y, z), 0.0f, 1e-3f) && Near(ik::Dot(x, z), 0.0f, 1e-3f) &&
	       NearPoint(ik::Cross(x, y), z, 1e-3f);
}

// The lower bone's rotation under the upper one: a turn about z only, and its
// angle (negative folds towards -y).
bool PureZ(const NiMatrix33& upper, const NiMatrix33& lower, float& angle) {
	const NiMatrix33 local = ik::Transposed(upper) * lower;
	angle = obvr::math::Atan2(local.data[1][0], local.data[0][0]);
	return Near(local.data[2][2], 1.0f, 1e-3f) && Near(local.data[0][2], 0.0f, 1e-3f) &&
	       Near(local.data[1][2], 0.0f, 1e-3f) && Near(local.data[2][0], 0.0f, 1e-3f) &&
	       Near(local.data[2][1], 0.0f, 1e-3f);
}

constexpr float kUpper = 21.85f;
constexpr float kLower = 14.78f;

LimbInput Arm(const NiPoint3& target, const NiPoint3& pole, float stretch = 1.0f) {
	LimbInput in;
	in.root = NiPoint3{0.0f, 0.0f, 0.0f};
	in.upperLength = kUpper;
	in.lowerLength = kLower;
	in.target = target;
	in.pole = pole;
	in.maxStretch = stretch;
	return in;
}

void TestLimbReach() {
	std::printf("the limb, its end within reach\n");
	const NiPoint3 target{10.0f, 20.0f, -15.0f};
	const NiPoint3 pole{0.0f, 0.0f, -1.0f};
	const LimbSolve s = SolveLimb(Arm(target, pole));
	Check(s.valid && s.reached && s.stretch == 1.0f, "valid, reached, nothing grown");
	Check(NearPoint(s.end, target), "the end at the target");
	Check(Near(Distance(s.joint, NiPoint3{0, 0, 0}), kUpper), "the joint an upper bone from the root");
	Check(Near(Distance(s.end, s.joint), kLower), "the end a lower bone from the joint");
	Check(IsRotation(s.upperRot) && IsRotation(s.lowerRot), "both bones turned by true rotations");
	Check(NearPoint(ik::Column(s.upperRot, 0), s.joint * (1.0f / kUpper), 1e-3f), "the upper bone's x runs to the joint");
	Check(NearPoint(ik::Column(s.lowerRot, 0), (s.end - s.joint) * (1.0f / kLower), 1e-3f),
	      "the lower bone's x runs to the end");
	float angle = 0.0f;
	Check(PureZ(s.upperRot, s.lowerRot, angle) && angle < 0.0f,
	      "the lower bone under the upper: a turn about z only, folding towards -y (the rig's hinge)");
	// The joint on the pole's side of the line from the root to the target.
	NiPoint3 u;
	ik::Unit(target, u);
	const NiPoint3 across = pole - u * ik::Dot(pole, u);
	Check(ik::Dot(s.joint - u * ik::Dot(s.joint, u), across) > 0.0f, "the joint on the pole's side");
	// And the upper bone's +y points out of the joint, the pole's way.
	Check(ik::Dot(ik::Column(s.upperRot, 1), across) > 0.0f, "the upper bone's +y out of the joint");
	// The other pole: the joint on the other side.
	const LimbSolve o = SolveLimb(Arm(target, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(o.valid && o.reached && ik::Dot(o.joint - u * ik::Dot(o.joint, u), across) < 0.0f,
	      "the opposite pole: the joint on the other side, the end still reached");
}

void TestLimbFar() {
	std::printf("the limb, its end out of reach\n");
	const NiPoint3 pole{0.0f, 0.0f, -1.0f};
	const float length = kUpper + kLower;
	// Past the length the softened reach saturates towards it: the end lands
	// as far as that goes, the bones grown only the soft zone's own 3 %.
	const float soft40 = SoftReach(40.0f, length);
	const float softGrowth = 1.0f / (1.0f - kLimbSoftShare * 0.36788f);
	const LimbSolve none = SolveLimb(Arm(NiPoint3{40.0f, 0.0f, 0.0f}, pole));
	Check(none.valid && !none.reached && Near(none.stretch, softGrowth, 1e-4f),
	      "no growth allowed: grown the soft zone's own 3 % all the same, short");
	Check(soft40 < length && soft40 > 0.98f * length, "(the softened reach of 40: just short of the length)");
	Check(NearPoint(none.end, NiPoint3{soft40 * softGrowth, 0.0f, 0.0f}, 0.02f),
	      "the end towards it, as far as the softened reach grown goes");
	Check(Near(none.shortUnits, 40.0f - soft40 * softGrowth, 0.02f), "short by the rest");
	float angle = 0.0f;
	Check(PureZ(none.upperRot, none.lowerRot, angle) && angle < -10.0f * kDeg && angle > -25.0f * kDeg,
	      "the elbow keeps its bend there (10-25 degrees), not locked straight");
	const LimbSolve grown = SolveLimb(Arm(NiPoint3{40.0f, 0.0f, 0.0f}, pole, 1.15f));
	Check(grown.valid && grown.reached && Near(grown.stretch, 40.0f / soft40, 1e-3f),
	      "allowed to grow 15 %: grown just enough to land the end, reached");
	Check(Near(Distance(grown.joint, NiPoint3{0, 0, 0}), kUpper * grown.stretch, 0.01f), "the upper bone grown");
	const LimbSolve tooFar = SolveLimb(Arm(NiPoint3{50.0f, 0.0f, 0.0f}, pole, 1.15f));
	Check(tooFar.valid && !tooFar.reached && Near(tooFar.stretch, 1.15f, 1e-4f),
	      "past what growing allows: grown all it may, still short");
	Check(Near(tooFar.shortUnits, 50.0f - length * 1.15f * kLimbStraightShare, 0.02f), "short by the rest");
	Check(PureZ(tooFar.upperRot, tooFar.lowerRot, angle) && angle <= 0.0f, "straight, the hinge still defined");
	const LimbSolve shrink = SolveLimb(Arm(NiPoint3{40.0f, 0.0f, 0.0f}, pole, 0.5f));
	Check(shrink.valid && Near(shrink.stretch, softGrowth, 1e-4f) && !shrink.reached,
	      "a growth below 1: the soft zone's own only");
	const LimbSolve nanStretch =
		SolveLimb(Arm(NiPoint3{40.0f, 0.0f, 0.0f}, pole, std::numeric_limits<float>::quiet_NaN()));
	Check(nanStretch.valid && Near(nanStretch.stretch, softGrowth, 1e-4f), "a growth of no number: the soft zone's own only");
	// Solved exactly (no soft share): the old straight reach, nothing grown.
	LimbInput exact = Arm(NiPoint3{40.0f, 0.0f, 0.0f}, pole);
	exact.softShare = 0.0f;
	const LimbSolve straight = SolveLimb(exact);
	Check(straight.stretch == 1.0f && NearPoint(straight.end, NiPoint3{length * kLimbStraightShare, 0.0f, 0.0f}, 0.02f),
	      "no soft share: nothing grown, the end straight towards it, 0.1 % short of straight");
}

void TestSoftReach() {
	std::printf("the softened reach\n");
	const float length = kUpper + kLower;
	const float start = length * (1.0f - kLimbSoftShare);
	Check(SoftReach(20.0f, length) == 20.0f, "well within the reach: as it is");
	Check(Near(SoftReach(start, length), start, 1e-4f), "at the zone's start: the same (no step)");
	Check(SoftReach(start + 0.5f, length) < start + 0.5f && SoftReach(start + 0.5f, length) > start + 0.45f,
	      "just past it: a little less, the slope near one");
	const float atLength = SoftReach(length, length);
	Check(atLength < length && atLength > 0.96f * length, "at the full length: about 3 % short of it");
	Check(SoftReach(2.0f * length, length) < length && SoftReach(2.0f * length, length) > atLength,
	      "far past it: never the length, still more than at the length");
	Check(SoftReach(40.0f, length, 0.0f) == 40.0f, "no share: as it is");
	Check(SoftReach(40.0f, length, std::numeric_limits<float>::quiet_NaN()) == 40.0f, "a share of no number: as it is");
	// The slope: the elbow's angle against the end's distance, in the body's
	// arm, never steeper than at the zone's start - a controller's tremor
	// at arm's length does not twitch the elbow.
	float worst = 0.0f;
	float atStart = 0.0f;
	for (int i = 0; i < 60; ++i) {
		const float d = start - 1.0f + static_cast<float>(i) * 0.1f;
		const LimbInput a = Arm(NiPoint3{d, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, 1.15f);
		const LimbInput b = Arm(NiPoint3{d + 0.05f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}, 1.15f);
		float angleA = 0.0f;
		float angleB = 0.0f;
		const LimbSolve sa = SolveLimb(a);
		const LimbSolve sb = SolveLimb(b);
		PureZ(sa.upperRot, sa.lowerRot, angleA);
		PureZ(sb.upperRot, sb.lowerRot, angleB);
		const float slope = (angleB > angleA ? angleB - angleA : angleA - angleB) / 0.05f / kDeg;
		worst = slope > worst ? slope : worst;
		if (i == 10) {
			atStart = slope;
		}
	}
	Check(worst < 9.0f && atStart > 6.0f, "the elbow's slope at most 9 degrees a unit, about 8 at the zone's start");
	// And the same arm solved exactly: 40 degrees a unit and more at the end.
	LimbInput a = Arm(NiPoint3{length * 0.9985f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f});
	LimbInput b = Arm(NiPoint3{length * 0.9985f + 0.02f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f});
	a.softShare = 0.0f;
	b.softShare = 0.0f;
	float angleA = 0.0f;
	float angleB = 0.0f;
	const LimbSolve sa = SolveLimb(a);
	const LimbSolve sb = SolveLimb(b);
	PureZ(sa.upperRot, sa.lowerRot, angleA);
	PureZ(sb.upperRot, sb.lowerRot, angleB);
	const float exactSlope = (angleA > angleB ? angleA - angleB : angleB - angleA) / 0.02f / kDeg;
	Check(exactSlope > 40.0f, "(solved exactly, the last tenth of a percent: over 40 degrees a unit)");
}

void TestLimbNear() {
	std::printf("the limb, its end nearer than it folds\n");
	const LimbSolve s = SolveLimb(Arm(NiPoint3{3.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -1.0f}));
	const float folded = kUpper - kLower;
	Check(s.valid && !s.reached, "valid, not reached");
	Check(Near(Distance(s.end, NiPoint3{0, 0, 0}), folded + 0.001f * (kUpper + kLower), 0.01f),
	      "folded as far as it goes, along the line to it");
	Check(Near(Distance(s.joint, NiPoint3{0, 0, 0}), kUpper) && Near(Distance(s.end, s.joint), kLower),
	      "the bones their own lengths");
}

void TestLimbNothing() {
	std::printf("the limb with nothing to go by\n");
	const NiPoint3 pole{0.0f, 0.0f, -1.0f};
	Check(!SolveLimb(Arm(NiPoint3{0.0f, 0.0f, 0.0f}, pole)).valid, "the target on the root: no solve");
	LimbInput zero = Arm(NiPoint3{10.0f, 0.0f, 0.0f}, pole);
	zero.upperLength = 0.0f;
	Check(!SolveLimb(zero).valid, "a bone of no length: no solve");
	zero = Arm(NiPoint3{10.0f, 0.0f, 0.0f}, pole);
	zero.lowerLength = -1.0f;
	Check(!SolveLimb(zero).valid, "a bone of negative length: no solve");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!SolveLimb(Arm(NiPoint3{nan, 0.0f, 0.0f}, pole)).valid, "a target of no number: no solve");
	Check(!SolveLimb(Arm(NiPoint3{10.0f, 0.0f, 0.0f}, NiPoint3{0.0f, nan, 0.0f})).valid, "a pole of no number: no solve");
	// A pole along the line: some direction across it, the end still reached.
	const LimbSolve along = SolveLimb(Arm(NiPoint3{0.0f, 20.0f, 0.0f}, NiPoint3{0.0f, 5.0f, 0.0f}));
	float angle = 0.0f;
	Check(along.valid && along.reached && PureZ(along.upperRot, along.lowerRot, angle) && angle < 0.0f,
	      "a pole along the line: bent across it all the same, reached");
	const LimbSolve up = SolveLimb(Arm(NiPoint3{0.0f, 0.0f, 20.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(up.valid && up.reached, "a pole along a vertical line: the other helper, reached");
}

void TestLimbOnTheRig() {
	std::printf("the limb on the skeleton as measured\n");
	// The left arm of the idle pose (Debug.BodyTreeProbe, 2026-10-11): the
	// solve put through the same three points gives the same elbow and the
	// same fold the animation has, -32.5 degrees about z.
	const NiPoint3 shoulder{244.9f, -1679.8f, -156.3f};
	const NiPoint3 elbow{237.8f, -1679.0f, -176.9f};
	const NiPoint3 wrist{238.5f, -1672.3f, -190.1f};
	LimbInput in;
	in.root = shoulder;
	in.upperLength = Distance(elbow, shoulder);
	in.lowerLength = Distance(wrist, elbow);
	in.target = wrist;
	in.pole = elbow - shoulder;
	in.softShare = 0.0f;
	const LimbSolve s = SolveLimb(in);
	float angle = 0.0f;
	Check(s.valid && s.reached && NearPoint(s.joint, elbow, 0.05f), "the animation's elbow found again (solved exactly)");
	Check(PureZ(s.upperRot, s.lowerRot, angle) && Near(angle, -32.5f * kDeg, 0.6f * kDeg),
	      "the forearm folded -32.5 degrees about the upper arm's z, as the probe read it");
	// The same hanging arm, 96 % of the reach, with the reach softened: the
	// elbow a little more bent, the bones grown a little, the wrist reached.
	in.softShare = kLimbSoftShare;
	const LimbSolve soft = SolveLimb(in);
	Check(soft.valid && soft.reached && soft.stretch > 1.0f && soft.stretch < 1.03f &&
	          PureZ(soft.upperRot, soft.lowerRot, angle) && angle < -32.5f * kDeg && angle > -50.0f * kDeg,
	      "softened: the wrist reached, the bones grown under 3 %, the elbow bent more");
	// The left leg, its foot lifted 20 units: the knee goes forward.
	const NiPoint3 hip{249.4f, -1679.6f, -191.6f};
	const NiPoint3 knee{247.9f, -1674.6f, -224.9f};
	const NiPoint3 ankle{247.5f, -1678.8f, -251.9f};
	LimbInput leg;
	leg.root = hip;
	leg.upperLength = Distance(knee, hip);
	leg.lowerLength = Distance(ankle, knee);
	leg.target = ankle + NiPoint3{0.0f, 0.0f, 20.0f};
	leg.pole = KneePole(-1.0f, NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f});
	const LimbSolve l = SolveLimb(leg);
	Check(l.valid && l.reached && l.joint.y > hip.y + 10.0f, "a foot lifted 20 units: the knee well forward");
	Check(PureZ(l.upperRot, l.lowerRot, angle) && angle < -60.0f * kDeg, "and the calf folded back under the thigh");
}

// The body's axes for the pole tests: right +x, forward +y, up +z.
const NiPoint3 kRight{1.0f, 0.0f, 0.0f};
const NiPoint3 kForward{0.0f, 1.0f, 0.0f};
const NiPoint3 kUp{0.0f, 0.0f, 1.0f};

// The test body: shoulders at x +-10, the middle at the origin, the pelvis
// 37 below.
TorsoKeepOut KeepOutBody() {
	TorsoKeepOut t;
	t.mid = NiPoint3{0.0f, 0.0f, 0.0f};
	t.shoulderLateral = 10.0f;
	t.shoulderZ = 0.0f;
	t.pelvisZ = -37.0f;
	return t;
}

LimbSolve ArmTo(float side, const NiPoint3& shoulder, const NiPoint3& wrist, const NiPoint3& handAlong,
                const TorsoKeepOut& torso = KeepOutBody()) {
	LimbInput in;
	in.root = shoulder;
	in.upperLength = kUpper;
	in.lowerLength = kLower;
	in.target = wrist;
	in.pole = ElbowPole(side, kRight, kForward, kUp, shoulder, wrist, handAlong, kLower);
	in.maxStretch = 1.15f;
	in.pole = ElbowClearOfTorso(in, side, torso, 0.0f).pole;
	return SolveLimb(in);
}

// How far a point is below (negative) or above the line from a to b, along up.
float AboveLine(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	NiPoint3 u;
	ik::Unit(b - a, u);
	const NiPoint3 off = (p - a) - u * ik::Dot(p - a, u);
	return ik::Dot(off, kUp);
}

void TestElbow() {
	std::printf("where the elbow goes\n");
	// A sword held before the chest, the knuckles forward.
	const NiPoint3 rightShoulder{10.0f, 0.0f, 0.0f};
	const NiPoint3 swordWrist{15.0f, 25.0f, -12.0f};
	const LimbSolve sword = ArmTo(1.0f, rightShoulder, swordWrist, kForward);
	Check(sword.valid && sword.reached, "a sword before the chest: reached");
	Check(AboveLine(sword.joint, rightShoulder, swordWrist) < -3.0f, "the elbow below the line to the hand");
	Check(sword.joint.x > 12.0f, "and out to the right");
	Check(sword.joint.y < swordWrist.y, "and behind the wrist");
	// The same with the left arm: mirrored.
	const NiPoint3 leftShoulder{-10.0f, 0.0f, 0.0f};
	const LimbSolve left = ArmTo(-1.0f, leftShoulder, NiPoint3{-15.0f, 25.0f, -12.0f}, kForward);
	Check(left.valid && left.joint.x < -12.0f && AboveLine(left.joint, leftShoulder, NiPoint3{-15.0f, 25.0f, -12.0f}) < -3.0f,
	      "the left arm: out to the left, below");
	// A hand raised over the head, the fingers up: the elbow below the hand,
	// out to the side.
	const NiPoint3 high{12.0f, 6.0f, 30.0f};
	const LimbSolve raised = ArmTo(1.0f, rightShoulder, high, kUp);
	Check(raised.valid && raised.reached && raised.joint.z < high.z - 5.0f && raised.joint.x > 10.5f,
	      "a hand over the head: the elbow below it, out");
	// Across the chest to the other shoulder, the fingers pointing left: the
	// elbow before the chest, below the shoulder.
	const NiPoint3 across{-6.0f, 14.0f, -2.0f};
	const LimbSolve crossed = ArmTo(1.0f, rightShoulder, across, NiPoint3{-1.0f, 0.0f, 0.0f});
	Check(crossed.valid && crossed.reached && crossed.joint.y > 3.0f && crossed.joint.z < -3.0f,
	      "across the chest: the elbow before it, low");
	// The harness's "across" (2026-10-11): the right hand before the left
	// shoulder, its axis forward and left - the forearm "straight behind the
	// wrist" lies inside the chest. The elbow stays in front of the chest,
	// below the shoulder, never behind it.
	const NiPoint3 harnessShoulder{266.0f, -1671.7f, -154.3f};
	const NiPoint3 harnessHand{247.1f, -1661.4f, -145.4f};
	NiPoint3 forwardLeft;
	ik::Unit(NiPoint3{-0.53f, 0.85f, 0.0f}, forwardLeft);
	TorsoKeepOut harnessTorso;
	harnessTorso.mid = NiPoint3{255.5f, -1672.05f, -154.35f};
	harnessTorso.shoulderLateral = 10.5f;
	harnessTorso.shoulderZ = -154.35f;
	harnessTorso.pelvisZ = -191.7f;
	const LimbSolve harness = ArmTo(1.0f, harnessShoulder, harnessHand, forwardLeft, harnessTorso);
	Check(harness.valid && harness.reached && harness.joint.y > harnessShoulder.y - 1.0f &&
	          harness.joint.z < harnessShoulder.z,
	      "across with the hand's axis forward-left: the elbow before the chest and below the shoulder, not in it");
	// Hanging at the side, the fingers down: nearly straight, the elbow back.
	const NiPoint3 hip{13.0f, 1.0f, -34.0f};
	const LimbSolve hanging = ArmTo(1.0f, rightShoulder, hip, NiPoint3{0.0f, 0.0f, -1.0f});
	Check(hanging.valid && hanging.reached && hanging.joint.y < 1.0f, "hanging at the side: the elbow not forward");
	// The hint along the line (the hand pointing along the arm): the body's
	// habit alone.
	const NiPoint3 ahead{10.0f, 34.0f, 0.0f};
	const NiPoint3 pole = ElbowPole(1.0f, kRight, kForward, kUp, rightShoulder, ahead, kForward, 34.0f);
	Check(pole.z < -0.5f, "the hand's hint on the line: the body's way - down");
	// Nothing across the line from either: down.
	const NiPoint3 down = ElbowPole(1.0f, kRight, kForward, kUp, rightShoulder, rightShoulder, kForward, kLower);
	Check(NearPoint(down, NiPoint3{0.0f, 0.0f, -1.0f}), "the wrist on the shoulder: straight down");
	// The body's habit along the line, the hand's across it: the hand's alone.
	const NiPoint3 bodyLine = ElbowPole(1.0f, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, kUp,
	                                    NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -30.0f}, kForward, kLower);
	Check(bodyLine.y < -0.9f, "the body's habit on the line: the hand's hint alone (behind)");
	// Both on the line: down.
	const NiPoint3 both = ElbowPole(1.0f, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, kUp,
	                                NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -30.0f}, kUp, 30.0f);
	Check(NearPoint(both, NiPoint3{0.0f, 0.0f, -1.0f}), "both on the line: straight down");
	// The hand's hint fades as the hand comes to point along the arm: a hand
	// held out ahead, its fingers turned 5 degrees off the arm's line one way
	// and the other, moves the elbow a few degrees, not from one side to the
	// other (the first version made the sliver across the line unit, and the
	// elbow flipped: the tester, 2026-10-12, "die ellenbogen wechseln zu
	// schnell beim drehen des handgelenks").
	const NiPoint3 held{10.0f, 30.0f, -5.0f};
	NiPoint3 along;
	ik::Unit(held - rightShoulder, along);
	const NiPoint3 sideways = ik::Cross(along, kUp);  // across the arm
	NiPoint3 aLittleRight;
	NiPoint3 aLittleLeft;
	ik::Unit(along + sideways * 0.09f, aLittleRight);
	ik::Unit(along - sideways * 0.09f, aLittleLeft);
	const NiPoint3 poleRight = ElbowPole(1.0f, kRight, kForward, kUp, rightShoulder, held, aLittleRight, kLower);
	const NiPoint3 poleLeft = ElbowPole(1.0f, kRight, kForward, kUp, rightShoulder, held, aLittleLeft, kLower);
	const float swing = ik::Acos(ik::Dot(poleRight, poleLeft)) / kDeg;
	Check(swing < 12.0f, "the fingers 5 degrees either side of the arm's line: the elbow's direction within 12 degrees");
	Check(poleRight.z < -0.6f && poleLeft.z < -0.6f, "and down, the body's way, both times");
	// Turned well across (90 degrees), the hand's hint counts in full.
	const NiPoint3 poleAcross = ElbowPole(1.0f, kRight, kForward, kUp, rightShoulder, held, sideways, kLower);
	Check(ik::Acos(ik::Dot(poleAcross, poleRight)) / kDeg > 20.0f, "the fingers turned right across the arm: the elbow sent well round");
}

void TestEase() {
	std::printf("the elbow's ease\n");
	const NiPoint3 down{0.0f, 0.0f, -1.0f};
	const NiPoint3 out{1.0f, 0.0f, 0.0f};
	Check(NearPoint(EasedDirection(down, out, 0.0f), down), "no share: where it was");
	Check(NearPoint(EasedDirection(down, out, 1.0f), out), "the whole share: the new direction");
	const NiPoint3 half = EasedDirection(down, out, 0.5f);
	Check(Near(ik::Length(half), 1.0f, 1e-4f) && Near(half.x, -half.z, 1e-4f) && half.x > 0.5f, "half: between, unit");
	Check(NearPoint(EasedDirection(down, down * -1.0f, 0.5f), down * -1.0f), "opposite directions: the new one whole");
	Check(NearPoint(EasedDirection(down, out, 2.0f), out) && NearPoint(EasedDirection(down, out, -1.0f), down),
	      "a share past 0..1 held to it");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(NearPoint(EasedDirection(NiPoint3{nan, 0.0f, 0.0f}, out, 0.5f), out), "a last direction of no number: the new one");
	Check(EaseShare(0.0f, 0.05f) == 1.0f && EaseShare(-1.0f, 0.05f) == 1.0f, "no time, or time run backwards: all of the way");
	Check(EaseShare(1.0f / 90.0f, 0.0f) == 1.0f, "no time constant: all of the way");
	Check(EaseShare(2.0f, 0.05f) == 1.0f, "a frame of seconds: all of the way");
	const float share = EaseShare(1.0f / 90.0f, 0.05f);
	Check(share > 0.18f && share < 0.22f, "a 90 Hz frame against 50 ms: a fifth of the way");
	Check(EaseShare(1.0f / 90.0f, nan) == 1.0f, "a constant of no number: all of the way");
}

void TestRollMemory() {
	std::printf("the rolls continued\n");
	const float pi = obvr::math::kPi;
	Check(Near(UnwrapAngle(3.0f, -3.0f), 3.0f + obvr::math::WrapAngle(-6.0f), 1e-5f),
	      "from 3.0 to -3.0: the short step on past the half turn, 3.28");
	Check(Near(UnwrapAngle(0.2f, 0.5f), 0.5f, 1e-6f), "a small step: the new angle");
	Check(Near(UnwrapAngle(-3.0f, 3.0f), -3.0f + obvr::math::WrapAngle(6.0f), 1e-5f), "and the other way round: -3.28");
	Check(Near(HalfOfRoll(1.0f), 0.5f, 1e-6f), "half a roll");
	Check(Near(HalfOfRoll(3.1f), 0.5f * kTwistMostRadians, 1e-5f) && Near(HalfOfRoll(-3.1f), -0.5f * kTwistMostRadians, 1e-5f),
	      "past 150 degrees: held there, either way");
	Check(Near(HoldRollMemory(1.0f), 1.0f, 1e-6f), "a roll within the memory's hold: kept");
	Check(Near(HoldRollMemory(10.0f), kRollMemoryMostRadians, 1e-5f) && Near(HoldRollMemory(-10.0f), -kRollMemoryMostRadians, 1e-5f),
	      "spun round and round: held a bit past the clamp, not run away");
	Check(kRollMemoryMostRadians > kTwistMostRadians && kRollMemoryMostRadians < pi + 1.0f,
	      "(the memory's hold past the clamp, short of a turn)");
	// A roll continued from the hold comes straight back.
	const float held = HoldRollMemory(10.0f);
	Check(UnwrapAngle(held, obvr::math::WrapAngle(held) - 0.3f) < held, "rolled back from the hold: the memory falls at once");
}

// The hand's roll spread over the forearm, its twist bone and the hand (the
// tester, 2026-10-12: the wrist pinched with the hands turned thumb down, the
// palms out).
void TestForearmRolls() {
	std::printf("the forearm's roll spread\n");
	const float deg = obvr::math::kDegreesToRadians;
	const ForearmRolls r90 = ForearmRollsFor(90.0f * deg);
	Check(Near(r90.forearm, 30.0f * deg, 1e-4f) && Near(r90.twist, 60.0f * deg, 1e-4f), "90 degrees: 30 and 60");
	const ForearmRolls rNeg = ForearmRollsFor(-120.0f * deg);
	Check(Near(rNeg.forearm, -40.0f * deg, 1e-4f) && Near(rNeg.twist, -80.0f * deg, 1e-4f), "and the other way");
	const ForearmRolls r200 = ForearmRollsFor(200.0f * deg);
	Check(Near(r200.forearm, 200.0f / 3.0f * deg, 1e-4f) && Near(r200.twist, 400.0f / 3.0f * deg, 1e-4f),
	      "past a half turn (the left hand's rest at -110 and a half turn more): still followed, not held at 150");
	const ForearmRolls rFar = ForearmRollsFor(400.0f * deg);
	Check(Near(rFar.forearm, kForeRollMostRadians / 3.0f, 1e-4f) &&
	          Near(rFar.twist, kForeRollMostRadians * 2.0f / 3.0f, 1e-4f),
	      "past three quarters of a turn: held there");
	const ForearmRolls rNan = ForearmRollsFor(std::numeric_limits<float>::quiet_NaN());
	Check(rNan.forearm == 0.0f && rNan.twist == 0.0f, "no number: none");
	Check(ForearmRollsFor(0.0f).forearm == 0.0f && ForearmRollsFor(0.0f).twist == 0.0f, "no roll: none");
	// No ring of the skin across more than a third: neighbours a third apart.
	const ForearmRolls half = ForearmRollsFor(180.0f * deg);
	Check(Near(half.twist - half.forearm, 60.0f * deg, 1e-4f) && Near(180.0f * deg - half.twist, 60.0f * deg, 1e-4f),
	      "a half turn: the forearm, its twist and the hand 60 degrees apart each");
	// The memory: past the range a little, not run away.
	Check(Near(HoldForeRollMemory(4.0f), 4.0f, 1e-6f), "a roll within the hold: kept");
	Check(Near(HoldForeRollMemory(20.0f), kForeRollMemoryMostRadians, 1e-5f) &&
	          Near(HoldForeRollMemory(-20.0f), -kForeRollMemoryMostRadians, 1e-5f),
	      "spun round and round: held a bit past the range");
	// A roll continued the short way past a half turn keeps counting.
	float roll = -110.0f * deg;
	for (int step = 0; step < 9; ++step) {
		roll = HoldForeRollMemory(UnwrapAngle(roll, obvr::math::WrapAngle(roll - 20.0f * deg)));
	}
	Check(Near(roll, -290.0f * deg, 1e-3f), "from -110 turned on 20 degrees at a time to -290: followed the whole way");
}

void TestTorsoKeepOut() {
	std::printf("the elbow kept out of the torso\n");
	const TorsoKeepOut t = KeepOutBody();
	Check(ElbowInsideTorso(NiPoint3{0.0f, 0.0f, -10.0f}, 1.0f, t), "in the middle, below the shoulders: inside");
	Check(!ElbowInsideTorso(NiPoint3{12.0f, 0.0f, -10.0f}, 1.0f, t), "out past its shoulder: outside");
	Check(!ElbowInsideTorso(NiPoint3{8.0f, 0.0f, -10.0f}, 1.0f, t), "beside the ribs (2 in from the shoulder): outside");
	Check(ElbowInsideTorso(NiPoint3{5.0f, 0.0f, -10.0f}, 1.0f, t), "5 in from the shoulder: inside");
	Check(!ElbowInsideTorso(NiPoint3{5.0f, 4.0f, -10.0f}, 1.0f, t), "in front of the chest: outside");
	Check(!ElbowInsideTorso(NiPoint3{5.0f, -9.0f, -10.0f}, 1.0f, t), "behind the back: outside");
	Check(!ElbowInsideTorso(NiPoint3{5.0f, 0.0f, -40.0f}, 1.0f, t), "below the pelvis: outside");
	Check(ElbowInsideTorso(NiPoint3{5.0f, 0.0f, 7.0f}, 1.0f, t), "7 above the shoulders, inwards: in the shoulder caps, inside");
	Check(!ElbowInsideTorso(NiPoint3{5.0f, 0.0f, 12.0f}, 1.0f, t), "12 above them: outside");
	Check(ElbowInsideTorso(NiPoint3{-5.0f, 0.0f, -10.0f}, -1.0f, t), "the left arm 5 in from its shoulder: inside");
	Check(ElbowInsideTorso(NiPoint3{5.0f, 0.0f, -10.0f}, -1.0f, t), "the left arm's elbow across on the right: inside");
	Check(!ElbowInsideTorso(NiPoint3{-12.0f, 0.0f, -10.0f}, -1.0f, t), "the left arm's out past its shoulder: outside");

	// The clearance: how far outside, continuous across the faces.
	Check(Near(TorsoClearance(NiPoint3{12.0f, 0.0f, -10.0f}, 1.0f, t), 5.0f, 1e-4f), "2 past the shoulder: 5 clear of the side (3 inwards is the side)");
	Check(Near(TorsoClearance(NiPoint3{5.0f, 4.0f, -10.0f}, 1.0f, t), 2.0f, 1e-4f), "4 before the chest: 2 clear of its front");
	Check(Near(TorsoClearance(NiPoint3{5.0f, 0.0f, -10.0f}, 1.0f, t), -2.0f, 1e-4f), "5 in from the shoulder: 2 inside (the nearest face)");
	Check(TorsoClearance(NiPoint3{5.0f, 0.0f, -40.0f}, 1.0f, t) > 0.0f, "below the pelvis: outside");

	// A limb whose elbow is outside: the pole as it was.
	LimbInput fine;
	fine.root = NiPoint3{10.0f, 0.0f, 0.0f};
	fine.upperLength = kUpper;
	fine.lowerLength = kLower;
	fine.target = NiPoint3{15.0f, 25.0f, -12.0f};
	fine.pole = NiPoint3{0.4f, -0.3f, -1.0f};
	const ElbowClear kept = ElbowClearOfTorso(fine, 1.0f, t, 0.0f);
	Check(!kept.moved && NearPoint(kept.pole, fine.pole) && kept.sign == 0.0f, "the elbow outside already: the pole kept");
	// A pole that sends it inside: moved round to the nearest place outside,
	// and clear of it by the margin.
	LimbInput bad = fine;
	bad.target = NiPoint3{-8.0f, 10.0f, 6.0f};
	bad.pole = NiPoint3{-0.3f, -0.6f, -0.3f};
	const LimbSolve before = SolveLimb(bad);
	Check(before.valid && ElbowInsideTorso(before.joint, 1.0f, t), "(the set-up: that pole puts the elbow in the chest)");
	const ElbowClear cleared = ElbowClearOfTorso(bad, 1.0f, t, 0.0f);
	bad.pole = cleared.pole;
	const LimbSolve after = SolveLimb(bad);
	Check(cleared.moved && after.valid && after.reached && TorsoClearance(after.joint, 1.0f, t) >= kElbowClearUnits - 0.01f,
	      "moved round: the hand still reached, the elbow outside by the margin");
	Check(cleared.angle > 0.0f && cleared.angle < 3.0f && (cleared.sign == 1.0f || cleared.sign == -1.0f),
	      "the turn and its way round answered");
	// Refined, not stepped: the hand moved a little moves the elbow a little.
	LimbInput near = bad;
	near.pole = NiPoint3{-0.3f, -0.6f, -0.3f};
	near.target = bad.target + NiPoint3{0.3f, 0.0f, 0.0f};
	const ElbowClear nearby = ElbowClearOfTorso(near, 1.0f, t, 0.0f);
	Check(nearby.moved && nearby.sign == cleared.sign && Near(nearby.angle, cleared.angle, 3.0f * kDeg),
	      "the hand 0.3 units over: the turn within 3 degrees of before (no 10 degree step)");
	// Nothing to solve: the pole as it was.
	LimbInput none = fine;
	none.target = none.root;
	const ElbowClear same = ElbowClearOfTorso(none, 1.0f, t, 0.0f);
	Check(!same.moved && NearPoint(same.pole, fine.pole), "no solve: the pole kept");
	// Every elbow inside (the whole circle within the torso's depth, its
	// width and height made huge): the pole kept, not moved.
	TorsoKeepOut everywhere = t;
	everywhere.shoulderLateral = 1000.0f;
	everywhere.shoulderZ = 1000.0f;
	everywhere.pelvisZ = -1000.0f;
	LimbInput along;
	along.root = NiPoint3{0.0f, -15.0f, 0.0f};
	along.upperLength = kUpper;
	along.lowerLength = kLower;
	along.target = NiPoint3{0.0f, 5.0f, 0.0f};
	along.pole = NiPoint3{0.0f, 0.0f, -1.0f};
	const ElbowClear stuck = ElbowClearOfTorso(along, 1.0f, everywhere, 0.0f);
	Check(!stuck.moved && NearPoint(stuck.pole, along.pole), "every elbow inside: the pole kept");

	// The way round kept: the right hand low before the chest, the pole sent
	// straight inwards - the elbow in the side, a way out forward (the chest's
	// front) and one back, a few degrees apart.
	LimbInput side;
	side.root = NiPoint3{10.0f, 0.0f, 0.0f};
	side.upperLength = kUpper;
	side.lowerLength = kLower;
	side.target = NiPoint3{10.0f, -2.0f, -20.0f};
	side.pole = NiPoint3{-1.0f, 0.0f, 0.0f};
	const LimbSolve inSide = SolveLimb(side);
	Check(inSide.valid && ElbowInsideTorso(inSide.joint, 1.0f, t), "(the set-up: the elbow in the side)");
	const ElbowClear shorter = ElbowClearOfTorso(side, 1.0f, t, 0.0f);
	const ElbowClear other = ElbowClearOfTorso(side, 1.0f, t, -shorter.sign);
	const ElbowClear kept2 = ElbowClearOfTorso(side, 1.0f, t, shorter.sign);
	Check(shorter.moved && other.moved && kept2.moved, "moved round every time");
	Check(shorter.angle > 10.0f * kDeg && shorter.angle < 60.0f * kDeg && other.angle >= shorter.angle,
	      "the shorter way first (10-60 degrees round)");
	Check(other.sign == -shorter.sign && other.angle - shorter.angle < kElbowSideHoldRadians,
	      "the way it went last frame kept while the other is not 30 degrees shorter");
	Check(kept2.sign == shorter.sign && Near(kept2.angle, shorter.angle, 1e-5f), "preferred the shorter: the shorter");
	// A way preferred that is far longer: the shorter wins. The torso's back
	// pushed far back: the way back round is far longer than the way forward.
	TorsoKeepOut deep = t;
	deep.forward = NiPoint3{0.0f, 1.0f, 0.0f};
	LimbInput sideDeep = side;
	sideDeep.target = NiPoint3{10.0f, -1.0f, -20.0f};
	const ElbowClear deepShort = ElbowClearOfTorso(sideDeep, 1.0f, deep, 0.0f);
	bool longerFound = false;
	float longerAngle = 0.0f;
	{
		// The other way's own angle: asked for with a huge hold, it is given.
		const ElbowClear longer = ElbowClearOfTorso(sideDeep, 1.0f, deep, -deepShort.sign);
		longerFound = longer.moved;
		longerAngle = longer.angle;
	}
	Check(deepShort.moved && longerFound, "(both ways found)");
	if (longerAngle - deepShort.angle >= kElbowSideHoldRadians) {
		Check(ElbowClearOfTorso(sideDeep, 1.0f, deep, -deepShort.sign).sign == deepShort.sign,
		      "the other way 30 degrees longer: the shorter wins over the preference");
	} else {
		Check(true, "(the two ways within 30 degrees here: the hold case above covers it)");
	}
}

// A point through a transform, the engine's way.
NiPoint3 Through(const obvr::NiTransform& t, const NiPoint3& p) { return t.pos + t.rot * (p * t.scale); }

obvr::NiTransform Compose(const obvr::NiTransform& parent, const obvr::NiTransform& local) {
	obvr::NiTransform w;
	w.rot = parent.rot * local.rot;
	w.pos = parent.pos + parent.rot * (local.pos * parent.scale);
	w.scale = parent.scale * local.scale;
	return w;
}

void TestCarried() {
	std::printf("the shield carried with the hand\n");
	obvr::NiTransform oldParent;
	oldParent.rot = ik::TurnAbout(NiPoint3{0.0f, 0.0f, 1.0f}, 0.4f) * ik::TurnAbout(NiPoint3{1.0f, 0.0f, 0.0f}, -0.3f);
	oldParent.pos = NiPoint3{10.0f, -5.0f, 3.0f};
	oldParent.scale = 1.0f;
	obvr::NiTransform newParent;
	newParent.rot = ik::TurnAbout(NiPoint3{0.0f, 1.0f, 0.0f}, 0.9f);
	newParent.pos = NiPoint3{12.0f, -2.0f, 1.0f};
	newParent.scale = 1.0f;
	obvr::NiTransform child;
	child.rot = ik::TurnAbout(NiPoint3{1.0f, 0.0f, 0.0f}, 1.2f);
	child.pos = NiPoint3{2.0f, 7.0f, -1.0f};
	child.scale = 1.0f;
	const obvr::NiTransform before = Compose(oldParent, child);
	const obvr::NiTransform after = Compose(newParent, CarriedLocal(oldParent, newParent, child));
	const NiPoint3 corner{4.0f, -3.0f, 6.0f};
	Check(NearPoint(Through(before, corner), Through(after, corner), 1e-3f) && NearPoint(after.pos, before.pos, 1e-3f),
	      "under the new parent: every point of it where it was");
	Check(IsRotation(after.rot) && Near(after.scale, 1.0f, 1e-5f), "turned by a true rotation, its size kept");
	// Scales: a parent of scale 2, the new one of 0.5.
	oldParent.scale = 2.0f;
	newParent.scale = 0.5f;
	const obvr::NiTransform scaledBefore = Compose(oldParent, child);
	const obvr::NiTransform scaledAfter = Compose(newParent, CarriedLocal(oldParent, newParent, child));
	Check(NearPoint(Through(scaledBefore, corner), Through(scaledAfter, corner), 1e-3f) &&
	          Near(scaledAfter.scale, scaledBefore.scale, 1e-4f),
	      "parents of other sizes: still where it was, its world size kept");
	// A new parent whose rotation carries a race's height (the third column
	// 1.10 long): every point of the child still where it was.
	obvr::NiTransform tallParent = newParent;
	for (int row = 0; row < 3; ++row) {
		tallParent.rot.data[row][2] *= 1.1f;
	}
	const obvr::NiTransform underTall = Compose(tallParent, CarriedLocal(oldParent, tallParent, child));
	Check(NearPoint(Through(scaledBefore, corner), Through(underTall, corner), 1e-3f),
	      "a parent carrying a race's height: still where it was");
	// A new parent of no size is taken as size one, not a division by zero.
	newParent.scale = 0.0f;
	const obvr::NiTransform zero = CarriedLocal(oldParent, newParent, child);
	Check(ik::Finite(zero.pos) && ik::Finite(zero.scale), "a parent of no size: a finite answer");
	// The same parent: the child as it was.
	newParent = oldParent;
	const obvr::NiTransform same = CarriedLocal(oldParent, newParent, child);
	Check(NearPoint(same.pos, child.pos, 1e-4f) && Near(same.scale, child.scale, 1e-5f) &&
	          NearPoint(ik::Column(same.rot, 0), ik::Column(child.rot, 0), 1e-5f),
	      "the parent where it was: the child's own transform back");
}

void TestClavicle() {
	std::printf("the clavicle\n");
	Check(ClavicleShare(0.6f, 0.0f) == 0.0f, "a hand within two thirds of the reach, not raised: no share");
	Check(Near(ClavicleShare(1.05f, 0.0f), 0.5f, 1e-4f), "at the full reach: half");
	Check(Near(ClavicleShare(0.0f, 0.8f), 0.6f, 1e-4f), "four fifths of an arm up: 0.6");
	Check(Near(ClavicleShare(1.05f, 0.8f), 0.6f, 1e-4f), "both: the larger");
	Check(ClavicleShare(0.85f, 0.0f) > 0.1f && ClavicleShare(0.85f, 0.0f) < 0.4f, "between: some");
	const NiPoint3 clavicle{2.0f, 0.0f, 0.0f};
	const NiPoint3 along{1.0f, 0.0f, 0.0f};
	const NiPoint3 shoulder{10.0f, 0.0f, 0.0f};
	const float arm = kUpper + kLower;
	// Close and low: nothing.
	const ClavicleTurn close = TurnClavicle(clavicle, along, shoulder, NiPoint3{14.0f, 10.0f, -15.0f}, arm, kUp);
	Check(close.angle == 0.0f, "a hand close and low: no turn");
	// Far ahead: towards it, half the angle, at most 40 degrees.
	const NiPoint3 far{12.0f, 37.0f, 0.0f};
	const ClavicleTurn ahead = TurnClavicle(clavicle, along, shoulder, far, arm, kUp);
	Check(ahead.angle > 0.0f && ahead.angle <= 0.5f * 40.0f * kDeg + 1e-4f, "far ahead: turned, half of 40 at most");
	NiPoint3 towards;
	ik::Unit(far - clavicle, towards);
	const NiPoint3 turned = ik::TurnAbout(ahead.axis, ahead.angle) * along;
	Check(ik::Dot(turned, towards) > ik::Dot(along, towards), "the shoulder turned towards the hand");
	// High: lifted.
	const ClavicleTurn high = TurnClavicle(clavicle, along, shoulder, NiPoint3{12.0f, 0.0f, 30.0f}, arm, kUp);
	const NiPoint3 lifted = ik::TurnAbout(high.axis, high.angle) * along;
	Check(high.angle > 0.0f && lifted.z > 0.1f, "a hand up high: the shoulder lifted");
	// Nothing to go by.
	Check(TurnClavicle(clavicle, along, shoulder, clavicle, arm, kUp).angle == 0.0f, "the hand on the clavicle: none");
	Check(TurnClavicle(clavicle, along, shoulder, far, 0.0f, kUp).angle == 0.0f, "an arm of no length: none");
	Check(TurnClavicle(clavicle, NiPoint3{0.0f, 0.0f, 0.0f}, shoulder, far, arm, kUp).angle == 0.0f,
	      "a clavicle of no direction: none");
	Check(TurnClavicle(clavicle, along, shoulder, NiPoint3{60.0f, 0.0f, 0.0f}, arm, kUp).angle == 0.0f,
	      "a hand straight along it, far: no axis to turn about");
}

void TestTwist() {
	std::printf("the twist\n");
	Check(Near(TwistAboutX(TurnAboutX(0.7f)), 0.7f, 1e-4f), "a turn about x: all twist");
	Check(Near(TwistAboutX(TurnAboutX(-0.7f)), -0.7f, 1e-4f), "and the other way");
	Check(Near(TwistAboutX(ik::TurnAbout(kUp, 0.5f)), 0.0f, 1e-4f), "a turn about z: no twist");
	Check(Near(TwistAboutX(ik::TurnAbout(kUp, 0.4f) * TurnAboutX(0.6f)), 0.6f, 1e-4f),
	      "a swing after a twist: the twist (the trace's case)");
	Check(Near(TwistAboutX(ik::TurnAbout(NiPoint3{0.0f, 1.0f, 0.0f}, 2.5f) * TurnAboutX(0.3f)), 0.3f, 1e-4f),
	      "a big swing about y: the twist (y's case)");
	Check(Near(TwistAboutX(ik::TurnAbout(kUp, 2.5f) * TurnAboutX(0.3f)), 0.3f, 1e-4f),
	      "a big swing about z: the twist (z's case)");
	const float half = TwistAboutX(TurnAboutX(obvr::math::kPi));
	Check(Near(half < 0.0f ? -half : half, obvr::math::kPi, 1e-3f),
	      "half a turn about x: a half turn either way, the same rotation (x's case)");
	Check(TwistAboutX(ik::TurnAbout(NiPoint3{0.0f, 1.0f, 0.0f}, obvr::math::kPi)) == 0.0f,
	      "half a turn about y: no twist to speak of");
	Check(Near(HalfRoll(0.0f, 1.0f), 0.5f, 1e-5f), "half a change of roll");
	Check(Near(HalfRoll(3.0f, -3.0f), 0.5f * obvr::math::WrapAngle(-6.0f), 1e-4f), "the change the short way round");
	Check(Near(HalfRoll(0.0f, 3.0f), 0.5f * kTwistMostRadians, 1e-4f), "past 150 degrees: held there");
}

void TestTorso() {
	std::printf("the torso after the head\n");
	const TorsoYawSettings set = TorsoYawFrom(40.0f, 1.0f);
	Check(Near(set.freeRadians, 40.0f * kDeg, 1e-5f) && set.followSeconds == 1.0f, "the settings as given");
	const TorsoYawSettings bad = TorsoYawFrom(-5.0f, 20.0f);
	Check(Near(bad.freeRadians, 40.0f * kDeg, 1e-5f) && bad.followSeconds == 1.0f, "out of range: the defaults");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const TorsoYawSettings none = TorsoYawFrom(nan, nan);
	Check(Near(none.freeRadians, 40.0f * kDeg, 1e-5f) && none.followSeconds == 1.0f, "no number: the defaults");
	const TorsoYawSettings zero = TorsoYawFrom(0.0f, 0.0f);
	Check(zero.freeRadians == 0.0f && zero.followSeconds == 0.0f, "zero is zero: no free turn, no lag");

	const float dt = 1.0f / 90.0f;
	TorsoYawState s;
	Check(StepTorsoYaw(s, 0.2f, dt, set) == 0.0f && s.valid, "the first frame: the torso on the head");
	// The head turned 10 degrees a frame to 100: past 40 it drags the torso.
	float rel = 0.0f;
	for (int i = 1; i <= 8; ++i) {
		rel = StepTorsoYaw(s, 0.2f + static_cast<float>(i) * 10.0f * kDeg, dt, set);
	}
	Check(rel < -38.0f * kDeg && rel >= -40.0f * kDeg - 1e-4f, "turned 80 degrees fast: dragged, 40 behind at most");
	// Held there: the torso closes on it.
	for (int i = 0; i < 270; ++i) {
		rel = StepTorsoYaw(s, 0.2f + 80.0f * kDeg, dt, set);
	}
	Check(rel < -0.5f * kDeg && rel > -3.0f * kDeg, "held three seconds: within a few degrees");
	// A glance: 30 degrees for a fifth of a second and back - the torso hardly moves.
	TorsoYawState g;
	StepTorsoYaw(g, 0.0f, dt, set);
	for (int i = 0; i < 18; ++i) {
		StepTorsoYaw(g, 25.0f * kDeg, dt, set);
	}
	const float glance = StepTorsoYaw(g, 0.0f, dt, set);
	Check(glance > 0.0f && glance < 6.0f * kDeg, "a glance aside: the torso a few degrees after it");
	// A jump no head makes: afresh.
	Check(StepTorsoYaw(s, 0.2f + 80.0f * kDeg + 90.0f * kDeg, dt, set) == 0.0f, "a jump of 90 degrees: on the head again");
	// No lag: always on the head.
	TorsoYawState n;
	StepTorsoYaw(n, 0.0f, dt, zero);
	Check(StepTorsoYaw(n, 20.0f * kDeg, dt, zero) == 0.0f, "no free turn, no lag: on the head");
	// No time: the free turn holds, nothing follows.
	TorsoYawState t;
	StepTorsoYaw(t, 0.0f, dt, set);
	Check(Near(StepTorsoYaw(t, 20.0f * kDeg, 0.0f, set), -20.0f * kDeg, 1e-5f), "no time: left where it was");
	Check(Near(StepTorsoYaw(t, 20.0f * kDeg, 2.0f, set), -20.0f * kDeg, 1e-5f), "a frame of seconds: no follow either");
	// Across the half turn: a short step stays short.
	TorsoYawState w;
	StepTorsoYaw(w, 170.0f * kDeg, dt, set);
	const float across = StepTorsoYaw(w, -170.0f * kDeg, dt, set);
	Check(across < 0.0f && across > -20.0f * kDeg, "170 to -170: twenty degrees, not 340");
	// The other way past the free turn.
	TorsoYawState l;
	StepTorsoYaw(l, 0.0f, dt, set);
	for (int i = 1; i <= 7; ++i) {
		rel = StepTorsoYaw(l, -static_cast<float>(i) * 10.0f * kDeg, dt, set);
	}
	Check(rel > 38.0f * kDeg && rel <= 40.0f * kDeg + 1e-4f, "turned the other way: dragged from the other side");
	// No number: the last answer.
	Check(Near(StepTorsoYaw(l, nan, dt, set), rel, 1e-6f), "a head of no number: as last frame");
	TorsoYawState fresh;
	Check(StepTorsoYaw(fresh, nan, dt, set) == 0.0f && !fresh.valid, "no number before any head: nothing");
}

void TestChest() {
	std::printf("the chest\n");
	float yaw = 1.0f;
	Check(ChestYaw(NiPoint3{-10.0f, 0.0f, 5.0f}, NiPoint3{10.0f, 0.0f, 3.0f}, yaw) && Near(yaw, 0.0f, 1e-5f),
	      "shoulders along +x: facing +y, yaw 0");
	Check(ChestYaw(NiPoint3{0.0f, -10.0f, 0.0f}, NiPoint3{0.0f, 10.0f, 0.0f}, yaw) && Near(yaw, 90.0f * kDeg, 1e-4f),
	      "shoulders along +y: yaw 90, HeadingOf's sense");
	Check(!ChestYaw(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.5f, 0.0f, 20.0f}, yaw), "one above the other: no yaw");
	Check(Near(ChestCorrection(0.5f, 0.2f), 0.3f, 1e-5f), "turned back by the twist");
	Check(Near(ChestCorrection(0.0f, 2.0f), -kChestMostRadians, 1e-5f), "held within 75 degrees");
	Check(Near(ChestCorrection(3.0f, -3.0f), obvr::math::WrapAngle(6.0f), 1e-4f), "the short way round");
}

void TestKnees() {
	std::printf("the knees\n");
	Check(FeetLiftUnits(2.0f) == 0.0f, "let down 2 units: calibration, no knee");
	Check(FeetLiftUnits(3.0f) == 0.0f, "3: still none");
	Check(Near(FeetLiftUnits(10.0f), 7.0f, 1e-5f), "10: the feet lifted 7");
	Check(FeetLiftUnits(-5.0f) == 0.0f, "raised: none");
	Check(FeetLiftUnits(std::numeric_limits<float>::quiet_NaN()) == 0.0f, "no number: none");
	Check(Near(FeetLiftUnits(10.0f, 0.0f), 10.0f, 1e-5f), "no free units: all of it");
	Check(NearPoint(KneePole(1.0f, kRight, kForward), NiPoint3{0.15f, 1.0f, 0.0f}), "the right knee forward, a little out");
	Check(NearPoint(KneePole(-1.0f, kRight, kForward), NiPoint3{-0.15f, 1.0f, 0.0f}), "the left one out the other way");
}

void TestFeet() {
	std::printf("the feet put back\n");
	Check(!FootSolveWanted(NiPoint3{0.0f, 0.0f, 2.0f}), "sunk 2 units into the floor: left (the floor hides it)");
	Check(!FootSolveWanted(NiPoint3{0.0f, 0.0f, 3.0f}), "sunk 3: still left");
	Check(FootSolveWanted(NiPoint3{0.0f, 0.0f, 10.0f}), "sunk 10 (a crouch): solved back up");
	Check(FootSolveWanted(NiPoint3{0.0f, 0.0f, -1.0f}), "floating a unit above: solved down");
	Check(!FootSolveWanted(NiPoint3{0.0f, 0.0f, -0.2f}), "a fifth of a unit above: left");
	Check(FootSolveWanted(NiPoint3{0.6f, 0.0f, 0.0f}), "off sideways by more than half a unit: solved");
	Check(!FootSolveWanted(NiPoint3{0.3f, 0.3f, 0.0f}), "off sideways by less: left");
	Check(FootSolveWanted(NiPoint3{0.0f, 0.0f, 1.0f}, 0.0f), "no free units: a unit sunk is solved");
	Check(!FootSolveWanted(NiPoint3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}), "no number: left");
}

void TestTrunk() {
	std::printf("the trunk's pose\n");
	const SInt32 none = -1;
	Check(PlayerStandsIdle(false, none, 0x0000, none), "not sneaking, no action, no step: idle");
	Check(PlayerStandsIdle(false, none, 0x0100, none), "the walk flag alone (no direction): idle");
	Check(!PlayerStandsIdle(true, none, 0x0000, none), "sneaking: not");
	Check(!PlayerStandsIdle(false, 2, 0x0000, none), "an action: not");
	Check(!PlayerStandsIdle(false, none, 0x0001, none), "stepping forward: not");
	Check(!PlayerStandsIdle(false, none, 0x0008, none), "stepping right: not");
	Check(HeadOffsetStands(NiPoint3{0.5f, -1.1f, 112.4f}), "the idle's head offset: standing");
	Check(!HeadOffsetStands(NiPoint3{0.3f, 17.4f, 72.5f}), "the sneak's (72 up, 17 forward): not");
	Check(!HeadOffsetStands(NiPoint3{0.0f, 0.0f, 140.0f}), "140 up: not");
	Check(!HeadOffsetStands(NiPoint3{0.0f, 13.0f, 112.0f}), "13 forward: not");
	Check(!HeadOffsetStands(NiPoint3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 112.0f}), "no number: not");
	float seconds = 0.0f;
	const float dt = 1.0f / 90.0f;
	bool settled = false;
	for (int i = 0; i < 30; ++i) {
		settled = StandingSettled(seconds, true, dt);
	}
	Check(!settled && seconds > 0.3f && seconds < 0.34f, "a third of a second idle: not yet settled");
	for (int i = 0; i < 20; ++i) {
		settled = StandingSettled(seconds, true, dt);
	}
	Check(settled && seconds >= kStandingSettleSeconds, "over half a second: settled");
	Check(!StandingSettled(seconds, false, dt) && seconds == 0.0f, "a step: the count starts over");
	seconds = 1.0f;
	Check(!StandingSettled(seconds, true, 2.0f) && seconds == 0.0f, "a frame of seconds: over, afresh");
	seconds = 1.0f;
	Check(!StandingSettled(seconds, true, std::numeric_limits<float>::quiet_NaN()) && seconds == 0.0f, "no time: over, afresh");
}

void TestGate() {
	std::printf("the gate of an arm\n");
	Check(ArmFollowsHand(true, true, true, false), "on, shown, pinned, own sides: follows");
	Check(!ArmFollowsHand(false, true, true, false), "the arms part off: no");
	Check(!ArmFollowsHand(true, false, true, false), "the body not shown: no");
	Check(!ArmFollowsHand(true, true, false, false), "the hand not pinned this frame: no");
	Check(!ArmFollowsHand(true, true, true, true), "left-handed, the hands swapped: no");
}

void TestHides() {
	std::printf("the body's nodes hidden while an arm follows its hand\n");
	Check(BodyNodeHiddenOnArm("Hand  (00000907)", "Player"), "the hand slot's model: hidden");
	Check(BodyNodeHiddenOnArm("Hand  (0001C9BE)", "Player"), "a glove's model: hidden");
	Check(BodyNodeHiddenOnArm("Hand", "UpperBody  (000651D2)"), "a suit's own Hand shape under its upper body: hidden");
	Check(BodyNodeHiddenOnArm("Hand", "LowerBody  (000347F7)"), "a Hand shape under any slot's model: hidden");
	Check(!BodyNodeHiddenOnArm("Hand", "Player"), "a Hand named without a form's node: left");
	Check(!BodyNodeHiddenOnArm("UpperBody", "UpperBody  (000651D2)"), "the suit's upper body: left");
	Check(!BodyNodeHiddenOnArm("Arms", "UpperBody  (000651D2)"), "the suit's arms: left");
	Check(!BodyNodeHiddenOnArm("Foot", "UpperBody  (000651D2)"), "the suit's feet: left");
	Check(!BodyNodeHiddenOnArm("Handle", "UpperBody  (000651D2)"), "a name that only begins with Hand: left");
	Check(BodyNodeHiddenOnArm("Weapon", "Bip01 R Hand"), "the weapon node on the right hand bone: hidden");
	Check(BodyNodeHiddenOnArm("Torch", "Bip01 L Hand"), "the torch node on the left hand bone: hidden");
	Check(!BodyNodeHiddenOnArm("Bip01 R Finger0", "Bip01 R Hand"), "a finger bone on the hand bone: left");
	Check(BodyNodeHiddenOnArm("Shield", "Bip01 L ForearmTwist"), "what hangs on the left forearm twist: hidden");
	Check(BodyNodeHiddenOnArm("Bip01 L Hand", "Bip01 R ForearmTwist"), "anything on the right forearm twist: hidden");
	Check(!BodyNodeHiddenOnArm("Bip01 L Forearm", "Bip01 L UpperArm"), "a forearm on its upper arm: left");
	Check(!BodyNodeHiddenOnArm(nullptr, "Player") && !BodyNodeHiddenOnArm("Hand", nullptr), "no name: left");
}

}  // namespace

void TestStartTrace() {
	std::printf("The arms traced over the first seconds after a load\n");
	StartTrace s;
	Check(StepStartTrace(s, 0.0f), "the first frame the arms follow: traced");
	Check(!StepStartTrace(s, 0.1f), "a tenth later: not yet");
	Check(StepStartTrace(s, 0.2f), "a quarter second on: traced again");
	bool any = false;
	for (int i = 0; i < 100; ++i) {
		any = StepStartTrace(s, 0.1f) || any;
	}
	Check(!StepStartTrace(s, 0.25f) && !StepStartTrace(s, 0.25f), "past the trace's seconds: quiet");
	Check(any, "and traced on the way");
	StartTrace back;
	StepStartTrace(back, 0.0f);
	Check(!StepStartTrace(back, -1.0f), "a step back in time: not counted");
	StartTrace nan;
	volatile float zero = 0.0f;
	StepStartTrace(nan, 0.0f);
	Check(!StepStartTrace(nan, zero / zero), "a time step that is no number: not counted");
}

int main() {
	TestStartTrace();
	TestLimbReach();
	TestLimbFar();
	TestSoftReach();
	TestLimbNear();
	TestLimbNothing();
	TestLimbOnTheRig();
	TestElbow();
	TestEase();
	TestRollMemory();
	TestForearmRolls();
	TestTorsoKeepOut();
	TestCarried();
	TestClavicle();
	TestTwist();
	TestTorso();
	TestChest();
	TestKnees();
	TestFeet();
	TestTrunk();
	TestGate();
	TestHides();
	if (g_failures == 0) {
		std::printf("all checks passed\n");
		return 0;
	}
	std::printf("%d check(s) failed\n", g_failures);
	return 1;
}

// Checks the weapon's weight as a lag (vr/WeaponWeight.h): the weight's
// share, the slider's, the tuning per weight and grip, the quaternion
// helpers, the plain lag's step - taken up where the hand is, a frame
// behind, the caps, no time, let go - and the swing-through: the exact
// oscillator step against an integrated oracle, the release from behind
// that overshoots by a quarter, the steady hand it trails as the plain lag
// does, the stop it overruns, the cap that takes its momentum, the switch
// in the middle of a draw.

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/WeaponWeight.h"

namespace {

using namespace obvr::vr;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

float Degrees(float radians) { return radians * obvr::math::kRadiansToDegrees; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kFrame = 1.0f / 90.0f;

// A frame's share of the gap at this time constant (the plain lag).
float Share(float tc) { return kFrame / (tc + kFrame); }

// How far past the target a release from rest swings, as a share of the
// distance: e^(-zeta pi / sqrt(1 - zeta^2)).
float Overshoot(float zeta) { return std::exp(-zeta * 3.14159265f / std::sqrt(1.0f - zeta * zeta)); }

void TestWeightFactor() {
	std::printf("The weight's share\n");
	Check(WeightFactor(3.0f) == 0.0f, "an iron dagger (3) lags not at all");
	Check(WeightFactor(1.0f) == 0.0f, "nor anything lighter");
	Check(WeightFactor(0.0f) == 0.0f, "nor a weightless one");
	Check(Near(WeightFactor(23.0f), 0.5f), "23 is half way");
	Check(Near(WeightFactor(42.0f), 0.975f), "a warhammer (42) nearly fully");
	Check(WeightFactor(43.0f) == 1.0f, "43 fully");
	Check(WeightFactor(100.0f) == 1.0f, "and no further");
	Check(WeightFactor(kNaN) == 0.0f, "not a number is no weight");
	Check(WeightFactor(-5.0f) == 0.0f, "nor is a negative one");
}

void TestStrength() {
	std::printf("The slider\n");
	Check(Near(WeaponWeightStrength(40.0f), 0.4f), "40 % is 0.4");
	Check(Near(WeaponWeightStrength(100.0f), 1.0f), "100 % is 1");
	Check(Near(WeaponWeightStrength(150.0f), 1.0f), "over 100 is 100");
	Check(Near(WeaponWeightStrength(1.0f), 0.01f), "1 % is 0.01");
	Check(Near(WeaponWeightStrength(0.0f), 0.01f), "0 is 1 %");
	Check(Near(WeaponWeightStrength(-5.0f), 0.01f), "so is a negative setting");
	Check(Near(WeaponWeightStrength(kNaN), 0.01f), "and one that is not a number");
}

void TestTuning() {
	std::printf("The tuning\n");
	const WeaponLagTuning dagger = WeaponLagFor(3.0f, 100.0f, false, false);
	Check(dagger.timeConstantSeconds == 0.0f, "a dagger has no time constant");
	Check(Near(dagger.capMetres, 0.25f) && Near(Degrees(dagger.capRadians), 35.0f),
	      "the caps are the slider's regardless");
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false, false);
	Check(Near(hammer.timeConstantSeconds, 0.12f), "the heaviest at 100 % has the full 120 ms");
	Check(!hammer.swingThrough, "the plain lag when asked for");
	const WeaponLagTuning twoHands = WeaponLagFor(43.0f, 100.0f, true, false);
	Check(Near(twoHands.timeConstantSeconds, 0.048f), "both hands on the handle: 0.4 of it");
	Check(Near(twoHands.capMetres, 0.25f), "the caps unchanged by the grip");
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false, false);
	Check(Near(forty.timeConstantSeconds, 0.048f), "at 40 % the time constant is 48 ms");
	Check(Near(forty.capMetres, 0.10f) && Near(Degrees(forty.capRadians), 14.0f), "and the caps 0.10 m and 14 degrees");
	const WeaponLagTuning sword = WeaponLagFor(23.0f, 40.0f, false, false);
	Check(Near(sword.timeConstantSeconds, 0.024f), "a longsword at 40 %: half of that");
	const WeaponLagTuning none = WeaponLagFor(kNaN, 40.0f, false, false);
	Check(none.timeConstantSeconds == 0.0f, "a weight that is not a number: none");
	const WeaponLagTuning momentum = WeaponLagFor(43.0f, 100.0f, false, true);
	Check(momentum.swingThrough && Near(momentum.timeConstantSeconds, 0.12f),
	      "the swing-through when asked for, with the same time constant");
}

void TestQuaternions() {
	std::printf("The quaternion helpers\n");
	const Quaternion identity = Quaternion::Identity();
	const Quaternion yaw90 = FromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f);
	const Quaternion yaw180 = FromAxisAngle(0.0f, 1.0f, 0.0f, 180.0f);
	Check(Near(Degrees(AngleBetween(identity, yaw90)), 90.0f), "90 degrees apart read 90");
	Check(Near(Degrees(AngleBetween(identity, yaw180)), 180.0f, 0.01f), "180 read 180");
	Check(Near(AngleBetween(identity, identity), 0.0f), "the same read 0");
	const Quaternion minus{-yaw90.x, -yaw90.y, -yaw90.z, -yaw90.w};
	Check(Near(AngleBetween(yaw90, minus), 0.0f), "q and -q are one orientation");
	Check(Near(Degrees(AngleBetween(SlerpTowards(identity, yaw90, 0.5f), identity)), 45.0f),
	      "half way from 0 to 90 is 45");
	Check(Near(Degrees(AngleBetween(SlerpTowards(identity, minus, 0.5f), identity)), 45.0f),
	      "and the short way round when the target is written as -q");
	Check(Near(Degrees(AngleBetween(SlerpTowards(identity, yaw90, 0.0f), identity)), 0.0f), "share 0 stays");
	Check(Near(Degrees(AngleBetween(SlerpTowards(identity, yaw90, 1.0f), yaw90)), 0.0f), "share 1 arrives");
	Check(Near(Degrees(AngleBetween(SlerpTowards(identity, yaw90, -1.0f), identity)), 0.0f),
	      "a negative share stays");
	const Quaternion yaw1 = FromAxisAngle(0.0f, 1.0f, 0.0f, 1.0f);
	Check(Near(Degrees(AngleBetween(SlerpTowards(identity, yaw1, 0.5f), identity)), 0.5f, 0.01f),
	      "the last degree is blended straight");

	// Rotation vectors.
	const NiPoint3 r90 = RotationVectorOf(yaw90);
	Check(Near(r90.x, 0.0f) && Near(Degrees(r90.y), 90.0f, 0.01f) && Near(r90.z, 0.0f),
	      "a 90 degree yaw is the y axis times 90 degrees");
	const NiPoint3 rMinus = RotationVectorOf(minus);
	Check(Near(Degrees(rMinus.y), 90.0f, 0.01f), "written as -q, the same vector");
	const NiPoint3 rNone = RotationVectorOf(identity);
	Check(rNone.x == 0.0f && rNone.y == 0.0f && rNone.z == 0.0f, "no turn is the zero vector");
	const Quaternion roll70 = FromAxisAngle(0.0f, 0.0f, 1.0f, 70.0f);
	Check(Near(Degrees(AngleBetween(TurnOfRotationVector(RotationVectorOf(roll70)), roll70)), 0.0f, 0.01f),
	      "a turn comes back through its vector");
	Check(Near(Degrees(AngleBetween(TurnOfRotationVector(NiPoint3{0.0f, 0.0f, 0.0f}), identity)), 0.0f),
	      "the zero vector is no turn");
}

void TestOscillatorStep() {
	std::printf("The oscillator's step\n");
	const float omega = 1.0f / 0.12f;
	const float zeta = kWeaponSwingThroughDamping;
	const SpringStep still = UnderDampedStep(omega, zeta, 0.0f);
	Check(still.pp == 1.0f && still.pv == 0.0f && still.vp == 0.0f && still.vv == 1.0f, "no time: nothing moves");
	const SpringStep none = UnderDampedStep(0.0f, zeta, kFrame);
	Check(none.pp == 1.0f && none.vv == 1.0f, "a spring that cannot swing: nothing moves");

	// Ten short steps are one long one: the closed form composes.
	float x = 1.0f;
	float v = 0.0f;
	const SpringStep ten = UnderDampedStep(omega, zeta, 0.01f);
	for (int i = 0; i < 10; ++i) {
		const float x1 = x * ten.pp + v * ten.pv;
		const float v1 = x * ten.vp + v * ten.vv;
		x = x1;
		v = v1;
	}
	const SpringStep one = UnderDampedStep(omega, zeta, 0.1f);
	Check(Near(x, one.pp, 1e-4f) && Near(v, one.vp, 1e-3f), "ten steps of 10 ms are one of 100 ms");

	// One period on, the swing is back where it started, smaller by the
	// decay over the period.
	const float omegaD = omega * std::sqrt(1.0f - zeta * zeta);
	const float period = 2.0f * 3.14159265f / omegaD;
	const SpringStep round = UnderDampedStep(omega, zeta, period);
	Check(Near(round.pp, std::exp(-zeta * omega * period), 1e-4f) && Near(round.pv, 0.0f, 1e-4f),
	      "a period on it is back, smaller by the decay");
}

void TestStep() {
	std::printf("The plain lag's step\n");
	const Quaternion identity = Quaternion::Identity();
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false, false);
	const WeaponLagTuning dagger = WeaponLagFor(3.0f, 100.0f, false, false);
	WeaponLagState state;

	WeaponLagVerdict v = StepWeaponLag(state, false, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(!v.lagging && Near(v.position.x, 0.1f) && !state.held, "no weapon in hand: the controller's pose, nothing held");
	v = StepWeaponLag(state, true, dagger, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(!v.lagging && Near(v.position.x, 0.1f) && !state.held, "a dagger: the same");

	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(v.lagging && state.held, "a warhammer drawn: the lag is on");
	Check(Near(v.position.x, 0.1f) && Near(v.gapMetres, 0.0f), "taken up where the hand is, no jump");

	// The hand moves 0.1 m: a frame later the drawn hand has closed the
	// frame's share of it.
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.2f, 0.0f, 0.0f}, kFrame);
	const float share = Share(0.12f);
	Check(Near(v.position.x, 0.1f + 0.1f * share), "a frame behind by the share dt / (tc + dt)");
	Check(Near(v.gapMetres, 0.1f - 0.1f * share), "the gap says how far");
	// Then catches up, and never passes the hand.
	float furthest = 0.0f;
	for (int frame = 0; frame < 270; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.2f, 0.0f, 0.0f}, kFrame);
		furthest = v.position.x > furthest ? v.position.x : furthest;
	}
	Check(Near(v.position.x, 0.2f) && v.gapMetres < 0.001f, "three seconds later it is on the hand");
	Check(furthest <= 0.2f + 1e-5f, "and was never past it");

	// No time: the drawn pose stays.
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.3f, 0.0f, 0.0f}, 0.0f);
	Check(Near(v.position.x, 0.2f), "dt 0 leaves the drawn hand where it was");
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.3f, 0.0f, 0.0f}, -1.0f);
	Check(Near(v.position.x, 0.2f), "a negative dt too");
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.3f, 0.0f, 0.0f}, kNaN);
	Check(Near(v.position.x, 0.2f), "and one that is not a number");

	// The cap: a hand that jumps a metre leaves the drawn hand the cap behind.
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{1.2f, 0.0f, 0.0f}, kFrame);
	Check(Near(v.gapMetres, 0.25f) && Near(v.position.x, 0.95f), "never more than the cap behind");
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false, false);
	v = StepWeaponLag(state, true, forty, identity, NiPoint3{3.0f, 0.0f, 0.0f}, kFrame);
	Check(Near(v.gapMetres, 0.10f) && Near(v.position.x, 2.9f), "the cap is the slider's");

	// Both hands: closer after the same frame.
	WeaponLagState one;
	WeaponLagState two;
	const WeaponLagTuning twoHands = WeaponLagFor(43.0f, 100.0f, true, false);
	StepWeaponLag(one, true, hammer, identity, origin, kFrame);
	StepWeaponLag(two, true, twoHands, identity, origin, kFrame);
	const WeaponLagVerdict oneHand = StepWeaponLag(one, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	const WeaponLagVerdict bothHands = StepWeaponLag(two, true, twoHands, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(bothHands.gapMetres < oneHand.gapMetres && bothHands.gapMetres > 0.0f,
	      "both hands on the handle: stiffer, still behind");

	// Let go: the controller's pose at once, and the next draw starts where
	// the hand is.
	v = StepWeaponLag(state, false, hammer, identity, NiPoint3{5.0f, 0.0f, 0.0f}, kFrame);
	Check(!v.lagging && Near(v.position.x, 5.0f) && !state.held, "sheathed: on the controller, nothing held");
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{7.0f, 0.0f, 0.0f}, kFrame);
	Check(v.lagging && Near(v.position.x, 7.0f) && Near(v.gapMetres, 0.0f), "drawn again: taken up where the hand is now");
}

void TestRotation() {
	std::printf("The plain lag's rotation\n");
	const Quaternion identity = Quaternion::Identity();
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false, false);
	WeaponLagState state;
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);

	// The hand turns 20 degrees: the drawn hand follows the frame's share.
	const Quaternion yaw20 = FromAxisAngle(0.0f, 1.0f, 0.0f, 20.0f);
	WeaponLagVerdict v = StepWeaponLag(state, true, hammer, yaw20, origin, kFrame);
	const float share = Share(0.12f);
	Check(Near(Degrees(v.gapRadians), 20.0f * (1.0f - share), 0.05f), "a frame behind in heading by the same share");
	Check(Near(Degrees(AngleBetween(v.orientation, identity)), 20.0f * share, 0.05f), "turned by that share");
	for (int frame = 0; frame < 270; ++frame) {
		v = StepWeaponLag(state, true, hammer, yaw20, origin, kFrame);
	}
	Check(Degrees(v.gapRadians) < 0.05f, "and catches up");

	// A jump of 120 degrees: the drawn hand is the cap (35 degrees) behind,
	// on the arc from the controller towards where it was.
	const Quaternion roll120 = FromAxisAngle(0.0f, 0.0f, 1.0f, 120.0f);
	v = StepWeaponLag(state, true, hammer, roll120, origin, kFrame);
	Check(Near(Degrees(v.gapRadians), 35.0f, 0.05f), "never more than the cap behind in heading");
	Check(Near(Degrees(AngleBetween(v.orientation, roll120)), 35.0f, 0.05f), "the drawn orientation is that far from the controller");
	// From yaw 20 towards roll 120 the arc is 123 degrees or so; 35 short of
	// the end is 88 or so from the start - not back at the start.
	Check(Degrees(AngleBetween(v.orientation, yaw20)) > 60.0f, "and well on its way from where it was");
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false, false);
	v = StepWeaponLag(state, true, forty, identity, origin, kFrame);
	Check(Near(Degrees(v.gapRadians), 14.0f, 0.05f), "the heading's cap is the slider's");
}

// The swing-through's own oracle: x'' = omega^2 (hand - x) - 2 zeta omega x'
// integrated with many small Runge-Kutta steps over a frame in which the
// hand moves evenly from `from` to `to` - one axis, which is all the step
// does per axis.
void IntegrateFrame(float& x, float& v, float from, float to, float omega, float zeta, float dt) {
	const int steps = 400;
	const double h = static_cast<double>(dt) / steps;
	double px = x;
	double pv = v;
	const double speed = (static_cast<double>(to) - from) / dt;
	auto accel = [&](double time, double pos, double vel) {
		const double hand = from + speed * time;
		return static_cast<double>(omega) * omega * (hand - pos) - 2.0 * zeta * omega * vel;
	};
	for (int i = 0; i < steps; ++i) {
		const double time = i * h;
		const double k1x = pv;
		const double k1v = accel(time, px, pv);
		const double k2x = pv + 0.5 * h * k1v;
		const double k2v = accel(time + 0.5 * h, px + 0.5 * h * k1x, pv + 0.5 * h * k1v);
		const double k3x = pv + 0.5 * h * k2v;
		const double k3v = accel(time + 0.5 * h, px + 0.5 * h * k2x, pv + 0.5 * h * k2v);
		const double k4x = pv + h * k3v;
		const double k4v = accel(time + h, px + h * k3x, pv + h * k3v);
		px += h / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
		pv += h / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
	}
	x = static_cast<float>(px);
	v = static_cast<float>(pv);
}

void TestSwingThrough() {
	std::printf("The swing-through\n");
	const Quaternion identity = Quaternion::Identity();
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false, true);
	const WeaponLagTuning plainHammer = WeaponLagFor(43.0f, 100.0f, false, false);
	const float tc = hammer.timeConstantSeconds;
	const float zeta = kWeaponSwingThroughDamping;
	const float omega = SwingThroughOmega(tc);
	const float omegaD = omega * std::sqrt(1.0f - zeta * zeta);
	Check(Near(omega, 0.8f / 0.12f, 1e-4f), "the natural frequency is 2 zeta / tc");
	Check(SwingThroughOmega(0.0f) == 0.0f && SwingThroughOmega(-1.0f) == 0.0f, "no time constant, no frequency");

	// Taken up where the hand is, without momentum.
	WeaponLagState state;
	WeaponLagVerdict v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(v.lagging && Near(v.position.x, 0.1f) && Near(v.gapMetres, 0.0f), "taken up where the hand is");
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(Near(v.position.x, 0.1f) && Near(v.gapMetres, 0.0f), "and stays there while the hand is still");

	// The step against the equation it solves: a hand that sweeps back and
	// forth, faster and slower, with one jump in it - frame by frame the
	// same as the integrated motion.
	state = WeaponLagState{};
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);
	float oracleX = 0.0f;
	float oracleV = 0.0f;
	float handWas = 0.0f;
	float worst = 0.0f;
	for (int frame = 1; frame <= 120; ++frame) {
		const float time = static_cast<float>(frame) * kFrame;
		float hand = 0.12f * std::sin(9.0f * time) + 0.05f * std::sin(23.0f * time);
		if (frame >= 60) {
			hand += 0.04f;  // a jump of 4 cm in one frame
		}
		v = StepWeaponLag(state, true, hammer, identity, NiPoint3{hand, 0.0f, 0.0f}, kFrame);
		IntegrateFrame(oracleX, oracleV, handWas, hand, omega, zeta, kFrame);
		handWas = hand;
		const float off = std::fabs(v.position.x - oracleX);
		worst = off > worst ? off : worst;
	}
	Check(worst < 1e-4f, "each frame is the closed form of x'' = omega^2 (hand - x) - 2 zeta omega x'");
	Check(Near(state.velocity.x, oracleV, 1e-3f), "and so is its velocity");

	// A release from rest, 0.1 m behind a still hand: it swings past the
	// hand by a quarter of the way (the damping ratio's overshoot) and
	// settles on it.
	state = WeaponLagState{};
	state.held = true;
	state.position = origin;
	state.lastWantedPosition = NiPoint3{0.1f, 0.0f, 0.0f};
	state.lastWantedOrientation = identity;
	float furthest = 0.0f;
	bool passed = false;
	for (int frame = 0; frame < 270; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
		furthest = v.position.x > furthest ? v.position.x : furthest;
		passed = passed || v.position.x > 0.1f;
	}
	Check(passed, "released from behind, it passes the hand");
	Check(Near(furthest, 0.1f + 0.1f * Overshoot(zeta), 0.002f), "by a quarter of the way (zeta 0.4)");
	Check(Near(v.position.x, 0.1f, 0.001f) && v.gapMetres < 0.001f, "and three seconds later rests on it");

	// A hand at a steady speed: the weapon trails it exactly as far as the
	// plain lag does - the weight - after being left further behind as the
	// hand starts.
	state = WeaponLagState{};
	WeaponLagState plain;
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);
	StepWeaponLag(plain, true, plainHammer, identity, origin, kFrame);
	float mostBehind = 0.0f;
	WeaponLagVerdict p;
	NiPoint3 hand = origin;
	for (int frame = 1; frame <= 270; ++frame) {
		hand.x = static_cast<float>(frame) * kFrame;  // 1 m/s
		v = StepWeaponLag(state, true, hammer, identity, hand, kFrame);
		p = StepWeaponLag(plain, true, plainHammer, identity, hand, kFrame);
		mostBehind = v.gapMetres > mostBehind ? v.gapMetres : mostBehind;
	}
	Check(Near(p.gapMetres, 0.12f, 0.001f), "the plain lag trails 1 m/s by speed * tc, 0.12 m");
	Check(Near(v.gapMetres, p.gapMetres, 0.001f) && v.position.x < hand.x,
	      "the swing-through trails it by the same - the weight is kept");
	Check(mostBehind > v.gapMetres * 1.4f && mostBehind < 0.25f,
	      "and as the hand starts its inertia leaves it half as far again behind");

	// The hand stops: the weapon overruns it - from tc behind at the hand's
	// speed, the free oscillator about the hand - and comes back.
	const float a = -1.0f * tc;
	const float b = (1.0f + zeta * omega * a) / omegaD;
	float expected = 0.0f;
	for (int frame = 1; frame <= 90; ++frame) {
		const float time = static_cast<float>(frame) * kFrame;
		const float x = std::exp(-zeta * omega * time) * (a * std::cos(omegaD * time) + b * std::sin(omegaD * time));
		expected = x > expected ? x : expected;
	}
	float past = 0.0f;
	for (int frame = 0; frame < 90; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, hand, kFrame);
		past = v.position.x - hand.x > past ? v.position.x - hand.x : past;
	}
	Check(expected > 0.05f && Near(past, expected, 0.001f), "a stopped hand is overrun, by half the trail");
	for (int frame = 0; frame < 270; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, hand, kFrame);
	}
	Check(v.gapMetres < 0.001f, "and it settles");

	// A hand that jumps (a frame of 9 m/s): the weapon is left where it was,
	// then swings up to the hand and past it, about as a release does.
	state = WeaponLagState{};
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
	Check(v.position.x < 0.01f && Near(v.gapMetres, 0.1f, 0.01f), "a jump leaves the weapon where it was");
	furthest = 0.0f;
	for (int frame = 0; frame < 90; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.1f, 0.0f, 0.0f}, kFrame);
		furthest = v.position.x > furthest ? v.position.x : furthest;
	}
	Check(furthest > 0.12f && furthest < 0.135f, "then overruns it by about a quarter");

	// The cap takes the momentum outward: a jump of two metres leaves the
	// weapon the cap behind, and the next frames bring it closer, not
	// further.
	v = StepWeaponLag(state, true, hammer, identity, NiPoint3{2.1f, 0.0f, 0.0f}, kFrame);
	Check(Near(v.gapMetres, 0.25f), "never more than the cap behind");
	float widest = 0.0f;
	for (int frame = 0; frame < 10; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, NiPoint3{2.1f, 0.0f, 0.0f}, kFrame);
		widest = v.gapMetres > widest ? v.gapMetres : widest;
	}
	Check(widest <= 0.25f + 1e-5f && v.gapMetres < 0.25f, "and it comes in from the cap, not out");
	// A hand too fast for the cap: the weapon rides the cap, and is not
	// pressed out past it frame after frame.
	state = WeaponLagState{};
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);
	hand = origin;
	for (int frame = 1; frame <= 90; ++frame) {
		hand.x = static_cast<float>(frame) * kFrame * 4.0f;  // 4 m/s: a trail of 0.48 m
		v = StepWeaponLag(state, true, hammer, identity, hand, kFrame);
	}
	Check(Near(v.gapMetres, 0.25f) && Near(state.velocity.x, 4.0f, 0.01f),
	      "at 4 m/s it rides the cap at the hand's speed");

	// No time: nothing moves, nothing is pushed.
	const NiPoint3 before = v.position;
	v = StepWeaponLag(state, true, hammer, identity, hand, 0.0f);
	Check(Near(v.position.x, before.x), "dt 0 leaves the drawn hand where it was");

	// Let go: the momentum goes with the pose.
	v = StepWeaponLag(state, false, hammer, identity, NiPoint3{5.0f, 0.0f, 0.0f}, kFrame);
	Check(!state.held && state.velocity.x == 0.0f, "sheathed: nothing held, no momentum");

	// Switched on in the middle of a draw, at a steady speed: the plain
	// lag's trail is the swing-through's rest point and its motion is kept,
	// so the weapon carries on where it was - no push.
	state = WeaponLagState{};
	StepWeaponLag(state, true, plainHammer, identity, origin, kFrame);
	hand = origin;
	for (int frame = 1; frame <= 270; ++frame) {
		hand.x = static_cast<float>(frame) * kFrame;
		v = StepWeaponLag(state, true, plainHammer, identity, hand, kFrame);
	}
	const float plainGap = v.gapMetres;
	float switchedMost = 0.0f;
	float switchedLeast = 1.0f;
	for (int frame = 271; frame <= 300; ++frame) {
		hand.x = static_cast<float>(frame) * kFrame;
		v = StepWeaponLag(state, true, hammer, identity, hand, kFrame);
		switchedMost = v.gapMetres > switchedMost ? v.gapMetres : switchedMost;
		switchedLeast = v.gapMetres < switchedLeast ? v.gapMetres : switchedLeast;
	}
	Check(switchedMost - plainGap < 0.002f && plainGap - switchedLeast < 0.002f,
	      "switched on mid-draw it takes over without a push");

	// The rotation: released 20 degrees behind a still hand, it swings past
	// by a quarter and settles.
	state = WeaponLagState{};
	state.held = true;
	state.orientation = identity;
	const Quaternion yaw20 = FromAxisAngle(0.0f, 1.0f, 0.0f, 20.0f);
	state.lastWantedOrientation = yaw20;
	float widestTurn = 0.0f;
	for (int frame = 0; frame < 270; ++frame) {
		v = StepWeaponLag(state, true, hammer, yaw20, origin, kFrame);
		const float turned = Degrees(AngleBetween(v.orientation, identity));
		widestTurn = turned > widestTurn ? turned : widestTurn;
	}
	Check(Near(widestTurn, 20.0f + 20.0f * Overshoot(zeta), 0.3f), "released behind in heading, it swings past by a quarter");
	Check(Degrees(v.gapRadians) < 0.05f, "and settles on the hand's heading");
	// A steady turn: trailed in heading as far as the plain lag trails it.
	state = WeaponLagState{};
	plain = WeaponLagState{};
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);
	StepWeaponLag(plain, true, plainHammer, identity, origin, kFrame);
	for (int frame = 1; frame <= 270; ++frame) {
		const Quaternion turned = FromAxisAngle(0.0f, 1.0f, 0.0f, static_cast<float>(frame) * kFrame * 90.0f);
		v = StepWeaponLag(state, true, hammer, turned, origin, kFrame);
		p = StepWeaponLag(plain, true, plainHammer, turned, origin, kFrame);
	}
	Check(Near(Degrees(p.gapRadians), 10.8f, 0.1f) && Near(Degrees(v.gapRadians), Degrees(p.gapRadians), 0.1f),
	      "a turn of 90 degrees a second is trailed 10.8 degrees, as the plain lag trails it");
	// A turn of 120 degrees: the cap in heading, and the spin outward taken.
	const Quaternion roll120 = FromAxisAngle(0.0f, 0.0f, 1.0f, 120.0f);
	state = WeaponLagState{};
	StepWeaponLag(state, true, hammer, identity, origin, kFrame);
	v = StepWeaponLag(state, true, hammer, roll120, origin, kFrame);
	Check(Near(Degrees(v.gapRadians), 35.0f, 0.05f), "never more than the cap behind in heading");
	float widestGap = 0.0f;
	for (int frame = 0; frame < 10; ++frame) {
		v = StepWeaponLag(state, true, hammer, roll120, origin, kFrame);
		widestGap = v.gapRadians > widestGap ? v.gapRadians : widestGap;
	}
	Check(Degrees(widestGap) <= 35.0f + 0.01f && Degrees(v.gapRadians) < 35.0f, "and it comes in from the cap");

	// A lighter setting swings quicker and overruns less.
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false, true);
	WeaponLagState light;
	StepWeaponLag(light, true, forty, identity, origin, kFrame);
	hand = origin;
	for (int frame = 1; frame <= 180; ++frame) {
		hand.x = static_cast<float>(frame) * kFrame;
		StepWeaponLag(light, true, forty, identity, hand, kFrame);
	}
	float lightPast = 0.0f;
	for (int frame = 0; frame < 90; ++frame) {
		v = StepWeaponLag(light, true, forty, identity, hand, kFrame);
		lightPast = v.position.x - hand.x > lightPast ? v.position.x - hand.x : lightPast;
	}
	Check(lightPast > 0.0f && Near(lightPast, past * 0.4f, 0.002f), "at 40 % the overrun is 0.4 of 100 %'s");
}

}  // namespace

int main() {
	TestWeightFactor();
	TestStrength();
	TestTuning();
	TestQuaternions();
	TestOscillatorStep();
	TestStep();
	TestRotation();
	TestSwingThrough();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

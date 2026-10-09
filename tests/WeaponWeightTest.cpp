// Checks the weapon's weight as a lag (vr/WeaponWeight.h): the weight's
// share, the slider's, the tuning per weight and grip, and the step - taken
// up where the hand is, a frame behind, the caps, no time, let go, and the
// quaternion helpers it is built on.

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

// A frame's share of the gap at this time constant.
float Share(float tc) { return kFrame / (tc + kFrame); }

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
	const WeaponLagTuning dagger = WeaponLagFor(3.0f, 100.0f, false);
	Check(dagger.timeConstantSeconds == 0.0f, "a dagger has no time constant");
	Check(Near(dagger.capMetres, 0.25f) && Near(Degrees(dagger.capRadians), 35.0f),
	      "the caps are the slider's regardless");
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false);
	Check(Near(hammer.timeConstantSeconds, 0.12f), "the heaviest at 100 % has the full 120 ms");
	const WeaponLagTuning twoHands = WeaponLagFor(43.0f, 100.0f, true);
	Check(Near(twoHands.timeConstantSeconds, 0.048f), "both hands on the handle: 0.4 of it");
	Check(Near(twoHands.capMetres, 0.25f), "the caps unchanged by the grip");
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false);
	Check(Near(forty.timeConstantSeconds, 0.048f), "at 40 % the time constant is 48 ms");
	Check(Near(forty.capMetres, 0.10f) && Near(Degrees(forty.capRadians), 14.0f), "and the caps 0.10 m and 14 degrees");
	const WeaponLagTuning sword = WeaponLagFor(23.0f, 40.0f, false);
	Check(Near(sword.timeConstantSeconds, 0.024f), "a longsword at 40 %: half of that");
	const WeaponLagTuning none = WeaponLagFor(kNaN, 40.0f, false);
	Check(none.timeConstantSeconds == 0.0f, "a weight that is not a number: none");
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
}

void TestStep() {
	std::printf("The step\n");
	const Quaternion identity = Quaternion::Identity();
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false);
	const WeaponLagTuning dagger = WeaponLagFor(3.0f, 100.0f, false);
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
	// Then catches up.
	for (int frame = 0; frame < 270; ++frame) {
		v = StepWeaponLag(state, true, hammer, identity, NiPoint3{0.2f, 0.0f, 0.0f}, kFrame);
	}
	Check(Near(v.position.x, 0.2f) && v.gapMetres < 0.001f, "three seconds later it is on the hand");

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
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false);
	v = StepWeaponLag(state, true, forty, identity, NiPoint3{3.0f, 0.0f, 0.0f}, kFrame);
	Check(Near(v.gapMetres, 0.10f) && Near(v.position.x, 2.9f), "the cap is the slider's");

	// Both hands: closer after the same frame.
	WeaponLagState one;
	WeaponLagState two;
	const WeaponLagTuning twoHands = WeaponLagFor(43.0f, 100.0f, true);
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
	std::printf("The rotation\n");
	const Quaternion identity = Quaternion::Identity();
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};
	const WeaponLagTuning hammer = WeaponLagFor(43.0f, 100.0f, false);
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
	const WeaponLagTuning forty = WeaponLagFor(43.0f, 40.0f, false);
	v = StepWeaponLag(state, true, forty, identity, origin, kFrame);
	Check(Near(Degrees(v.gapRadians), 14.0f, 0.05f), "the heading's cap is the slider's");
}

}  // namespace

int main() {
	TestWeightFactor();
	TestStrength();
	TestTuning();
	TestQuaternions();
	TestStep();
	TestRotation();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

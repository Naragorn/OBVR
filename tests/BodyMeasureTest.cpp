// The T-pose measurement (vr/BodyMeasure.h): the pose, its hold, and what
// the two numbers set.

#include <cmath>
#include <cstdio>

#include "vr/BodyMeasure.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 1e-4f) { return std::fabs(a - b) <= within; }
bool Near(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

TPoseInput Pose(float out = 0.75f, float up = -0.20f) {
	TPoseInput in;
	in.rightValid = in.leftValid = true;
	in.right = NiPoint3{out, 0.0f, up};
	in.left = NiPoint3{-out, 0.0f, up};
	in.rightTrigger = in.leftTrigger = true;
	in.dtSeconds = 0.1f;
	return in;
}

void TestPose() {
	std::printf("The T-pose\n");
	Check(InTPose(Pose()), "arms out to each side at shoulder height: a T");
	TPoseInput swapped = Pose();
	swapped.right.x = -0.75f;
	swapped.left.x = 0.75f;
	Check(InTPose(swapped), "the controllers on the other sides (crossed roles): a T all the same");
	Check(!InTPose(Pose(0.40f)), "hands only 0.40 m out: not a T");
	TPoseInput oneSide = Pose();
	oneSide.left.x = 0.6f;
	Check(!InTPose(oneSide), "both hands on one side: not a T");
	TPoseInput ahead = Pose();
	ahead.right.y = 0.5f;
	Check(!InTPose(ahead), "a hand held 0.5 m ahead: not a T");
	Check(!InTPose(Pose(0.75f, -0.70f)), "hands at the hips: not a T");
	Check(!InTPose(Pose(0.75f, 0.20f)), "hands over the head: not a T");
	TPoseInput tilted = Pose();
	tilted.right.z = -0.05f;
	tilted.left.z = -0.35f;
	Check(!InTPose(tilted), "one hand 0.30 m lower than the other: not a T");
	TPoseInput lost = Pose();
	lost.leftValid = false;
	Check(!InTPose(lost), "a controller not tracked: not a T");
}

void TestHold() {
	std::printf("Holding the pose with both triggers\n");
	TPoseState s;
	TPoseInput in = Pose();
	bool fired = false;
	int steps = 0;
	while (!fired && steps < 20) {
		fired = StepTPose(s, in);
		++steps;
	}
	Check(fired && steps == 5, "half a second in the pose with both triggers: measured, once");
	Check(!StepTPose(s, in), "held on: not again");
	TPoseInput released = in;
	released.rightTrigger = released.leftTrigger = false;
	StepTPose(s, released);
	bool again = false;
	for (int i = 0; i < 5; ++i) {
		again = StepTPose(s, in) || again;
	}
	Check(again, "both triggers let go and pulled again: measured again");

	TPoseState t;
	TPoseInput one = Pose();
	one.leftTrigger = false;
	bool any = false;
	for (int i = 0; i < 20; ++i) {
		any = StepTPose(t, one) || any;
	}
	Check(!any, "one trigger only: never");
	TPoseState u;
	for (int i = 0; i < 4; ++i) {
		StepTPose(u, in);
	}
	TPoseInput broken = in;
	broken.right.x = 0.2f;
	StepTPose(u, broken);
	bool early = false;
	for (int i = 0; i < 4; ++i) {
		early = StepTPose(u, in) || early;
	}
	Check(!early, "the pose broken on the way: the hold starts over");
	TPoseState v;
	TPoseInput stall = in;
	stall.dtSeconds = 2.0f;
	Check(!StepTPose(v, stall), "a stalled frame of two seconds does not count as the hold");
}

void TestMeasure() {
	std::printf("What the measurement sets\n");
	BodyMeasureInput in;
	in.spanMetres = 1.48f;
	in.eyeHeightKnown = true;
	in.eyeHeightMetres = 1.6f;
	BodyMeasure m = MeasureBody(in);
	const HolsterSettings holster{};
	const ArcherySettings archery{};
	const StowSettings stow{};
	Check(m.ok && m.placesSet && Near(m.placeScale, 1.0f) && Near(m.oneHand, holster.oneHandZone) &&
	          Near(m.twoHand, holster.twoHandZone) && Near(m.bow, holster.bowZone) &&
	          Near(m.quiver, archery.quiverZone) && Near(m.stowUp, stow.centreUp) &&
	          Near(m.hipBelowEyes, kHipBelowEyesMetres),
	      "eyes at 1.6 m: the places are the defaults");
	in.eyeHeightMetres = 1.8f;
	m = MeasureBody(in);
	Check(m.placesSet && Near(m.placeScale, 1.125f) && Near(m.oneHand.z, holster.oneHandZone.z * 1.125f) &&
	          Near(m.bow.x, holster.bowZone.x * 1.125f) && Near(m.stowForward, stow.centreForward * 1.125f),
	      "eyes at 1.8 m: every place 1.125 times as far from the eyes, sides kept");
	Check(Near(m.mouthDown, stow.mouthDown * 1.125f) && Near(m.mouthForward, stow.mouthForward * 1.125f),
	      "the mouth too, from the eyes (Nadi, 2026-10-25: from the T-pose)");
	Check(Near(m.hipBelowEyes, kHipBelowEyesMetres * 1.125f),
	      "the hip too, for a two-hander held up in one hand (vr/OneHandTwoHander.h)");
	in.eyeHeightMetres = 2.05f;
	Check(Near(MeasureBody(in).placeScale, kPlaceScaleMost), "eyes at 2.05 m: the scale held at 1.25");
	in.eyeHeightMetres = 1.25f;
	Check(Near(MeasureBody(in).placeScale, kPlaceScaleLeast), "eyes at 1.25 m: the scale held at 0.8");
	in.eyeHeightMetres = 2.3f;
	m = MeasureBody(in);
	Check(m.ok && !m.placesSet && m.placesWhy != nullptr, "eyes read at 2.3 m: no places, the arms still measured");
	in.eyeHeightMetres = 1.0f;
	Check(!MeasureBody(in).placesSet, "eyes read at 1.0 m (a floor too high): no places");
	in.eyeHeightMetres = 1.7f;
	in.eyeHeightKnown = false;
	m = MeasureBody(in);
	Check(m.ok && !m.placesSet, "no floor read: no places");

	// The arms.
	in.eyeHeightKnown = true;
	in.spanMetres = 1.48f;  // a 1.75 m player
	m = MeasureBody(in);
	const float character = kCharacterArmUnits / 69.99125f;
	Check(Near(m.armMetres, 0.395f * 1.48f) &&
	          Near(m.armStretch, std::fmax(kArmStretchLeast, 1.05f * m.armMetres / character)),
	      "span 1.48 m: the arm 0.58 m, the stretch 5 % past what it needs, at least the default");
	in.spanMetres = 1.30f;  // a 1.55 m player: needs no stretch at all
	Check(Near(MeasureBody(in).armStretch, kArmStretchLeast), "a short span: the default stretch kept, never less");
	in.spanMetres = 2.15f;
	Check(Near(MeasureBody(in).armStretch, kArmStretchMost), "a very long span: the stretch held at 1.5");
	in.spanMetres = 0.6f;
	m = MeasureBody(in);
	Check(!m.ok && !m.placesSet && m.refusal != nullptr, "hands 0.6 m apart: refused, nothing changes");
	in.spanMetres = 2.5f;
	Check(!MeasureBody(in).ok, "hands 2.5 m apart: refused");
	in.spanMetres = 1.48f;
	in.unitsPerMetre = 0.0f;
	Check(MeasureBody(in).ok && Near(MeasureBody(in).armMetres, 0.395f * 1.48f),
	      "a units-per-metre of zero: the default scale taken, no division by zero");
}

}  // namespace

int main() {
	TestPose();
	TestHold();
	TestMeasure();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

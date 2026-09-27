// Checks the guided fit of the weapon places (vr/HolsterFit.h).

#include <cmath>
#include <cstdio>

#include "vr/HolsterFit.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

HolsterFitInput Hands() {
	HolsterFitInput in;
	in.weaponValid = true;
	in.otherValid = true;
	in.weaponRelative = NiPoint3{-0.2f, 0.05f, -0.6f};
	in.otherRelative = NiPoint3{-0.15f, -0.1f, -0.2f};
	return in;
}

// A trigger pulled and let go, over two frames.
HolsterFitVerdict Pull(HolsterFitState& s, HolsterFitInput& in, bool weapon) {
	(weapon ? in.weaponTrigger : in.otherTrigger) = true;
	const HolsterFitVerdict v = StepHolsterFit(s, in);
	(weapon ? in.weaponTrigger : in.otherTrigger) = false;
	StepHolsterFit(s, in);
	return v;
}

void TestFit() {
	std::printf("Fitting the weapon places\n");
	{
		HolsterFitState s;
		HolsterFitInput in = Hands();
		in.weaponTrigger = true;
		HolsterFitVerdict v = StepHolsterFit(s, in);
		Check(!v.active && !v.finished, "idle: a trigger does nothing");
		in.start = true;
		v = StepHolsterFit(s, in);
		Check(v.active && v.step == HolsterFitStep::OneHand && v.stepChanged,
		      "started: the one-handed place first");
		in.start = false;
		v = StepHolsterFit(s, in);
		Check(v.step == HolsterFitStep::OneHand, "a trigger held since the start is not a press");
		in.weaponTrigger = false;
		StepHolsterFit(s, in);
		v = Pull(s, in, false);
		Check(v.step == HolsterFitStep::OneHand, "the other hand's trigger does not take it");
		v = Pull(s, in, true);
		Check(v.step == HolsterFitStep::TwoHand && v.stepChanged,
		      "the weapon hand's trigger: on to the two-handed place");
		in.weaponRelative = NiPoint3{0.18f, -0.1f, -0.15f};
		v = Pull(s, in, false);
		Check(v.step == HolsterFitStep::TwoHand, "the other hand's trigger does not take that either");
		v = Pull(s, in, true);
		Check(v.step == HolsterFitStep::Bow && v.stepChanged, "the weapon hand again: on to the bow");
		in.weaponRelative = NiPoint3{9.0f, 9.0f, 9.0f};
		v = Pull(s, in, true);
		Check(v.step == HolsterFitStep::Bow && !v.finished, "the weapon hand does not take the bow's");
		v = Pull(s, in, false);
		Check(v.finished && !v.active && Near(v.oneHand.x, -0.2f) && Near(v.oneHand.z, -0.6f) &&
		          Near(v.twoHand.x, 0.18f) && Near(v.twoHand.z, -0.15f) && Near(v.bow.y, -0.1f),
		      "the other hand's trigger: all three places, as they were when taken");
	}
	{
		HolsterFitState s;
		HolsterFitInput in = Hands();
		in.start = true;
		StepHolsterFit(s, in);
		in.start = false;
		in.cancel = true;
		const HolsterFitVerdict v = StepHolsterFit(s, in);
		Check(v.cancelled && !v.active && !v.finished, "a menu button cancels");
	}
	{
		HolsterFitState s;
		HolsterFitInput in = Hands();
		in.start = true;
		StepHolsterFit(s, in);
		in.start = false;
		StepHolsterFit(s, in);
		Pull(s, in, true);
		Pull(s, in, true);
		in.cancel = true;
		const HolsterFitVerdict v = StepHolsterFit(s, in);
		Check(v.cancelled && !v.finished, "cancelled at the last place: nothing kept");
	}
	{
		HolsterFitState s;
		HolsterFitInput in = Hands();
		in.weaponValid = false;
		in.start = true;
		StepHolsterFit(s, in);
		in.start = false;
		StepHolsterFit(s, in);
		const HolsterFitVerdict v = Pull(s, in, true);
		Check(v.step == HolsterFitStep::OneHand, "an untracked hand takes no place");
	}
	{
		HolsterFitState s;
		HolsterFitInput in = Hands();
		in.leftHanded = true;
		in.start = true;
		StepHolsterFit(s, in);
		in.start = false;
		StepHolsterFit(s, in);
		in.weaponRelative = NiPoint3{0.2f, 0.05f, -0.6f};
		Pull(s, in, true);
		in.weaponRelative = NiPoint3{-0.18f, -0.1f, -0.15f};
		Pull(s, in, true);
		in.otherRelative = NiPoint3{0.15f, -0.1f, -0.2f};
		const HolsterFitVerdict v = Pull(s, in, false);
		Check(v.finished && Near(v.oneHand.x, -0.2f) && Near(v.twoHand.x, 0.18f) &&
		          Near(v.bow.x, -0.15f),
		      "left-handed: stored mirrored, as a right-handed body");
	}
	{
		HolsterFitState s;
		HolsterFitInput in = Hands();
		in.start = true;
		StepHolsterFit(s, in);
		const HolsterFitVerdict v = StepHolsterFit(s, in);
		Check(v.step == HolsterFitStep::OneHand && !v.stepChanged,
		      "a second start while running: no restart");
	}
}

}  // namespace

int main() {
	TestFit();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

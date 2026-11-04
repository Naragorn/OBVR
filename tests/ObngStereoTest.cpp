// Checks when OBVR renders the world in one pass through OBNG's stereo draw
// route instead of its own two passes (render/ObngStereoLogic.h): only with
// the setting on, OBNG.dll's three functions bound, its plan "draw twice"
// and the eye data taken; every other case keeps the dual pass of today.

#include <cstdio>
#include <cstring>

#include "render/ObngStereoLogic.h"

using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

ObngStereoState Ready() {
	ObngStereoState s;
	s.enabled = true;
	s.bound = true;
	s.plan = kObngPlanDrawTwice;
	s.eyesTaken = true;
	return s;
}

void TestPath() {
	std::printf("Which path renders the world\n");
	Check(StereoPathFor(Ready()) == StereoPath::SinglePass, "all four given: one pass through OBNG");
	ObngStereoState off = Ready();
	off.enabled = false;
	Check(StereoPathFor(off) == StereoPath::DualPass, "the setting off: the dual pass");
	ObngStereoState unbound = Ready();
	unbound.bound = false;
	Check(StereoPathFor(unbound) == StereoPath::DualPass, "OBNG.dll absent or a function missing: the dual pass");
	ObngStereoState oneEye = Ready();
	oneEye.plan = 1;
	Check(StereoPathFor(oneEye) == StereoPath::DualPass, "OBNG's plan one eye (the 1:1 gate): the dual pass");
	ObngStereoState routeOff = Ready();
	routeOff.plan = 0;
	Check(StereoPathFor(routeOff) == StereoPath::DualPass, "OBNG's route off: the dual pass");
	ObngStereoState unknown = Ready();
	unknown.plan = 7;
	Check(StereoPathFor(unknown) == StereoPath::DualPass, "a plan OBVR does not know: the dual pass");
	ObngStereoState refused = Ready();
	refused.eyesTaken = false;
	Check(StereoPathFor(refused) == StereoPath::DualPass, "the eye data refused: the dual pass");
}

void TestNames() {
	std::printf("The setting's and the result's names for the log\n");
	Check(std::strcmp(ObngStereoResultName(0), "ok") == 0, "0: ok");
	Check(std::strcmp(ObngStereoResultName(6), "the route is off or has one eye") == 0, "6: the route off");
	Check(std::strcmp(ObngStereoResultName(99), "an unknown answer") == 0, "anything else: unknown");
	Check(std::strcmp(StereoPathName(StereoPath::SinglePass), "single pass through OBNG") == 0, "the single pass");
	Check(std::strcmp(StereoPathName(StereoPath::DualPass), "dual pass") == 0, "the dual pass");
}

}  // namespace

int main() {
	TestPath();
	TestNames();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

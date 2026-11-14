#include <cmath>
#include <cstdio>

#include "vr/HolsterGlance.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::vr;

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void TestTarget() {
	std::printf("how visible the places are at a head pitch\n");
	const HolsterGlanceSettings s;
	Check(Near(GlanceTarget(s, 0.0f, true), 0.0f), "looking ahead: nothing");
	Check(Near(GlanceTarget(s, -kGlanceStartDegrees, true), 0.0f), "at the start: still nothing");
	Check(Near(GlanceTarget(s, -kGlanceFullDegrees, true), kGlanceOpacity), "at full: the full faint opacity");
	Check(Near(GlanceTarget(s, -80.0f, true), kGlanceOpacity), "straight down: the full faint opacity");
	const float half = GlanceTarget(s, -0.5f * (kGlanceStartDegrees + kGlanceFullDegrees), true);
	Check(half > 0.0f && half < kGlanceOpacity, "between: in proportion");
	Check(Near(GlanceTarget(s, 30.0f, true), 0.0f), "looking up: nothing");
	Check(Near(GlanceTarget(s, -80.0f, false), 0.0f), "not allowed (a menu, no world, placing them): nothing");
	HolsterGlanceSettings off;
	off.enabled = false;
	Check(Near(GlanceTarget(off, -80.0f, true), 0.0f), "switched off: nothing");
	Check(Near(GlanceTarget(s, std::nanf(""), true), 0.0f), "NaN pitch: nothing");
	Check(kGlanceOpacity > 0.0f && kGlanceOpacity <= 0.5f, "faint: half or less");
}

void TestFade() {
	std::printf("the places fade in and out, not at once\n");
	float a = 0.0f;
	a = StepGlanceAlpha(a, kGlanceOpacity, 0.01f);
	Check(a > 0.0f && a < kGlanceOpacity, "one frame in: on the way");
	for (int i = 0; i < 100; ++i) {
		a = StepGlanceAlpha(a, kGlanceOpacity, 0.01f);
	}
	Check(Near(a, kGlanceOpacity), "a second later: there, not past it");
	a = StepGlanceAlpha(a, 0.0f, 0.01f);
	Check(a < kGlanceOpacity && a > 0.0f, "fading out");
	for (int i = 0; i < 100; ++i) {
		a = StepGlanceAlpha(a, 0.0f, 0.01f);
	}
	Check(Near(a, 0.0f), "gone, not below 0");
	Check(Near(StepGlanceAlpha(0.2f, 0.3f, std::nanf("")), 0.2f), "NaN time: unchanged");
	Check(Near(StepGlanceAlpha(0.2f, 0.3f, -1.0f), 0.2f), "negative time: unchanged");
	const float fromNan = StepGlanceAlpha(std::nanf(""), 0.3f, 0.01f);
	Check(fromNan > 0.0f && fromNan < 0.1f, "NaN now: taken as 0, one step on");
}

void TestShown() {
	std::printf("the circles are up while any of them can be seen\n");
	Check(!GlanceShown(0.0f), "0: hidden");
	Check(!GlanceShown(0.001f), "almost 0: hidden");
	Check(GlanceShown(0.05f), "faint: shown");
}

}  // namespace

int main() {
	TestTarget();
	TestFade();
	TestShown();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

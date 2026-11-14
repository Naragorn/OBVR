#include <cmath>
#include <cstdio>

#include "render/VignetteShape.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1.0e-5f; }

}  // namespace

int main() {
	using obvr::render::VignetteRampAlpha;
	std::printf("the ramp's ends, its middle, both curves\n");
	for (int s = 0; s < 2; ++s) {
		const bool smooth = s == 1;
		Check(Near(VignetteRampAlpha(0.0f, smooth), 0.0f) && Near(VignetteRampAlpha(-1.0f, smooth), 0.0f),
		      "before the ramp: clear");
		Check(Near(VignetteRampAlpha(1.0f, smooth), 1.0f) && Near(VignetteRampAlpha(3.0f, smooth), 1.0f),
		      "past it: full");
		Check(Near(VignetteRampAlpha(std::nanf(""), smooth), 0.0f), "not a number: clear");
		float last = 0.0f;
		bool rising = true;
		for (int i = 1; i <= 100; ++i) {
			const float a = VignetteRampAlpha(static_cast<float>(i) / 100.0f, smooth);
			rising = rising && a >= last;
			last = a;
		}
		Check(rising, "never lighter further out");
	}
	Check(Near(VignetteRampAlpha(0.5f, false), 0.25f), "the square: a quarter dark half way");
	Check(Near(VignetteRampAlpha(0.5f, true), 0.5f), "the smoothstep: half dark half way");
	Check(VignetteRampAlpha(0.75f, true) > VignetteRampAlpha(0.75f, false) + 0.2f,
	      "the smoothstep is darker over the outer ramp - the edge no longer light");
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

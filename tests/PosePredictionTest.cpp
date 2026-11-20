// Checks how far ahead the poses are predicted when the frame's wait comes
// late (vr/PosePrediction.h): to the photons of the frame being updated.

#include <cstdio>

#include "vr/PosePrediction.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(double a, double b) {
	const double d = a - b;
	return d < 1e-6 && d > -1e-6;
}

void TestSteadyFrame() {
	std::printf("A steady frame at 90 Hz\n");
	const double period = 1.0 / 90.0;
	// The last wait ended at 10.000 s; the camera hook runs 0.9 ms later (the
	// present 0.8 ms after the wait's end plus a little).
	const double s = SecondsToPhotons(10.0009, 10.0, period, 0.003, 0.011);
	// The next wait ends at 10.0111; from there 3 ms to its vsync, a period to
	// the one it is shown at, 11 ms to the photons.
	Check(Near(s, (10.0 + period - 10.0009) + 0.003 + period + 0.011), "the next beat, a running start, a period, the photons");
	Check(s > 0.024 && s < 0.037, "about 25 to 36 ms ahead, as WaitGetPoses' own");
}

void TestLateUpdate() {
	std::printf("An update that has already run past the next beat\n");
	const double period = 1.0 / 90.0;
	// The hook comes 13 ms after the last wait's end - past the next beat at
	// 11.1 ms: this frame's wait ends on the beat after it.
	const double s = SecondsToPhotons(10.013, 10.0, period, 0.003, 0.011);
	Check(Near(s, (10.0 + 2.0 * period - 10.013) + 0.003 + period + 0.011), "the first beat not already past");
	// A load: seconds since the last wait. Predicted as if the next beat is now.
	const double load = SecondsToPhotons(12.0, 10.0, period, 0.003, 0.011);
	Check(Near(load, 0.003 + period + 0.011), "a long stall: from now, not eight periods on");
}

void TestUnknowns() {
	std::printf("What cannot be said\n");
	Check(SecondsToPhotons(10.0, 0.0, 0.011, 0.003, 0.011) < 0.0, "no wait yet: wait at once");
	Check(SecondsToPhotons(10.0, 9.99, 0.0, 0.003, 0.011) < 0.0, "no period: wait at once");
	Check(SecondsToPhotons(9.0, 9.99, 0.011, 0.003, 0.011) < 0.0, "a clock that ran backwards: wait at once");
	const double period = 1.0 / 75.0;
	const double s = SecondsToPhotons(10.001, 10.0, period, -1.0, -1.0);
	Check(Near(s, (10.0 + period - 10.001) + period), "no running start or photons read: the beats alone");
}

}  // namespace

int main() {
	TestSteadyFrame();
	TestLateUpdate();
	TestUnknowns();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

// Checks thrown things hitting people (game/ThrowLogic.h): what a speed does,
// and how long a throw is watched.

#include <cstdio>

#include "game/ThrowLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }

void TestKind() {
	std::printf("What a throw does\n");
	const ThrowHitSettings s;
	Check(ThrowHitFor(s, 2.0f) == ShoveKind::None, "2 m/s: nothing");
	Check(ThrowHitFor(s, 5.0f) == ShoveKind::Light, "5 m/s: staggered");
	Check(ThrowHitFor(s, 9.0f) == ShoveKind::Hard, "9 m/s: knocked down");
	ThrowHitSettings off;
	off.enabled = false;
	Check(ThrowHitFor(off, 20.0f) == ShoveKind::None, "switched off: nothing");
	volatile float zero = 0.0f;
	Check(ThrowHitFor(s, zero / zero) == ShoveKind::None, "no speed: nothing");
}

void TestFlight() {
	std::printf("A throw in flight\n");
	const ThrowHitSettings s;
	ThrowFlight f;
	Check(!StepThrowFlight(f, s, NiPoint3{0, 0, 0}, 0.01f, 70.0f), "nothing thrown: not watched");
	StartThrowFlight(f, 0x1234);
	Check(StepThrowFlight(f, s, NiPoint3{0, 0, 0}, 0.01f, 70.0f) && f.speed == 0.0f, "the first frame: no speed yet");
	Check(StepThrowFlight(f, s, NiPoint3{7, 0, 0}, 0.01f, 70.0f) && Near(f.speed, 10.0f),
	      "7 units in 10 ms: 10 m/s");
	for (int i = 0; i < 30; ++i) {
		StepThrowFlight(f, s, NiPoint3{7, 0, 0}, 0.01f, 70.0f);
	}
	Check(f.ref == 0, "come to rest after a quarter second: no longer watched");
	StartThrowFlight(f, 0x1234);
	bool watched = true;
	float x = 0.0f;
	for (int i = 0; i < 400 && watched; ++i) {
		x += 7.0f;
		watched = StepThrowFlight(f, s, NiPoint3{x, 0, 0}, 0.01f, 70.0f);
	}
	Check(!watched && f.ref == 0, "still flying after 3 s: no longer watched");
	StartThrowFlight(f, 0x1234);
	Check(StepThrowFlight(f, s, NiPoint3{0, 0, 0}, 0.01f, 70.0f) &&
	          StepThrowFlight(f, s, NiPoint3{0, 0, 0}, 0.01f, 70.0f),
	      "still in the first quarter second: watched even at rest");
}

}  // namespace

int main() {
	TestKind();
	TestFlight();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

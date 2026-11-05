// Checks where a thrown thing makes its noise (game/ThrowNoiseLogic.h): at
// its first hard stop, else where it comes to rest, once per throw, never for
// a gentle toss or a flight that does not end; and the 'WNOI' message.

#include <cstdio>

#include "game/ThrowNoiseLogic.h"

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

bool Near(float a, float b, float tolerance = 0.01f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

constexpr float kDt = 0.01f;
constexpr float kUnitsPerMetre = 70.0f;

// Moves the thing along x at `metresPerSecond` for `frames` frames, from x;
// answers the step of the last frame.
NoiseStep Fly(ThrowNoise& n, float& x, float metresPerSecond, int frames) {
	NoiseStep step;
	for (int i = 0; i < frames; ++i) {
		x += metresPerSecond * kUnitsPerMetre * kDt;
		step = StepThrowNoise(n, NiPoint3{x, 0.0f, 0.0f}, kDt, kUnitsPerMetre);
		if (!step.watching) {
			break;
		}
	}
	return step;
}

void TestImpact() {
	std::printf("A throw that hits something\n");
	ThrowNoise n;
	StartThrowNoise(n, 0x1234);
	float x = 0.0f;
	NoiseStep step = Fly(n, x, 5.0f, 20);
	Check(step.watching && !step.emit, "in flight at 5 m/s: watched, no noise yet");
	step = Fly(n, x, 1.0f, 1);
	Check(step.emit && !step.watching && Near(step.at.x, x), "it stops hard (5 to 1 m/s): the noise there, once");
	step = StepThrowNoise(n, NiPoint3{x, 0.0f, 0.0f}, kDt, kUnitsPerMetre);
	Check(!step.emit && !step.watching, "after it: no second noise");
}

void TestRest() {
	std::printf("A throw that slows down by itself\n");
	ThrowNoise n;
	StartThrowNoise(n, 0x1234);
	float x = 0.0f;
	Fly(n, x, 3.0f, 10);
	NoiseStep step = Fly(n, x, 2.0f, 5);
	Check(step.watching && !step.emit, "3 then 2 m/s: no hard stop, still watched");
	step = Fly(n, x, 1.2f, 5);
	step = Fly(n, x, 0.8f, 5);
	step = Fly(n, x, 0.1f, 1);
	Check(step.emit && Near(step.at.x, x), "slowing to rest: the noise where it lies");
}

void TestNone() {
	std::printf("Throws that make no noise\n");
	ThrowNoise none;
	Check(!StepThrowNoise(none, NiPoint3{0.0f, 0.0f, 0.0f}, kDt, kUnitsPerMetre).watching, "nothing thrown: nothing");
	ThrowNoise gentle;
	StartThrowNoise(gentle, 0x1234);
	float x = 0.0f;
	Fly(gentle, x, 0.7f, 30);
	const NoiseStep rest = Fly(gentle, x, 0.0f, 1);
	Check(!rest.emit && !rest.watching, "a toss never past kNoiseLeastPeak m/s: comes to rest silently");
	ThrowNoise slowStart;
	StartThrowNoise(slowStart, 0x1234);
	x = 0.0f;
	Fly(slowStart, x, 1.5f, 10);
	const NoiseStep halved = Fly(slowStart, x, 0.6f, 1);
	Check(!halved.emit && halved.watching, "1.5 to 0.6 m/s: below kNoiseImpactFrom, no hard stop");
	ThrowNoise forever;
	StartThrowNoise(forever, 0x1234);
	x = 0.0f;
	const NoiseStep gone = Fly(forever, x, 4.0f, 1000);
	Check(!gone.emit && !gone.watching, "still flying after kNoiseFollowSeconds: let go, no noise");
	ThrowNoise first;
	StartThrowNoise(first, 0x1234);
	const NoiseStep start = StepThrowNoise(first, NiPoint3{0.0f, 0.0f, 0.0f}, kDt, kUnitsPerMetre);
	Check(start.watching && !start.emit, "the first frame: no speed yet, no noise");
	ThrowNoise badDt;
	StartThrowNoise(badDt, 0x1234);
	x = 0.0f;
	Fly(badDt, x, 5.0f, 5);
	const NoiseStep nan = StepThrowNoise(badDt, NiPoint3{x, 0.0f, 0.0f}, 0.0f / 1.0f, kUnitsPerMetre);
	Check(nan.watching && !nan.emit, "a frame of no time: no speed read from it");
}

void TestSentOff() {
	std::printf("A weapon or torch let go from the hand\n");
	Check(IsThrowSpeed(NiPoint3{0.0f, 40.0f, 0.0f}, 35.0f), "sent off faster than a throw's least speed: followed");
	Check(IsThrowSpeed(NiPoint3{0.0f, 35.0f, 0.0f}, 35.0f), "at exactly that speed: followed");
	Check(!IsThrowSpeed(NiPoint3{0.0f, 20.0f, 10.0f}, 35.0f), "slower, a fall: not followed");
	Check(!IsThrowSpeed(NiPoint3{0.0f / 1.0f, 0.0f, 0.0f}, 35.0f), "still: not followed");
}

void TestMessage() {
	std::printf("'WNOI' to Walkers\n");
	const WalkersNoiseMessage msg = NoiseMessageFor(NiPoint3{1.5f, -2.0f, 3.25f}, kNoiseThrow);
	Check(kWalkersNoiseMessage == 0x494F4E57u, "the type 'WNOI'");
	Check(sizeof(msg) == 16, "16 bytes");
	Check(msg.x == 1.5f && msg.y == -2.0f && msg.z == 3.25f && msg.kind == 1, "x, y, z, then the kind: 1 a throw");
}

}  // namespace

int main() {
	TestImpact();
	TestRest();
	TestNone();
	TestSentOff();
	TestMessage();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

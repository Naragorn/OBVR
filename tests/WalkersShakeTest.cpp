#include <cstdio>
#include <cstring>

#include "vr/WalkersShake.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::vr;

constexpr float kFrame = 1.0f / 90.0f;

// Moves the hand from `from` to `to` in `frames` frames, counting the strokes.
int Sweep(ShakeStrokeState& s, const ShakeSettings& settings, NiPoint3 from, NiPoint3 to, int frames) {
	int strokes = 0;
	for (int i = 1; i <= frames; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(frames);
		const NiPoint3 at{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, from.z + (to.z - from.z) * t};
		if (StepShakeStroke(s, settings, at, kFrame, true)) {
			++strokes;
		}
	}
	return strokes;
}

void TestStrokes() {
	std::printf("a stroke is a turn of the hand's way after enough travel and speed\n");
	const ShakeSettings settings;
	ShakeStrokeState s;
	const NiPoint3 low{0.0f, 1.0f, 0.0f};
	const NiPoint3 high{0.0f, 1.3f, 0.0f};
	Check(!StepShakeStroke(s, settings, low, kFrame, true), "the first frame: the start, no stroke");
	Check(Sweep(s, settings, low, high, 15) == 0, "0.3 m up in 1/6 s: travel, but no turn yet");
	Check(Sweep(s, settings, high, low, 15) == 1, "and down again: the turn - one stroke");
	Check(Sweep(s, settings, low, high, 15) == 1, "up again: the next turn - another");
	Check(Sweep(s, settings, high, low, 15) == 1, "and down: a third");

	ShakeStrokeState sideways;
	StepShakeStroke(sideways, settings, low, kFrame, true);
	const NiPoint3 left{-0.15f, 1.0f, 0.0f};
	const NiPoint3 right{0.15f, 1.0f, 0.0f};
	Sweep(sideways, settings, low, right, 8);
	Check(Sweep(sideways, settings, right, left, 15) == 1, "side to side: a stroke as well, any way counts");

	ShakeStrokeState small;
	StepShakeStroke(small, settings, low, kFrame, true);
	const NiPoint3 bit{0.0f, 1.05f, 0.0f};
	int strokes = 0;
	for (int i = 0; i < 6; ++i) {
		strokes += Sweep(small, settings, low, bit, 5);
		strokes += Sweep(small, settings, bit, low, 5);
	}
	Check(strokes == 0, "a jitter of 5 cm: no stroke");

	ShakeStrokeState slow;
	StepShakeStroke(slow, settings, low, kFrame, true);
	strokes = Sweep(slow, settings, low, high, 180);
	strokes += Sweep(slow, settings, high, low, 180);
	Check(strokes == 0, "0.3 m up and down over 4 s: too slow, no stroke");
}

void TestStrokeEdges() {
	std::printf("the stroke's edges\n");
	const ShakeSettings settings;
	ShakeStrokeState s;
	const NiPoint3 low{0.0f, 1.0f, 0.0f};
	const NiPoint3 high{0.0f, 1.3f, 0.0f};
	StepShakeStroke(s, settings, low, kFrame, true);
	Sweep(s, settings, low, high, 15);
	Check(!StepShakeStroke(s, settings, low, kFrame, false), "the controller lost: no stroke");
	Check(!s.started, "and the stroke starts anew");
	Check(!StepShakeStroke(s, settings, high, 0.0f, true), "a frame without time: nothing");
	ResetShakeStroke(s);
	Check(!s.started && s.extremeMetres == 0.0f, "reset: from the start");
}

void TestHolds() {
	std::printf("the holders from Walkers' 'WGRB'\n");
	WalkersHolds h;
	const UInt32 two[5] = {2, 0xFF000EE7u, 2, 0xFF000EE8u, 1};
	Check(ParseWalkersHolds(two, sizeof(two), h) && h.count == 2, "two holders read");
	Check(ArmHeld(h, kWalkersArmRight) && ArmHeld(h, kWalkersArmLeft), "both arms held");
	Check(HolderOf(h, kWalkersArmLeft) == 0xFF000EE8u, "the left arm's holder");
	const UInt32 none[1] = {0};
	Check(ParseWalkersHolds(none, sizeof(none), h) && h.count == 0 && !ArmHeld(h, kWalkersArmRight),
	      "count 0: nobody holds");
	const UInt32 shortOne[2] = {1, 0xFF000EE7u};
	Check(!ParseWalkersHolds(shortOne, sizeof(shortOne), h), "one holder announced, its arm missing: refused");
	const UInt32 three[1] = {3};
	Check(!ParseWalkersHolds(three, sizeof(three), h), "three holders: more arms than a body has, refused");
	const UInt32 badArm[3] = {1, 0xFF000EE7u, 3};
	Check(!ParseWalkersHolds(badArm, sizeof(badArm), h), "arm 3: refused");
	Check(!ParseWalkersHolds(nullptr, 0, h), "nothing: refused");
}

void TestBite() {
	std::printf("a bite from Walkers' 'WGBT'\n");
	WalkersBite b;
	const UInt32 words[4] = {1, 2, 3, 0};
	Check(ParseWalkersBite(words, sizeof(words), b) && b.arm == 1 && b.number == 2 && b.toDie == 3 && !b.lethal,
	      "the left arm, bite 2 of 3");
	const UInt32 last[4] = {2, 3, 3, 1};
	Check(ParseWalkersBite(last, sizeof(last), b) && b.lethal, "the last one: lethal");
	Check(!ParseWalkersBite(words, 12, b), "three words: refused");
	const UInt32 badArm[4] = {0, 1, 3, 0};
	Check(!ParseWalkersBite(badArm, sizeof(badArm), b), "arm 0: refused");
}

void TestFlash() {
	std::printf("the bite's red flash\n");
	Check(BiteFlashAlpha(-1.0f, false) == 0.0f, "no bite: no flash");
	Check(BiteFlashAlpha(0.0f, false) > 0.0f, "at the bite: showing");
	Check(BiteFlashAlpha(kBiteFlashRiseSeconds, false) >= BiteFlashAlpha(0.0f, false), "rising first");
	Check(BiteFlashAlpha(kBiteFlashRiseSeconds, true) > BiteFlashAlpha(kBiteFlashRiseSeconds, false),
	      "the last bite: stronger");
	Check(BiteFlashAlpha(kBiteFlashSeconds, false) == 0.0f, "gone after kBiteFlashSeconds");
	Check(BiteFlashAlpha(kBiteFlashSeconds * 0.75f, false) < BiteFlashAlpha(kBiteFlashRiseSeconds, false),
	      "fading out");
}

void TestPrompt() {
	std::printf("the prompt at the held hand\n");
	char line[48];
	ShakePromptText(0, 0, line, sizeof(line));
	Check(std::strcmp(line, "") == 0, "no bite yet: no second line");
	ShakePromptText(1, 3, line, sizeof(line));
	Check(std::strcmp(line, "Bitten 1 of 3") == 0, "after a bite: bitten 1 of 3");
	Check(std::strcmp(kShakePromptText, "Shake it off!") == 0, "the call");
	Check(ShakePromptHand(kWalkersArmRight, false) && !ShakePromptHand(kWalkersArmLeft, false),
	      "the right arm: the right controller");
	Check(!ShakePromptHand(kWalkersArmRight, true) && ShakePromptHand(kWalkersArmLeft, true),
	      "left-handed roles: the right arm is still the right controller, in the left role");
}

void TestWalkLock() {
	std::printf("walking off while held\n");
	WalkersHolds h;
	Check(!WalkLocked(h, true), "no holder: free to walk");
	const UInt32 one[3] = {1, 0xFF000EE7u, 2};
	ParseWalkersHolds(one, sizeof(one), h);
	Check(WalkLocked(h, true), "one holder: the stick does not walk");
	Check(!WalkLocked(h, false), "the setting off: walks as before");
}

}  // namespace

int main() {
	TestStrokes();
	TestStrokeEdges();
	TestHolds();
	TestBite();
	TestFlash();
	TestPrompt();
	TestWalkLock();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

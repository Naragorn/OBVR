// Checks the draw speed's arithmetic (game/WeaponDrawSpeed.h): which groups
// count, the clock's step, the hastened offset and the shrunk blend.

#include <cmath>
#include <cstdio>

#include "game/WeaponDrawSpeed.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void TestGroups() {
	std::printf("Which groups\n");
	Check(IsWeaponDrawGroup(17) && IsWeaponDrawGroup(18), "Equip and Unequip");
	Check(!IsWeaponDrawGroup(0) && !IsWeaponDrawGroup(16) && !IsWeaponDrawGroup(19), "nothing else");
	Check(IsBlendState(kSequenceEaseIn) && IsBlendState(kSequenceEaseOut) && IsBlendState(kSequenceTransDest),
	      "easing in, out, and the transition's destination are blends");
	Check(!IsBlendState(kSequenceInactive) && !IsBlendState(kSequenceAnimating) && !IsBlendState(4) &&
	          !IsBlendState(6),
	      "playing, inactive, a transition's source and a morph are not");
}

void TestClock() {
	std::printf("The clock\n");
	Check(Near(ClockStep(6.02f, 6.01f), 0.01f), "a frame's step");
	Check(ClockStep(6.0f, 6.0f) == 0.0f, "no step");
	Check(ClockStep(1.0f, 6.0f) == 0.0f, "a clock going back (a new one): nothing");
	Check(ClockStep(7.0f, 6.0f) == 0.0f, "a whole second (a pause, a load): nothing");
}

void TestOffset() {
	std::printf("The offset\n");
	// Clock 6.8 at the start, offset -6.8: the sequence's time is 0.
	float offset = -6.8f;
	float clock = 6.8f;
	for (int frame = 0; frame < 2; ++frame) {
		clock += 0.01f;
		offset = HastenedOffset(offset, 0.01f, 10.0f);
	}
	Check(Near(clock + offset, 0.2f), "two frames of 10 ms at 10x: 0.2 s of the sequence");
	Check(Near(HastenedOffset(-3.0f, 0.01f, 1.0f), -3.0f) && Near(HastenedOffset(-3.0f, 0.01f, 0.5f), -3.0f),
	      "a speed of 1 or less: left");
	Check(Near(HastenedOffset(-3.0f, 0.0f, 10.0f), -3.0f), "no step: left");
}

void TestBlend() {
	std::printf("The blend\n");
	float start = 7.0f;
	float end = 7.2f;
	Check(ShrinkBlend(7.05f, start, end, 10.0f), "a blend under way is shrunk");
	Check(Near(start, 7.045f) && Near(end, 7.065f), "around the clock: 0.05 passed and 0.15 left, each over 10");
	Check(Near((7.05f - start) / (end - start), 0.25f), "the weight goes on from where it was (a quarter)");
	float pendingStart = -1.0f;
	float pendingEnd = 0.2f;
	Check(!ShrinkBlend(7.05f, pendingStart, pendingEnd, 10.0f) && pendingStart == -1.0f && pendingEnd == 0.2f,
	      "a window the engine has not set yet (start -1): left");
	float overStart = 6.0f;
	float overEnd = 6.5f;
	Check(!ShrinkBlend(7.0f, overStart, overEnd, 10.0f) && overEnd == 6.5f, "a blend already over: left");
	float laterStart = 8.0f;
	float laterEnd = 8.2f;
	Check(!ShrinkBlend(7.0f, laterStart, laterEnd, 10.0f) && laterStart == 8.0f,
	      "a blend not begun: left");
	float slowStart = 7.0f;
	float slowEnd = 7.2f;
	Check(!ShrinkBlend(7.05f, slowStart, slowEnd, 1.0f) && slowEnd == 7.2f, "a speed of 1: left");
}

}  // namespace

// A taken weapon's draw moved on to its Attach key in its first playing
// frame (free-weapons-spec 5.6): the key found by its name in the
// sequence's text keys as the game's files write them, and the offset that
// puts the time just past it.
void TestAttach() {
	std::printf("The Attach key and the jump to it\n");
	const float times[] = {0.0f, 0.3f, 0.3f, 0.6333f};
	const char* const texts[] = {"start", "Attach", "Enum: Equip", "end"};
	float at = -1.0f;
	Check(AttachKeyTime(times, texts, 4, at) && Near(at, 0.3f), "onehandequip.kf: Attach at 0.300");
	const float h2hTimes[] = {0.0f, 0.0333f, 0.1f, 0.1f, 0.2f};
	const char* const h2hTexts[] = {"start", "Blend: 6", "Attach ", "Enum: Equip", "end"};
	Check(AttachKeyTime(h2hTimes, h2hTexts, 5, at) && Near(at, 0.1f), "handtohandequip.kf's \"Attach \" counts");
	const char* const lower[] = {"start", "attach", "end"};
	const float lowerTimes[] = {0.0f, 0.5f, 0.8f};
	Check(AttachKeyTime(lowerTimes, lower, 3, at) && Near(at, 0.5f), "any case");
	const char* const none[] = {"start", "Enum: Unequip ", "Detach", "end"};
	at = -1.0f;
	Check(!AttachKeyTime(times, none, 4, at) && at == -1.0f, "an unequip has none: false, nothing written");
	const char* const holes[] = {nullptr, "Attachment", "Attach"};
	Check(AttachKeyTime(times, holes, 3, at) && Near(at, 0.3f), "a missing text skipped; Attachment is not Attach");
	Check(!AttachKeyTime(nullptr, texts, 4, at) && !AttachKeyTime(times, nullptr, 4, at), "no keys: false");

	Check(Near(AttachJumpOffset(2.0f, 0.12f, 0.3f, 1.0f), 2.0f + 0.18f + kAttachJumpPastSeconds),
	      "0.12 s in: the offset moved on to just past 0.300");
	Check(Near(AttachJumpOffset(2.0f, 0.12f, 0.3f, 2.0f), 2.0f + (0.18f + kAttachJumpPastSeconds) / 2.0f),
	      "a sequence at twice the frequency: half the offset");
	Check(Near(AttachJumpOffset(2.0f, 0.31f, 0.3f, 1.0f), 2.0f), "already past it: left");
	Check(Near(AttachJumpOffset(2.0f, 0.12f, 0.3f, 0.0f), 2.0f + 0.18f + kAttachJumpPastSeconds),
	      "a frequency of 0 or none read: taken as 1");
}

int main() {
	TestGroups();
	TestClock();
	TestOffset();
	TestBlend();
	TestAttach();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

#include <cmath>
#include <cstdio>
#include <cstring>

#include "vr/HintLogic.h"

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

constexpr float kFrame = 0.1f;
constexpr SInt32 kShield = static_cast<SInt32>(HintId::ShieldBash);
constexpr SInt32 kKick = static_cast<SInt32>(HintId::Kick);

HintFrame Wanting(SInt32 hint, float dt = kFrame) {
	HintFrame f;
	if (hint >= 0) {
		f.wants[hint] = true;
	}
	f.dt = dt;
	return f;
}

// Steps `seconds` of frames; the last step's result, and how many frames showed `hint`.
HintStep Run(HintState& s, const HintSettings& settings, const HintFrame& frame, float seconds, SInt32 hint,
             int* shownFrames = nullptr) {
	HintStep last;
	const int frames = static_cast<int>(std::lround(seconds / frame.dt));
	for (int i = 0; i < frames; ++i) {
		last = StepHints(s, settings, frame);
		if (shownFrames != nullptr && last.show == hint) {
			++*shownFrames;
		}
	}
	return last;
}

void TestWaitsForTheMoment() {
	std::printf("a hint shows once its moment has held wantSeconds, not before\n");
	const HintSettings settings;
	HintState s;
	HintStep r = Run(s, settings, Wanting(kShield), 1.4f, kShield);
	Check(r.show == -1, "1.4 s of the moment: nothing yet");
	r = StepHints(s, settings, Wanting(kShield));
	Check(r.show == kShield && r.started, "at 1.5 s: the shield's hint starts");
	Check(r.memoryChanged && s.memory.shown[kShield] == 1, "the showing is counted and written");
	r = StepHints(s, settings, Wanting(kShield));
	Check(r.show == kShield && !r.started && !r.memoryChanged, "the next frame: still up, nothing to write");
}

void TestMomentBroken() {
	std::printf("a moment broken before wantSeconds starts its wait again\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.0f, kShield);
	StepHints(s, settings, Wanting(-1));
	const HintStep r = Run(s, settings, Wanting(kShield), 1.0f, kShield);
	Check(r.show == -1, "1 s, a break, 1 s: nothing");
}

void TestShowTime() {
	std::printf("a showing lasts showSeconds\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	int frames = 0;
	Run(s, settings, Wanting(kShield), 10.0f, kShield, &frames);
	Check(frames >= 58 && frames <= 60, "up about 6 s of the 10");
	Check(s.active == -1, "then gone");
	Check(s.memory.shown[kShield] == 1, "shown once");
}

void TestGap() {
	std::printf("no showing within gapSeconds of the last one's start\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	int frames = 0;
	Run(s, settings, Wanting(kShield), 80.0f, kShield, &frames);
	Check(s.memory.shown[kShield] == 1, "80 s of the moment more: still one showing");
	const HintStep r = Run(s, settings, Wanting(kShield), 11.0f, kShield);
	Check(s.memory.shown[kShield] == 2 && r.show == kShield, "past 90 s: the second showing");
}

void TestMaxShows() {
	std::printf("a hint ignored maxShows times is not shown again\n");
	const HintSettings settings;
	HintState s;
	for (int i = 0; i < 6; ++i) {
		Run(s, settings, Wanting(kShield), 95.0f, kShield);
	}
	Check(s.memory.shown[kShield] == 3, "three showings in 570 s, no more");
	Check(!HintOpen(settings, s.memory, kShield), "the hint is closed");
}

void TestLearned() {
	std::printf("done once, the hint is learned and never shown\n");
	const HintSettings settings;
	HintState s;
	HintFrame f = Wanting(-1);
	f.did[kShield] = true;
	HintStep r = StepHints(s, settings, f);
	Check(r.learnedNow && r.memoryChanged && s.memory.learned[kShield], "the bash learns it, written");
	r = StepHints(s, settings, f);
	Check(!r.learnedNow && !r.memoryChanged, "a second bash changes nothing");
	r = Run(s, settings, Wanting(kShield), 10.0f, kShield);
	Check(r.show == -1 && s.memory.shown[kShield] == 0, "the moment after: never shown");
}

void TestLearnedWhileUp() {
	std::printf("done while its sign is up: the sign goes at once\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	Check(s.active == kShield, "up");
	HintFrame f = Wanting(kShield);
	f.did[kShield] = true;
	const HintStep r = StepHints(s, settings, f);
	Check(r.show == -1 && s.active == -1 && r.learnedNow, "the bash takes it down and learns it");
}

void TestLearnedFromMemory() {
	std::printf("learned in an earlier session: never shown\n");
	const HintSettings settings;
	HintState s;
	s.memory.learned[kShield] = true;
	const HintStep r = Run(s, settings, Wanting(kShield), 5.0f, kShield);
	Check(r.show == -1, "nothing");
}

void TestOff() {
	std::printf("hints off: nothing shows, but a deed is still learned\n");
	HintSettings settings;
	settings.enabled = false;
	HintState s;
	HintStep r = Run(s, settings, Wanting(kShield), 5.0f, kShield);
	Check(r.show == -1 && s.memory.shown[kShield] == 0, "nothing shown, nothing counted");
	HintFrame f = Wanting(-1);
	f.did[kKick] = true;
	r = StepHints(s, settings, f);
	Check(r.learnedNow && s.memory.learned[kKick], "the kick learned");
	settings.enabled = true;
	HintState up;
	Run(up, settings, Wanting(kShield), 1.5f, kShield);
	settings.enabled = false;
	r = StepHints(up, settings, Wanting(kShield));
	Check(r.show == -1 && up.active == -1, "turned off while up: gone");
}

void TestQuiet() {
	std::printf("a menu or a load: nothing shows, the moment's wait starts over\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	HintFrame quiet = Wanting(kShield);
	quiet.quiet = true;
	HintStep r = StepHints(s, settings, quiet);
	Check(r.show == -1 && s.active == -1, "a menu takes the sign down");
	HintState fresh;
	Run(fresh, settings, Wanting(kShield), 1.0f, kShield);
	StepHints(fresh, settings, quiet);
	r = Run(fresh, settings, Wanting(kShield), 1.0f, kShield);
	Check(r.show == -1, "1 s, a menu, 1 s: nothing");
}

void TestMomentGone() {
	std::printf("the moment gone while up: the sign goes\n");
	const HintSettings settings;
	HintState s;
	Run(s, settings, Wanting(kShield), 1.5f, kShield);
	const HintStep r = StepHints(s, settings, Wanting(-1));
	Check(r.show == -1 && s.active == -1, "the fight over, the sign down");
}

void TestOrder() {
	std::printf("two hints in one moment: the first in order, the other after the gap\n");
	const HintSettings settings;
	HintState s;
	HintFrame both = Wanting(kShield);
	both.wants[kKick] = true;
	HintStep r = Run(s, settings, both, 1.5f, kShield);
	Check(r.show == kShield, "the shield bash first");
	int kickFrames = 0;
	Run(s, settings, both, 85.0f, kKick, &kickFrames);
	Check(kickFrames == 0, "no kick hint within the gap");
	HintFrame f = both;
	f.did[kShield] = true;
	StepHints(s, settings, f);
	r = Run(s, settings, both, 6.0f, kKick);
	Check(r.show == kKick && s.memory.shown[kKick] == 1, "the shield learned, past the gap: the kick's hint");
}

void TestBadTime() {
	std::printf("a NaN or negative frame time counts as none\n");
	const HintSettings settings;
	HintState s;
	HintStep r = Run(s, settings, Wanting(kShield, -1.0f), 1.0f, kShield);
	HintFrame nan = Wanting(kShield);
	nan.dt = std::nanf("");
	for (int i = 0; i < 100; ++i) {
		r = StepHints(s, settings, nan);
	}
	Check(r.show == -1 && s.wantHeld[kShield] == 0.0f, "no time passes, nothing shows");
}

void TestWords() {
	std::printf("every hint has a name and words, none names a side\n");
	for (UInt32 i = 0; i < kHintCount; ++i) {
		const HintWords w = HintWordsFor(i);
		Check(std::strcmp(HintName(i), "?") != 0, "a name");
		Check(w.title[0] != '\0' && w.line[0] != '\0', "a title and a line");
		Check(std::strstr(w.line, "left") == nullptr && std::strstr(w.line, "right") == nullptr, "no side named");
	}
	Check(std::strcmp(HintName(kHintCount), "?") == 0, "past the end: ?");
	Check(HintWordsFor(kHintCount).title[0] == '\0', "past the end: no words");
	Check(!HintOnWeaponHand(kShield) && HintOnWeaponHand(kKick), "the shield's sign on the other hand, the kick's on the weapon hand");
}

}  // namespace

int main() {
	TestWaitsForTheMoment();
	TestMomentBroken();
	TestShowTime();
	TestGap();
	TestMaxShows();
	TestLearned();
	TestLearnedWhileUp();
	TestLearnedFromMemory();
	TestOff();
	TestQuiet();
	TestMomentGone();
	TestOrder();
	TestBadTime();
	TestWords();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

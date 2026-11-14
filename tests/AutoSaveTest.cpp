#include <cstdio>

#include "perf/AutoSaveLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::perf;

// A stretch from `start` lasting `seconds`; the reason of its end's step.
AutoSaveReason Stretch(AutoSaveState& s, const AutoSaveSettings& settings, double start, double seconds,
                       bool ring = true) {
	StepAutoSave(s, settings, ring, HalfRateEvent::Entered, start);
	return StepAutoSave(s, settings, ring, HalfRateEvent::Left, start + seconds).reason;
}

void TestEnded() {
	std::printf("a stretch of slowSeconds or more is saved when it ends\n");
	const AutoSaveSettings settings;
	AutoSaveState s;
	Check(Stretch(s, settings, 0.0, 2.9) == AutoSaveReason::None, "2.9 s: not slow enough");
	AutoSaveStep step;
	StepAutoSave(s, settings, true, HalfRateEvent::Entered, 10.0);
	Check(StepAutoSave(s, settings, true, HalfRateEvent::None, 12.0).reason == AutoSaveReason::None,
	      "while it lasts, under holdSeconds: nothing yet");
	step = StepAutoSave(s, settings, true, HalfRateEvent::Left, 15.0);
	Check(step.reason == AutoSaveReason::StretchEnded && step.stretchSeconds == 5.0, "5 s, ended: saved, 5 s said");
	Check(s.saves == 1, "counted");
}

void TestLong() {
	std::printf("a stretch still going is saved holdSeconds in, once\n");
	const AutoSaveSettings settings;
	AutoSaveState s;
	StepAutoSave(s, settings, true, HalfRateEvent::Entered, 0.0);
	Check(StepAutoSave(s, settings, true, HalfRateEvent::None, 29.9).reason == AutoSaveReason::None, "29.9 s: not yet");
	Check(StepAutoSave(s, settings, true, HalfRateEvent::None, 30.0).reason == AutoSaveReason::StretchLong, "30 s: saved");
	Check(StepAutoSave(s, settings, true, HalfRateEvent::None, 100.0).reason == AutoSaveReason::None, "later in it: not again");
	Check(StepAutoSave(s, settings, true, HalfRateEvent::Left, 120.0).reason == AutoSaveReason::None,
	      "its end: not again");
	Check(s.saves == 1, "one save for the stretch");
}

void TestCooldown() {
	std::printf("no save within cooldownSeconds of the last\n");
	const AutoSaveSettings settings;
	AutoSaveState s;
	Check(Stretch(s, settings, 0.0, 5.0) == AutoSaveReason::StretchEnded, "the first at 5 s");
	Check(Stretch(s, settings, 20.0, 5.0) == AutoSaveReason::None, "another ending at 25 s: within the cooldown");
	Check(Stretch(s, settings, 60.0, 5.0) == AutoSaveReason::StretchEnded, "one ending at 65 s: saved");
}

void TestMax() {
	std::printf("at most maxSaves a session\n");
	const AutoSaveSettings settings;
	AutoSaveState s;
	UInt32 saves = 0;
	for (int i = 0; i < 10; ++i) {
		saves += Stretch(s, settings, i * 100.0, 5.0) != AutoSaveReason::None ? 1 : 0;
	}
	Check(saves == 5 && s.saves == 5, "ten slow stretches, five saved");
}

void TestOff() {
	std::printf("off, or no ring: nothing\n");
	AutoSaveSettings settings;
	settings.slowSeconds = 0;
	AutoSaveState s;
	Check(Stretch(s, settings, 0.0, 50.0) == AutoSaveReason::None, "AutoSaveSlowSeconds=0: nothing");
	AutoSaveState r;
	const AutoSaveSettings on;
	Check(Stretch(r, on, 0.0, 50.0, false) == AutoSaveReason::None && r.saves == 0, "no ring recording: nothing");
	settings = on;
	settings.maxSaves = 0;
	AutoSaveState m;
	Check(Stretch(m, settings, 0.0, 50.0) == AutoSaveReason::None, "AutoSaveMax=0: nothing");
}

void TestStray() {
	std::printf("a Left without an Entered, and a new stretch after a saved one\n");
	const AutoSaveSettings settings;
	AutoSaveState s;
	Check(StepAutoSave(s, settings, true, HalfRateEvent::Left, 50.0).reason == AutoSaveReason::None,
	      "a stray end: nothing");
	Check(StepAutoSave(s, settings, true, HalfRateEvent::None, 80.0).reason == AutoSaveReason::None,
	      "outside a stretch: nothing");
	StepAutoSave(s, settings, true, HalfRateEvent::Entered, 100.0);
	StepAutoSave(s, settings, true, HalfRateEvent::None, 130.0);
	Check(Stretch(s, settings, 200.0, 5.0) == AutoSaveReason::StretchEnded, "the next stretch after the cooldown: saved");
}

}  // namespace

int main() {
	TestEnded();
	TestLong();
	TestCooldown();
	TestMax();
	TestOff();
	TestStray();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

// Checks the count of frames a weapon is out of sight while it changes hands
// (game/WeaponTimeline.h): the marks in the frame they came in, the first of
// each kept, the end once every wanted mark is in or the time ran out, and
// the span between two marks.

#include <cmath>
#include <cstdio>

#include "game/WeaponTimeline.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

constexpr float kFrame = 1.0f / 90.0f;

void TestBegin() {
	std::printf("A timeline begun\n");
	WeaponTimeline t;
	Check(!t.running, "a new one is not running");
	Check(StepTimeline(t, kFrame) == TimelineEnd::Idle, "stepped idle it says so");
	BeginTimeline(t, TimelineBit(kTakeInHand));
	Check(t.running && t.frames == 0 && Near(t.seconds, 0.0f), "running from frame 0");
	bool none = true;
	for (UInt32 i = 0; i < kTimelineMarks; ++i) {
		none = none && t.at[i] < 0;
	}
	Check(none, "no mark yet");
	MarkTimeline(t, kTakeTaken);
	Check(t.at[kTakeTaken] == 0 && Near(t.atSeconds[kTakeTaken], 0.0f), "a mark in the first frame is frame 0");
	Check(StepTimeline(t, kFrame) == TimelineEnd::Running, "still running without the wanted mark");
	Check(t.frames == 1 && Near(t.seconds, kFrame), "one frame on");
	BeginTimeline(t, TimelineBit(kTakeInHand));
	Check(t.at[kTakeTaken] < 0 && t.frames == 0, "begun again it starts over");
}

void TestMarks() {
	std::printf("Marks\n");
	WeaponTimeline t;
	BeginTimeline(t, TimelineBit(kTakeInHand));
	StepTimeline(t, kFrame);
	StepTimeline(t, kFrame);
	MarkTimeline(t, kTakeEquipped);
	Check(t.at[kTakeEquipped] == 2 && Near(t.atSeconds[kTakeEquipped], 2 * kFrame), "in the frame it came in");
	StepTimeline(t, kFrame);
	MarkTimeline(t, kTakeEquipped);
	Check(t.at[kTakeEquipped] == 2, "the first of a mark is kept");
	MarkTimeline(t, kTimelineMarks);
	Check(t.running, "a mark out of range changes nothing");
	WeaponTimeline idle;
	MarkTimeline(idle, kTakeTaken);
	Check(idle.at[kTakeTaken] < 0, "a mark on a timeline not running is dropped");
}

void TestEnd() {
	std::printf("The end\n");
	WeaponTimeline t;
	BeginTimeline(t, TimelineBit(kTakeInHand));
	MarkTimeline(t, kTakeTaken);
	for (int i = 0; i < 5; ++i) {
		StepTimeline(t, kFrame);
	}
	MarkTimeline(t, kTakeInHand);
	Check(StepTimeline(t, kFrame) == TimelineEnd::Done, "done once the wanted mark is in");
	Check(!t.running, "and no longer running");
	Check(t.at[kTakeInHand] == 5, "the marks stay readable after the end");
	Check(StepTimeline(t, kFrame) == TimelineEnd::Idle, "said once");

	WeaponTimeline r;
	BeginTimeline(r, TimelineBit(kReleaseOutOfHand));
	MarkTimeline(r, kReleaseOutOfHand);
	WantTimeline(r, kReleaseBody);
	Check(StepTimeline(r, kFrame) == TimelineEnd::Running, "a mark wanted later holds the end");
	MarkTimeline(r, kReleaseBody);
	Check(StepTimeline(r, kFrame) == TimelineEnd::Done, "done with it");
	WantTimeline(r, kReleaseShown);
	Check(!r.running, "wanting on an ended timeline starts nothing");

	WeaponTimeline out;
	BeginTimeline(out, TimelineBit(kTakeInHand));
	TimelineEnd end = TimelineEnd::Running;
	UInt32 steps = 0;
	while (end == TimelineEnd::Running && steps < 1000) {
		end = StepTimeline(out, kFrame);
		++steps;
	}
	Check(end == TimelineEnd::RanOut, "a mark that never comes runs out");
	Check(Near(out.seconds, steps * kFrame) && out.seconds >= kTimelineMostSeconds - kFrame,
	      "after the time allowed");

	WeaponTimeline c;
	BeginTimeline(c, TimelineBit(kTakeInHand));
	CancelTimeline(c);
	Check(!c.running && StepTimeline(c, kFrame) == TimelineEnd::Idle, "cancelled it ends without a word");
}

void TestTime() {
	std::printf("The time\n");
	WeaponTimeline t;
	BeginTimeline(t, TimelineBit(kTakeInHand));
	StepTimeline(t, NAN);
	StepTimeline(t, -1.0f);
	StepTimeline(t, 0.75f);
	Check(t.frames == 3 && Near(t.seconds, 0.0f), "frames count, a step not a number, back or a pause adds no time");
}

void TestSpan() {
	std::printf("The span between two marks\n");
	WeaponTimeline t;
	BeginTimeline(t, TimelineBit(kTakeInHand));
	StepTimeline(t, kFrame);
	MarkTimeline(t, kTakeTaken);
	for (int i = 0; i < 6; ++i) {
		StepTimeline(t, kFrame);
	}
	MarkTimeline(t, kTakeInHand);
	UInt32 frames = 0;
	float seconds = 0.0f;
	Check(TimelineSpan(t, kTakeTaken, kTakeInHand, frames, seconds) && frames == 6 && Near(seconds, 6 * kFrame),
	      "taken at 1, in the hand at 7: six frames");
	Check(!TimelineSpan(t, kTakeTaken, kTakeDrawn, frames, seconds), "a mark missing: no span");
	Check(!TimelineSpan(t, kTakeInHand, kTakeTaken, frames, seconds), "backwards: no span");
	Check(!TimelineSpan(t, kTakeTaken, kTimelineMarks, frames, seconds), "out of range: no span");
}

}  // namespace

int main() {
	TestBegin();
	TestMarks();
	TestEnd();
	TestTime();
	TestSpan();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

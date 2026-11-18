// Checks the recording mode's plan (test/RecordingPlan.h): which frames a
// film takes after a mark and how they are named, the part of the eye cut
// and the film's size, the halving steps down to it, and the read-back slots.

#include <cstdio>
#include <cstring>
#include <limits>

#include "test/RecordingPlan.h"

using namespace obvr;
using namespace obvr::test;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

RecordSettings On() {
	RecordSettings s;
	s.enabled = true;
	s.seconds = 0.1f;
	s.every = 2;
	return s;
}

void TestRun() {
	std::printf("The film's frames\n");
	RecordRun run;
	char name[128] = "";
	RecordSettings s = On();
	Check(!StepRecordRun(run, s, false, nullptr, 0.02f, name, sizeof(name)), "no mark yet: nothing filmed");
	Check(StepRecordRun(run, s, true, "pull", 0.02f, name, sizeof(name)) &&
	          std::strcmp(name, "OBVR-Rec-pull-0000.bmp") == 0,
	      "the mark's own frame: the film's first picture");
	Check(!StepRecordRun(run, s, false, nullptr, 0.02f, name, sizeof(name)), "every 2nd: the next frame skipped");
	Check(StepRecordRun(run, s, false, nullptr, 0.02f, name, sizeof(name)) &&
	          std::strcmp(name, "OBVR-Rec-pull-0001.bmp") == 0,
	      "the third frame: the second picture, numbered without a gap");
	StepRecordRun(run, s, false, nullptr, 0.02f, name, sizeof(name));
	Check(!StepRecordRun(run, s, false, nullptr, 0.05f, name, sizeof(name)), "past 0.1 s: the film ends");
	Check(StepRecordRun(run, s, true, "held", 0.02f, name, sizeof(name)) &&
	          std::strcmp(name, "OBVR-Rec-held-0000.bmp") == 0,
	      "a new mark: a new film from 0");
	RecordSettings off = s;
	off.enabled = false;
	Check(!StepRecordRun(run, off, true, "x", 0.02f, name, sizeof(name)) && run.mark[0] == '\0',
	      "the switch off: nothing, the run forgotten");
	RecordSettings zero = On();
	zero.every = 0;
	RecordRun r2;
	Check(StepRecordRun(r2, zero, true, "a", 0.02f, name, sizeof(name)) &&
	          StepRecordRun(r2, zero, false, nullptr, 0.02f, name, sizeof(name)),
	      "every 0 taken as every frame");
	RecordSettings huge = On();
	huge.seconds = 1000.0f;
	RecordRun r3;
	StepRecordRun(r3, huge, true, "a", 0.02f, name, sizeof(name));
	Check(r3.left <= kRecordMaxSeconds, "a film is held to a minute");
	RecordSettings nan = On();
	nan.seconds = std::numeric_limits<float>::quiet_NaN();
	RecordRun r4;
	Check(!StepRecordRun(r4, nan, true, "a", 0.02f, name, sizeof(name)), "seconds not a number: no film");
	RecordRun r5;
	Check(!StepRecordRun(r5, On(), true, "", 0.02f, name, sizeof(name)), "a mark with no name starts nothing");
}

void TestCut() {
	std::printf("The cut and the film's size\n");
	RecordSettings s = On();
	RecordCut c = PlanRecordCut(4036, 3376, s);
	Check(c.valid && c.left == 0 && c.right == 4036 && c.outWidth == 640 && c.outHeight == 534,
	      "the whole eye: 640 wide, its shape kept, even");
	s.cropLeft = 0.25f;
	s.cropRight = 0.75f;
	s.cropTop = 0.5f;
	s.cropBottom = 1.0f;
	c = PlanRecordCut(4036, 3376, s);
	Check(c.valid && c.left == 1009 && c.right == 3027 && c.top == 1688 && c.bottom == 3376 && c.outWidth == 640 &&
	          c.outHeight == 534,
	      "the lower middle: cut in source pixels, the film its shape");
	s = On();
	s.width = 5000;
	c = PlanRecordCut(1000, 500, s);
	Check(c.valid && c.outWidth == 1000 && c.outHeight == 500, "wider than the eye: the eye's own width");
	s.width = 3;
	c = PlanRecordCut(1000, 500, s);
	Check(c.valid && c.outWidth == kRecordMinWidth, "narrower than the least: the least");
	s = On();
	s.cropLeft = 0.6f;
	s.cropRight = 0.4f;
	c = PlanRecordCut(1000, 500, s);
	Check(c.valid && c.left == 0 && c.right == 1000, "a cut turned inside out: the whole width");
	s = On();
	s.cropTop = std::numeric_limits<float>::quiet_NaN();
	s.cropBottom = 2.0f;
	c = PlanRecordCut(1000, 500, s);
	Check(c.valid && c.top == 0 && c.bottom == 500, "fractions not a number or past 1: clamped");
	Check(!PlanRecordCut(8, 8, On()).valid, "no eye to speak of: no film");
}

void TestSteps() {
	std::printf("The halving steps\n");
	UInt32 w[kRecordMaxSteps];
	UInt32 h[kRecordMaxSteps];
	UInt32 n = PlanRecordSteps(4036, 3376, 640, 534, w, h);
	Check(n == 2 && w[0] == 2018 && h[0] == 1688 && w[1] == 640 && h[1] == 534,
	      "4036 to 640: halved once (1009 is under twice 640), then the film's size");
	n = PlanRecordSteps(1000, 500, 640, 320, w, h);
	Check(n == 1 && w[0] == 640 && h[0] == 320, "under twice the film: one blit");
	n = PlanRecordSteps(100000, 100000, 64, 64, w, h);
	Check(n == kRecordMaxSteps && w[n - 1] == 64, "never more steps than kept");
}

void TestSlots() {
	std::printf("The read-back slots\n");
	RecordSlots s;
	Check(FreeRecordSlot(s) == 0, "all free: the first");
	s.busy[0] = true;
	s.copiedAt[0] = 10;
	Check(FreeRecordSlot(s) == 1, "the first busy: the next");
	Check(!RecordSlotReady(s, 0, 11), "a frame later: not yet read");
	Check(RecordSlotReady(s, 0, 12), "the lag passed: read");
	Check(!RecordSlotReady(s, 1, 50), "a free slot is never read");
	for (bool& b : s.busy) {
		b = true;
	}
	Check(FreeRecordSlot(s) == kRecordSlots, "all busy: none - the frame is skipped");
	Check(!RecordSlotReady(s, kRecordSlots, 99), "no such slot: never read");
}

}  // namespace

int main() {
	TestRun();
	TestCut();
	TestSteps();
	TestSlots();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}

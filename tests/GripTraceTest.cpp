// Checks the grip trace (vr/GripTrace.h): when a line about a hand's grip
// signals is due - at the hand's first tracked frame, every half second
// after, and at once when the grip's click changes - and the note each
// signal carries about where it came from.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "vr/GripTrace.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// A sixteenth of a second: exact in a float, so eight frames are half a
// second to the bit.
constexpr float kFrame = 1.0f / 16.0f;

GripTraceHand Tracked(bool click = false) {
	GripTraceHand h;
	h.valid = true;
	h.click = click;
	return h;
}

// Steps `frames` frames with the trace on; how many lines `side` got, and
// the reason of the last.
int Run(GripTraceState& s, const GripTraceHand hands[2], int frames, GripTraceWhy* last = nullptr,
        int side = 0) {
	int lines = 0;
	for (int i = 0; i < frames; ++i) {
		const GripTraceVerdict v = StepGripTrace(s, hands, true, kFrame);
		if (v.why[side] != GripTraceWhy::Nothing) {
			++lines;
			if (last != nullptr) {
				*last = v.why[side];
			}
		}
	}
	return lines;
}

void TestOffAndFirst() {
	std::printf("The trace off, and a hand's first tracked frame\n");
	GripTraceState s;
	GripTraceHand hands[2] = {Tracked(), GripTraceHand{}};
	GripTraceVerdict v = StepGripTrace(s, hands, false, kFrame);
	Check(v.why[0] == GripTraceWhy::Nothing && v.why[1] == GripTraceWhy::Nothing, "off: no line");
	// Off clears what was seen: on again, the first line comes again.
	s.seen[0] = true;
	s.since[0] = 0.4f;
	StepGripTrace(s, hands, false, kFrame);
	Check(!s.seen[0] && s.since[0] == 0.0f, "off: the state cleared");
	v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::First, "on, the right hand tracked: its first line at once");
	Check(v.why[1] == GripTraceWhy::Nothing, "the left hand untracked: nothing");
	v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::Nothing, "the next frame: nothing yet");
}

void TestPeriod() {
	std::printf("A line every half second\n");
	GripTraceState s;
	GripTraceHand hands[2] = {Tracked(), Tracked()};
	const GripTraceVerdict first = StepGripTrace(s, hands, true, kFrame);
	Check(first.why[0] == GripTraceWhy::First && first.why[1] == GripTraceWhy::First,
	      "both hands tracked: a first line each");
	// Seven frames are 0.4375 s: nothing. The eighth reaches 0.5 s.
	Check(Run(s, hands, 7) == 0, "seven frames (0.44 s): no line");
	GripTraceWhy why = GripTraceWhy::Nothing;
	GripTraceVerdict v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::Period && v.why[1] == GripTraceWhy::Period,
	      "the eighth frame (0.50 s): a periodic line for each");
	Check(Run(s, hands, 7) == 0 && Run(s, hands, 1, &why) == 1 && why == GripTraceWhy::Period,
	      "and again half a second later");
	// A hand tracked later keeps its own clock: three frames behind.
	GripTraceState later;
	GripTraceHand two[2] = {Tracked(), GripTraceHand{}};
	Run(later, two, 3);
	two[1] = Tracked();
	v = StepGripTrace(later, two, true, kFrame);
	Check(v.why[1] == GripTraceWhy::First && v.why[0] == GripTraceWhy::Nothing,
	      "the left hand tracked three frames later: its first line, the right's clock untouched");
	Run(later, two, 4);
	v = StepGripTrace(later, two, true, kFrame);
	Check(v.why[0] == GripTraceWhy::Period && v.why[1] == GripTraceWhy::Nothing,
	      "the right hand's half second: its line alone");
	Run(later, two, 2);
	v = StepGripTrace(later, two, true, kFrame);
	Check(v.why[1] == GripTraceWhy::Period && v.why[0] == GripTraceWhy::Nothing,
	      "the left hand's half second, three frames after: its line alone");
}

void TestClickEdge() {
	std::printf("The click changing\n");
	GripTraceState s;
	GripTraceHand hands[2] = {Tracked(), Tracked()};
	StepGripTrace(s, hands, true, kFrame);
	Run(s, hands, 3);
	hands[0].click = true;
	GripTraceVerdict v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::Click, "the right grip closes: a line at once");
	Check(v.why[1] == GripTraceWhy::Nothing, "the left hand, unchanged: nothing");
	// The half second counts from the click line, not from the earlier one.
	Check(Run(s, hands, 7) == 0, "seven frames after the click line: nothing");
	GripTraceWhy why = GripTraceWhy::Nothing;
	Check(Run(s, hands, 1, &why) == 1 && why == GripTraceWhy::Period, "the eighth: the periodic line");
	hands[0].click = false;
	v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::Click, "the grip opens: a line at once");
	v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::Nothing, "held open: nothing more");
	// The left hand's click is its own line.
	hands[1].click = true;
	v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[1] == GripTraceWhy::Click && v.why[0] == GripTraceWhy::Nothing,
	      "the left grip closes: the left hand's line, not the right's");
}

void TestTrackingLost() {
	std::printf("A hand losing tracking\n");
	GripTraceState s;
	GripTraceHand hands[2] = {Tracked(true), Tracked()};
	StepGripTrace(s, hands, true, kFrame);
	Run(s, hands, 3);
	hands[0].valid = false;
	Check(Run(s, hands, 40) == 0, "untracked for forty frames: no line");
	Check(!s.seen[0] && s.since[0] == 0.0f, "and forgotten");
	hands[0] = Tracked(true);
	const GripTraceVerdict v = StepGripTrace(s, hands, true, kFrame);
	Check(v.why[0] == GripTraceWhy::First, "tracked again: a first line at once");
}

void TestTime() {
	std::printf("Frames of no time, of no number, and a long one\n");
	GripTraceState s;
	GripTraceHand hands[2] = {Tracked(), Tracked()};
	StepGripTrace(s, hands, true, kFrame);
	int lines = 0;
	for (int i = 0; i < 200; ++i) {
		const GripTraceVerdict v = StepGripTrace(s, hands, true, 0.0f);
		lines += v.why[0] != GripTraceWhy::Nothing ? 1 : 0;
	}
	Check(lines == 0, "two hundred frames of no time: no periodic line");
	StepGripTrace(s, hands, true, std::nanf(""));
	StepGripTrace(s, hands, true, -1.0f);
	Check(s.since[0] == 0.0f, "a frame that is not a number, or negative, counts no time");
	const GripTraceVerdict v = StepGripTrace(s, hands, true, 3.0f);
	Check(v.why[0] == GripTraceWhy::Period && s.since[0] == 0.0f, "a 3 s frame: one line, the clock restarted");
}

void TestSignalNote() {
	std::printf("Where a signal came from\n");
	Check(std::strcmp(GripSignalNote(false, 0, 7), " (legacy)") == 0, "legacy input: legacy");
	Check(std::strcmp(GripSignalNote(true, 1u << 7, 7), "") == 0, "the action bound and active: nothing to note");
	Check(std::strcmp(GripSignalNote(true, 1u << 8, 7), " (not bound)") == 0, "the action inactive: not bound");
	Check(std::strcmp(GripTraceWhyText(GripTraceWhy::First), "first") == 0 &&
	          std::strcmp(GripTraceWhyText(GripTraceWhy::Period), "every 0.5 s") == 0 &&
	          std::strcmp(GripTraceWhyText(GripTraceWhy::Click), "click changed") == 0 &&
	          std::strcmp(GripTraceWhyText(GripTraceWhy::Nothing), "") == 0,
	      "each reason has its words");
}

}  // namespace

int main() {
	TestOffAndFirst();
	TestPeriod();
	TestClickEdge();
	TestTrackingLost();
	TestTime();
	TestSignalNote();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

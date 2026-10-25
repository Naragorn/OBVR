// Checks the script clock's decisions (game/ScriptClockLogic.h): the kind a
// script's type names, the threshold the INI value becomes, the nesting of
// runs into their own time and their nested time, the per-script table with
// its overflow and its ranking, and when a frame line and a window line are
// written and what they say.

#include <cstdio>
#include <cstring>

#include "game/ScriptClockLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr long long kHz = 10000000;   // a 10 MHz counter, as Proton's and Windows' QPC
constexpr long long kMs = kHz / 1000;

ScriptRun Run(UInt32 scriptId, UInt32 refId, long long selfTicks, long long inclusiveTicks,
              ScriptKind kind = ScriptKind::Object) {
	ScriptRun run;
	run.scriptId = scriptId;
	run.refId = refId;
	run.kind = kind;
	run.selfTicks = selfTicks;
	run.inclusiveTicks = inclusiveTicks;
	return run;
}

void TestKinds() {
	std::printf("The kind a script's type names\n");
	Check(ScriptKindOf(0) == ScriptKind::Object, "type 0: an object's script");
	Check(ScriptKindOf(1) == ScriptKind::Quest, "type 1: a quest's script");
	Check(ScriptKindOf(0x100) == ScriptKind::Effect, "type 0x100: a magic effect's script");
	Check(ScriptKindOf(7) == ScriptKind::Other, "anything else: other");
	Check(std::strcmp(ScriptKindName(ScriptKind::Object), "object") == 0 &&
	          std::strcmp(ScriptKindName(ScriptKind::Quest), "quest") == 0 &&
	          std::strcmp(ScriptKindName(ScriptKind::Effect), "effect") == 0 &&
	          std::strcmp(ScriptKindName(ScriptKind::Other), "other") == 0,
	      "each kind has its word");
}

void TestThreshold() {
	std::printf("The threshold from the INI\n");
	Check(ScriptClockThresholdMs(4.0f) == 4.0f, "4 ms stays 4 ms");
	Check(ScriptClockThresholdMs(0.0f) == kScriptClockMinThresholdMs, "zero: the floor (a line every frame)");
	Check(ScriptClockThresholdMs(-3.0f) == kScriptClockMinThresholdMs, "negative: the floor");
	Check(ScriptClockThresholdMs(5000.0f) == kScriptClockMaxThresholdMs, "too large: the ceiling");
	const float nan = 0.0f / 0.0f;
	Check(ScriptClockThresholdMs(nan) == kScriptClockDefaultThresholdMs, "not a number: the default");
}

void TestStack() {
	std::printf("Runs inside runs\n");
	ScriptRunStack stack;
	ScriptRun run;
	Check(!stack.Exit(100, run), "an exit with nothing open: nothing closed");
	Check(stack.Depth() == 0, "and the depth stays zero");

	stack.Enter(0x01000001, 0x00000014, ScriptKind::Quest, 1000);
	Check(stack.Depth() == 1, "one open run");
	Check(stack.Exit(1000 + 3 * kMs, run), "closing it");
	Check(run.scriptId == 0x01000001 && run.refId == 0x00000014 && run.kind == ScriptKind::Quest,
	      "the closed run keeps its script, reference and kind");
	Check(run.inclusiveTicks == 3 * kMs && run.selfTicks == 3 * kMs,
	      "alone: its whole time is its own");

	// outer 0..10 ms, inner 2..6 ms, a second inner 7..8 ms
	stack.Enter(0xA, 1, ScriptKind::Object, 0);
	stack.Enter(0xB, 2, ScriptKind::Object, 2 * kMs);
	Check(stack.Depth() == 2, "a nested run: depth two");
	Check(stack.Exit(6 * kMs, run) && run.scriptId == 0xB && run.selfTicks == 4 * kMs &&
	          run.inclusiveTicks == 4 * kMs,
	      "the inner run closes first, with its 4 ms");
	stack.Enter(0xC, 3, ScriptKind::Object, 7 * kMs);
	Check(stack.Exit(8 * kMs, run) && run.scriptId == 0xC && run.selfTicks == 1 * kMs,
	      "a second inner run, 1 ms");
	Check(stack.Exit(10 * kMs, run) && run.scriptId == 0xA && run.inclusiveTicks == 10 * kMs &&
	          run.selfTicks == 5 * kMs,
	      "the outer run: 10 ms in all, 5 ms its own");
	Check(stack.Depth() == 0, "all closed");

	// A counter that went backwards gives no negative time.
	stack.Enter(0xD, 0, ScriptKind::Object, 5000);
	Check(stack.Exit(4000, run) && run.inclusiveTicks == 0 && run.selfTicks == 0,
	      "an exit before the entry: zero, not negative");

	// Deeper than the stack holds: the runs beyond are not timed on their own,
	// their time stays with the deepest one held.
	for (UInt32 i = 0; i < ScriptRunStack::kMaxDepth + 2; ++i) {
		stack.Enter(0x100 + i, 0, ScriptKind::Object, static_cast<long long>(i));
	}
	Check(stack.Depth() == ScriptRunStack::kMaxDepth + 2, "the depth counts past the stack");
	Check(!stack.Exit(100, run) && !stack.Exit(100, run), "the two runs beyond it close untimed");
	Check(stack.Exit(100, run) && run.scriptId == 0x100 + ScriptRunStack::kMaxDepth - 1 &&
	          run.selfTicks == 100 - static_cast<long long>(ScriptRunStack::kMaxDepth - 1),
	      "the deepest held run closes timed, the untimed ones' time its own");
	while (stack.Depth() > 0) {
		stack.Exit(200, run);
	}
	stack.Enter(0xE, 0, ScriptKind::Object, 0);
	stack.Reset();
	Check(stack.Depth() == 0 && !stack.Exit(10, run), "reset closes everything without a run");
}

void TestTable() {
	std::printf("The table of scripts\n");
	static ScriptTimeTable table;   // static: the tables are too large for a 1 MB stack
	Check(table.Scripts() == 0 && table.Runs() == 0 && table.SelfTicks() == 0, "empty at the start");

	table.Add(Run(0x01000001, 0x14, 2 * kMs, 2 * kMs, ScriptKind::Quest));
	table.Add(Run(0x01000001, 0x15, 1 * kMs, 5 * kMs, ScriptKind::Quest));
	table.Add(Run(0x02000002, 0x16, 4 * kMs, 4 * kMs));
	table.Add(Run(0x00000000, 0, 1, 1));   // a run without a script still counts
	Check(table.Scripts() == 3, "three scripts, the script-less run one of them");
	Check(table.Runs() == 4 && table.SelfTicks() == 7 * kMs + 1, "four runs, their own time summed");

	const ScriptTimeEntry* top[5] = {};
	const UInt32 count = table.Top(top, 5);
	Check(count == 3, "the top lists every script there is when fewer than asked");
	Check(top[0]->scriptId == 0x02000002 && top[1]->scriptId == 0x01000001 &&
	          top[2]->scriptId == 0x00000000,
	      "ranked by their own time, the most first");
	Check(top[1]->runs == 2 && top[1]->selfTicks == 3 * kMs, "a script's runs and own time add up");
	Check(top[1]->longestTicks == 5 * kMs && top[1]->longestRefId == 0x15,
	      "the longest run (with what it ran) and the reference it ran on");
	Check(top[1]->kind == ScriptKind::Quest, "the kind is kept");
	Check(table.Top(top, 1) == 1 && top[0]->scriptId == 0x02000002, "a top of one: the most");
	Check(table.Top(top, 0) == 0, "a top of none: nothing");

	// Equal time: the lower form ID first, so a line never reorders by chance.
	static ScriptTimeTable ties;
	ties.Add(Run(0x30, 0, kMs, kMs));
	ties.Add(Run(0x10, 0, kMs, kMs));
	ties.Add(Run(0x20, 0, kMs, kMs));
	Check(ties.Top(top, 3) == 3 && top[0]->scriptId == 0x10 && top[1]->scriptId == 0x20 &&
	          top[2]->scriptId == 0x30,
	      "equal time: by form ID");

	// More scripts than the table keeps: the rest are counted together.
	static ScriptTimeTable full;
	for (UInt32 i = 0; i < ScriptTimeTable::kMaxScripts + 10; ++i) {
		full.Add(Run(0x05000000 + i * 977, 0, kMs, kMs));
	}
	Check(full.Scripts() == ScriptTimeTable::kMaxScripts, "the table keeps its maximum of scripts");
	Check(full.OtherRuns() == 10 && full.OtherTicks() == 10 * kMs, "the ten beyond are counted as other");
	Check(full.Runs() == ScriptTimeTable::kMaxScripts + 10, "and still in the runs");
	full.Add(Run(0x05000000, 0, kMs, kMs));
	Check(full.Scripts() == ScriptTimeTable::kMaxScripts && full.OtherRuns() == 10,
	      "a script already kept still finds its entry when the table is full");

	// IDs that collide in the hash still get entries of their own.
	static ScriptTimeTable collide;
	for (UInt32 i = 0; i < 8; ++i) {
		collide.Add(Run(i * ScriptTimeTable::kSlots, 0, kMs * (i + 1), kMs));
	}
	Check(collide.Scripts() == 8 && collide.Top(top, 1) == 1 &&
	          top[0]->scriptId == 7 * ScriptTimeTable::kSlots,
	      "colliding IDs keep their own entries");

	table.Clear();
	Check(table.Scripts() == 0 && table.Runs() == 0 && table.SelfTicks() == 0 && table.OtherRuns() == 0 &&
	          table.Top(top, 5) == 0,
	      "clear empties it");
	table.Add(Run(0x01000001, 0x14, kMs, kMs));
	Check(table.Scripts() == 1 && table.Top(top, 5) == 1 && top[0]->runs == 1,
	      "and a script after the clear starts from nothing");
}

void TestFrameLines() {
	std::printf("The lines at the end of a frame\n");
	static ScriptClock clock;
	char frameLine[1024];
	char windowLine[1024];

	// A quiet frame: 1 ms of scripts against a 4 ms threshold.
	clock.Enter(0x01000001, 0x14, ScriptKind::Quest, 0);
	clock.Exit(kMs);
	ScriptClockLines lines = clock.FrameEnd(2 * kMs, kHz, 4.0, 0, frameLine, sizeof(frameLine),
	                                        windowLine, sizeof(windowLine));
	Check(!lines.frameLine && !lines.windowLine, "under the threshold, inside the window: no line");

	// A long frame: one script 60 ms in 1000 runs, another 9 ms in one run.
	long long t = 10 * kMs;
	for (int i = 0; i < 1000; ++i) {
		clock.Enter(0x1F012345, 0x00000014, ScriptKind::Quest, t);
		t += 6 * kMs / 100;
		clock.Exit(t);
	}
	clock.Enter(0x1F00ABCD, 0x1F00EEEE, ScriptKind::Object, t);
	t += 9 * kMs;
	clock.Exit(t);
	lines = clock.FrameEnd(t, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.frameLine && !lines.windowLine, "over the threshold: a frame line");
	Check(std::strstr(frameLine, "69.0 ms") != nullptr && std::strstr(frameLine, "1001 runs") != nullptr &&
	          std::strstr(frameLine, "2 scripts") != nullptr,
	      "the line says the frame's script time, runs and scripts");
	const char* first = std::strstr(frameLine, "1F012345 quest 60.0 ms in 1000 runs");
	const char* second = std::strstr(frameLine, "1F00ABCD object 9.0 ms in 1 run,");
	Check(first != nullptr && second != nullptr && first < second, "the scripts, the most time first");
	Check(std::strstr(frameLine, "the longest 9.00 ms on 1F00EEEE") != nullptr,
	      "a script's longest run and its reference");
	Check(std::strstr(frameLine, "the longest 0.06 ms on 00000014") != nullptr,
	      "the many short runs show as short");
	char tick[64];
	std::snprintf(tick, sizeof(tick), "at tick %lld", t);
	Check(std::strstr(frameLine, tick) != nullptr, "the frame's end tick, to find it in a capture");

	// A run without a reference says so.
	clock.Enter(0x01000002, 0, ScriptKind::Quest, t);
	t += 5 * kMs;
	clock.Exit(t);
	lines = clock.FrameEnd(t, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.frameLine && std::strstr(frameLine, "without a reference") != nullptr,
	      "no reference: said in words");

	// The frame table starts again after each frame.
	lines = clock.FrameEnd(t + kMs, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(!lines.frameLine, "the next frame without scripts: no line");

	// Exactly the threshold is over it.
	clock.Enter(0x01000003, 0, ScriptKind::Quest, t);
	clock.Exit(t + 4 * kMs);
	lines = clock.FrameEnd(t + 5 * kMs, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.frameLine, "exactly the threshold: a line");

	// No frequency: nothing can be said in milliseconds.
	clock.Enter(0x01000003, 0, ScriptKind::Quest, t);
	clock.Exit(t + 400 * kMs);
	lines = clock.FrameEnd(t + 500 * kMs, 0, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(!lines.frameLine && !lines.windowLine, "a frequency of zero: no line at all");

	// A buffer too small for the line: no line, rather than a cut one.
	static ScriptClock small;
	small.Enter(0x01000003, 0, ScriptKind::Quest, 0);
	small.Exit(50 * kMs);
	lines = small.FrameEnd(50 * kMs, kHz, 4.0, 0, frameLine, 24, windowLine, sizeof(windowLine));
	Check(!lines.frameLine, "a short buffer: no frame line");

	// More scripts than a line names: the rest summed at its end.
	static ScriptClock many;
	long long m = 0;
	for (UInt32 i = 0; i < ScriptClock::kTop + 3; ++i) {
		many.Enter(0x01000100 + i, 0, ScriptKind::Object, m);
		m += kMs;
		many.Exit(m);
	}
	lines = many.FrameEnd(m, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.frameLine && std::strstr(frameLine, "3 more scripts 3.0 ms") != nullptr,
	      "the scripts beyond the line's five: counted with their time");
}

void TestLineBudget() {
	std::printf("The budget of frame lines in a window\n");
	static ScriptClock clock;
	char frameLine[1024];
	char windowLine[1024];
	long long t = 0;
	UInt32 written = 0;
	for (UInt32 frame = 0; frame < ScriptClock::kFrameLinesPerWindow + 5; ++frame) {
		clock.Enter(0x01000001, 0, ScriptKind::Quest, t);
		t += 5 * kMs;
		clock.Exit(t);
		const ScriptClockLines lines =
			clock.FrameEnd(t, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
		if (lines.frameLine) ++written;
	}
	Check(written == ScriptClock::kFrameLinesPerWindow, "every frame over: only the budget's lines");

	// The window closes ten seconds after it opened; its line counts the
	// frames over the threshold and the lines held back.
	t = 11 * kHz;
	const ScriptClockLines lines =
		clock.FrameEnd(t, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.windowLine, "past ten seconds: a window line");
	Check(std::strstr(windowLine, "36 frames") != nullptr, "the window's frames");
	Check(std::strstr(windowLine, "35 frames at 4.0 ms or more (5 of their lines held back)") != nullptr,
	      "the frames over the threshold and the lines held back");
	Check(std::strstr(windowLine, "the longest frame's 5.0 ms") != nullptr, "the longest frame's scripts");
	Check(std::strstr(windowLine, "01000001 quest 175.0 ms in 35 runs") != nullptr,
	      "the window's scripts, summed over its frames");
	Check(std::strstr(windowLine, "off the game's thread") == nullptr,
	      "no word of other threads when none ran there");

	// The next window starts empty, and its budget is full again.
	clock.Enter(0x01000001, 0, ScriptKind::Quest, t);
	clock.Exit(t + 5 * kMs);
	const ScriptClockLines again =
		clock.FrameEnd(t + 6 * kMs, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(again.frameLine && !again.windowLine, "a new window: frame lines again");
}

void TestWindow() {
	std::printf("The window line\n");
	static ScriptClock clock;
	char frameLine[1024];
	char windowLine[1024];
	// The window opens at the first frame end.
	ScriptClockLines lines =
		clock.FrameEnd(5 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(!lines.windowLine, "the first frame opens the window, no line");
	clock.Enter(0x01000001, 0x14, ScriptKind::Quest, 6 * kHz);
	clock.Exit(6 * kHz + 2 * kMs);
	lines = clock.FrameEnd(6 * kHz + 3 * kMs, kHz, 4.0, 7, frameLine, sizeof(frameLine), windowLine,
	                       sizeof(windowLine));
	Check(!lines.windowLine, "one second in: no window line");
	lines = clock.FrameEnd(15 * kHz, kHz, 4.0, 3, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.windowLine && !lines.frameLine, "ten seconds after it opened: the window line, no frame line");
	Check(std::strstr(windowLine, "10.0 s, 3 frames") != nullptr, "its length and frames");
	Check(std::strstr(windowLine, "0.67 ms a frame on average") != nullptr,
	      "the scripts' time a frame on average");
	Check(std::strstr(windowLine, "0 frames at 4.0 ms or more (0 of their lines held back)") != nullptr,
	      "no frame over the threshold");
	Check(std::strstr(windowLine, "10 runs off the game's thread not timed") != nullptr,
	      "the runs on other threads, summed over the window");
	Check(std::strstr(windowLine, "01000001 quest 2.0 ms in 1 run,") != nullptr, "the script that ran");

	// A window without any script run still has its line.
	lines = clock.FrameEnd(26 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.windowLine && std::strstr(windowLine, "no script ran") != nullptr,
	      "an empty window: said so");

	// A frame line can come with a window line in the same frame.
	clock.Enter(0x01000001, 0x14, ScriptKind::Quest, 30 * kHz);
	clock.Exit(30 * kHz + 8 * kMs);
	lines = clock.FrameEnd(37 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.frameLine && lines.windowLine, "a long frame closing a window: both lines");

	// A buffer too small for the window line: no line, and the window still restarts.
	clock.Enter(0x01000001, 0x14, ScriptKind::Quest, 40 * kHz);
	clock.Exit(40 * kHz + kMs);
	lines = clock.FrameEnd(48 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, 24);
	Check(!lines.windowLine, "a short buffer: no window line");
	lines = clock.FrameEnd(49 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(!lines.windowLine, "the window restarted all the same");

	// Reset forgets the window and every open run.
	clock.Enter(0x01000001, 0x14, ScriptKind::Quest, 50 * kHz);
	clock.Reset();
	clock.Exit(50 * kHz + 50 * kMs);   // the exit of the run open at the reset
	lines = clock.FrameEnd(51 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(!lines.frameLine && !lines.windowLine, "after a reset: the run open across it is not counted");
	lines = clock.FrameEnd(62 * kHz, kHz, 4.0, 0, frameLine, sizeof(frameLine), windowLine, sizeof(windowLine));
	Check(lines.windowLine && std::strstr(windowLine, "2 frames") != nullptr,
	      "and the window reopened at the first frame after it");
}

}  // namespace

int main() {
	TestKinds();
	TestThreshold();
	TestStack();
	TestTable();
	TestFrameLines();
	TestLineBudget();
	TestWindow();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

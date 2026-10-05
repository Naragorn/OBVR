// Checks when the grip's target is marked (game/TargetMarker.h).

#include <cstdio>

#include "game/TargetMarker.h"

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

}  // namespace

int main() {
	std::printf("The mark on the target\n");
	MarkState s;
	MarkStep m;
	for (UInt32 i = 1; i < kMarkSettleFrames; ++i) {
		m = StepMark(s, 0x100);
		Check(m.start == 0 && m.stop == 0, "a new target: not marked until it settles");
	}
	m = StepMark(s, 0x100);
	Check(m.start == 0x100 && m.stop == 0 && s.marked == 0x100, "settled: marked");
	m = StepMark(s, 0x100);
	Check(m.start == 0 && m.stop == 0, "still the same: nothing to do");
	StepMark(s, 0x200);
	m = StepMark(s, 0x100);
	Check(m.start == 0 && m.stop == 0 && s.marked == 0x100, "a frame on another and back: the mark stays");
	for (UInt32 i = 1; i < kMarkSettleFrames; ++i) {
		m = StepMark(s, 0x200);
	}
	m = StepMark(s, 0x200);
	Check(m.start == 0x200 && m.stop == 0x100, "another one settled: the mark moves");
	for (UInt32 i = 0; i < kMarkSettleFrames; ++i) {
		m = StepMark(s, 0);
	}
	Check(s.marked == 0 && m.stop == 0x200 && m.start == 0, "nothing wanted for a while: unmarked");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

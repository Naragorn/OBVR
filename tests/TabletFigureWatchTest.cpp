// Checks the watch on the tablet's figure (vr/TabletFigureWatch.h): said
// once in 90 frames when its rectangle moved or was not read, quiet while
// it stands still, started over when it is not wanted, and said no more
// than its lines allow.

#include <cstdio>

#include "vr/TabletFigureWatch.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

UiRect Rect(float left, float top, float right, float bottom) {
	UiRect r;
	r.left = left;
	r.top = top;
	r.right = right;
	r.bottom = bottom;
	r.valid = true;
	return r;
}

// `frames` frames of the same rectangle; the last frame's report.
FigureWatchReport Hold(FigureWatchState& s, const UiRect& r, UInt32 frames, bool* said = nullptr) {
	FigureWatchReport last;
	for (UInt32 i = 0; i < frames; ++i) {
		last = StepFigureWatch(s, true, r);
		if (said != nullptr && last.say) {
			*said = true;
		}
	}
	return last;
}

void TestStill() {
	std::printf("A figure standing still\n");
	FigureWatchState s;
	bool said = false;
	Hold(s, Rect(2200.0f, 0.0f, 3685.0f, 2270.0f), kFigureWatchFrames * 3, &said);
	Check(!said, "the same rectangle for 270 frames: nothing said");
	const UiRect wobble = Rect(2200.4f, 0.0f, 3685.4f, 2270.0f);
	const FigureWatchReport r = Hold(s, wobble, kFigureWatchFrames);
	Check(!r.say, "moved under a layer pixel: no move");
}

void TestMoves() {
	std::printf("A figure whose cut moves\n");
	FigureWatchState s;
	FigureWatchReport r;
	for (UInt32 i = 0; i < kFigureWatchFrames; ++i) {
		// Read anew every 15 frames, 4 pixels to the side and back.
		const float shift = (i / 15) % 2 == 0 ? 0.0f : 4.0f;
		r = StepFigureWatch(s, true, Rect(2200.0f + shift, 0.0f, 3685.0f, 2270.0f));
		if (i + 1 < kFigureWatchFrames) {
			Check(!r.say, "before the 90th frame: not said yet");
			if (r.say) {
				break;
			}
		}
	}
	Check(r.say && r.frames == kFigureWatchFrames && r.moves == 5 && r.drops == 0 && r.mostPixels == 4.0f,
	      "the 90th frame: said - 5 moves of 4 pixels, nothing dropped");
	r = Hold(s, Rect(2204.0f, 0.0f, 3685.0f, 2270.0f), kFigureWatchFrames);
	Check(!r.say, "the next 90 frames where it last stood: the counts started over, nothing said");
}

void TestDrops() {
	std::printf("A figure not read\n");
	FigureWatchState s;
	const UiRect figure = Rect(2200.0f, 0.0f, 3685.0f, 2270.0f);
	FigureWatchReport r;
	for (UInt32 i = 0; i < kFigureWatchFrames; ++i) {
		r = StepFigureWatch(s, true, i >= 30 && i < 45 ? UiRect{} : figure);
	}
	Check(r.say && r.drops == 15 && r.moves == 0, "15 frames unread among 90: said, no move counted across the gap");
}

void TestNotWanted() {
	std::printf("The figure not wanted\n");
	FigureWatchState s;
	Hold(s, Rect(2200.0f, 0.0f, 3685.0f, 2270.0f), 40);
	const FigureWatchReport off = StepFigureWatch(s, false, UiRect{});
	Check(!off.say && s.frames == 0 && !s.seen, "the pages closed: nothing said, the count starts over");
	FigureWatchReport r = Hold(s, Rect(2300.0f, 0.0f, 3785.0f, 2270.0f), kFigureWatchFrames);
	Check(!r.say, "opened again where the figure now stands: no move against the last opening");
}

void TestLines() {
	std::printf("Said a limited number of times\n");
	FigureWatchState s;
	UInt32 said = 0;
	for (UInt32 window = 0; window < kFigureWatchLines + 5; ++window) {
		for (UInt32 i = 0; i < kFigureWatchFrames; ++i) {
			const float shift = i % 2 == 0 ? 0.0f : 2.0f;
			said += StepFigureWatch(s, true, Rect(2200.0f + shift, 0.0f, 3685.0f, 2270.0f)).say ? 1u : 0u;
		}
	}
	Check(said == kFigureWatchLines, "a figure moving every frame: said 40 times, then no more");
	StepFigureWatch(s, false, UiRect{});
	Check(s.linesLeft == 0, "closing does not give the lines back");
}

}  // namespace

int main() {
	TestStill();
	TestMoves();
	TestDrops();
	TestNotWanted();
	TestLines();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

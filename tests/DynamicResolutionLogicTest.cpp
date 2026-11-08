// Dynamic resolution's decisions (render/DynamicResolutionLogic.h): the scale
// from the GPU's time against its budget, its steps, holds and bounds, and
// the layout a scale means - the eye's extents and the camera port whose
// truncation by the engine lands exactly on them.

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "render/DynamicResolutionLogic.h"

using namespace obvr::render::dynres;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void Section(const char* what) {
	std::printf("%s\n", what);
}

bool Near(float a, float b, float tolerance = 1e-4f) {
	return std::fabs(a - b) <= tolerance;
}

Settings On() {
	Settings s;
	s.enabled = true;
	return s;
}

struct Rect {
	int left;
	int top;
	int right;
	int bottom;
};

void TestBounds() {
	Section("The settings are bounded");
	Check(ClampMinScale(0.1f) == kMinScaleFloor && ClampMinScale(-1.0f) == kMinScaleFloor, "a minimum below the floor reads as the floor");
	Check(ClampMinScale(0.5f) == 0.5f && ClampMinScale(2.0f) == 1.0f, "the minimum within its range, capped at one");
	Check(ClampTargetShare(0.0f) == 0.1f && ClampTargetShare(0.85f) == 0.85f && ClampTargetShare(1.5f) == 1.0f, "the budget share within a tenth and one");
	Check(Near(Quantize(0.76f, 0.5f), 0.75f) && Near(Quantize(0.79f, 0.5f), 0.78125f), "the scale snaps to 1/32 steps");
	Check(Quantize(0.3f, 0.5f) == 0.5f && Quantize(1.3f, 0.5f) == 1.0f, "and stays inside the minimum and one");
}

void TestUpdate() {
	Section("Off: the scale is one and nothing moves");
	State state;
	state.scale = 0.7f;
	Settings off;
	Check(Update(state, off, 20.0f, 11.1f) == Move::kReset && state.scale == 1.0f, "turning off puts the scale back to one, once");
	Check(Update(state, off, 20.0f, 11.1f) == Move::kNone && state.scale == 1.0f, "then nothing");

	Section("Unknown times leave the scale alone");
	State s2;
	Check(Decide(s2, On(), 0.0f, 11.1f) == Move::kNone && s2.scale == 1.0f, "no GPU time");
	Check(Decide(s2, On(), 9.0f, 0.0f) == Move::kNone && s2.scale == 1.0f, "no period");

	Section("Over the budget: down, by what the square root asks, at most an eighth a frame");
	State s3;
	const Settings on = On();  // budget 0.85 * 11.1 = 9.435 ms
	Check(Decide(s3, on, 10.5f, 11.1f) == Move::kDown && Near(s3.scale, 0.9375f), "10.5 of 9.4 ms: sqrt(9.435/10.5) = 0.948, snapped to 0.9375");
	State s4;
	Check(Decide(s4, on, 40.0f, 11.1f) == Move::kDown && Near(s4.scale, 0.875f), "40 ms would ask for 0.49 at once: an eighth only");
	Check(Decide(s4, on, 40.0f, 11.1f) == Move::kDown && Near(s4.scale, 0.75f), "the next frame another eighth");
	Settings floor = On();
	floor.minScale = 0.75f;
	Check(Decide(s4, floor, 40.0f, 11.1f) == Move::kNone && Near(s4.scale, 0.75f), "at the minimum: no move");

	Section("Under three quarters of the budget for fifteen frames: up one step");
	State s5;
	s5.scale = 0.75f;
	Move last = Move::kNone;
	for (unsigned i = 0; i < kUpHoldFrames - 1; ++i) {
		last = Decide(s5, on, 5.0f, 11.1f);
	}
	Check(last == Move::kNone && Near(s5.scale, 0.75f), "fourteen quiet frames: still waiting");
	Check(Decide(s5, on, 5.0f, 11.1f) == Move::kUp && Near(s5.scale, 0.78125f), "the fifteenth: one step up");
	for (unsigned i = 0; i < kUpHoldFrames - 1; ++i) {
		Decide(s5, on, 5.0f, 11.1f);
	}
	Check(Decide(s5, on, 8.0f, 11.1f) == Move::kNone && s5.underBudgetFrames == 0 && Near(s5.scale, 0.78125f),
	      "a frame between three quarters and the budget resets the hold");
	State s6;
	for (unsigned i = 0; i < kUpHoldFrames; ++i) {
		last = Decide(s6, on, 5.0f, 11.1f);
	}
	Check(last == Move::kNone && s6.scale == 1.0f, "at one there is nowhere up: no move");

	Section("The readings: implausible ones left out, the median of five decides");
	State s7;
	Check(!Feed(s7, 0.13f, 13.3f) && !Feed(s7, 46.0f, 13.3f) && !Feed(s7, 5.0f, 0.0f) && s7.sampleCount == 0,
	      "an empty frame, a hitch and an unknown period are not readings");
	Check(Feed(s7, 7.0f, 13.3f) && Feed(s7, 7.2f, 13.3f) && Feed(s7, 20.0f, 13.3f) && Feed(s7, 7.1f, 13.3f) && MedianGpuMs(s7) == 0.0f,
	      "four readings: no median yet");
	Check(Feed(s7, 6.9f, 13.3f) && Near(MedianGpuMs(s7), 7.1f), "five: the median, the 20 ms outlier outvoted");
	Check(Feed(s7, 6.8f, 13.3f) && Near(MedianGpuMs(s7), 7.1f), "the sixth replaces the oldest (7.0): 6.8 6.9 7.1 7.2 20, the median stays 7.1");
	State s8;
	for (int i = 0; i < 4; ++i) {
		Check(Update(s8, on, 30.0f, 11.1f) == Move::kNone && s8.scale == 1.0f, i == 0 ? "Update: the first four heavy frames move nothing yet" : "...");
	}
	Check(Update(s8, on, 30.0f, 11.1f) == Move::kDown && Near(s8.scale, 0.875f), "the fifth: the median is over the budget, down");
	Check(Update(s8, on, 0.1f, 11.1f) == Move::kDown && Near(s8.scale, 0.75f), "an empty frame is left out, the median still says down");
}

void TestLayout() {
	Section("The layout at the full scale is the engine's left half");
	const Layout full = LayoutFor(5136, 2568, 1.0f);
	Check(full.full && full.width == 2568 && full.height == 2568 && full.portRight == 0.5f && full.portBottom == 0.0f, "the whole half, the port untouched");

	Section("A scaled layout: extents rounded, the port edges half a pixel past them");
	const Layout l = LayoutFor(5136, 2568, 0.75f);
	Check(!l.full && l.width == 1926 && l.height == 1926, "three quarters of 2568 is 1926");
	Check(EngineExtent(l.portRight - 0.0f, 5136) == 1926, "the engine's truncation of (right - left) * W lands on the width");
	Check(EngineExtent(1.0f - l.portBottom, 2568) == 1926, "and of (top - bottom) * H on the height");
	Check(EngineExtent(1.0f - 1.0f, 2568) == 0, "the top edge at one: Y = 0");
	bool everyScale = true;
	for (int step = 8; step <= 32; ++step) {
		const float scale = static_cast<float>(step) / 32.0f;
		for (std::uint32_t width : {5136u, 8072u, 4096u, 2570u}) {
			for (std::uint32_t height : {2568u, 3376u, 1444u, 2049u}) {
				const Layout x = LayoutFor(width, height, scale);
				everyScale = everyScale && EngineExtent(x.portRight, width) == x.width && EngineExtent(1.0f - x.portBottom, height) == x.height;
			}
		}
	}
	Check(everyScale, "for every step of the scale and four frame sizes the truncation lands on the extents");
	Check(ScaledExtent(100, 0.004f) == 1 && ScaledExtent(0, 0.5f) == 0 && ScaledExtent(100, 1.2f) == 100, "an extent is at least a pixel and at most the whole");
	const Layout tiny = LayoutFor(4, 4, 0.25f);
	Check(tiny.width == 1 && tiny.height == 1 && !tiny.full, "a tiny frame still has a pixel");

	Section("A crop cut to the layout keeps its corner");
	const Rect left{0, 0, 2568, 2568};
	const Rect right{2568, 0, 5136, 2568};
	const Rect cutLeft = CroppedTo(left, l);
	const Rect cutRight = CroppedTo(right, l);
	Check(cutLeft.left == 0 && cutLeft.top == 0 && cutLeft.right == 1926 && cutLeft.bottom == 1926, "the left eye's crop");
	Check(cutRight.left == 2568 && cutRight.right == 2568 + 1926 && cutRight.bottom == 1926, "the right eye's crop at the right half's edge");
	const Rect whole = CroppedTo(left, full);
	Check(whole.right == 2568 && whole.bottom == 2568, "the full layout leaves the crop whole");
}

}  // namespace

int main() {
	TestBounds();
	TestUpdate();
	TestLayout();
	std::printf("%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures, g_failures == 1 ? "" : "s");
	return g_failures;
}

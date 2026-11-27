// Checks the figure's rectangle held steady for the tablet
// (vr/TabletFigureSteady.h).
//
// Nadi's Windows run of 2026-11-27: the edge round the figure on its own
// page flickered, and the figure behind its tab too. The run's log has three
// reads of the breathing figure - 0.545..0.913, 0.544..0.913, 0.545..0.913 of
// the screen - and the page and the tab are laid out from each read anew.
// The first test lays those reads out as the tablet did and as it does now.

#include <cstdio>

#include "vr/TabletFigureSteady.h"
#include "vr/TabletLogic.h"

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

bool Same(const UiRect& a, const UiRect& b) {
	return a.valid == b.valid && a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

// The layer of the run: 4036 x 2270 pixels of 2D, the figure over its whole height.
const float kWidth = 4036.0f;
const float kHeight = 2270.0f;
const UiRect kScreen = Rect(0.0f, 0.0f, kWidth, kHeight);

UiRect RunRead(float leftShare) { return Rect(leftShare * kWidth, 0.0f, 0.913f * kWidth, kHeight); }

// The tablet's frame with the page up, as the layout functions want it.
TabletFrame PageUp() {
	TabletFrame f;
	f.up = true;
	f.paper = Rect(100.0f, 150.0f, 1860.0f, 2180.0f);
	f.pageCrop = f.paper;
	f.crop = f.paper;
	return f;
}

void TestTheRunsReads() {
	std::printf("The run's reads, laid out behind the tab and on the figure's page\n");
	const float reads[3] = {0.545f, 0.544f, 0.545f};

	// As the tablet did: each read laid out as it came.
	TabletFrame tabA = PageUp();
	TabletFrame tabB = PageUp();
	SetFigureTab(tabA, RunRead(reads[0]));
	SetFigureTab(tabB, RunRead(reads[1]));
	Check(tabA.figureTab && tabB.figureTab, "both reads laid out behind the tab");
	Check(!Same(tabA.figureFit, tabB.figureFit),
	      "the reads as they came: the figure fitted anew, another place and size (the failing run)");
	TabletFrame pageA = PageUp();
	TabletFrame pageB = PageUp();
	SetFigurePage(pageA, RunRead(reads[0]));
	SetFigurePage(pageB, RunRead(reads[1]));
	Check(!Same(pageA.backing, pageB.backing),
	      "the reads as they came: the figure's page another width (the failing run)");

	// Held steady.
	FigureSteadyState s;
	UiRect steady[3];
	for (int i = 0; i < 3; ++i) {
		steady[i] = StepFigureSteady(s, RunRead(reads[i]), kScreen);
	}
	Check(Same(steady[0], steady[1]) && Same(steady[1], steady[2]), "held steady: the three reads give one rectangle");
	TabletFrame tabC = PageUp();
	TabletFrame tabD = PageUp();
	SetFigureTab(tabC, steady[0]);
	SetFigureTab(tabD, steady[1]);
	Check(Same(tabC.figureFit, tabD.figureFit), "held steady: the figure behind its tab stands still");
	TabletFrame pageC = PageUp();
	TabletFrame pageD = PageUp();
	SetFigurePage(pageC, steady[0]);
	SetFigurePage(pageD, steady[1]);
	Check(Same(pageC.backing, pageD.backing) && Same(pageC.figureAt, pageD.figureAt),
	      "held steady: the figure's page keeps its edge");
}

void TestTaking() {
	std::printf("Taking a rectangle\n");
	FigureSteadyState s;
	const UiRect read = Rect(1000.0f, 200.0f, 1600.0f, 1200.0f);
	const UiRect taken = StepFigureSteady(s, read, kScreen);
	const float pad = kFigureSteadyPad * 1000.0f;
	Check(taken.valid && taken.left == 1000.0f - pad && taken.top == 200.0f - pad && taken.right == 1600.0f + pad &&
	          taken.bottom == 1200.0f + pad,
	      "the first read: taken with the breath's room on every side");
	FigureSteadyState atEdge;
	const UiRect full = StepFigureSteady(atEdge, Rect(2200.0f, 0.0f, kWidth, kHeight), kScreen);
	Check(full.top == 0.0f && full.bottom == kHeight && full.right == kWidth && full.left < 2200.0f,
	      "at the screen's edges: the room ends at the screen");
	FigureSteadyState noBounds;
	const UiRect open = StepFigureSteady(noBounds, Rect(10.0f, 0.0f, 500.0f, 900.0f), UiRect{});
	Check(open.top < 0.0f, "no bounds given: the room is not cut");
}

void TestKeepingAndLeaving() {
	std::printf("Keeping it, and taking it anew\n");
	FigureSteadyState s;
	const UiRect first = StepFigureSteady(s, Rect(1000.0f, 200.0f, 1600.0f, 1200.0f), kScreen);
	Check(Same(StepFigureSteady(s, Rect(1004.0f, 203.0f, 1597.0f, 1196.0f), kScreen), first),
	      "a breath in: the kept one");
	Check(Same(StepFigureSteady(s, Rect(995.0f, 195.0f, 1606.0f, 1208.0f), kScreen), first),
	      "a breath out, inside the room: the kept one");
	const UiRect wider = StepFigureSteady(s, Rect(960.0f, 200.0f, 1640.0f, 1200.0f), kScreen);
	Check(!Same(wider, first) && wider.left < 960.0f && wider.right > 1640.0f,
	      "turned, wider than the kept one: taken anew");
	Check(Same(StepFigureSteady(s, Rect(962.0f, 201.0f, 1638.0f, 1199.0f), kScreen), wider), "and that one kept");
	const float slack = kFigureSteadySlack * 1000.0f;
	const UiRect nearly = Rect(wider.left + slack - 1.0f, 200.0f, 1640.0f, 1200.0f);
	Check(Same(StepFigureSteady(s, nearly, kScreen), wider), "an edge just inside the slack: still the kept one");
	const UiRect narrow = StepFigureSteady(s, Rect(1200.0f, 200.0f, 1400.0f, 1200.0f), kScreen);
	Check(!Same(narrow, wider) && narrow.left > 1150.0f && narrow.right < 1450.0f,
	      "turned side on, much narrower: taken anew");
	const UiRect moved = StepFigureSteady(s, Rect(2200.0f, 200.0f, 2400.0f, 1200.0f), kScreen);
	Check(moved.left > 2100.0f, "the figure somewhere else: taken anew");
}

void TestNotRead() {
	std::printf("A read that is none\n");
	FigureSteadyState s;
	const UiRect first = StepFigureSteady(s, Rect(1000.0f, 200.0f, 1600.0f, 1200.0f), kScreen);
	UiRect none;
	Check(!StepFigureSteady(s, none, kScreen).valid, "not read: passed on as not valid");
	Check(Same(StepFigureSteady(s, Rect(1001.0f, 201.0f, 1599.0f, 1199.0f), kScreen), first),
	      "read again where it was: the kept one, not taken anew");
	UiRect flat = Rect(1000.0f, 200.0f, 1600.0f, 200.0f);
	Check(Same(StepFigureSteady(s, flat, kScreen), flat), "no height: passed on as it is");
	UiRect thin = Rect(1000.0f, 200.0f, 1000.0f, 1200.0f);
	Check(Same(StepFigureSteady(s, thin, kScreen), thin), "no width: passed on as it is");
	s = FigureSteadyState{};
	const UiRect fresh = StepFigureSteady(s, Rect(1004.0f, 203.0f, 1597.0f, 1196.0f), kScreen);
	Check(!Same(fresh, first), "started over (the tablet put down): taken from the read");
}

}  // namespace

int main() {
	TestTheRunsReads();
	TestTaking();
	TestKeepingAndLeaving();
	TestNotRead();
	if (g_failures != 0) {
		std::printf("\n%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("\nall checks passed\n");
	return 0;
}

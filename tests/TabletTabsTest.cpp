// Checks the tablet's tab strip (vr/TabletTabs.h, vr/TabletTabStrip.h): which
// tabs there are for each [Tablet] Figure mode and the Mirror setting, where
// the strip stands, which tab a click picks, what a page turn does to the
// Mirror tab, and what a press on the Mirror page reaches (nothing).

#include <cstdio>
#include <cstring>

#include "vr/TabletTabStrip.h"

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

bool Near(float a, float b, float tolerance = 0.01f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

bool SameRect(const UiRect& r, float left, float top, float right, float bottom) {
	return r.valid && Near(r.left, left) && Near(r.top, top) && Near(r.right, right) && Near(r.bottom, bottom);
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

TabsWanted Wanted(bool figureTab, bool mirror) {
	TabsWanted w;
	w.up = true;
	w.inScene = true;
	w.figureTab = figureTab;
	w.mirror = mirror;
	return w;
}

bool Is(const TabSet& set, UInt32 count, TabletTab a, TabletTab b = TabletTab::Page, TabletTab c = TabletTab::Page) {
	return set.count == count && (count < 1 || set.kind[0] == a) && (count < 2 || set.kind[1] == b) &&
	       (count < 3 || set.kind[2] == c);
}

void TestWhichTabs() {
	std::printf("Which tabs there are\n");
	Check(Is(TabsFor(Wanted(true, true)), 3, TabletTab::Page, TabletTab::Figure, TabletTab::Mirror),
	      "Figure=tab with a figure and the mirror: page, figure, mirror - the mirror last");
	Check(Is(TabsFor(Wanted(false, true)), 2, TabletTab::Page, TabletTab::Mirror),
	      "Figure=beside or off: page, mirror");
	Check(Is(TabsFor(Wanted(true, false)), 2, TabletTab::Page, TabletTab::Figure),
	      "Figure=tab, [Tablet] Mirror=0: page, figure - as before");
	Check(TabsFor(Wanted(false, false)).count == 0, "no figure tab and no mirror: no strip, as before");
	TabsWanted down = Wanted(true, true);
	down.up = false;
	Check(TabsFor(down).count == 0, "the tablet not up (disabled, or closed): no strip, so no mirror there");
	TabsWanted overlay = Wanted(true, true);
	overlay.inScene = false;
	Check(TabsFor(overlay).count == 0, "[Tablet] InScene=0: the strip is drawn in the scene only - none");
	const TabSet all = TabsFor(Wanted(true, true));
	Check(all.Has(TabletTab::Mirror) && all.Has(TabletTab::Figure) && all.Has(TabletTab::Page), "Has finds each");
	Check(!TabsFor(Wanted(false, true)).Has(TabletTab::Figure), "Has: no figure tab where there is none");
	Check(!TabsFor(Wanted(false, false)).Has(TabletTab::Page), "Has: nothing in an empty set");
}

TabletFrame FrameWith(const TabSet& set) {
	TabletFrame f;
	f.up = true;
	f.paper = Rect(100.0f, 200.0f, 1100.0f, 1200.0f);
	f.pageCrop = f.paper;
	f.crop = f.paper;
	SetTabs(f, set);
	return f;
}

void TestStrip() {
	std::printf("The strip over the page\n");
	const TabletFrame three = FrameWith(TabsFor(Wanted(true, true)));
	Check(three.tabCount == 3 && three.tabKind[2] == TabletTab::Mirror, "three tabs, the mirror's the last");
	Check(SameRect(three.tabs[0], 150.0f, 110.0f, 430.0f, 200.0f) && SameRect(three.tabs[1], 480.0f, 110.0f, 760.0f, 200.0f) &&
	          SameRect(three.tabs[2], 810.0f, 110.0f, 1090.0f, 200.0f),
	      "standing on the page's top edge, 28 percent wide, the third inside the page's width");
	Check(SameRect(three.crop, 100.0f, 110.0f, 1100.0f, 1200.0f), "the face grows up by the tabs, not wider");
	const TabletFrame two = FrameWith(TabsFor(Wanted(false, true)));
	Check(two.tabCount == 2 && two.tabKind[1] == TabletTab::Mirror &&
	          SameRect(two.tabs[1], 480.0f, 110.0f, 760.0f, 200.0f),
	      "page and mirror: the mirror's tab in the second place");

	TabletFrame beside;
	beside.up = true;
	beside.paper = Rect(100.0f, 200.0f, 1100.0f, 1200.0f);
	beside.pageCrop = beside.paper;
	beside.crop = Rect(100.0f, 200.0f, 2000.0f, 1200.0f);  // the figure's page beside it
	SetTabs(beside, TabsFor(Wanted(false, true)));
	Check(SameRect(beside.crop, 100.0f, 110.0f, 2000.0f, 1200.0f), "the figure's page beside: its width kept, the tabs add height");

	TabletFrame none;
	SetTabs(none, TabsFor(Wanted(true, true)));
	Check(none.tabCount == 0, "the tablet not up: no tabs");
	TabletFrame noPaper = FrameWith(TabSet{});
	noPaper.paper = UiRect{};
	SetTabs(noPaper, TabsFor(Wanted(true, true)));
	Check(noPaper.tabCount == 0, "no page read: no tabs");
	TabletFrame flat = FrameWith(TabSet{});
	flat.paper = Rect(100.0f, 200.0f, 100.0f, 1200.0f);
	SetTabs(flat, TabsFor(Wanted(true, true)));
	Check(flat.tabCount == 0, "a page without a width: no tabs");
	TabletFrame single = FrameWith(TabSet{});
	TabSet one;
	one.count = 1;
	SetTabs(single, one);
	Check(single.tabCount == 0, "one tab alone is no strip");
	TabletFrame stale = three;
	SetTabs(stale, TabSet{});
	Check(stale.tabCount == 0, "an empty set clears an earlier strip");

	Check(TabletTabAt(three, 200.0f, 150.0f) == 0 && TabletTabAt(three, 600.0f, 150.0f) == 1 &&
	          TabletTabAt(three, 900.0f, 150.0f) == 2,
	      "each of the three tabs found");
	Check(TabletTabAt(three, 455.0f, 150.0f) == -1 && TabletTabAt(three, 900.0f, 300.0f) == -1,
	      "between them, or on the page: none");
	Check(TabletTabAt(TabletFrame{}, 200.0f, 150.0f) == -1, "no tabs: none");
	const TabSet back = TabsOf(three);
	Check(Is(back, 3, TabletTab::Page, TabletTab::Figure, TabletTab::Mirror), "the set read back from the frame");
	Check(TabsOf(TabletFrame{}).count == 0, "no strip: an empty set");
}

void TestMirrorPagePress() {
	std::printf("What a press on the mirror reaches\n");
	const TabletFrame f = FrameWith(TabsFor(Wanted(true, true)));
	float x = 600.0f;
	float y = 700.0f;
	Check(!MirrorTabToLayer(f, x, y), "on the page's rectangle: nothing of the game's");
	x = 900.0f;
	y = 150.0f;
	Check(!MirrorTabToLayer(f, x, y), "on a tab: nothing of the game's");
	x = 1300.0f;
	y = 700.0f;
	Check(MirrorTabToLayer(f, x, y) && x == 1300.0f && y == 700.0f, "off the page: the point itself");
	x = 100.0f;
	y = 200.0f;
	Check(!MirrorTabToLayer(f, x, y), "the page's own corner is on the glass");
}

void TestChoosing() {
	std::printf("Choosing a tab\n");
	const TabSet all = TabsFor(Wanted(true, true));
	const int mirror = static_cast<int>(TabletTab::Mirror);
	const int figure = static_cast<int>(TabletTab::Figure);
	const int page = static_cast<int>(TabletTab::Page);
	TabletTabState s;
	Check(StepTabletTab(s, all, mirror, false, false) == TabletTab::Page, "the page shown first");
	Check(StepTabletTab(s, all, mirror, true, false) == TabletTab::Mirror, "a click on the mirror's tab shows the mirror");
	Check(StepTabletTab(s, all, -1, true, false) == TabletTab::Mirror, "held off the tab: still the mirror");
	Check(StepTabletTab(s, all, -1, false, false) == TabletTab::Mirror, "released: still the mirror");
	Check(StepTabletTab(s, all, -1, false, true) == TabletTab::Page, "a popup over the page: the page");
	Check(StepTabletTab(s, all, -1, false, false) == TabletTab::Mirror, "the popup gone: the mirror again");
	Check(StepTabletTab(s, all, page, false, false) == TabletTab::Mirror, "hovering the page's tab without a click: nothing changes");
	Check(StepTabletTab(s, all, page, true, false) == TabletTab::Page, "a click on the page's tab: the page");
	Check(StepTabletTab(s, all, mirror, true, false) == TabletTab::Page, "a click held over from before is no new click");
	StepTabletTab(s, all, mirror, false, false);
	Check(StepTabletTab(s, all, figure, true, false) == TabletTab::Figure, "a new click on the figure's tab: the figure");
	StepTabletTab(s, all, -1, false, false);
	Check(StepTabletTab(s, all, mirror, true, false) == TabletTab::Mirror, "from the figure straight to the mirror");
	StepTabletTab(s, all, -1, false, false);
	Check(StepTabletTab(s, all, figure, true, false) == TabletTab::Figure, "and from the mirror to the figure");

	TabletTabState gone;
	StepTabletTab(gone, all, mirror, true, false);
	Check(StepTabletTab(gone, TabSet{}, -1, false, false) == TabletTab::Page, "the tablet closed (no strip): the page");
	Check(StepTabletTab(gone, all, -1, false, false) == TabletTab::Page, "opened again: the page, the mirror forgotten");

	TabletTabState off;
	StepTabletTab(off, all, mirror, true, false);
	const TabSet noMirror = TabsFor(Wanted(true, false));
	Check(StepTabletTab(off, noMirror, -1, false, false) == TabletTab::Page, "[Tablet] Mirror turned off while shown: the page");
	Check(StepTabletTab(off, all, -1, false, false) == TabletTab::Page, "turned on again: the page, not the mirror");

	TabletTabState lost;
	StepTabletTab(lost, all, figure, true, false);
	const TabSet noFigure = TabsFor(Wanted(false, true));
	Check(StepTabletTab(lost, noFigure, -1, false, false) == TabletTab::Page, "a page with no figure: the figure's tab is gone, the page");
	Check(StepTabletTab(lost, all, -1, false, false) == TabletTab::Page, "and it is forgotten");

	TabletTabState keep;
	StepTabletTab(keep, all, mirror, true, false);
	Check(StepTabletTab(keep, noFigure, -1, false, false) == TabletTab::Mirror, "the mirror kept over a page with no figure");

	TabletTabState wrong;
	Check(StepTabletTab(wrong, noFigure, figure, true, false) == TabletTab::Page, "a click on a tab the strip has not: nothing");
	StepTabletTab(wrong, noFigure, -1, false, false);
	Check(StepTabletTab(wrong, noFigure, mirror, true, true) == TabletTab::Page,
	      "a click on the mirror's tab under a popup: chosen, but the page shown");
	Check(StepTabletTab(wrong, noFigure, -1, false, false) == TabletTab::Mirror, "the popup gone: the mirror");
}

void TestNames() {
	std::printf("The tabs' names in the log\n");
	Check(std::strcmp(TabletTabName(TabletTab::Page), "page's") == 0, "the page's");
	Check(std::strcmp(TabletTabName(TabletTab::Figure), "figure's") == 0, "the figure's");
	Check(std::strcmp(TabletTabName(TabletTab::Mirror), "mirror's") == 0, "the mirror's");
}

void TestPageTurn() {
	std::printf("A page turn and the mirror\n");
	const TabSet all = TabsFor(Wanted(true, true));
	const int mirror = static_cast<int>(TabletTab::Mirror);
	TabletTabState s;
	StepTabletTab(s, all, mirror, true, false);
	Check(TurnTabletPage(s, 0) == 0 && s.chosen == TabletTab::Mirror, "no turn: the mirror stays");
	Check(TurnTabletPage(s, 1) == 1 && s.chosen == TabletTab::Page, "a turn to the right leaves the mirror and still goes on");
	StepTabletTab(s, all, -1, false, false);
	StepTabletTab(s, all, mirror, true, false);
	Check(TurnTabletPage(s, -1) == -1 && s.chosen == TabletTab::Page, "a turn to the left leaves it too");
	TabletTabState figure;
	StepTabletTab(figure, all, static_cast<int>(TabletTab::Figure), true, false);
	Check(TurnTabletPage(figure, 1) == 1 && figure.chosen == TabletTab::Figure, "the figure's tab is as it was: a turn leaves it chosen");
	TabletTabState page;
	Check(TurnTabletPage(page, -1) == -1 && page.chosen == TabletTab::Page, "on the page: nothing changes");
}

}  // namespace

int main() {
	TestWhichTabs();
	TestStrip();
	TestMirrorPagePress();
	TestChoosing();
	TestPageTurn();
	TestNames();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

// Checks the main menu's dome (ui/MainMenuDome.h): when OBVR's dome shows around the main menu, what is held
// off the headset, and that the guide's stage and the main menu's share one dome - no out and in between.

#include <cstdio>
#include <cstring>

#include "ui/MainMenuDome.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::ui;

DomeShareInput MenuUp() {
	DomeShareInput in;
	in.menu.enabled = true;
	in.menu.scene = true;
	in.menu.mainMenuInStack = true;
	return in;
}

void TestEnabled() {
	std::printf("the setting and [Render] LoadingStage\n");
	Check(MainMenuDomeEnabled(true, true), "both on: enabled");
	Check(!MainMenuDomeEnabled(false, true), "MainMenuDome off: no dome");
	Check(!MainMenuDomeEnabled(true, false), "LoadingStage off: no dome anywhere, the main menu's too");
	Check(!MainMenuDomeEnabled(false, false), "both off: no dome");
}

void TestComesIn() {
	std::printf("the dome comes in with the main menu\n");
	DomeShareState st;
	DomeSharePlan p = StepDomeShare(st, MenuUp());
	Check(p.holdScene, "the scene is held off the headset");
	Check(p.grid == DomeGrid::In, "the grid fades in");
	Check(p.menuDomeIn && !p.menuDomeOut, "the main menu's dome is said to go in");
	p = StepDomeShare(st, MenuUp());
	Check(p.holdScene && p.grid == DomeGrid::None && !p.menuDomeIn, "the next frame: held, nothing new asked, nothing said");
}

void TestRefusals() {
	std::printf("refusals: no dome, nothing held, nothing asked\n");
	struct Case {
		const char* name;
		void (*set)(DomeShareInput&);
		DomeOutReason reason;
	};
	const Case cases[] = {
		{"the setting off", [](DomeShareInput& i) { i.menu.enabled = false; }, DomeOutReason::Disabled},
		{"OBVR not holding the compositor", [](DomeShareInput& i) { i.menu.scene = false; }, DomeOutReason::NoScene},
		{"no main menu in the stack", [](DomeShareInput& i) { i.menu.mainMenuInStack = false; }, DomeOutReason::NotMainMenu},
		{"a load running", [](DomeShareInput& i) { i.menu.loading = true; }, DomeOutReason::Loading},
		{"a film owning the screen", [](DomeShareInput& i) { i.menu.filmOwnsScreen = true; }, DomeOutReason::Film},
		{"a world frame", [](DomeShareInput& i) { i.menu.worldFrame = true; }, DomeOutReason::World},
	};
	for (const Case& c : cases) {
		DomeShareState st;
		DomeShareInput in = MenuUp();
		c.set(in);
		const DomeSharePlan p = StepDomeShare(st, in);
		char what[160];
		std::snprintf(what, sizeof(what), "%s: not held, no grid, no line, state stays down", c.name);
		Check(!p.holdScene && p.grid == DomeGrid::None && !p.menuDomeIn && !p.menuDomeOut && !st.menuDomeOn && !st.up, what);
		std::snprintf(what, sizeof(what), "%s: the refusal is named", c.name);
		Check(MainMenuDomeRefusal(in.menu) == c.reason, what);
	}
	Check(MainMenuDomeRefusal(MenuUp().menu) == DomeOutReason::None, "all in order: no refusal");
	DomeOutReason order = DomeOutReason::None;
	DomeShareInput both = MenuUp();
	both.menu.enabled = false;
	both.menu.loading = true;
	order = MainMenuDomeRefusal(both.menu);
	Check(order == DomeOutReason::Disabled, "two refusals: the first of the list is named");
}

void TestReasonText() {
	std::printf("the words for the log\n");
	Check(std::strlen(DomeOutReasonText(DomeOutReason::Loading)) > 0, "loading has a text");
	Check(std::strcmp(DomeOutReasonText(DomeOutReason::Loading), DomeOutReasonText(DomeOutReason::World)) != 0,
	      "loading and a world frame read differently");
	Check(std::strcmp(DomeOutReasonText(DomeOutReason::Disabled), DomeOutReasonText(DomeOutReason::NoScene)) != 0,
	      "the setting and a missing scene read differently");
	Check(std::strlen(DomeOutReasonText(DomeOutReason::NotMainMenu)) > 0 && std::strlen(DomeOutReasonText(DomeOutReason::Film)) > 0,
	      "the others have text too");
}

void TestToLoading() {
	std::printf("the main menu to a load: the dome is not faded out\n");
	DomeShareState st;
	StepDomeShare(st, MenuUp());
	DomeShareInput in = MenuUp();
	in.menu.loading = true;
	DomeSharePlan p = StepDomeShare(st, in);
	Check(!p.holdScene, "the load's own picture or dome takes over: not held");
	Check(p.grid == DomeGrid::None, "no fade out into the load - the loading stage's dome is not switched through black");
	Check(p.menuDomeOut && p.reason == DomeOutReason::Loading, "said once, with the reason");
	p = StepDomeShare(st, in);
	Check(!p.menuDomeOut && p.grid == DomeGrid::None && !p.holdScene, "the next frame: quiet");
	in.menu.loading = false;
	in.menu.mainMenuInStack = false;
	in.menu.worldFrame = true;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::None && !p.menuDomeIn, "the first world frame: nothing held, nothing asked");
	in = MenuUp();
	p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::In && p.menuDomeIn, "quit to the main menu: the dome comes in again");
}

void TestLeavesOtherWays() {
	std::printf("the dome goes out any other way: faded out\n");
	DomeShareState st;
	StepDomeShare(st, MenuUp());
	DomeShareInput in = MenuUp();
	in.menu.worldFrame = true;
	DomeSharePlan p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::Out && p.menuDomeOut && p.reason == DomeOutReason::World,
	      "a world frame: scene back, grid out, said");
	st = DomeShareState{};
	StepDomeShare(st, MenuUp());
	in = MenuUp();
	in.menu.enabled = false;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::Out && p.reason == DomeOutReason::Disabled, "switched off while up: grid out");
	st = DomeShareState{};
	StepDomeShare(st, MenuUp());
	in = MenuUp();
	in.menu.scene = false;
	p = StepDomeShare(st, in);
	Check(p.grid == DomeGrid::Out && p.reason == DomeOutReason::NoScene, "the compositor lost: grid out is asked (a no-op there)");
	st = DomeShareState{};
	StepDomeShare(st, MenuUp());
	in = MenuUp();
	in.menu.mainMenuInStack = false;
	p = StepDomeShare(st, in);
	Check(p.grid == DomeGrid::Out && p.reason == DomeOutReason::NotMainMenu, "the main menu gone without a load: grid out");
	st = DomeShareState{};
	StepDomeShare(st, MenuUp());
	in = MenuUp();
	in.menu.filmOwnsScreen = true;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::Out && p.reason == DomeOutReason::Film, "a film owning the screen: grid out");
}

void TestGuide() {
	std::printf("the guide's stage and the main menu's dome share one dome\n");
	// Guide first, the main menu's dome off: as today.
	DomeShareState st;
	DomeShareInput in;
	in.guideOn = true;
	in.guideHoldsScene = true;
	DomeSharePlan p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::In && !p.menuDomeIn, "guide alone: held, grid in, no main-menu line");
	p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::None, "guide alone, steady: held, nothing asked");
	in.guideOn = false;
	in.guideHoldsScene = false;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::Out && !p.menuDomeOut, "guide ends, no main-menu dome: as today, released and faded out");
	// The guide's hold alone, GuideHoldsScene setting off: the grid still follows the stage.
	st = DomeShareState{};
	in = DomeShareInput{};
	in.guideOn = true;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::In, "guide up with its hold off: grid in, not held");
	// Main menu's dome first, guide comes on in front of it, then ends: one dome all along.
	st = DomeShareState{};
	StepDomeShare(st, MenuUp());
	in = MenuUp();
	in.guideOn = true;
	in.guideHoldsScene = true;
	p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::None && !p.menuDomeIn && !p.menuDomeOut, "menu dome then guide: no change at all");
	in.guideOn = false;
	in.guideHoldsScene = false;
	p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::None && !p.menuDomeOut, "the guide ends over the menu's dome: held on, no fade out and in");
	// Guide up, the main menu's dome joins when its conditions come true.
	st = DomeShareState{};
	in = DomeShareInput{};
	in.menu = MenuUp().menu;
	in.menu.mainMenuInStack = false;
	in.guideOn = true;
	in.guideHoldsScene = true;
	StepDomeShare(st, in);
	in.menu.mainMenuInStack = true;
	p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::None && p.menuDomeIn, "the menu's dome joins a guide's dome: no grid call, said");
	in.guideOn = false;
	in.guideHoldsScene = false;
	p = StepDomeShare(st, in);
	Check(p.holdScene && p.grid == DomeGrid::None && !p.menuDomeOut, "then the guide ends: the dome stays");
	// Both end together: out once.
	st = DomeShareState{};
	in = MenuUp();
	in.guideOn = true;
	in.guideHoldsScene = true;
	StepDomeShare(st, in);
	in.menu.loading = true;
	in.guideOn = false;
	in.guideHoldsScene = false;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::None && p.menuDomeOut, "guide and menu end into a load: released, no fade out");
	// Guide ends into a load with the main menu's dome off: the guide's fade out as today.
	st = DomeShareState{};
	in = DomeShareInput{};
	in.guideOn = true;
	in.guideHoldsScene = true;
	StepDomeShare(st, in);
	in = DomeShareInput{};
	in.menu.scene = true;
	in.menu.mainMenuInStack = true;
	in.menu.loading = true;
	p = StepDomeShare(st, in);
	Check(!p.holdScene && p.grid == DomeGrid::Out, "guide only, ends: faded out as today (the main menu's setting off)");
}

}  // namespace

int main() {
	TestEnabled();
	TestComesIn();
	TestRefusals();
	TestReasonText();
	TestToLoading();
	TestLeavesOtherWays();
	TestGuide();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

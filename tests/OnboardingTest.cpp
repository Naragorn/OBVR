// Checks the first-start walkthrough: every page's rows come out as the
// painter's kinds, the choice between the two ways to play writes the mode
// and turns the page, the highlight skips text and wraps, Next and Back
// turn pages, Finish closes, settings on a page change the configuration
// and answer the definition to write back, and every setting a page names
// exists in the table.

#include <cstdio>
#include <cstring>

#include "core/Config.h"
#include "ui/Onboarding.h"

namespace {

using namespace obvr::ui;
using obvr::Config;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestPagesAreSound() {
	std::printf("Pages\n");
	Check(OnboardingPageCount() == 4, "four pages");
	bool everySettingKnown = true;
	bool everyPageHasAWayOn = true;
	bool everyChoiceHasAPicture = true;
	for (UInt32 p = 0; p < OnboardingPageCount(); ++p) {
		const OnboardingPage& page = OnboardingPages()[p];
		bool wayOn = false;
		for (UInt32 r = 0; r < page.rowCount; ++r) {
			const OnboardingRow& row = page.rows[r];
			if ((row.kind == OnboardingRowKind::Setting || row.kind == OnboardingRowKind::Choice) &&
			    FindSetting(row.iniSection, row.iniKey) == nullptr) {
				std::printf("        unknown setting %s.%s on page %u\n", row.iniSection,
				            row.iniKey, p);
				everySettingKnown = false;
			}
			if (row.kind == OnboardingRowKind::Action || row.kind == OnboardingRowKind::Choice) {
				wayOn = true;
			}
			if (row.kind == OnboardingRowKind::Choice && (row.icon == nullptr || row.iconRows == 0)) {
				everyChoiceHasAPicture = false;
			}
		}
		if (!wayOn) {
			everyPageHasAWayOn = false;
		}
	}
	Check(everySettingKnown, "every setting a page names is in the table");
	Check(everyPageHasAWayOn, "every page has a way onwards");
	Check(everyChoiceHasAPicture, "every choice carries a picture");
	Check(FindSetting("Onboarding", "ShowAtStart") != nullptr,
	      "the do-not-show-again switch is a real setting");
	Check(FindSetting("Hands", "Enabled") != nullptr, "the mode switch is a real setting");

	// The kinds the painter is handed: text ignores keys, actions show a
	// marker and take no value.
	MenuItem text;
	text.kind = ItemKind::Text;
	text.value = 3.0f;
	Check(AdjustValue(text, MenuAction::Increase) == 3.0f, "a text row ignores Right");
	MenuItem action;
	action.kind = ItemKind::Action;
	Check(AdjustValue(action, MenuAction::Decrease) == action.value, "an action row ignores Left");
	char out[16];
	FormatValue(text, out, sizeof(out));
	Check(out[0] == '\0', "a text row shows no value");
	FormatValue(action, out, sizeof(out));
	Check(std::strcmp(out, ">") == 0, "an action row shows its marker");
}

void TestChoice() {
	std::printf("The choice of how to play\n");
	OnboardingMenu menu;
	Config config;
	Check(!menu.IsOpen(), "closed to begin with");
	menu.Open();
	Check(menu.IsOpen() && menu.Page() == 0, "opens on the first page");
	Check(std::strcmp(menu.Title(), "Welcome") == 0, "which is the welcome");

	MenuItem items[16];
	const char* categories[16];
	const UInt32 count = menu.BuildRows(config, items, categories, 16);
	Check(count == 5, "the welcome page has five rows");
	Check(items[0].kind == ItemKind::Text && items[1].kind == ItemKind::Text,
	      "two lines of text");
	Check(items[2].kind == ItemKind::Action && items[2].icon != nullptr && items[2].iconRows > 0,
	      "then the seated choice, with its picture");
	Check(items[3].kind == ItemKind::Action && items[3].icon != nullptr,
	      "and the standing one, with its picture");
	Check(items[4].kind == ItemKind::Text && std::strstr(items[4].label, "under construction") != nullptr,
	      "and a line saying standing is under construction");
	Check(std::strstr(items[3].help, "nder construction") != nullptr,
	      "the standing choice's help says so too");
	Check(items[2].chosen && !items[3].chosen, "seated is marked as current while hands are off");
	Check(menu.State().selected == 2, "the highlight starts on the seated choice");
	Check(std::strcmp(categories[0], "Welcome") == 0, "rows carry the page title");

	Check(ChoiceIsCurrent(OnboardingAction::ChooseSeated, config), "seated is current by default");
	Check(!ChoiceIsCurrent(OnboardingAction::ChooseStanding, config), "and standing is not");
	Check(!ChoiceIsCurrent(OnboardingAction::Next, config), "an action is never current");

	// Down goes to standing, Up wraps back.
	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 3, "Down goes to the standing choice");
	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 2, "and wraps past the text back to seated");
	menu.Apply(MenuAction::Up, config);
	Check(menu.State().selected == 3, "Up wraps the other way");

	// Right on standing is refused while the mode is under construction:
	// nothing is written, the page stays, the picture is not repainted.
	Check(StandingUnderConstruction(), "the standing experience is under construction");
	const UInt32 before = menu.Revision();
	const SettingDefinition* changed = menu.Apply(MenuAction::Increase, config);
	Check(changed == nullptr, "Right on standing answers nothing to write back");
	Check(!config.handTracking && !config.hands.enabled, "and hand tracking stays off");
	Check(menu.Page() == 0 && menu.State().selected == 3, "and the page and the highlight stay");
	Check(menu.Revision() == before, "and nothing is repainted");
	changed = menu.Apply(MenuAction::Decrease, config);
	Check(changed == nullptr && !config.handTracking && menu.Page() == 0,
	      "Left on standing is refused the same way");

	// A configuration that switched the mode on elsewhere (the INI, the
	// settings menu) is still shown as such: standing is the marked one.
	config.handTracking = true;
	config.hands.enabled = true;
	menu.BuildRows(config, items, categories, 16);
	Check(!items[2].chosen && items[3].chosen, "standing is marked when the INI switched it on");

	// Seated is chosen with Right or Left alike - Left is "decrease", and
	// seated is still written - and switches hand tracking off again.
	menu.Apply(MenuAction::Down, config);  // from standing wraps past the text to seated
	Check(menu.State().selected == 2, "on seated again");
	changed = menu.Apply(MenuAction::Decrease, config);
	Check(changed != nullptr && std::strcmp(changed->iniKey, "Enabled") == 0,
	      "Left on seated answers the mode setting to write back");
	Check(!config.handTracking && !config.hands.enabled, "and switches hand tracking off");
	Check(menu.Page() == 1 && std::strcmp(menu.Title(), "Controls") == 0,
	      "and turns to the controls page");
	Check(menu.Revision() > before, "and the picture is repainted");

	// Back to the choice: seated is the one marked now.
	while (menu.Page() == 1) {
		const OnboardingPage& page = OnboardingPages()[1];
		if (page.rows[menu.State().selected].action == OnboardingAction::Back) {
			menu.Apply(MenuAction::Increase, config);
		} else {
			menu.Apply(MenuAction::Down, config);
		}
	}
	Check(menu.Page() == 0, "Back returns to the choice");
	menu.BuildRows(config, items, categories, 16);
	Check(items[2].chosen && !items[3].chosen, "and seated is marked");
	changed = menu.Apply(MenuAction::Increase, config);
	Check(changed != nullptr && !config.handTracking && menu.Page() == 1,
	      "Right on seated confirms it and turns the page as well");
}

void TestNavigation() {
	std::printf("Navigation\n");
	OnboardingMenu menu;
	Config config;
	menu.Open();
	menu.Apply(MenuAction::Increase, config);  // choose seated: page 1
	Check(menu.Page() == 1, "on the controls page");
	MenuItem items[16];
	const char* categories[16];
	const UInt32 count = menu.BuildRows(config, items, categories, 16);
	Check(count == 8, "the controls page has eight rows");
	Check(menu.State().selected == 6, "the highlight starts on Back, past the text");
	Check(items[6].kind == ItemKind::Action && items[7].kind == ItemKind::Action,
	      "Back and Next are actions");
	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 7, "Down goes to Next");
	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 6, "and wraps past the text to Back");
	// A pointer over a row: an action takes the highlight, text does not.
	const UInt32 before = menu.Revision();
	Check(menu.Hover(7) && menu.State().selected == 7 && menu.Revision() != before,
	      "hovering Next selects it");
	Check(!menu.Hover(1) && menu.State().selected == 7, "hovering a text row is refused");
	Check(!menu.Hover(7), "hovering the selected row again is nothing");
	Check(!menu.Hover(20) && menu.State().selected == 7, "a row past the page is refused");
	menu.Hover(6);
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Page() == 0, "Right on Back goes back");
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Page() == 1, "and the choice forward again");
	menu.Apply(MenuAction::Down, config);
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Page() == 2 && std::strcmp(menu.Title(), "Comfort") == 0,
	      "Right on Next reaches the comfort page");
}

void TestSettingsOnPages() {
	std::printf("Settings on pages\n");
	OnboardingMenu menu;
	Config config;
	menu.Open();
	menu.Apply(MenuAction::Increase, config);  // seated, to controls
	menu.Apply(MenuAction::Down, config);      // Next
	menu.Apply(MenuAction::Increase, config);  // to comfort
	Check(menu.Page() == 2, "on the comfort page");
	MenuItem items[16];
	const char* categories[16];
	menu.BuildRows(config, items, categories, 16);
	Check(items[menu.State().selected].kind == ItemKind::Toggle,
	      "the highlight starts on the smooth-turning toggle");
	const bool before = config.look.smoothTurning;
	const UInt32 revision = menu.Revision();
	const SettingDefinition* changed = menu.Apply(MenuAction::Increase, config);
	if (changed == nullptr) {
		changed = menu.Apply(MenuAction::Decrease, config);
	}
	Check(changed != nullptr && std::strcmp(changed->iniKey, "SmoothTurning") == 0,
	      "Left or Right on the toggle answers the setting to write back");
	Check(config.look.smoothTurning != before, "and the value changed");
	Check(menu.Revision() > revision, "and the picture is repainted");
}

void TestFinish() {
	std::printf("Finish\n");
	OnboardingMenu menu;
	Config config;
	menu.Open();
	menu.Apply(MenuAction::Increase, config);  // seated, to controls
	for (int page = 1; page <= 2; ++page) {
		while (menu.IsOpen() && menu.Page() == static_cast<UInt32>(page)) {
			const OnboardingPage& current = OnboardingPages()[menu.Page()];
			if (current.rows[menu.State().selected].action == OnboardingAction::Next) {
				menu.Apply(MenuAction::Increase, config);
			} else {
				menu.Apply(MenuAction::Down, config);
			}
		}
	}
	Check(menu.IsOpen() && menu.Page() == 3, "Next on each page reaches the last");
	Check(std::strcmp(menu.Title(), "Done") == 0, "which is Done");

	MenuItem items[16];
	const char* categories[16];
	menu.BuildRows(config, items, categories, 16);
	Check(items[menu.State().selected].kind == ItemKind::Toggle,
	      "the highlight starts on the show-at-start toggle");
	Check(config.onboardingShowAtStart, "which is on by default");
	const SettingDefinition* changed = menu.Apply(MenuAction::Decrease, config);
	Check(changed != nullptr && !config.onboardingShowAtStart,
	      "Left switches it off and answers the setting to save");

	menu.Apply(MenuAction::Down, config);
	menu.Apply(MenuAction::Down, config);
	const OnboardingPage& done = OnboardingPages()[3];
	Check(done.rows[menu.State().selected].action == OnboardingAction::Finish,
	      "two Downs reach Finish");
	menu.Apply(MenuAction::Increase, config);
	Check(!menu.IsOpen(), "Right on Finish closes the walkthrough");
	Check(menu.Apply(MenuAction::Down, config) == nullptr, "and closed it ignores keys");
}

}  // namespace

int main() {
	TestPagesAreSound();
	TestChoice();
	TestNavigation();
	TestSettingsOnPages();
	TestFinish();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

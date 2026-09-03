// Checks the first-start walkthrough: every page's rows come out as the
// painter's kinds, the highlight skips text and wraps, Next and Back turn
// pages, Finish closes, settings on a page change the configuration and
// answer the definition to write back, and every setting a page names
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
	bool everyPageHasAnAction = true;
	for (UInt32 p = 0; p < OnboardingPageCount(); ++p) {
		const OnboardingPage& page = OnboardingPages()[p];
		bool action = false;
		for (UInt32 r = 0; r < page.rowCount; ++r) {
			const OnboardingRow& row = page.rows[r];
			if (row.kind == OnboardingRowKind::Setting &&
			    FindSetting(row.iniSection, row.iniKey) == nullptr) {
				std::printf("        unknown setting %s.%s on page %u\n", row.iniSection,
				            row.iniKey, p);
				everySettingKnown = false;
			}
			if (row.kind == OnboardingRowKind::Action) {
				action = true;
			}
		}
		everyPageHasAnAction = everyPageHasAnAction && action;
	}
	Check(everySettingKnown, "every setting a page names is in the table");
	Check(everyPageHasAnAction, "every page has a way onwards");
	Check(FindSetting("Onboarding", "ShowAtStart") != nullptr,
	      "the do-not-show-again switch is a real setting");
	Check(FindSetting("Hands", "Enabled") != nullptr, "the mode switch is a real setting");

	// The two row kinds the walkthrough added to the model: neither has a
	// value a key could change, and each formats the way the painter expects.
	MenuItem text;
	text.kind = ItemKind::Text;
	text.value = 3.0f;
	Check(AdjustValue(text, MenuAction::Increase) == 3.0f, "a text row ignores Right");
	MenuItem action;
	action.kind = ItemKind::Action;
	Check(AdjustValue(action, MenuAction::Decrease) == action.value, "an action row ignores Left");
	char out[8];
	FormatValue(text, out, sizeof(out));
	Check(out[0] == '\0', "a text row shows no value");
	FormatValue(action, out, sizeof(out));
	Check(std::strcmp(out, ">") == 0, "an action row shows its marker");
}

void TestNavigation() {
	std::printf("Navigation\n");
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
	Check(items[0].kind == ItemKind::Text, "text rows are text");
	Check(items[4].kind == ItemKind::Action, "the last is the Next action");
	Check(menu.State().selected == 4, "and it is what the highlight starts on");
	Check(std::strcmp(categories[0], "Welcome") == 0, "rows carry the page title");

	Check(menu.Apply(MenuAction::Up, config) == nullptr && menu.State().selected == 4,
	      "with one selectable row the highlight stays");
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Page() == 1, "Right on Next turns the page");
	Check(std::strcmp(menu.Title(), "How to play") == 0, "to the mode page");
	const UInt32 modeRows = menu.BuildRows(config, items, categories, 16);
	Check(modeRows == 7, "the mode page has seven rows");
	Check(menu.State().selected == 4, "the highlight starts on the setting, past the text");
	Check(items[4].kind == ItemKind::Toggle, "which is the hand-tracking toggle");

	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 5, "Down goes to Back");
	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 6, "then Next");
	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected == 4, "then wraps past the text to the setting");
	menu.Apply(MenuAction::Up, config);
	Check(menu.State().selected == 6, "Up wraps the other way");

	menu.Apply(MenuAction::Up, config);
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Page() == 0, "Right on Back goes back");
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Page() == 1, "and Next again forward");
}

void TestSettingsOnPages() {
	std::printf("Settings on pages\n");
	OnboardingMenu menu;
	Config config;
	menu.Open();
	menu.Apply(MenuAction::Increase, config);  // to the mode page
	Check(!config.handTracking, "hand tracking is off to begin with");
	const UInt32 before = menu.Revision();
	const SettingDefinition* changed = menu.Apply(MenuAction::Increase, config);
	Check(changed != nullptr && std::strcmp(changed->iniKey, "Enabled") == 0,
	      "Right on the toggle answers the setting to write back");
	Check(config.handTracking && config.hands.enabled, "and the mode is switched on in both places");
	Check(menu.Revision() > before, "and the picture is repainted");
	changed = menu.Apply(MenuAction::Decrease, config);
	Check(changed != nullptr && !config.handTracking, "Left switches it off again");
}

void TestFinish() {
	std::printf("Finish\n");
	OnboardingMenu menu;
	Config config;
	menu.Open();
	for (int i = 0; i < 3; ++i) {
		menu.Apply(MenuAction::Increase, config);  // Next on each page's highlighted action
		// On pages with a setting first, Increase changed the setting; move to Next.
		while (menu.IsOpen() && menu.Page() == static_cast<UInt32>(i)) {
			menu.Apply(MenuAction::Down, config);
			const OnboardingPage& page = OnboardingPages()[menu.Page()];
			if (page.rows[menu.State().selected].action == OnboardingAction::Next) {
				menu.Apply(MenuAction::Increase, config);
			}
		}
	}
	Check(menu.IsOpen() && menu.Page() == 3, "three Nexts reach the last page");
	Check(std::strcmp(menu.Title(), "Done") == 0, "which is Done");

	// The highlight starts on the do-not-show-again toggle.
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

#pragma once

#include "core/Config.h"
#include "ui/MenuModel.h"
#include "ui/SettingsList.h"

namespace obvr::ui {

// The first-start walkthrough: a few pages in the headset that say what OBVR
// is, how it is steered, which of its two shapes to play in - head-tracked
// or hand-tracked - and a couple of comfort settings, ending on "do not show
// this again".
//
// The same machinery as the settings menu: rows the painter already knows
// how to draw, settings written through the same table, so a choice made
// here is the same INI key the settings menu changes later. Holds no
// Direct3D and no Windows; the caller feeds it MenuActions and the layer
// draws whatever BuildRows hands back, so every page and every key press
// can be walked through in a test.
//
// A page is a list of rows: explanatory text (not selectable), settings by
// INI section and key, and actions - Back, Next, Finish. The highlight
// skips text rows; Left or Right on a setting changes it and on an action
// fires it.

enum class OnboardingRowKind { Text, Setting, Action };
enum class OnboardingAction { None, Back, Next, Finish };

struct OnboardingRow {
	OnboardingRowKind kind = OnboardingRowKind::Text;
	const char* text = "";
	const char* help = "";
	const char* iniSection = "";
	const char* iniKey = "";
	OnboardingAction action = OnboardingAction::None;
};

struct OnboardingPage {
	const char* title = "";
	const OnboardingRow* rows = nullptr;
	UInt32 rowCount = 0;
};

const OnboardingPage* OnboardingPages();
UInt32 OnboardingPageCount();

class OnboardingMenu {
public:
	bool IsOpen() const { return m_open; }
	UInt32 Page() const { return m_page; }
	const char* Title() const;

	// Opens on the first page with the first selectable row highlighted.
	void Open();
	void Close();

	void SetVisibleRows(UInt32 rows);

	// One key press. Movement skips text rows; Left or Right on a setting
	// changes it and answers the definition to write back; Left or Right on
	// an action turns the page, and Finish closes the walkthrough. Anything
	// that changes the picture bumps the revision.
	const SettingDefinition* Apply(MenuAction action, Config& config);

	// The current page as rows for the painter. Text rows come out as
	// ItemKind::Text, actions as ItemKind::Action, settings as the table's
	// own items. Categories are the page title, so the painter shows it as
	// the heading of the list.
	UInt32 BuildRows(const Config& config, MenuItem* items, const char** categories,
	                 UInt32 capacity) const;

	MenuState State() const { return m_state; }
	UInt32 Revision() const { return m_revision; }

private:
	const OnboardingPage& CurrentPage() const;
	UInt32 FirstSelectable(const OnboardingPage& page) const;
	UInt32 StepSelection(const OnboardingPage& page, UInt32 from, bool down) const;

	bool m_open = false;
	UInt32 m_page = 0;
	MenuState m_state;
	UInt32 m_revision = 1;
	UInt32 m_visibleRows = 10;
};

}  // namespace obvr::ui

#include "ui/Onboarding.h"

namespace obvr::ui {
namespace {

constexpr OnboardingRow kWelcome[] = {
	{OnboardingRowKind::Text, "Welcome to OBVR: Oblivion in your headset.", "", "", "",
	 OnboardingAction::None},
	{OnboardingRowKind::Text, "Del recenters the view. Insert opens the settings menu.", "",
	 "", "", OnboardingAction::None},
	{OnboardingRowKind::Text, "Up and Down choose a row, Left and Right change it.", "", "", "",
	 OnboardingAction::None},
	{OnboardingRowKind::Text, "Press Right on Next to go on.", "", "", "", OnboardingAction::None},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next},
};

constexpr OnboardingRow kMode[] = {
	{OnboardingRowKind::Text, "Two ways to play. Change it any time in the settings.", "", "",
	 "", OnboardingAction::None},
	{OnboardingRowKind::Text, "Head-tracked: the gaze aims, mouse, keyboard or gamepad do the rest.",
	 "", "", "", OnboardingAction::None},
	{OnboardingRowKind::Text, "Hand-tracked: motion controllers hold weapon, shield and menus.",
	 "", "", "", OnboardingAction::None},
	{OnboardingRowKind::Text, "Hand tracking is new; the head-tracked way is the proven one.", "",
	 "", "", OnboardingAction::None},
	{OnboardingRowKind::Setting, "", "", "Hands", "Enabled", OnboardingAction::None},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next},
};

constexpr OnboardingRow kComfort[] = {
	{OnboardingRowKind::Text, "Turning left and right stays with the mouse or the stick.", "",
	 "", "", OnboardingAction::None},
	{OnboardingRowKind::Text, "Smooth turning eases it; snap turning is planned.", "", "", "",
	 OnboardingAction::None},
	{OnboardingRowKind::Setting, "", "", "Look", "SmoothTurning", OnboardingAction::None},
	{OnboardingRowKind::Setting, "", "", "Look", "TurnSpeed", OnboardingAction::None},
	{OnboardingRowKind::Text, "Menus can hang in the world or on a cinema screen.", "", "", "",
	 OnboardingAction::None},
	{OnboardingRowKind::Setting, "", "", "Render", "Menus", OnboardingAction::None},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next},
};

constexpr OnboardingRow kDone[] = {
	{OnboardingRowKind::Text, "That is all. Everything here lives in OBVR.ini and in the", "",
	 "", "", OnboardingAction::None},
	{OnboardingRowKind::Text, "settings menu (Insert), where this walkthrough can be reopened.",
	 "", "", "", OnboardingAction::None},
	{OnboardingRowKind::Setting, "", "", "Onboarding", "ShowAtStart", OnboardingAction::None},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back},
	{OnboardingRowKind::Action, "Finish", "Close the walkthrough", "", "",
	 OnboardingAction::Finish},
};

constexpr OnboardingPage kPages[] = {
	{"Welcome", kWelcome, sizeof(kWelcome) / sizeof(kWelcome[0])},
	{"How to play", kMode, sizeof(kMode) / sizeof(kMode[0])},
	{"Comfort", kComfort, sizeof(kComfort) / sizeof(kComfort[0])},
	{"Done", kDone, sizeof(kDone) / sizeof(kDone[0])},
};

constexpr UInt32 kPageCount = sizeof(kPages) / sizeof(kPages[0]);

}  // namespace

const OnboardingPage* OnboardingPages() { return kPages; }
UInt32 OnboardingPageCount() { return kPageCount; }

const OnboardingPage& OnboardingMenu::CurrentPage() const {
	return kPages[m_page < kPageCount ? m_page : kPageCount - 1];
}

const char* OnboardingMenu::Title() const { return CurrentPage().title; }

UInt32 OnboardingMenu::FirstSelectable(const OnboardingPage& page) const {
	for (UInt32 i = 0; i < page.rowCount; ++i) {
		if (page.rows[i].kind != OnboardingRowKind::Text) {
			return i;
		}
	}
	return 0;
}

// The next selectable row in the given direction, wrapping, or `from` when
// nothing else can be selected.
UInt32 OnboardingMenu::StepSelection(const OnboardingPage& page, UInt32 from, bool down) const {
	if (page.rowCount == 0) {
		return 0;
	}
	UInt32 at = from;
	for (UInt32 step = 0; step < page.rowCount; ++step) {
		at = down ? (at + 1) % page.rowCount : (at + page.rowCount - 1) % page.rowCount;
		if (page.rows[at].kind != OnboardingRowKind::Text) {
			return at;
		}
	}
	return from;
}

void OnboardingMenu::Open() {
	m_open = true;
	m_page = 0;
	m_state = MenuState{};
	m_state.selected = FirstSelectable(CurrentPage());
	++m_revision;
}

void OnboardingMenu::Close() {
	if (m_open) {
		m_open = false;
		++m_revision;
	}
}

void OnboardingMenu::SetVisibleRows(UInt32 rows) { m_visibleRows = rows == 0 ? 1 : rows; }

const SettingDefinition* OnboardingMenu::Apply(MenuAction action, Config& config) {
	if (!m_open || action == MenuAction::None) {
		return nullptr;
	}
	const OnboardingPage& page = CurrentPage();
	if (page.rowCount == 0) {
		return nullptr;
	}
	if (m_state.selected >= page.rowCount) {
		m_state.selected = FirstSelectable(page);
	}

	if (action == MenuAction::Up || action == MenuAction::Down || action == MenuAction::PageUp ||
	    action == MenuAction::PageDown) {
		const bool down = action == MenuAction::Down || action == MenuAction::PageDown;
		const UInt32 moved = StepSelection(page, m_state.selected, down);
		if (moved != m_state.selected) {
			m_state.selected = moved;
			// The window follows the selection the way the settings menu's
			// does, by the least it can.
			if (m_state.selected < m_state.firstVisible) {
				m_state.firstVisible = m_state.selected;
			} else if (m_state.selected >= m_state.firstVisible + m_visibleRows) {
				m_state.firstVisible = m_state.selected + 1 - m_visibleRows;
			}
			++m_revision;
		}
		return nullptr;
	}

	const OnboardingRow& row = page.rows[m_state.selected];
	if (row.kind == OnboardingRowKind::Action) {
		switch (row.action) {
		case OnboardingAction::Next:
			if (m_page + 1 < kPageCount) {
				++m_page;
				m_state = MenuState{};
				m_state.selected = FirstSelectable(CurrentPage());
				++m_revision;
			}
			break;
		case OnboardingAction::Back:
			if (m_page > 0) {
				--m_page;
				m_state = MenuState{};
				m_state.selected = FirstSelectable(CurrentPage());
				++m_revision;
			}
			break;
		case OnboardingAction::Finish:
			Close();
			break;
		case OnboardingAction::None:
			break;
		}
		return nullptr;
	}

	if (row.kind == OnboardingRowKind::Setting) {
		const SettingDefinition* definition = FindSetting(row.iniSection, row.iniKey);
		if (definition == nullptr) {
			return nullptr;
		}
		const MenuItem item = ItemFor(*definition, config);
		const float wanted = AdjustValue(item, action);
		if (wanted == item.value) {
			return nullptr;
		}
		ApplySetting(*definition, config, wanted);
		++m_revision;
		return definition;
	}
	return nullptr;
}

UInt32 OnboardingMenu::BuildRows(const Config& config, MenuItem* items, const char** categories,
                                 UInt32 capacity) const {
	const OnboardingPage& page = CurrentPage();
	UInt32 count = 0;
	for (UInt32 i = 0; i < page.rowCount && count < capacity; ++i) {
		const OnboardingRow& row = page.rows[i];
		MenuItem item;
		switch (row.kind) {
		case OnboardingRowKind::Text:
			item.label = row.text;
			item.kind = ItemKind::Text;
			break;
		case OnboardingRowKind::Action:
			item.label = row.text;
			item.help = row.help;
			item.kind = ItemKind::Action;
			break;
		case OnboardingRowKind::Setting: {
			const SettingDefinition* definition = FindSetting(row.iniSection, row.iniKey);
			if (definition == nullptr) {
				// A row naming a setting the table does not have is a bug in
				// this file; shown as text so it is seen rather than skipped.
				item.label = row.iniKey;
				item.kind = ItemKind::Text;
			} else {
				item = ItemFor(*definition, config);
			}
			break;
		}
		}
		items[count] = item;
		categories[count] = page.title;
		++count;
	}
	return count;
}

}  // namespace obvr::ui

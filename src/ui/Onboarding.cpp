#include "ui/Onboarding.h"

namespace obvr::ui {
namespace {

// The two pictures, as rows of characters the canvas draws at twice the
// text's scale: '#' is the figure, 'o' the controllers. Kept here rather
// than in an image file because the DLL loads nothing from disk for its
// menus, and a picture that can be read in the source can be fixed there.

constexpr const char* kSeatedIcon[] = {
	"         ####           ",
	"         ####           ",
	"          ##            ",
	"        ######          ",
	"       ########         ",
	"       ## ## ##         ",
	"       ## ## ##         ",
	"      oo ##   oo        ",
	"       ooooooooo        ",
	"         ##  ######     ",
	"   ###   ##      ##     ",
	"   # #   ##      ##     ",
	"   # ##########  ##     ",
	"   #     #    #  ##     ",
	"   #     #    #  ##     ",
	"   #     #    #  ##     ",
};

constexpr const char* kStandingIcon[] = {
	"          ####          ",
	"          ####          ",
	"           ##           ",
	"   oo    ######    oo   ",
	"   oo   ########   oo   ",
	"    ## ##  ##  ## ##    ",
	"     ###   ##   ###     ",
	"           ##           ",
	"         ######         ",
	"         ######         ",
	"          #  #          ",
	"         ##  ##         ",
	"         ##  ##         ",
	"        ##    ##        ",
	"        ##    ##        ",
	"       ###    ###       ",
};

constexpr UInt32 kSeatedRows = sizeof(kSeatedIcon) / sizeof(kSeatedIcon[0]);
constexpr UInt32 kStandingRows = sizeof(kStandingIcon) / sizeof(kStandingIcon[0]);

constexpr OnboardingRow kWelcome[] = {
	{OnboardingRowKind::Text, "Welcome to OBVR: Oblivion in your headset.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Choose how you want to play. Right confirms.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Choice, "Seated Experience", "Keyboard, mouse or gamepad. Oblivion in VR.",
	 "Hands", "Enabled", OnboardingAction::ChooseSeated, kSeatedIcon, kSeatedRows},
	{OnboardingRowKind::Choice, "Standing Experience",
	 "Motion controllers and hands, like Skyrim VR.", "Hands", "Enabled",
	 OnboardingAction::ChooseStanding, kStandingIcon, kStandingRows},
};

constexpr OnboardingRow kControls[] = {
	{OnboardingRowKind::Text, "Seated: Del recenters the view, Insert opens the settings.", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "The arrow keys steer OBVR's menus.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Standing: click both sticks for the settings menu.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Touch the wrist menu with a finger to press an entry.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Recenter view is the first row of the settings menu.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Either way, this can be changed later in the settings.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next, nullptr,
	 0},
};

constexpr OnboardingRow kComfort[] = {
	{OnboardingRowKind::Text, "Turning left and right stays with the mouse or the stick.", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Smooth turning eases it; snap turning is planned.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Look", "SmoothTurning", OnboardingAction::None, nullptr,
	 0},
	{OnboardingRowKind::Setting, "", "", "Look", "TurnSpeed", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Menus can hang in the world or on a cinema screen.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Render", "Menus", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next, nullptr,
	 0},
};

constexpr OnboardingRow kDone[] = {
	{OnboardingRowKind::Text, "That is all. Everything here lives in OBVR.ini and in the", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "settings menu, where this walkthrough can be reopened.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Onboarding", "ShowAtStart", OnboardingAction::None,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Finish", "Close the walkthrough", "", "",
	 OnboardingAction::Finish, nullptr, 0},
};

constexpr OnboardingPage kPages[] = {
	{"Welcome", kWelcome, sizeof(kWelcome) / sizeof(kWelcome[0])},
	{"Controls", kControls, sizeof(kControls) / sizeof(kControls[0])},
	{"Comfort", kComfort, sizeof(kComfort) / sizeof(kComfort[0])},
	{"Done", kDone, sizeof(kDone) / sizeof(kDone[0])},
};

constexpr UInt32 kPageCount = sizeof(kPages) / sizeof(kPages[0]);

bool Selectable(const OnboardingRow& row) { return row.kind != OnboardingRowKind::Text; }

}  // namespace

const OnboardingPage* OnboardingPages() { return kPages; }
UInt32 OnboardingPageCount() { return kPageCount; }

bool ChoiceIsCurrent(OnboardingAction choice, const Config& config) {
	switch (choice) {
	case OnboardingAction::ChooseSeated:
		return !config.handTracking;
	case OnboardingAction::ChooseStanding:
		return config.handTracking;
	default:
		return false;
	}
}

const OnboardingPage& OnboardingMenu::CurrentPage() const {
	return kPages[m_page < kPageCount ? m_page : kPageCount - 1];
}

const char* OnboardingMenu::Title() const { return CurrentPage().title; }

UInt32 OnboardingMenu::FirstSelectable(const OnboardingPage& page) const {
	for (UInt32 i = 0; i < page.rowCount; ++i) {
		if (Selectable(page.rows[i])) {
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
		if (Selectable(page.rows[at])) {
			return at;
		}
	}
	return from;
}

void OnboardingMenu::TurnTo(UInt32 page) {
	m_page = page;
	m_state = MenuState{};
	m_state.selected = FirstSelectable(CurrentPage());
	++m_revision;
}

void OnboardingMenu::Open() {
	m_open = true;
	TurnTo(0);
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
	if (row.kind == OnboardingRowKind::Choice) {
		// The choice writes the mode through the settings table - the same
		// key the settings menu changes - and goes on to the next page. The
		// definition is answered so the caller saves it, even when the mode
		// was already the one chosen: the file then says what was confirmed.
		const SettingDefinition* definition = FindSetting(row.iniSection, row.iniKey);
		if (definition == nullptr) {
			return nullptr;
		}
		ApplySetting(*definition, config,
		             row.action == OnboardingAction::ChooseStanding ? 1.0f : 0.0f);
		if (m_page + 1 < kPageCount) {
			TurnTo(m_page + 1);
		} else {
			++m_revision;
		}
		return definition;
	}

	if (row.kind == OnboardingRowKind::Action) {
		switch (row.action) {
		case OnboardingAction::Next:
			if (m_page + 1 < kPageCount) {
				TurnTo(m_page + 1);
			}
			break;
		case OnboardingAction::Back:
			if (m_page > 0) {
				TurnTo(m_page - 1);
			}
			break;
		case OnboardingAction::Finish:
			Close();
			break;
		default:
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
		case OnboardingRowKind::Choice:
			item.label = row.text;
			item.help = row.help;
			item.kind = ItemKind::Action;
			item.icon = row.icon;
			item.iconRows = row.iconRows;
			item.chosen = ChoiceIsCurrent(row.action, config);
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

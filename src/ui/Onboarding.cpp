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
	{OnboardingRowKind::Text, "Welcome to Oblivion in VR.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "How do you want to play? Press Right to pick.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Choice, "Vanilla VR", "Gamepad or mouse and keyboard. The headset is your view.",
	 "Hands", "Enabled", OnboardingAction::ChooseSeated, kSeatedIcon, kSeatedRows},
	{OnboardingRowKind::Choice, "Full VR",
	 "Play with your hands. Beta, so please tell us what breaks.", "Hands",
	 "Enabled", OnboardingAction::ChooseStanding, kStandingIcon, kStandingRows},
	{OnboardingRowKind::Text, "Full VR is still in beta, so expect a few rough edges.",
	 "", "", "", OnboardingAction::None, nullptr, 0},
};

constexpr OnboardingRow kControls[] = {
	{OnboardingRowKind::Text, "Vanilla VR: Del recenters, Insert opens the settings.", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Use the arrow keys in OBVR's menus.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Full VR: grab at your hip or back to draw, raise your shield to block.",
	 "", "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Click both sticks for the settings, hold them to recenter.",
	 "", "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Recenter view is also at the top of the settings.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "You can switch later in the settings.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next, nullptr,
	 0},
};

constexpr OnboardingRow kComfort[] = {
	{OnboardingRowKind::Text, "You still turn with the mouse or the stick.", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Turns can glide or snap. Both are in the settings.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Look", "SmoothTurning", OnboardingAction::None, nullptr,
	 0},
	{OnboardingRowKind::Setting, "", "", "Look", "TurnSpeed", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Menus float in front of you or on a big screen.", "", "", "",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Render", "Menus", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Full VR: your inventory and map sit in your left hand. Tap with the other.",
	 "", "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Tablet", "Enabled", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "Full VR: while you sneak, the edges darken or an eye shows if you're hidden.",
	 "", "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Hands", "SneakIndicator", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next, nullptr,
	 0},
};

// Full VR only; shown to everyone, since the choice can be changed later and
// the seated mode ignores it.
constexpr OnboardingRow kTeleport[] = {
	{OnboardingRowKind::Text, "Full VR: hold the right stick up, aim and let go", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "to teleport. A quick flick kicks, B jumps.", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Locomotion", "Teleport", OnboardingAction::None, nullptr,
	 0},
	{OnboardingRowKind::Setting, "", "", "Locomotion", "TeleportInstant", OnboardingAction::None,
	 nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Locomotion", "TeleportRangeMetres",
	 OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Locomotion", "TeleportInCombat", OnboardingAction::None,
	 nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Locomotion", "TeleportVignette", OnboardingAction::None,
	 nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Locomotion", "TeleportBlink", OnboardingAction::None,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Next", "The next page", "", "", OnboardingAction::Next, nullptr,
	 0},
};

constexpr OnboardingRow kDone[] = {
	{OnboardingRowKind::Text, "That's it. You'll find all of this in the settings,", "",
	 "", "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Text, "and you can bring this intro back from there.", "", "",
	 "", OnboardingAction::None, nullptr, 0},
	{OnboardingRowKind::Setting, "", "", "Onboarding", "ShowAtStart", OnboardingAction::None,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Back", "The previous page", "", "", OnboardingAction::Back,
	 nullptr, 0},
	{OnboardingRowKind::Action, "Finish", "Close the intro", "", "",
	 OnboardingAction::Finish, nullptr, 0},
};

constexpr OnboardingPage kPages[] = {
	{"Welcome", kWelcome, sizeof(kWelcome) / sizeof(kWelcome[0])},
	{"Controls", kControls, sizeof(kControls) / sizeof(kControls[0])},
	{"Comfort", kComfort, sizeof(kComfort) / sizeof(kComfort[0])},
	{"Teleport", kTeleport, sizeof(kTeleport) / sizeof(kTeleport[0])},
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
		return !config.fullVrMode;
	case OnboardingAction::ChooseStanding:
		return config.fullVrMode;
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
		// Either choice is taken (the standing one too, since 2026-10-15: in
		// beta, as the native tile has had it since 2026-09-27). The choice
		// writes the mode through the settings table - the same
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

bool OnboardingMenu::Hover(UInt32 index) {
	if (!m_open) {
		return false;
	}
	const OnboardingPage& page = CurrentPage();
	if (index >= page.rowCount || !Selectable(page.rows[index]) || index == m_state.selected) {
		return false;
	}
	m_state.selected = index;
	++m_revision;
	return true;
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

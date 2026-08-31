#include "ui/SettingsMenu.h"

namespace obvr::ui {

namespace {

bool SameText(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

// How many DRAWN lines the rows from `first` to `last` take up, headings
// included.
//
// Not the same as the number of rows, and that difference is the whole reason
// this exists. The painter gives a category heading a line of its own, so a
// window holding two headings shows two fewer settings than it has room for -
// and a selection counted in rows can then sit below the bottom of a window
// measured in lines, with the highlight simply not drawn. The wearer sees a
// list that stops responding to the down key.
UInt32 LinesFor(UInt32 first, UInt32 last) {
	const SettingDefinition* const settings = SettingDefinitions();

	UInt32 lines = 0;
	for (UInt32 at = first; at <= last; ++at) {
		if (at > first && !SameText(settings[at].category, settings[at - 1].category)) {
			++lines;
		}
		++lines;
	}
	return lines;
}

}  // namespace

void SettingsMenu::EnsureSelectionFits() {
	const UInt32 count = SettingDefinitionCount();
	if (count == 0 || m_state.selected >= count) {
		return;
	}

	// The window is pulled down one row at a time until the selection fits in
	// the lines available. One row at a time rather than by calculation,
	// because the answer depends on where the category boundaries fall and a
	// closed form for that is a harder thing to get right than a loop over
	// twenty entries.
	while (m_state.firstVisible < m_state.selected &&
	       LinesFor(m_state.firstVisible, m_state.selected) > m_visibleRows) {
		++m_state.firstVisible;
	}
}

void SettingsMenu::Toggle() {
	m_open = !m_open;

	// The revision moves even though the rows did not, because the layer uses
	// it to decide whether to repaint - and the menu that comes back has to be
	// drawn at least once after being hidden.
	++m_revision;
}

void SettingsMenu::SetVisibleRows(UInt32 rows) {
	if (rows == 0) {
		rows = 1;
	}
	if (rows == m_visibleRows) {
		return;
	}

	m_visibleRows = rows;

	// The window may now be showing rows that are no longer there, or leaving
	// blank space below the last one. AdvanceMenu with no action is exactly the
	// routine that puts a window back in range, so it is used rather than
	// repeated here.
	m_state = AdvanceMenu(m_state, MenuAction::None, SettingDefinitionCount(), m_visibleRows);
	EnsureSelectionFits();
	++m_revision;
}

const SettingDefinition* SettingsMenu::Apply(MenuAction action, Config& config) {
	if (!m_open || action == MenuAction::None) {
		return nullptr;
	}

	const UInt32 count = SettingDefinitionCount();
	if (count == 0) {
		return nullptr;
	}

	if (action == MenuAction::Decrease || action == MenuAction::Increase) {
		const MenuState before = m_state;

		// Brought into range first. The selection can be past the end if the
		// table shrank, and writing through that row would be writing through
		// whatever is next to the table.
		m_state = AdvanceMenu(m_state, MenuAction::None, count, m_visibleRows);
		if (m_state.selected >= count) {
			m_state = before;
			return nullptr;
		}

		const SettingDefinition& definition = SettingDefinitions()[m_state.selected];
		const MenuItem item = ItemFor(definition, config);
		const float wanted = AdjustValue(item, action);

		// Nothing changed - a value already at the end of its range - so
		// nothing is written and nothing is repainted. Without this the layer
		// would repaint on every press of a key that does nothing, and the INI
		// would be written on every one of them too.
		if (wanted == item.value) {
			return nullptr;
		}

		ApplySetting(definition, config, wanted);
		++m_revision;
		return &definition;
	}

	const MenuState moved = AdvanceMenu(m_state, action, count, m_visibleRows);
	if (moved.selected == m_state.selected && moved.firstVisible == m_state.firstVisible) {
		return nullptr;
	}

	m_state = moved;
	EnsureSelectionFits();
	++m_revision;
	return nullptr;
}

UInt32 SettingsMenu::BuildRows(const Config& config, MenuItem* items, const char** categories,
                               UInt32 capacity) const {
	if (items == nullptr || categories == nullptr) {
		return 0;
	}

	const SettingDefinition* const settings = SettingDefinitions();
	UInt32 count = SettingDefinitionCount();
	if (count > capacity) {
		count = capacity;
	}

	for (UInt32 at = 0; at < count; ++at) {
		items[at] = ItemFor(settings[at], config);
		categories[at] = settings[at].category;
	}
	return count;
}

}  // namespace obvr::ui

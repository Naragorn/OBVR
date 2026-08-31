#include "ui/SettingsMenu.h"

namespace obvr::ui {

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

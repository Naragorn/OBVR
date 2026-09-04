#pragma once

#include "core/Config.h"
#include "ui/MenuModel.h"
#include "ui/SettingsList.h"

namespace obvr::ui {

// The settings menu as a thing with a state, between the table that says what
// the settings are and the layer that draws them.
//
// Holds no Direct3D and no Windows: which key does what is decided by the
// caller and arrives here as a MenuAction, so all of this can be driven from a
// test. What it does own is the part that would otherwise be scattered through
// the frame hook - whether the menu is open, where the highlight is, and the
// revision the layer repaints on.
//
// The rows are rebuilt from the configuration every time they are asked for
// rather than kept. It costs almost nothing, and it means a value changed from
// somewhere else - the INI hot reload, most likely - shows up in the menu
// instead of the menu showing a stale copy and then writing it back.

class SettingsMenu {
public:
	bool IsOpen() const { return m_open; }

	// Opening does not reset the selection. Somebody who closes the menu to
	// look at something and opens it again is almost always coming back to the
	// same row, and losing their place is a small insult every time.
	void Toggle();

	// The visible row count comes from the layer, which owns the texture and
	// therefore knows how many rows fit. Kept here because the scrolling has to
	// use the same number the painter does, or the highlight scrolls a row too
	// early or a row too late.
	void SetVisibleRows(UInt32 rows);

	// Moves the highlight, or changes the selected setting.
	//
	// Both go through here so that the revision - the thing the layer repaints
	// on - is advanced in exactly one place. A change that did not bump it
	// would leave the wearer looking at the old picture and concluding the menu
	// had stopped responding.
	//
	// Answers the setting whose value changed, and null when nothing did -
	// which includes every movement, and a value already at the end of its
	// range. The caller writes that setting back to the INI. A row of kind
	// Action is answered too, unchanged: it is a button, and the caller is
	// the one who knows what pressing it does.
	//
	// Reported rather than written here on purpose. Writing needs the Windows
	// INI functions, and pulling those in would take this whole class out of
	// reach of a test for the sake of one line at the call site.
	const SettingDefinition* Apply(MenuAction action, Config& config);

	// Fills `items` and `categories` from the configuration, and answers how
	// many rows were written. Never writes more than `capacity`.
	UInt32 BuildRows(const Config& config, MenuItem* items, const char** categories,
	                 UInt32 capacity) const;

	MenuState State() const { return m_state; }

	// Advanced whenever the picture would differ. The layer repaints on a
	// change and holds its last picture otherwise.
	UInt32 Revision() const { return m_revision; }

private:
	// Pulls the window down until the selected row is one of the lines actually
	// drawn - see LinesFor, and the note there on why rows and lines differ.
	void EnsureSelectionFits();

	MenuState m_state;
	UInt32 m_revision = 1;
	UInt32 m_visibleRows = 10;
	bool m_open = false;
};

}  // namespace obvr::ui

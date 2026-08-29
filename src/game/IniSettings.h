#pragma once

#include "core/Types.h"

namespace obvr::game {

// The one place Oblivion's believed screen size lives, and the two settings
// that hold it.
//
// The layout probe settled how the 2D breaks on an eye-sized frame: films and
// the main menu's background laid out against the INI's 2560x1440, the ESC
// and inventory menus against the real 4028x3380 buffer, and the mouse fell
// between the two - cursor everywhere, clicks nowhere. Writing the game's own
// numbers back into its CreateDevice parameters changed nothing, measured to
// the pixel: nothing reads that structure back. What everything early reads
// is "iSize W" and "iSize H", the INI settings the engine keeps alive in
// memory as an IniSettingCollection.
//
// So instead of teaching every consumer about two sizes, the two settings are
// rewritten to the eye size at the moment the device is created - before the
// main menu, the videos, and the mouse mapping are built. From then on there
// is one screen size in the whole process, which is the state the game shipped
// in.
//
// Everything here refuses rather than guesses. The collection object is
// validated before it is believed (vtable inside the executable, an .ini path
// in its path field), and a setting is only written when its name matches
// exactly and its current value is the very number the game is asking
// CreateDevice for. See kIniSettingCollection in GameAddresses.h for the
// address and its sources.

// The engine's own shapes, as xOBSE lays them out in obse/GameAPI.h. The
// value union first, then the name - the name's first letter is the type,
// 'i' for the integers the screen size is stored as.
struct IniSettingInfo {
	union {
		bool b;
		float f;
		int i;
		char* s;
		UInt32 u;
	};
	const char* name;
};

struct IniSettingEntry {
	IniSettingInfo* data;
	IniSettingEntry* next;
};

// Finds "iSize W" and "iSize H" in the list and rewrites both - or neither.
//
// Both are located and checked before either is written, so a refusal can
// never leave the game believing a screen that is wide like one size and
// tall like the other. expectedWidth/Height are what the settings must
// currently hold (the size the game just asked CreateDevice for); a mismatch
// means the list is not what it is believed to be, and nothing is touched.
//
// The walk is bounded, so a corrupt or cyclic list ends in a refusal rather
// than a hang. Entries with no data or no name are skipped - the engine's
// list is allowed to be ragged without being wrong.
bool OverrideSizeSettings(IniSettingEntry* first, UInt32 expectedWidth,
                          UInt32 expectedHeight, UInt32 newWidth, UInt32 newHeight);

// The collection at its known address, believed only after validation:
// the vtable pointer must lie inside the executable image and the path field
// must read as a printable .ini path. Returns the head of the setting list,
// or null with the reason in the log. Lives apart from the pure walk above
// because this half needs the process and the walk does not.
IniSettingEntry* ResolveIniSettingsList();

}  // namespace obvr::game

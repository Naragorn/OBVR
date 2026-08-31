#pragma once

#include "core/Config.h"
#include "ui/MenuModel.h"

namespace obvr::ui {

// Which settings the in-headset menu offers, and how each one reaches the
// configuration it belongs to.
//
// This is the bridge the model deliberately does not have. Everything with a
// decision in it - moving, stepping, clamping, formatting - lives in MenuModel
// over plain values; this file is the one place that knows a row is a field,
// and it is kept to a table for exactly that reason. A table can be checked
// exhaustively, and it is: the tests write through every entry and read it back,
// and then confirm that no entry disturbs any other. That second check is the
// one worth having. A table of getters and setters is written by copying the
// row above and editing it, and the way that goes wrong is a row that reads its
// own field and writes the one it was copied from - which behaves perfectly
// until somebody changes the setting, and then quietly changes a different one.
//
// Not every setting is here, and that is a choice rather than an oversight.
// OBVR.ini has sixty-five, and most of the ones left out are probes, addresses
// and diagnostics that would only be set while reading the file that explains
// them. What is here is what somebody would want to change with the headset on.

struct SettingDefinition {
	// The heading this row sits under. Rows sharing a category are adjacent in
	// the table, and the menu draws the name when it changes.
	const char* category = "";

	const char* label = "";
	const char* help = "";

	ItemKind kind = ItemKind::Toggle;

	float minimum = 0.0f;
	float maximum = 1.0f;
	float step = 0.1f;
	UInt32 decimals = 2;

	// True when the value is only read while the device is being created, so
	// changing it here does nothing until the next start. Shown on the row -
	// a menu that silently ignores a change teaches people the menu is broken.
	bool needsRestart = false;

	// Where the value lives in OBVR.ini.
	//
	// A change is written back there, and that is not only about surviving a
	// quit. ReloadEveryFrames re-reads the file while the game runs, so a
	// change held only in memory is overwritten by the file within a couple of
	// seconds - which is exactly what the first run of this menu did, and it
	// looked like the menu was ignoring every keypress. Writing to the file
	// makes the file agree, and then the reload changes nothing.
	//
	// These have to match what Config.cpp reads, character for character. A key
	// that is written but not read looks identical to no menu at all, so there
	// is a test that writes through every row into a real INI and loads it back
	// through Config's own reader.
	const char* iniSection = "";
	const char* iniKey = "";

	float (*Read)(const Config& config) = nullptr;
	void (*Write)(Config& config, float value) = nullptr;

	// For the settings Config reads as a WORD rather than as a number - Menus
	// is "cinema" or "world", not 0 or 1. Writing a 1 into one of those leaves
	// a value its own reader rejects, which it reports and then ignores, and
	// the setting silently never changes.
	//
	// Last in the struct so that the rows which do not need them can simply
	// stop after the writer. Both empty means the value is written as a number.
	const char* falseWord = "";
	const char* trueWord = "";
};

// The table, and its length. A pointer to static data - there is one settings
// list and it does not change.
const SettingDefinition* SettingDefinitions();
UInt32 SettingDefinitionCount();

// One row, filled in from the configuration as it stands.
//
// A definition with no Read gives an item at its minimum rather than reading
// through a null pointer, so a half-finished table row is a wrong-looking menu
// instead of a crash.
MenuItem ItemFor(const SettingDefinition& definition, const Config& config);

// Puts a changed value back. Clamped again here rather than trusted from the
// caller: the value arrives from a menu, and a menu is a place a value can be
// carried in from a saved state that no longer matches the range.
void ApplySetting(const SettingDefinition& definition, Config& config, float value);

}  // namespace obvr::ui

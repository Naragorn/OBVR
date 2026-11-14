#include "ui/SettingsList.h"
#include "ui/SettingsPartBody.h"
#include "ui/SettingsPartCombat.h"
#include "ui/SettingsPartHands.h"
#include "ui/SettingsPartStart.h"
#include "ui/SettingsPartView.h"

namespace obvr::ui {

namespace {

// Captureless lambdas so that each row's reach into the configuration sits on
// the row itself. Naming a member twice - once to read, once to write - is what
// makes the "no row disturbs another" test able to find a mistake at all: there
// is nothing else to compare against.
//
// The rows live in five parts by the menu's headings (ui/SettingsPart*.h) and
// are joined here at compile time into the one table the menu indexes, in the
// order the parts are named below. A row goes into the part of its heading; a
// new heading goes where it should appear in the menu. tests/settings-rows.txt
// pins every row and its place.

using namespace settings_rows;

template <UInt32 N>
struct RowTable {
	SettingDefinition rows[N];
};

template <UInt32 Total, UInt32... N>
constexpr RowTable<Total> JoinRows(const SettingDefinition (&... parts)[N]) {
	RowTable<Total> table{};
	UInt32 at = 0;
	auto put = [&table, &at](const SettingDefinition* part, UInt32 count) {
		for (UInt32 i = 0; i < count; ++i) {
			table.rows[at++] = part[i];
		}
	};
	(put(parts, N), ...);
	return table;
}

// The pack is named for MSVC, which does not parse an unnamed pack of array
// references (C2988 at "(&...)[N]", every CI run since a47f13b).
template <UInt32... N>
constexpr UInt32 RowCount(const SettingDefinition (&... parts)[N]) {
	((void)parts, ...);
	return (N + ...);
}

constexpr UInt32 kRowCount = RowCount(kStartRows, kCombatRows, kHandsRows, kBodyRows, kViewRows);
constexpr RowTable<kRowCount> kSettings =
	JoinRows<kRowCount>(kStartRows, kCombatRows, kHandsRows, kBodyRows, kViewRows);

}  // namespace

const SettingDefinition* SettingDefinitions() { return kSettings.rows; }

UInt32 SettingDefinitionCount() { return kRowCount; }

}  // namespace obvr::ui

#pragma once

#include "core/Types.h"

namespace obvr::game {

// One of the eight hotkeys as the quick menu shows it: whether it holds
// anything, the form's type (0x10 a spell; the item types as the engine
// numbers them) and its display name, cut to fit.
struct QuickKeySlot {
	bool filled = false;
	UInt8 formType = 0;
	char name[40] = "";
	// The form's icon path, relative to Textures\Menus\Icons\ (game/ItemIcons.h);
	// empty when it has none.
	char iconPath[128] = "";
};

constexpr int kQuickKeyCount = 8;

// Reads the eight hotkeys (addr::kQuickKeyLists). A slot that cannot be read
// stays empty; a form without a name keeps an empty name and is still filled.
void ReadQuickKeys(QuickKeySlot (&slots)[kQuickKeyCount]);

// The eight lists as raw words - start node and count each - for the log:
// what an "all empty" ring was read from.
void DescribeQuickKeyLists(char* out, UInt32 size);

// A form's TESFullName into `out`, cut to fit; empty when it has none or the
// form cannot be read. For a reference, pass its base form.
void ReadFormFullName(UInt32 form, char* out, UInt32 size);

}  // namespace obvr::game

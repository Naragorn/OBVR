#pragma once

#include "core/Types.h"
#include "obse/PluginInterface.h"

namespace obvr::game {

// Pages of hotkeys for the quick menu's ring (docs/controls-spec.md 4.4):
// the game keeps eight, and the ring turns through several pages of eight.
//
// The page shown is always the game's own eight - using a slot taps its
// number key, setting one in the inventory or the magic menu goes the
// vanilla way, and a save keeps it as vanilla does. Turning the page writes
// the next page's forms into the game's eight and keeps the ones that were
// there; the pages not shown are kept by OBVR, in the xOBSE co-save beside
// the save.
//
// How the game keeps its eight (read with dumpbin, 2026-09-28):
// - g_quickKeyList, 0x00B3B440: eight pointer lists (GameAddresses.h). The
//   save writes each list's forms (0x005C0E30, the form's refID at +0xC).
// - An item's hotkey is kept on its stack in the inventory as well: extra
//   data 0x55, a signed byte, the slot (0x00422C40 reads it, 0x00422BA0 sets
//   it). The engine checks the two against each other at 0x005C1900 - called
//   from about ten places - and takes out of a list any item whose stack
//   does not carry that slot, and any spell the player does not know.
// - Setting a hotkey from the inventory (0x005ABC60-0x005ABCC9, then
//   0x005C25C0): the item that was on the slot loses its extra
//   (ExtraContainerChanges 0x004895B0(form, slot)), the list is emptied
//   (0x00573880) and the form added at its end (0x005B1E20); an item's stack
//   gets the slot - set on its extra when it has one (0x00422BA0), else
//   through 0x00489820(form, extra list, slot); the byte 0x00B3B43C is set.
// Turning a page does the same for each of the eight, without the sound
// and the menu, and then lets the engine's own check (0x005C1900) take out
// whatever the player no longer has.

constexpr int kQuickKeyPageSlots = 8;
constexpr int kQuickKeyPagesMax = 5;

struct QuickKeyPages {
	int current = 0;
	// Each page's forms by form id, 0 for an empty slot. The current page's
	// are what the game's eight held when it was last turned away from or
	// saved; the game's lists are the truth for it.
	UInt32 forms[kQuickKeyPagesMax][kQuickKeyPageSlots] = {};
};

// The page after `current` of `count`, round to the first.
inline int NextQuickKeyPage(int current, int count) {
	if (count <= 1) {
		return 0;
	}
	const int next = current + 1;
	return next >= count || next < 0 ? 0 : next;
}

// Turns to page `to`: what the game's eight hold now is kept as the current
// page, and `write` is what goes into them. The pure half of a turn.
inline void TurnQuickKeyPages(QuickKeyPages& pages, const UInt32 (&now)[kQuickKeyPageSlots], int to,
                              UInt32 (&write)[kQuickKeyPageSlots]) {
	if (to < 0 || to >= kQuickKeyPagesMax) {
		to = 0;
	}
	if (pages.current >= 0 && pages.current < kQuickKeyPagesMax) {
		for (int i = 0; i < kQuickKeyPageSlots; ++i) {
			pages.forms[pages.current][i] = now[i];
		}
	}
	for (int i = 0; i < kQuickKeyPageSlots; ++i) {
		write[i] = pages.forms[to][i];
	}
	pages.current = to;
}

// The co-save record: 'QKPG', version 1, the current page and every page's
// forms as the save knew them.
constexpr UInt32 kQuickKeyPagesRecord = 0x51'4B'50'47;  // 'QKPG'
constexpr UInt32 kQuickKeyPagesVersion = 1;
struct QuickKeyPagesRecord {
	UInt32 current = 0;
	UInt32 forms[kQuickKeyPagesMax][kQuickKeyPageSlots] = {};
};

inline QuickKeyPagesRecord RecordOfPages(const QuickKeyPages& pages, const UInt32 (&now)[kQuickKeyPageSlots]) {
	QuickKeyPagesRecord record;
	const int current = pages.current >= 0 && pages.current < kQuickKeyPagesMax ? pages.current : 0;
	record.current = static_cast<UInt32>(current);
	for (int p = 0; p < kQuickKeyPagesMax; ++p) {
		for (int i = 0; i < kQuickKeyPageSlots; ++i) {
			record.forms[p][i] = p == current ? now[i] : pages.forms[p][i];
		}
	}
	return record;
}

// The pages from a record read back, each form id passed through `resolve`
// (the co-save's ResolveRefID: a mod list that changed moves ids, a mod
// that went takes them) - an id it refuses becomes an empty slot. A record
// of the wrong version or size leaves the pages empty and answers false.
template <class Resolve>
bool PagesOfRecord(const void* data, UInt32 length, UInt32 version, Resolve resolve, QuickKeyPages& out) {
	out = QuickKeyPages{};
	if (data == nullptr || version != kQuickKeyPagesVersion || length != sizeof(QuickKeyPagesRecord)) {
		return false;
	}
	const QuickKeyPagesRecord& record = *static_cast<const QuickKeyPagesRecord*>(data);
	out.current = record.current < static_cast<UInt32>(kQuickKeyPagesMax) ? static_cast<int>(record.current) : 0;
	for (int p = 0; p < kQuickKeyPagesMax; ++p) {
		for (int i = 0; i < kQuickKeyPageSlots; ++i) {
			UInt32 resolved = 0;
			out.forms[p][i] = record.forms[p][i] != 0 && resolve(record.forms[p][i], resolved) ? resolved : 0;
		}
	}
	return true;
}

// ------------------------------------------------------------ the game side

// Registers the co-save callbacks. False when xOBSE offers no co-save (the
// ring then has one page).
bool InstallQuickKeyPages(const obse::Interface* obse);
bool QuickKeyPagesAvailable();

// The page shown, 0-based.
int CurrentQuickKeyPage();

// Turns to the next page of `count` (or to the first, when `toFirst`):
// the game's eight kept, the next page's written. Answers the page shown.
int TurnQuickKeyPage(int count, bool toFirst = false);

}  // namespace obvr::game

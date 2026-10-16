#pragma once

// The spell ring on the other hand (docs/controls-spec.md 4.44; the tester,
// 2026-10-15: "linker stick press nicht mehr quests sondern eine zauberbuch
// als hotkey wie wir das schon rechts haben"): the player's spellbook on the
// quick menu's ring (vr/QuickMenu.h), opened at the other hand by its
// stick's short click, latched, the next click making the chosen spell the
// active one. The slots are the spells the player knows that can be cast -
// spells, greater and lesser powers; not diseases, not abilities - in the
// order of their names, eight to a page, the other hand's trigger turning
// the pages. The ring is the quick menu's state machine with its own state;
// what differs is where the slots come from and what a use does, and that
// is here. Pure, covered by spell_ring_test; the game side reads the book
// (game/SpellBook.h).

#include "core/Types.h"

namespace obvr::vr {

constexpr int kSpellRingSlots = 8;
// Ten pages of eight: the page dots (ui/QuickMenuPainter.h) stand clear of
// the side slots' boxes up to ten.
constexpr int kSpellRingMaxPages = 10;
constexpr int kSpellRingMaxSpells = kSpellRingSlots * kSpellRingMaxPages;

// A SpellItem's type (xOBSE GameForms.h, spellType at +0x38): a spell, a
// disease, a greater power, a lesser power, an ability.
enum class SpellKind : UInt32 { Spell = 0, Disease = 1, Power = 2, LesserPower = 3, Ability = 4 };

// Whether the magic menu would offer it to cast: spells and powers, not a
// disease or an ability, which the player carries but never casts.
inline bool SpellIsCastable(UInt32 spellType) {
	return spellType == static_cast<UInt32>(SpellKind::Spell) ||
	       spellType == static_cast<UInt32>(SpellKind::Power) ||
	       spellType == static_cast<UInt32>(SpellKind::LesserPower);
}

struct SpellEntry {
	UInt32 form = 0;  // the SpellItem
	char name[40] = "";
	// The icon path, relative to Textures\Menus\Icons\ (game/ItemIcons.h);
	// empty when it has none.
	char iconPath[128] = "";
};

struct SpellBook {
	int count = 0;
	SpellEntry spells[kSpellRingMaxSpells];
};

// The lower-case letter for the order; other characters as they are.
inline char LowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// Name before name, the letters' case left out of it; the same name by form
// id, so the order is the same every time the book is read.
inline bool SpellBefore(const SpellEntry& a, const SpellEntry& b) {
	for (int i = 0;; ++i) {
		const char ca = LowerAscii(a.name[i]);
		const char cb = LowerAscii(b.name[i]);
		if (ca != cb) {
			return static_cast<unsigned char>(ca) < static_cast<unsigned char>(cb);
		}
		if (ca == '\0') {
			break;
		}
	}
	return a.form < b.form;
}

// Adds a spell to the book: a castable one with room in the book. Answers
// whether it went in; the caller counts the ones left out.
inline bool AddSpell(SpellBook& book, UInt32 form, UInt32 spellType, const char* name, const char* iconPath) {
	if (form == 0 || !SpellIsCastable(spellType) || book.count >= kSpellRingMaxSpells) {
		return false;
	}
	SpellEntry& e = book.spells[book.count];
	e.form = form;
	int i = 0;
	for (; name != nullptr && name[i] != '\0' && i < static_cast<int>(sizeof(e.name)) - 1; ++i) {
		e.name[i] = name[i];
	}
	e.name[i] = '\0';
	i = 0;
	for (; iconPath != nullptr && iconPath[i] != '\0' && i < static_cast<int>(sizeof(e.iconPath)) - 1; ++i) {
		e.iconPath[i] = iconPath[i];
	}
	e.iconPath[i] = '\0';
	++book.count;
	return true;
}

// The book in the order of the names (an insertion sort: the book is small
// and read again each time the ring opens).
inline void SortSpellBook(SpellBook& book) {
	for (int i = 1; i < book.count; ++i) {
		const SpellEntry moving = book.spells[i];
		int j = i - 1;
		while (j >= 0 && SpellBefore(moving, book.spells[j])) {
			book.spells[j + 1] = book.spells[j];
			--j;
		}
		book.spells[j + 1] = moving;
	}
}

// How many pages a book of `count` spells fills: one at least, ten at most.
inline int SpellRingPageCount(int count) {
	if (count <= 0) {
		return 1;
	}
	const int pages = (count + kSpellRingSlots - 1) / kSpellRingSlots;
	return pages > kSpellRingMaxPages ? kSpellRingMaxPages : pages;
}

// A page held to the pages there are: a page past the end (the book shrank
// since) is the first.
inline int SpellRingPageClamp(int page, int pageCount) {
	return page < 0 || page >= pageCount ? 0 : page;
}

// The page after `page` of `pageCount`, round to the first.
inline int SpellRingNextPage(int page, int pageCount) {
	const int next = page + 1;
	return next >= pageCount || next < 0 ? 0 : next;
}

// The index into the book of slot `slot` on page `page`, -1 for an empty
// slot (past the book's end).
inline int SpellRingIndex(int page, int slot, int count) {
	if (page < 0 || slot < 0 || slot >= kSpellRingSlots) {
		return -1;
	}
	const int index = page * kSpellRingSlots + slot;
	return index < count ? index : -1;
}

// The slot of `form` on `page`, -1 when it is not on that page: where the
// active spell's mark goes.
inline int SpellRingSlotOf(const SpellBook& book, int page, UInt32 form) {
	if (form == 0) {
		return -1;
	}
	for (int slot = 0; slot < kSpellRingSlots; ++slot) {
		const int index = SpellRingIndex(page, slot, book.count);
		if (index >= 0 && book.spells[index].form == form) {
			return slot;
		}
	}
	return -1;
}

}  // namespace obvr::vr

#pragma once

// The game's book menu as it stands (docs/inventory-tablet-spec.md 25): its
// look - a book or a scroll, which a note or a letter is drawn on - its left
// page and its scroll bar, read from the menu's own tiles. The look decides
// how the tablet's finger and the right stick turn it (vr/TabletBook.h); the
// page and the bar are said at each hand-script mark, the harness's proof of
// a turn or a scroll.

#include "core/Types.h"

namespace obvr::game {

struct BookMenuRead {
	bool up = false;            // the book menu is loaded
	bool scroll = true;         // its scroll look (vr::BookMenuIsScroll of its user0)
	float user0 = 0.0f;         // as the game wrote it: 2 a book, 1 a scroll
	bool pageRead = false;
	float leftPage = 0.0f;      // a book's left page, counted from 0 (its root's user9: book_background's _pagenum)
	bool barRead = false;
	float barValue = 0.0f;      // a scroll's bar (book_scroll_bar's user7, its current value)
	bool rootScrollRead = false;
	float rootScroll = 0.0f;    // the root's user7: what the wheel 0x00595DA0 compares in the scroll look
	bool takeable = false;      // Take shown (its root's user3)
};

// Reads the book menu's root; `up` false when the menu is not loaded. Walks
// its tiles for the bar by name: at marks only, never per frame.
BookMenuRead ReadBookMenu();

// The look alone, per frame: the root and its user0, no walk. True (the
// scroll's rules) when the menu or its user0 is not read.
bool BookMenuScrollLook();

// Said at a hand-script mark while the book menu is loaded: the mark's name,
// the look, the page and the bar ("Book: at the mark <name> ...").
void LogBookMenuAtMark();

}  // namespace obvr::game

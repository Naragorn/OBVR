#pragma once

// Books and letters in the hand (docs/inventory-tablet-spec.md 25; Nadi,
// 2026-11-27: "bücher und letters wie das inventar in die hand mit swipes für
// nä seite usw."): the game's book menu read alone - a book, a note or a
// letter activated in the world - is held in the off hand as the F1-F4 page
// is (vr::TabletMenusUp), and the other hand's finger turns its pages or
// scrolls it. Pure; covered by tablet_book_test.
//
// The book menu has two looks (menus/book_menu.xml): a book, two pages turned
// by Prev and Next, and a scroll - what a note or a letter is drawn on - one
// page that scrolls. [R in Oblivion.exe 1.2.416] The menu's setup 0x00596020
// writes its user0 (trait 0x0FAE) as 1.0 when the BOOK record's flag 1 (a
// scroll) is set and 2.0 otherwise - the XML's &false; and &true;.

#include "core/Types.h"
#include "game/MenuType.h"
#include "vr/TabletLogic.h"

namespace obvr::vr {

// What the tablet holds: the F1-F4 pages (and what they raise, a book from
// the inventory among them), a book read alone, or a letter read alone.
enum class TabletLook : UInt8 { Pages, Book, Letter };

inline TabletLook TabletLookOf(const TabletFrame& f) {
	if (!f.up || f.page != game::kMenuIdBook) {
		return TabletLook::Pages;
	}
	return f.bookScroll ? TabletLook::Letter : TabletLook::Book;
}

// The book menu's user0 as the game wrote it: anything but its book's 2.0 is
// the scroll look - a menu whose user0 is not read takes the scroll's rules,
// which turn nothing.
inline bool BookMenuIsScroll(float user0) { return !(user0 > 1.5f); }

// The finger's swipe on it (vr/TabletTouch.h): sideways turns the F1-F4 page
// or a book's, never a letter's - it has one page; up and down scroll a list
// or a letter, never a book - the game's wheel turns a book's pages, and a
// finger drawn down the text is no turn (spec 25.4.4).
inline bool TabletTouchTurns(TabletLook look) { return look != TabletLook::Letter; }
inline bool TabletTouchScrolls(TabletLook look) { return look != TabletLook::Book; }

// What a turn (the finger's sideways swipe or the right stick's flick: +1 the
// page to the right, -1 the one to the left) sends: on the F1-F4 pages the key
// of the page beside the one shown (vr::TabletPageKeyAfter, by the caller);
// on a book one wheel notch, the menu's own turn by two pages with its page
// sound; on a letter nothing. [R in Oblivion.exe 1.2.416] The book menu's
// wheel 0x00595DA0 sets its page step (user6, 0x0FB4) to -2 for a notch up
// (a positive delta) and +2 for one down, then back to 0, and plays its user8
// sound when the left page (user9) moved; [M in the harness, book-probe.txt]
// see the spec's 25.7. So the finger moved left - the next pages, as a page
// is turned - is a notch down.
struct TabletTurn {
	bool pageKey = false;  // the F1-F4 page's key, as before
	int wheel = 0;         // notches, positive up
};

inline TabletTurn TabletTurnFor(TabletLook look, int step) {
	TabletTurn turn;
	if (step == 0) {
		return turn;
	}
	if (look == TabletLook::Pages) {
		turn.pageKey = true;
	} else if (look == TabletLook::Book) {
		turn.wheel = step > 0 ? -1 : 1;
	}
	return turn;
}

// The book's page from its pictures: the largest is the paper - the book's
// two pages or the letter's sheet - which the width is measured by
// (TabletPaperWidthMetres), and the face holds every picture of the menu: a
// letter's roller and its Take and Exit hang below its sheet (vanilla
// book_menu.xml: the roller 1171 units wide under a 990 sheet, the buttons
// 72 under it), which the F1-F4 page's join share (kPaperJoinShare) would cut
// off. Invalid as PaperOf is.
inline TabletRects BookPaperOf(const PaperTile* tiles, UInt32 count, float screenWidth, float screenHeight) {
	TabletRects rects = PaperOf(tiles, count, screenWidth, screenHeight);
	if (rects.valid) {
		rects.crop = UnionRect(rects.crop, PopupRect(tiles, count, screenWidth, screenHeight));
		rects.outline = rects.crop;
	}
	return rects;
}

// What the left B does on the tablet: the menu key, which opens and puts down
// the F1-F4 pages - and closes a book read alone as its own Exit does
// [M in the harness, book-probe.txt; spec 25.7].

}  // namespace obvr::vr

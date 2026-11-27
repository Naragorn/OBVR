#include "game/BookMenuRead.h"

#include <cstdio>

#include "core/Log.h"
#include "game/HudTiles.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/MenuPausePolicy.h"
#include "test/HandScriptRuntime.h"
#include "vr/TabletBook.h"

namespace obvr::game {
namespace {

// The traits (xOBSE's GameTiles.h ids; user0 0x0FAE counts up by one).
constexpr UInt16 kTraitUser0 = 0x0FAE;
constexpr UInt16 kTraitUser3 = 0x0FB1;
constexpr UInt16 kTraitUser7 = 0x0FB5;
constexpr UInt16 kTraitUser9 = 0x0FB7;
// The XML's &true;.
constexpr float kXmlTrue = 2.0f;

}  // namespace

BookMenuRead ReadBookMenu() {
	BookMenuRead read;
	const void* const root = MenuRootTile(kMenuIdBook);
	if (root == nullptr) {
		return read;
	}
	read.up = true;
	if (ReadTileNumber(root, kTraitUser0, read.user0)) {
		read.scroll = vr::BookMenuIsScroll(read.user0);
	}
	float takeable = 0.0f;
	read.takeable = ReadTileNumber(root, kTraitUser3, takeable) && takeable == kXmlTrue;
	read.pageRead = ReadTileNumber(root, kTraitUser9, read.leftPage);
	read.rootScrollRead = ReadTileNumber(root, kTraitUser7, read.rootScroll);
	// The bar by its vanilla name: a UI mod's may differ, then it is unread.
	const void* const bar = FindTileNamed(root, "book_scroll_bar");
	read.barRead = bar != nullptr && ReadTileNumber(bar, kTraitUser7, read.barValue);
	return read;
}

bool BookMenuScrollLook() {
	const void* const root = MenuRootTile(kMenuIdBook);
	float user0 = 0.0f;
	return root == nullptr || !ReadTileNumber(root, kTraitUser0, user0) || vr::BookMenuIsScroll(user0);
}

namespace {

void SayNumber(char (&out)[48], bool read, float value) {
	if (read) {
		std::snprintf(out, sizeof(out), "%.0f", static_cast<double>(value));
	}
}

}  // namespace

void LogBookMenuAtMark() {
	const BookMenuRead read = ReadBookMenu();
	if (!read.up) {
		return;
	}
	char page[48] = "unread";
	SayNumber(page, read.pageRead, read.leftPage);
	char bar[48] = "unread";
	SayNumber(bar, read.barRead, read.barValue);
	char rootScroll[48] = "unread";
	SayNumber(rootScroll, read.rootScrollRead, read.rootScroll);
	UInt32 stack[kMenuStackEntries];
	const UInt32 count = IsMenuMode() ? MenuStackEntries(stack, kMenuStackEntries) : 0;
	// The look's own measure first, so a scenario can expect or reject it at a mark.
	OBVR_LOG("Book: at the mark %s %s%s - the book menu loaded, %s, user0 %.0f, the left page %s (user9), the scroll bar "
	         "at %s (the root's user7 %s), Take %s",
	         test::HandScriptMarkName(),
	         read.scroll ? "the letter's scroll bar is at " : "the book's left page is ", read.scroll ? bar : page,
	         MenuStackHas(stack, count, kMenuIdBook) ? "in the menu stack" : "not in the menu stack",
	         static_cast<double>(read.user0), page, bar, rootScroll, read.takeable ? "shown" : "not shown");
}

}  // namespace obvr::game

// Checks the quick menu's pages of hotkeys (game/QuickKeyPages.h): which page
// comes next, what a turn keeps and writes, and the co-save record there and
// back, with every refusal.

#include <cstdio>
#include <cstring>

#include "game/QuickKeyPages.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestNext() {
	std::printf("The next page\n");
	Check(NextQuickKeyPage(0, 3) == 1 && NextQuickKeyPage(1, 3) == 2, "one on");
	Check(NextQuickKeyPage(2, 3) == 0, "after the last: the first");
	Check(NextQuickKeyPage(0, 1) == 0 && NextQuickKeyPage(0, 0) == 0, "one page or none: the first");
	Check(NextQuickKeyPage(4, 3) == 0, "past the count (lowered since): the first");
	Check(NextQuickKeyPage(-2, 3) == 0, "a page that cannot be: the first");
}

void TestTurn() {
	std::printf("A turn\n");
	QuickKeyPages pages;
	pages.forms[1][0] = 0x111;
	pages.forms[1][7] = 0x118;
	const UInt32 now[8] = {0xA1, 0, 0xA3, 0, 0, 0, 0, 0xA8};
	UInt32 write[8];
	TurnQuickKeyPages(pages, now, 1, write);
	Check(pages.current == 1, "the page turned to");
	Check(pages.forms[0][0] == 0xA1 && pages.forms[0][2] == 0xA3 && pages.forms[0][7] == 0xA8 &&
	          pages.forms[0][1] == 0,
	      "what the game held kept as the page turned from");
	Check(write[0] == 0x111 && write[7] == 0x118 && write[3] == 0, "the new page's forms to write, empty ones empty");
	const UInt32 later[8] = {0x111, 0x222, 0, 0, 0, 0, 0, 0x118};  // a hotkey set on page 2
	TurnQuickKeyPages(pages, later, 0, write);
	Check(pages.forms[1][1] == 0x222, "a hotkey set while a page was shown is kept with it");
	Check(write[0] == 0xA1 && write[2] == 0xA3, "and the first page comes back as it was");
	QuickKeyPages odd;
	odd.current = 9;
	TurnQuickKeyPages(odd, now, 7, write);
	Check(odd.current == 0 && write[0] == 0, "turned to a page that cannot be: the first; from one: nothing kept");
}

void TestRecord() {
	std::printf("The co-save record\n");
	QuickKeyPages pages;
	pages.current = 1;
	pages.forms[0][3] = 0x0300;
	pages.forms[1][3] = 0xDEAD;  // stale: the game's eight are the truth for the page shown
	pages.forms[4][7] = 0x4700;
	const UInt32 now[8] = {0, 0, 0, 0x1300, 0, 0, 0, 0};
	const QuickKeyPagesRecord record = RecordOfPages(pages, now);
	Check(record.current == 1 && record.forms[1][3] == 0x1300, "the page shown written from the game's eight");
	Check(record.forms[0][3] == 0x0300 && record.forms[4][7] == 0x4700, "the others as kept");

	QuickKeyPages back;
	auto same = [](UInt32 id, UInt32& out) {
		out = id;
		return true;
	};
	Check(PagesOfRecord(&record, sizeof(record), kQuickKeyPagesVersion, same, back), "read back");
	Check(back.current == 1 && back.forms[1][3] == 0x1300 && back.forms[0][3] == 0x0300 && back.forms[4][7] == 0x4700,
	      "every page as written");
	// A mod list that changed: ids moved; one mod gone.
	auto moved = [](UInt32 id, UInt32& out) {
		if (id == 0x4700) {
			return false;
		}
		out = id + 0x01000000;
		return true;
	};
	PagesOfRecord(&record, sizeof(record), kQuickKeyPagesVersion, moved, back);
	Check(back.forms[0][3] == 0x01000300, "ids passed through the co-save's resolving");
	Check(back.forms[4][7] == 0, "an id whose mod went: an empty slot");
	bool asked = false;
	auto spy = [&asked](UInt32, UInt32& out) {
		asked = true;
		out = 1;
		return true;
	};
	QuickKeyPagesRecord empty;
	PagesOfRecord(&empty, sizeof(empty), kQuickKeyPagesVersion, spy, back);
	Check(!asked && back.forms[0][0] == 0, "an empty slot is not resolved, stays empty");

	back.current = 3;
	back.forms[0][0] = 5;
	Check(!PagesOfRecord(&record, sizeof(record), 2, same, back) && back.current == 0 && back.forms[0][0] == 0,
	      "another version: refused, the pages empty");
	Check(!PagesOfRecord(&record, sizeof(record) - 4, kQuickKeyPagesVersion, same, back), "another size: refused");
	Check(!PagesOfRecord(nullptr, sizeof(record), kQuickKeyPagesVersion, same, back), "no data: refused");
	QuickKeyPagesRecord beyond = record;
	beyond.current = 17;
	PagesOfRecord(&beyond, sizeof(beyond), kQuickKeyPagesVersion, same, back);
	Check(back.current == 0, "a page shown that cannot be: the first");
	QuickKeyPages wrong;
	wrong.current = -1;
	Check(RecordOfPages(wrong, now).current == 0 && RecordOfPages(wrong, now).forms[0][3] == 0x1300,
	      "written with a page that cannot be: as the first");
}

}  // namespace

int main() {
	TestNext();
	TestTurn();
	TestRecord();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

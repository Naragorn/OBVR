// The spell ring's book (vr/SpellRing.h): which spells go in, their order,
// the pages and the slots.

#include <cstdio>
#include <cstring>

#include "vr/SpellRing.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestCastable() {
	std::printf("What the ring offers\n");
	Check(SpellIsCastable(0), "a spell");
	Check(!SpellIsCastable(1), "not a disease");
	Check(SpellIsCastable(2), "a greater power");
	Check(SpellIsCastable(3), "a lesser power");
	Check(!SpellIsCastable(4), "not an ability");
	Check(!SpellIsCastable(0xFFFFFFFF), "not a type never set");
	SpellBook book;
	Check(AddSpell(book, 0xA97DF, 0, "Flare", "magic\\fire.dds") && book.count == 1, "a spell goes in");
	Check(!AddSpell(book, 0x1234, 4, "Resist Disease", "") && book.count == 1, "an ability stays out");
	Check(!AddSpell(book, 0x1235, 1, "Ataxia", "") && book.count == 1, "a disease stays out");
	Check(!AddSpell(book, 0, 0, "No form", "") && book.count == 1, "no form, nothing");
	Check(std::strcmp(book.spells[0].name, "Flare") == 0 && std::strcmp(book.spells[0].iconPath, "magic\\fire.dds") == 0,
	      "the name and the icon kept");
	Check(AddSpell(book, 0x1236, 2, nullptr, nullptr) && book.spells[1].name[0] == '\0' &&
	          book.spells[1].iconPath[0] == '\0',
	      "a spell without a name or an icon still goes in, empty");
	SpellBook full;
	for (int i = 0; i < kSpellRingMaxSpells; ++i) {
		AddSpell(full, 0x1000 + static_cast<UInt32>(i), 0, "x", "");
	}
	Check(full.count == kSpellRingMaxSpells, "the book holds eighty");
	Check(!AddSpell(full, 0x2000, 0, "one more", "") && full.count == kSpellRingMaxSpells,
	      "the eighty-first is refused");
	char longName[200];
	std::memset(longName, 'n', sizeof(longName) - 1);
	longName[sizeof(longName) - 1] = '\0';
	SpellBook cut;
	AddSpell(cut, 1, 0, longName, longName);
	Check(std::strlen(cut.spells[0].name) == 39 && std::strlen(cut.spells[0].iconPath) == 127,
	      "a long name and a long path are cut to fit");
}

void TestOrder() {
	std::printf("The order of the names\n");
	SpellBook book;
	AddSpell(book, 3, 0, "Snowball", "");
	AddSpell(book, 1, 0, "flare", "");
	AddSpell(book, 4, 2, "Voice of the Emperor", "");
	AddSpell(book, 2, 0, "Heal Minor Wounds", "");
	AddSpell(book, 6, 0, "Flare", "");
	AddSpell(book, 5, 3, "Flare", "");
	SortSpellBook(book);
	Check(book.spells[0].form == 1 && book.spells[1].form == 5 && book.spells[2].form == 6,
	      "the three Flares first, the letters' case left out, the same name by form id");
	Check(book.spells[3].form == 2 && book.spells[4].form == 3 && book.spells[5].form == 4,
	      "then Heal, Snowball, Voice");
	SortSpellBook(book);
	Check(book.spells[0].form == 1 && book.spells[5].form == 4, "sorted again: the same");
	SpellBook empty;
	SortSpellBook(empty);
	Check(empty.count == 0, "an empty book sorts to nothing");
	SpellEntry a;
	SpellEntry b;
	std::strcpy(a.name, "Fire");
	std::strcpy(b.name, "Fireball");
	Check(SpellBefore(a, b) && !SpellBefore(b, a), "a name that is the start of another comes first");
}

void TestPages() {
	std::printf("The pages\n");
	Check(SpellRingPageCount(0) == 1, "no spells: one page");
	Check(SpellRingPageCount(1) == 1 && SpellRingPageCount(8) == 1, "up to eight: one page");
	Check(SpellRingPageCount(9) == 2 && SpellRingPageCount(16) == 2, "nine to sixteen: two");
	Check(SpellRingPageCount(80) == 10, "eighty: ten");
	Check(SpellRingPageCount(81) == 10 && SpellRingPageCount(1000) == 10, "never more than ten");
	Check(SpellRingPageClamp(0, 1) == 0 && SpellRingPageClamp(2, 3) == 2, "a page that exists stays");
	Check(SpellRingPageClamp(3, 3) == 0 && SpellRingPageClamp(-1, 3) == 0, "a page past the end is the first");
	Check(SpellRingNextPage(0, 3) == 1 && SpellRingNextPage(2, 3) == 0, "the next page, round to the first");
	Check(SpellRingNextPage(0, 1) == 0, "one page: the next is itself");
	Check(SpellRingIndex(0, 0, 3) == 0 && SpellRingIndex(0, 2, 3) == 2, "page 1: the first slots are the first spells");
	Check(SpellRingIndex(0, 3, 3) == -1, "a slot past the book's end is empty");
	Check(SpellRingIndex(1, 0, 12) == 8 && SpellRingIndex(1, 3, 12) == 11 && SpellRingIndex(1, 4, 12) == -1,
	      "page 2: the ninth spell on its first slot, the thirteenth slot empty");
	Check(SpellRingIndex(-1, 0, 12) == -1 && SpellRingIndex(0, 8, 12) == -1 && SpellRingIndex(0, -1, 12) == -1,
	      "no page or no slot: empty");
}

void TestActiveSlot() {
	std::printf("The active spell's slot\n");
	SpellBook book;
	for (int i = 0; i < 12; ++i) {
		char name[8];
		std::snprintf(name, sizeof(name), "s%02d", i);
		AddSpell(book, 0x100 + static_cast<UInt32>(i), 0, name, "");
	}
	Check(SpellRingSlotOf(book, 0, 0x102) == 2, "on the first page: its slot");
	Check(SpellRingSlotOf(book, 1, 0x102) == -1, "not on the second");
	Check(SpellRingSlotOf(book, 1, 0x10B) == 3, "the twelfth spell on the second page's fourth slot");
	Check(SpellRingSlotOf(book, 0, 0x999) == -1, "a spell not in the book: none");
	Check(SpellRingSlotOf(book, 0, 0) == -1, "no active spell: none");
}

}  // namespace

int main() {
	TestCastable();
	TestOrder();
	TestPages();
	TestActiveSlot();
	if (g_failures > 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("spell_ring_test: all passed\n");
	return 0;
}

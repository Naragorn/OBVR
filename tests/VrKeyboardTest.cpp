// Checks the SteamVR keyboard's decisions (vr/VrKeyboard.h): which menus and
// clicks open it, when it closes, and how characters become key taps.

#include <cstdio>

#include "vr/VrKeyboard.h"

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

void TestWhere() {
	std::printf("Where the game takes text\n");
	Check(TextMenuTakesKeyboard(kMenuIdTextEdit) && !TextMenuTakesKeyboard(kMenuIdAlchemy) &&
	          !TextMenuTakesKeyboard(0x40C),
	      "the text edit menu is the field; the race menu and the others are not");
	Check(NameTileTakesKeyboard(kMenuIdSpellmaking, "spell_name_text") &&
	          NameTileTakesKeyboard(kMenuIdSpellmaking, "spell_name_background") &&
	          NameTileTakesKeyboard(kMenuIdEnchantment, "ench_name_text") &&
	          NameTileTakesKeyboard(kMenuIdAlchemy, "name_text") && NameTileTakesKeyboard(kMenuIdAlchemy, "name_background"),
	      "the three naming menus' fields and their backgrounds");
	Check(!NameTileTakesKeyboard(kMenuIdSpellmaking, "name_text") && !NameTileTakesKeyboard(kMenuIdAlchemy, "spell_name_text") &&
	          !NameTileTakesKeyboard(kMenuIdAlchemy, nullptr) && !NameTileTakesKeyboard(kMenuIdRaceSex, "race_background"),
	      "another menu's field, no tile, or the race menu's background: not");
	Check(NameTileTakesKeyboard(kMenuIdRaceSex, "race_name"), "the race menu's name field: the character's name");
	Check(SameText(KeyboardPromptFor(kMenuIdAlchemy), "The potion's name") && SameText(KeyboardPromptFor(kMenuIdRaceSex), "Your name") && SameText(KeyboardPromptFor(0x400), "Type") &&
	          SameText(KeyboardPromptFor(kMenuIdTextEdit), "Type, then Done"),
	      "a line over the keyboard for each, a plain one otherwise");
}

void TestKeys() {
	std::printf("Characters as keys\n");
	UInt32 vk = 0;
	bool shift = true;
	Check(KeyStrokeFor('a', vk, shift) && vk == 'A' && !shift, "a lower-case letter: its key");
	Check(KeyStrokeFor('Q', vk, shift) && vk == 'Q' && shift, "a capital: the key with Shift");
	Check(KeyStrokeFor('7', vk, shift) && vk == '7' && !shift, "a digit");
	Check(KeyStrokeFor(' ', vk, shift) && vk == kVkSpace && !shift, "a space");
	Check(KeyStrokeFor('\b', vk, shift) && vk == kVkBack, "the runtime's backspace");
	Check(KeyStrokeFor('\n', vk, shift) && vk == kVkReturn, "a line end: Enter");
	Check(KeyStrokeFor('-', vk, shift) && vk == kVkOemMinus && !shift && KeyStrokeFor('_', vk, shift) && shift,
	      "the dash, and the underscore with Shift");
	Check(KeyStrokeFor('\'', vk, shift) && vk == kVkOemApostrophe && !shift && KeyStrokeFor('.', vk, shift) &&
	          vk == kVkOemPeriod && KeyStrokeFor(',', vk, shift) && vk == kVkOemComma,
	      "apostrophe, period, comma");
	Check(!KeyStrokeFor(static_cast<char>(0xE4), vk, shift) && !KeyStrokeFor('!', vk, shift) && !KeyStrokeFor('\t', vk, shift),
	      "an umlaut, a bang, a tab: not on the US keys");
}

void TestQueue() {
	std::printf("One key a frame\n");
	KeyTapQueue q;
	Check(!q.Step().any && q.Idle(), "empty: nothing to send");
	Check(QueueTypedText(q, "Ab", false, kMenuIdAlchemy) == 0 && q.count == 2, "two characters queued");
	KeyTapAction a = q.Step();
	Check(a.any && a.down && a.vk == 'A' && a.shift, "frame 1: A and Shift down");
	a = q.Step();
	Check(a.any && !a.down && a.vk == 'A' && a.shift, "frame 2: up again");
	a = q.Step();
	Check(a.any && a.down && a.vk == 'B' && !a.shift, "frame 3: b down, no Shift");
	a = q.Step();
	Check(a.any && !a.down, "frame 4: up");
	Check(!q.Step().any && q.Idle(), "then idle");
	Check(QueueTypedText(q, "x\xE4y", false, kMenuIdAlchemy) == 1 && q.count == 2, "an umlaut dropped and counted, the rest queued");
	q.Clear();
	Check(QueueTypedText(q, "", true, kMenuIdTextEdit) == 0 && q.count == 1 && q.Step().vk == kVkReturn,
	      "Done in the text edit menu: Enter for its OK");
	q.Clear();
	Check(QueueTypedText(q, "", true, kMenuIdSpellmaking) == 0 && q.count == 0, "Done in a naming menu: nothing");
	q.Clear();
	for (UInt32 i = 0; i < kKeyTapCapacity; ++i) {
		q.Push('A', false);
	}
	Check(!q.Push('B', false) && QueueTypedText(q, "c", false, kMenuIdAlchemy) == 1, "full: a tap dropped and counted");
	// The ring wraps: send a few, push a few more, all come out in order.
	q.Clear();
	for (UInt32 i = 0; i < kKeyTapCapacity - 1; ++i) {
		q.Push('A', false);
	}
	for (UInt32 i = 0; i < 4; ++i) {
		q.Step();
	}
	Check(q.Push('Z', false) && q.Push('Y', false), "room again after two taps went");
	while (q.count > 2) {
		q.Step();
	}
	Check(q.Step().vk == 'Z' && q.Step().vk == 'Z' && q.Step().vk == 'Y', "the ring keeps the order");
}

void TestNewInput() {
	std::printf("The key of a character event\n");
	Check(SameText(KeyboardNewInput(1, "a", "zz"), "a"), "the event's own character: taken, the runtime's text left");
	Check(SameText(KeyboardNewInput(1, "", "g"), "g"), "the event empty: the runtime's text is the key");
	Check(SameText(KeyboardNewInput(1, nullptr, "g"), "g"), "no event text at all: the same");
	Check(SameText(KeyboardNewInput(0, "", "g"), ""), "no character event: the runtime's text is stale, nothing");
	Check(SameText(KeyboardNewInput(0, "", nullptr), "") && SameText(KeyboardNewInput(1, "", nullptr), ""), "nothing anywhere: nothing");
	Check(SameText(KeyboardNewInput(1, "\x1b", "x"), "") && SameText(KeyboardNewInput(1, "", "\x1b[D"), ""), "the runtime's escape and an arrow key: not typed");
	Check(SameText(KeyboardNewInput(1, "\b", ""), "\b"), "a backspace: typed");
}

void TestSession() {
	std::printf("The session\n");
	KeyboardSession s;
	Check(StepKeyboardSession(s, true, kMenuIdAlchemy, nullptr, false) == KeyboardStep::Nothing && !s.open,
	      "the alchemy menu alone: no keyboard");
	Check(StepKeyboardSession(s, true, kMenuIdAlchemy, "ingredient_1", false) == KeyboardStep::Nothing && !s.open,
	      "a click elsewhere in it: no keyboard");
	Check(StepKeyboardSession(s, true, kMenuIdAlchemy, "name_text", false) == KeyboardStep::Open && s.open &&
	          s.menuId == kMenuIdAlchemy,
	      "a click on its name field: opens");
	Check(StepKeyboardSession(s, true, kMenuIdAlchemy, "name_text", false) == KeyboardStep::Nothing && s.open,
	      "another click while up: nothing");
	Check(StepKeyboardSession(s, true, kMenuIdAlchemy, nullptr, true) == KeyboardStep::Nothing && !s.open,
	      "the runtime closed it: noted, nothing to close");
	Check(StepKeyboardSession(s, true, kMenuIdTextEdit, nullptr, false) == KeyboardStep::Open && s.open &&
	          s.menuId == kMenuIdTextEdit,
	      "the text edit menu: opens by itself");
	Check(StepKeyboardSession(s, true, 0, nullptr, false) == KeyboardStep::Close && !s.open,
	      "its menu gone: closed by OBVR");
	Check(StepKeyboardSession(s, false, kMenuIdTextEdit, nullptr, false) == KeyboardStep::Nothing && !s.open,
	      "switched off: never opens");
	StepKeyboardSession(s, true, kMenuIdTextEdit, nullptr, false);
	Check(StepKeyboardSession(s, false, kMenuIdTextEdit, nullptr, false) == KeyboardStep::Close && !s.open,
	      "switched off while up: closed");
	StepKeyboardSession(s, true, kMenuIdSpellmaking, "spell_name_text", false);
	Check(StepKeyboardSession(s, true, kMenuIdEnchantment, nullptr, false) == KeyboardStep::Close && !s.open,
	      "another menu on top: closed");
}

}  // namespace

int main() {
	TestWhere();
	TestKeys();
	TestQueue();
	TestNewInput();
	TestSession();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

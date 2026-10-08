#pragma once

// Typing in the headset (the tester, 2026-10-07: "für die wenigen stellen im
// spiel für die man ein keyboard zum text eingeben braucht ... den standard
// best practice weg ... ein keyboard zu rendern mit dem man mit dem
// laserpointer interagieren kann und text in textfelder einfügen kann").
//
// The keyboard is SteamVR's own (IVROverlay ShowKeyboard): the runtime
// draws it in the headset, the controllers' lasers type on it, and each
// character comes back as an event (OpenVRBackend::PollKeyboard). OBVR's
// part is to know WHEN a text field is being typed into, and to turn the
// characters into the key presses the game reads - Oblivion takes its text
// from the keyboard's scan codes by the US layout (docs/hand-script-harness.md:
// "plazer"), so each character is the US key that carries it, with Shift
// for the capitals.
//
// Where the game takes text (read from its menu XMLs, Oblivion - Misc.bsa):
// - TextEditMenu (0x41B): the character's name at the end of the tutorial,
//   a custom class's name, and whatever else asks by CreateTextEditMenu -
//   its "textedit_text" is the field, OK and Back its buttons. The
//   keyboard opens with the menu, and Done presses Enter for its OK.
// - SpellmakingMenu (0x411) "spell_name_text", EnchantmentMenu (0x412)
//   "ench_name_text", AlchemyMenu (0x410) "name_text": a name field each,
//   typed into once clicked. The keyboard opens when the laser's click
//   lands on the field (or its background rect), and Done only closes it.
// - RaceSexMenu (0x40C) "race_name": the character's name at the top of the
//   race menu (where the tester's clicks landed, 2026-10-07).
//
// Pure; vr_keyboard_test covers it.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr UInt32 kMenuIdTextEdit = 0x41B;
inline constexpr UInt32 kMenuIdAlchemy = 0x410;
inline constexpr UInt32 kMenuIdSpellmaking = 0x411;
inline constexpr UInt32 kMenuIdEnchantment = 0x412;
inline constexpr UInt32 kMenuIdRaceSex = 0x40C;

// The menu whose text field is the whole menu: the keyboard opens as it
// does.
inline bool TextMenuTakesKeyboard(UInt32 menuId) { return menuId == kMenuIdTextEdit; }

inline bool SameText(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

// A click on a name field in one of the naming menus: the keyboard opens.
inline bool NameTileTakesKeyboard(UInt32 menuId, const char* tileName) {
	switch (menuId) {
	case kMenuIdSpellmaking:
		return SameText(tileName, "spell_name_text") || SameText(tileName, "spell_name_background");
	case kMenuIdEnchantment:
		return SameText(tileName, "ench_name_text") || SameText(tileName, "ench_name_background");
	case kMenuIdAlchemy:
		return SameText(tileName, "name_text") || SameText(tileName, "name_background");
	case kMenuIdRaceSex:
		// The character's name at the top of the race menu (the tester's clicks
		// on it landed on "race_name", 2026-10-07); "race_background" is the
		// whole menu's and opens nothing.
		return SameText(tileName, "race_name");
	default:
		return false;
	}
}

// Whether the keyboard that is up still has its menu.
inline bool KeyboardMenuStillUp(UInt32 openedFor, UInt32 topMenuId) { return openedFor == topMenuId; }

// The key a character is typed with: the virtual key (game::UsScanCode
// knows its US scan code) and whether Shift is held. False for a character
// the US keys do not carry - left out of the name, said in the log.
inline constexpr UInt32 kVkBack = 0x08;
inline constexpr UInt32 kVkReturn = 0x0D;
inline constexpr UInt32 kVkShift = 0x10;
inline constexpr UInt32 kVkSpace = 0x20;
inline constexpr UInt32 kVkOemMinus = 0xBD;
inline constexpr UInt32 kVkOemComma = 0xBC;
inline constexpr UInt32 kVkOemPeriod = 0xBE;
inline constexpr UInt32 kVkOemApostrophe = 0xDE;

inline bool KeyStrokeFor(char c, UInt32& vk, bool& shift) {
	shift = false;
	if (c >= 'a' && c <= 'z') {
		vk = static_cast<UInt32>('A' + (c - 'a'));
		return true;
	}
	if (c >= 'A' && c <= 'Z') {
		vk = static_cast<UInt32>(c);
		shift = true;
		return true;
	}
	if (c >= '0' && c <= '9') {
		vk = static_cast<UInt32>(c);
		return true;
	}
	switch (c) {
	case ' ': vk = kVkSpace; return true;
	case '\b': vk = kVkBack; return true;
	case '\n':
	case '\r': vk = kVkReturn; return true;
	case '-': vk = kVkOemMinus; return true;
	case '_': vk = kVkOemMinus; shift = true; return true;
	case ',': vk = kVkOemComma; return true;
	case '.': vk = kVkOemPeriod; return true;
	case '\'': vk = kVkOemApostrophe; return true;
	case '"': vk = kVkOemApostrophe; shift = true; return true;
	default: return false;
	}
}

// The key presses, one key a frame: the game polls the keyboard's state
// once a frame, so a press and its release in the same frame would never
// be seen. Each tap is a frame down (Shift first, when the character
// wants it) and a frame up. Full, a tap is dropped and the caller told.
inline constexpr UInt32 kKeyTapCapacity = 128;

struct KeyTap {
	UInt32 vk = 0;
	bool shift = false;
};

// What the caller sends this frame.
struct KeyTapAction {
	bool any = false;
	bool down = false;  // the key (and Shift) go down; else up
	UInt32 vk = 0;
	bool shift = false;
};

struct KeyTapQueue {
	KeyTap taps[kKeyTapCapacity];
	UInt32 head = 0;   // the next tap to send
	UInt32 count = 0;  // taps queued, the one being sent included
	bool pressed = false;  // the tap at head is down, its release due

	bool Push(UInt32 vk, bool shift) {
		if (count >= kKeyTapCapacity) {
			return false;
		}
		taps[(head + count) % kKeyTapCapacity] = KeyTap{vk, shift};
		++count;
		return true;
	}

	KeyTapAction Step() {
		KeyTapAction a;
		if (count == 0) {
			return a;
		}
		const KeyTap& tap = taps[head];
		a.any = true;
		a.vk = tap.vk;
		a.shift = tap.shift;
		if (!pressed) {
			a.down = true;
			pressed = true;
			return a;
		}
		a.down = false;
		pressed = false;
		head = (head + 1) % kKeyTapCapacity;
		--count;
		return a;
	}

	bool Idle() const { return count == 0; }
	void Clear() {
		head = 0;
		count = 0;
		pressed = false;
	}
};

inline UInt32 QueueTypedText(KeyTapQueue& q, const char* chars, bool done, UInt32 menuId);

// The keyboard's session: which menu it is up for, and what to do with
// what it sends.
struct KeyboardSession {
	bool open = false;
	UInt32 menuId = 0;
};

// The keyboard is opened in its minimal mode (KeyboardFlag_Minimal, 1 << 0:
// "makes the keyboard send key events immediately instead of accumulating
// a buffer" - openvr.h, SDK 1.10.30): no text box of its own, each key a
// character event, the game's own field the only place the text shows.
// Valve's keyboard sample takes the key from the event's cNewInput in that
// mode. The tester's runtime sent its character events with cNewInput
// EMPTY and the key in GetKeyboardText instead - one key at a time, the
// same one four times for four presses, "g" alone at Done after a dozen
// keys (OBVR.log, 2026-10-08), so that read is the latest input and no
// accumulated text. The input to type this frame is the event's own when
// it has one, else the buffer when a character event came, else nothing.
// The runtime's escape (a lone "\x1b", or "\x1b[" and a letter for an
// arrow key) is not typed. Done is not an input: its buffer is the last
// key over again.
inline constexpr UInt32 kKeyboardTextCapacity = 65;

inline const char* KeyboardNewInput(UInt32 charEvents, const char* eventChars, const char* buffer) {
	const char* input = eventChars != nullptr && eventChars[0] != '\0' ? eventChars
	                    : charEvents > 0 && buffer != nullptr        ? buffer
	                                                                 : "";
	return input[0] == '\x1b' ? "" : input;
}

// What this frame asks of the keyboard, decided from the menus and the
// click (the engine side is the caller's: OpenVRBackend, the tile under the
// cursor, the key sender).
enum class KeyboardStep : UInt8 { Nothing, Open, Close };

// `clickedTile`: the tile under the cursor as the laser's click went down
// this frame, else null. `enabled`: [Hands] VrKeyboard. `keyboardGone`:
// the runtime said closed or done.
inline KeyboardStep StepKeyboardSession(KeyboardSession& s, bool enabled, UInt32 topMenuId,
                                        const char* clickedTile, bool keyboardGone) {
	if (s.open) {
		if (!enabled || keyboardGone || !KeyboardMenuStillUp(s.menuId, topMenuId)) {
			s.open = false;
			s.menuId = 0;
			return keyboardGone ? KeyboardStep::Nothing : KeyboardStep::Close;
		}
		return KeyboardStep::Nothing;
	}
	if (!enabled) {
		return KeyboardStep::Nothing;
	}
	if (TextMenuTakesKeyboard(topMenuId) || (clickedTile != nullptr && NameTileTakesKeyboard(topMenuId, clickedTile))) {
		s.open = true;
		s.menuId = topMenuId;
		return KeyboardStep::Open;
	}
	return KeyboardStep::Nothing;
}

// The line over the keyboard, by the menu it is up for.
inline const char* KeyboardPromptFor(UInt32 menuId) {
	switch (menuId) {
	case kMenuIdTextEdit: return "Type, then Done";
	case kMenuIdSpellmaking: return "The spell's name";
	case kMenuIdEnchantment: return "The item's name";
	case kMenuIdAlchemy: return "The potion's name";
	case kMenuIdRaceSex: return "Your name";
	default: return "Type";
	}
}

// The characters the runtime sent, into key taps. `done` with the keyboard
// up for TextEditMenu presses Enter for its OK. Answers how many characters
// could not be typed (not on the US keys, or the queue full).
inline UInt32 QueueTypedText(KeyTapQueue& q, const char* chars, bool done, UInt32 menuId) {
	UInt32 dropped = 0;
	for (const char* c = chars; c != nullptr && *c != '\0'; ++c) {
		UInt32 vk = 0;
		bool shift = false;
		if (!KeyStrokeFor(*c, vk, shift) || !q.Push(vk, shift)) {
			++dropped;
		}
	}
	if (done && TextMenuTakesKeyboard(menuId)) {
		if (!q.Push(kVkReturn, false)) {
			++dropped;
		}
	}
	return dropped;
}

}  // namespace obvr::vr

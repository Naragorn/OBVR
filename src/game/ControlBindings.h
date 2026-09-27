#pragma once

#include "core/Types.h"

namespace obvr::game {

// The game's own control bindings, as Oblivion.ini's [Controls] keeps them:
// eight hex digits, "00" then the keyboard scan code, the mouse button and the
// joystick button, FF where there is none. Read from the tester's file
// (2026-09-27): "Block=003801FF" is Left Alt (0x38) and the right mouse button
// (01); "Use=00FF00FF" is no key and the left mouse button (00); "Run=002AFFFF"
// is Left Shift (0x2A).
struct ControlBinding {
	bool valid = false;
	UInt8 key = 0xFF;
	UInt8 mouse = 0xFF;
	UInt8 joystick = 0xFF;
};

inline int HexDigit(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

inline ControlBinding ParseControlBinding(const char* text) {
	ControlBinding b;
	if (text == nullptr) {
		return b;
	}
	int digits[8];
	for (int i = 0; i < 8; ++i) {
		digits[i] = HexDigit(text[i]);
		if (digits[i] < 0) {
			return b;  // too short, or not hex
		}
	}
	b.key = static_cast<UInt8>(digits[2] * 16 + digits[3]);
	b.mouse = static_cast<UInt8>(digits[4] * 16 + digits[5]);
	b.joystick = static_cast<UInt8>(digits[6] * 16 + digits[7]);
	b.valid = true;
	return b;
}

// The inventory's drop is vanilla's Shift and a left click (help.bethesda.net
// answer 9938). OBVR presses its RunKey for the Shift. It works when the
// game's Run is on the key OBVR presses and that key is Left Shift.
enum class DropBindingVerdict : UInt8 {
	Fine,
	Unknown,         // Oblivion.ini or its Run line could not be read
	RunNotOnShift,   // the game's Run is not Left Shift
	RunKeyDiffers,   // OBVR's RunKey is not the key the game's Run is on
};

constexpr UInt8 kScanLeftShift = 0x2A;

inline DropBindingVerdict JudgeDropBinding(const ControlBinding& gameRun, UInt8 runKeyScan) {
	if (!gameRun.valid) {
		return DropBindingVerdict::Unknown;
	}
	if (gameRun.key != kScanLeftShift) {
		return DropBindingVerdict::RunNotOnShift;
	}
	if (runKeyScan != gameRun.key) {
		return DropBindingVerdict::RunKeyDiffers;
	}
	return DropBindingVerdict::Fine;
}

// Reads [Controls] Run from the user's Oblivion.ini (My Games, or the game's
// folder when the game keeps it there) and logs once when the drop cannot be
// expected to work. `runKey` is OBVR's RunKey, a virtual-key code.
void CheckDropBinding(UInt32 runKey);

}  // namespace obvr::game

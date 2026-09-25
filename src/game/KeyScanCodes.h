#pragma once

#include "core/Types.h"

namespace obvr::game {

// The scan code a virtual key has on a US keyboard, or 0 for a key not in
// the table.
//
// The game reads the keyboard through DirectInput, which sees scan codes -
// the physical key - and binds its controls to them: Oblivion.ini [Controls]
// has "Grab=002CFFFF", 0x2C being the key a US layout calls Z. The hand keys
// are virtual-key codes, and translating them through the active layout
// sends a different physical key wherever the layout moves the letter: on a
// German layout VK 'Z' is scan 0x15, the key the game knows as Y, so the
// grab never arrived (2026-09-25; MapVirtualKeyA(0x5A) answered 0x15 with
// layout 00000407 loaded). The game's defaults are named for a US keyboard,
// so the US scan code is the one it listens for, whatever the layout says.
//
// The values are DirectInput's DIK_ codes, which are the set 1 scan codes;
// the game's own [Controls] block agrees for every key it binds by default
// (W 0x11, S 0x1F, A 0x1E, D 0x20, Space 0x39, C 0x2E, F 0x21, Ctrl 0x1D,
// Shift 0x2A, E 0x12, R 0x13, Tab 0x0F, F1 0x3B, Z 0x2C, 1 0x02).
inline UInt32 UsScanCode(UInt32 virtualKey) {
	// Letters A to Z, in alphabetical order.
	static const UInt8 kLetters[26] = {
		0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
		0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,
	};
	if (virtualKey >= 'A' && virtualKey <= 'Z') {
		return kLetters[virtualKey - 'A'];
	}
	if (virtualKey >= '1' && virtualKey <= '9') {
		return 0x02 + (virtualKey - '1');
	}
	if (virtualKey == '0') {
		return 0x0B;
	}
	if (virtualKey >= 0x70 && virtualKey <= 0x79) {  // F1 to F10
		return 0x3B + (virtualKey - 0x70);
	}
	switch (virtualKey) {
	case 0x7A: return 0x57;  // F11
	case 0x7B: return 0x58;  // F12
	case 0x08: return 0x0E;  // Backspace
	case 0x09: return 0x0F;  // Tab
	case 0x0D: return 0x1C;  // Enter
	case 0x10: return 0x2A;  // Shift - the left one, as the game binds Run
	case 0xA0: return 0x2A;  // left Shift
	case 0xA1: return 0x36;  // right Shift
	case 0x11: return 0x1D;  // Ctrl - the left one, as the game binds sneak
	case 0xA2: return 0x1D;  // left Ctrl
	case 0x12: return 0x38;  // Alt - the left one
	case 0xA4: return 0x38;  // left Alt
	case 0x14: return 0x3A;  // Caps Lock
	case 0x1B: return 0x01;  // Esc
	case 0x20: return 0x39;  // Space
	default: return 0;
	}
}

}  // namespace obvr::game

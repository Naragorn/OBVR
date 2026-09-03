#pragma once

#include "core/Types.h"
#include "vr/HandInput.h"

namespace obvr::game {

// The keys the hand-tracked mode presses on the player's behalf, as
// virtual-key codes. Defaults are vanilla's bindings (UESP, Oblivion:Controls;
// Bethesda's own control list): attack and block on the mouse buttons, C to
// cast, Space to activate, Z to grab, E to jump, Ctrl to sneak, F to ready
// the weapon, Tab for the menus, Esc, F1 for the quick menu, WASD to move.
// A player who rebound a control sets the same code here; OBVR does not read
// the engine's control map.
struct HandKeyMap {
	UInt32 attack = 0x01;      // VK_LBUTTON
	UInt32 block = 0x02;       // VK_RBUTTON
	UInt32 cast = 0x43;        // C
	UInt32 activate = 0x20;    // Space
	UInt32 grab = 0x5A;        // Z
	UInt32 jump = 0x45;        // E
	UInt32 sneak = 0x11;       // Ctrl
	UInt32 readyWeapon = 0x46; // F
	UInt32 menu = 0x09;        // Tab
	UInt32 escape = 0x1B;      // Esc
	UInt32 quickMenu = 0x70;   // F1
	UInt32 forward = 0x57;     // W
	UInt32 back = 0x53;        // S
	UInt32 left = 0x41;        // A
	UInt32 right = 0x44;       // D
};

// Presses and releases the game's controls to match what the hands want,
// sending only the edges: a key that is already down is left down. Mouse
// buttons go through mouse_event, keys through keybd_event with the scan
// code the layout maps them to - the form DirectInput sees.
//
// The turn is a relative mouse movement, scaled by turnSpeed pixels per
// frame at full deflection. A menu click is the left mouse button, which is
// what the game's cursor clicks with.
void ApplyHandControls(const vr::HandControlsWanted& wanted, const HandKeyMap& keys,
                       float turnSpeed);

// Releases everything the mode is holding. For switching the mode off, for a
// lost controller, and for leaving the game window - a key held by nobody is
// the worst thing an input mode can leave behind.
void ReleaseHandControls(const HandKeyMap& keys);

// A relative mouse movement, for the laser cursor walking the game's cursor.
void MoveMouseBy(int dx, int dy);

}  // namespace obvr::game

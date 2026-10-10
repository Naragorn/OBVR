#pragma once

#include "core/Types.h"

namespace obvr::game {

// The laser places the game's menu cursor instead of steering it
// (docs/main-menu-laser-analysis.md, 1; the tester, 2026-10-10: the main
// menu's buttons "bisher nicht sehr robust").
//
// The engine decides the hover highlight, the active tile and with them
// what a click lands on in its tile search (addr::kFindTileAtCursor,
// thiscall on the InterfaceManager with one dword argument, called once
// per interface update at 0x00582406), which reads the cursor as two floats
// of the manager, +0x2C and +0x34 (addr::kInterfaceCursorXOffset and
// kInterfaceCursorYOffset), in the pixels of the believed screen size.
// Until now OBVR walked those floats by relative mouse steps the engine
// integrated itself - a step cap, a gain, the window's focus and the
// engine's own mouse scale all between the beam and the cursor. Now the
// search's entry is detoured: with a laser on a menu this frame, the two
// floats are set to the laser's own pixel right before the search reads
// them, every frame, and the engine's own integration of the mouse never
// matters while the laser points. Without a laser on a menu the search
// runs untouched and the mouse is the cursor as in vanilla.

// Detours the search's entry; logs its outcome. False when the bytes are
// not the ones read (another mod there) - then the laser keeps steering.
bool InstallMenuCursorHook();
bool MenuCursorHookInstalled();

// The pixel the laser wants the cursor at this frame (the believed-size
// space game::InterfaceCursorPosition reads in), or none.
void SetMenuCursorWanted(bool wanted, float x, float y);

// The engine's own cursor, where the engine itself last put it - not the
// pixel written for the search. A drag (a slider, a scroll bar's marker, the
// map) moves with the engine's own cursor, which only the mouse steps walk
// (the tester, 2026-10-10: "slider kann man nicht ziehen. sidebar scrolls
// auch nicht. die map im Tab menü auch nicht"), so the steps are reckoned
// from here.
//
// The engine writes the two floats only when its own cursor moves (the
// cursor update 0x0057E7C0 sets them, and the moved flag +0xB9, only when the
// sprite's position changed), so on a frame without a move they still hold
// the pixel written last. Taken in the search's entry, before the write:
// what is there is the engine's own unless it is exactly the pixel written
// last (CursorReadIsEngines). False until the engine's own has been seen.
bool EngineMenuCursor(float& x, float& y);

// Pure (menu_cursor_test): whether the two floats read in the search's entry
// are the engine's own - nothing written yet, or not the pixel written last -
// and a place on a screen at all (game::InterfaceCursorPosition's bounds; a
// NaN fails every comparison).
inline bool CursorReadIsEngines(bool wroteBefore, float lastX, float lastY, float readX, float readY) {
	const bool onAScreen = readX >= -64.0f && readX <= 16384.0f && readY >= -64.0f && readY <= 16384.0f;
	return onAScreen && (!wroteBefore || readX != lastX || readY != lastY);
}

// ------------------------------------------------------------ The knob snap
//
// A slider is dragged by its knob alone (read 2026-10-10: the options menus'
// drag handler, slot +0x20 - GameplayMenu's 0x005A3460 - acts only for the
// pressed tile whose id, trait 0xFA8, is the knob's 2; the knob is the
// "horizontal_scroll_marker", 39 layout units wide). Pressed anywhere else
// on the slider - its track "gameplay_difficulty_slider_marker" (id -1), the
// bar's "horizontal_scroll_leftside/rightside" - the drag moves nothing.
// The tester's pulls all landed on the track, 40 pixels under a knob a
// harness pull on it moved (2026-10-10: "slide gehen nicht"). So a press
// that starts on a slider's or scroll bar's part other than the knob looks
// for the knob around the beam - the engine's own tile search tried at
// rings of points round it - and takes it when one is that near.

// Ends with "scroll_marker": a slider's or scroll bar's knob.
inline bool IsScrollKnobName(const char* name) {
	if (name == nullptr) {
		return false;
	}
	const char* const tail = "scroll_marker";
	UInt32 length = 0;
	while (name[length] != '\0') {
		++length;
	}
	constexpr UInt32 kTail = 13;
	if (length < kTail) {
		return false;
	}
	for (UInt32 i = 0; i < kTail; ++i) {
		const char c = name[length - kTail + i];
		if ((c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c) != tail[i]) {
			return false;
		}
	}
	return true;
}

inline bool NameContains(const char* name, const char* part) {
	if (name == nullptr) {
		return false;
	}
	for (const char* at = name; *at != '\0'; ++at) {
		const char* a = at;
		const char* b = part;
		while (*b != '\0' && *a != '\0' && ((*a >= 'A' && *a <= 'Z') ? *a + ('a' - 'A') : *a) == *b) {
			++a;
			++b;
		}
		if (*b == '\0') {
			return true;
		}
	}
	return false;
}

// A slider's or scroll bar's part that is not its knob: where a press looks
// for the knob.
inline bool IsScrollPartName(const char* name) {
	return (NameContains(name, "scroll") || NameContains(name, "slider")) && !IsScrollKnobName(name);
}

// The points tried round the beam: rings `step` pixels apart, eight points
// each - straight up and down first (the tester's misses were under the
// knob), then sideways, then the diagonals. False past the last.
inline constexpr UInt32 kKnobProbeRings = 4;
inline constexpr UInt32 kKnobProbePoints = kKnobProbeRings * 8;
inline constexpr float kKnobProbeShare = 0.01f;  // of the layer's height, a ring's step

inline bool KnobProbeOffset(UInt32 i, float step, float& dx, float& dy) {
	if (i >= kKnobProbePoints || !(step > 0.0f)) {
		return false;
	}
	static constexpr float kDirections[8][2] = {{0.0f, -1.0f},     {0.0f, 1.0f},     {-1.0f, 0.0f},
	                                            {1.0f, 0.0f},      {-0.7071f, -0.7071f}, {0.7071f, -0.7071f},
	                                            {-0.7071f, 0.7071f}, {0.7071f, 0.7071f}};
	const float radius = step * static_cast<float>(i / 8 + 1);
	dx = kDirections[i % 8][0] * radius;
	dy = kDirections[i % 8][1] * radius;
	return true;
}

// The press's knob search: armed while a laser press runs on a menu (with the
// probe ring's step in cursor pixels), decided once at its start inside the
// search's entry. MenuCursorKnobOffset gives what it found, for the rest of
// the press; nothing while unarmed.
void SetMenuCursorKnobSnap(bool armed, float stepPixels);
bool MenuCursorKnobOffset(float& dx, float& dy);

}  // namespace obvr::game

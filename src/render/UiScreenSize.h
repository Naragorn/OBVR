#pragma once

#include "core/Types.h"

namespace obvr::render {

// The one screen size the whole 2D believes in, found by disassembly, and the
// cut that closes the eye-sized frame's last split.
//
// The trail, because every step of it was paid for. The layout probe showed
// the 2D split against itself on an eye-sized frame - films and the main
// menu's background at the game's own 2560x1440, menus and freshly built text
// spread over the real 4028x3380, the mouse lost between the two, nothing
// clickable. Rewriting the in-memory iSize settings closed the split and
// poisoned the user's Oblivion.ini, because the engine persists those; that
// route is banned. The NiDX9Renderer's own width/height pair was read next
// and measured already holding the believed size - not the lever either.
// So the binary was disassembled at the one place UESP describes: the UI
// normalization to a height of 960. It reads its screen size from a pair of
// integers that Oblivion fills ONCE, while creating its window, by copying
// the iSize setting values - a working copy at 0x00B06C4C/0x00B06C50 (see
// GameAddresses.h for the instruction-level evidence). Some ninety reads all
// over the interface, input and window code; exactly one write; and the INI
// is saved from the settings themselves, never from this copy. A value the
// engine does not persist, which is what the retreat asked for.
//
// The cut: after the window and display decisions are made with the game's
// own numbers - by CreateDevice time they are - the copy is raised to the
// frame's real size. Every menu built afterwards, the 2D projection, and the
// cursor checks that read the same copy then live in one coordinate space
// again, the frame's own, isotropically. The settings keep the INI's
// numbers, so nothing reaches disk.

// The decision alone, pure so every flow is testable. asked is what the game
// requested (and what the copy must still read, having been copied from the
// same settings); created is the frame the device was given.
enum class UiSizeLockAction {
	// The switch is off; nothing is read or written.
	NotWanted,
	// Asked and created agree, so there is no split to close.
	NothingToDo,
	// The copy does not read as the asked-for size, so it is not the copy
	// this was built against - nothing is written.
	WrongValues,
	// The copy reads exactly the asked-for size: raise it to the frame.
	Lock,
};

UiSizeLockAction DecideUiSizeLock(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                                  UInt32 createdWidth, UInt32 createdHeight,
                                  UInt32 readWidth, UInt32 readHeight);

// Reads the copy, logs what it found, applies the decision, and says whether
// the UI now follows the frame. Called from the CreateDevice hook, where the
// window is already built (the copy is filled during that) and no menu, film
// or cursor mapping exists yet.
bool UiScreenSizeFollowsFrame(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                              UInt32 createdWidth, UInt32 createdHeight);

// The way back, for the path where CreateDevice refuses OBVR's parameters and
// the game gets its own frame after all: a copy raised to a frame that never
// came to be must be lowered again. Validated the same way - written only
// when it still reads what the raise left in it.
bool WriteUiScreenSize(UInt32 expectedWidth, UInt32 expectedHeight, UInt32 newWidth,
                       UInt32 newHeight);

}  // namespace obvr::render

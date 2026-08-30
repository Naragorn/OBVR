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

// The size the 2D is given to believe. The frame itself closes the split but
// hands the UI a nearly square screen, and the first run in the headset said
// what that looks like: everything 4:3, the main menu too narrow. A 16:9
// window into the frame keeps the cure and returns the shape - the UI lays
// out at the cinema aspect, draws isotropically into the top-left slice of
// the frame, and the mouse maps against the very same numbers, which is what
// made clicking work. menuAspect below 0.1 means the whole frame.
struct UiSize {
	UInt32 width;
	UInt32 height;
};

UiSize UiSizeForFrame(UInt32 frameWidth, UInt32 frameHeight, float menuAspect);

// The decision alone, pure so every flow is testable. asked is what the game
// requested (and what the copy must still read, having been copied from the
// same settings); the new size is what the 2D is to believe instead.
enum class UiSizeLockAction {
	// The switch is off; nothing is read or written.
	NotWanted,
	// Asked and new agree, so there is no split to close.
	NothingToDo,
	// The copy does not read as the asked-for size, so it is not the copy
	// this was built against - nothing is written.
	WrongValues,
	// The copy reads exactly the asked-for size: raise it.
	Lock,
};

UiSizeLockAction DecideUiSizeLock(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                                  UInt32 newWidth, UInt32 newHeight, UInt32 readWidth,
                                  UInt32 readHeight);

// Reads the copy, logs what it found, applies the decision, and says whether
// the UI now believes the new size. Called from the CreateDevice hook, where
// the window is already built (the copy is filled during that) and no menu,
// film or cursor mapping exists yet.
bool UiScreenSizeFollowsFrame(bool enabled, UInt32 askedWidth, UInt32 askedHeight,
                              UInt32 newWidth, UInt32 newHeight);

// The way back, for the path where CreateDevice refuses OBVR's parameters and
// the game gets its own frame after all: a copy raised to a frame that never
// came to be must be lowered again. Validated the same way - written only
// when it still reads what the raise left in it.
bool WriteUiScreenSize(UInt32 expectedWidth, UInt32 expectedHeight, UInt32 newWidth,
                       UInt32 newHeight);

// The drawing half of the same split, decided per SetViewport call.
//
// The copy governs the layout and the mouse, but not the pixels: the 2D
// draws untransformed vertices through an orthographic projection - built
// from the copy, unit-free - and the viewport is what turns those into
// pixels. The engine sets that viewport itself, per pass, to the real
// render target's full size out of its own bookkeeping, which no copy raise
// reaches. The cinema run measured the consequence exactly: the Esc menu
// drawn at the layout rectangle times frameHeight/believedHeight - buttons
// below their own hit tests, the bottom past the overlay's slice.
//
// So while the interface pass runs, a viewport of exactly the frame's full
// size is shrunk to the believed rectangle, and layout, mouse and pixels
// agree again. Any other viewport is the pass's own business - and when the
// belief IS the frame, there is nothing to shrink.
enum class InterfaceViewportAction {
	// Not the frame-sized viewport, or nothing believed differently.
	LeaveAlone,
	// The full-frame viewport during an interface pass: make it believed.
	Shrink,
};

InterfaceViewportAction DecideInterfaceViewport(UInt32 viewportX, UInt32 viewportY,
                                                UInt32 viewportWidth, UInt32 viewportHeight,
                                                UInt32 frameWidth, UInt32 frameHeight,
                                                UInt32 believedWidth, UInt32 believedHeight);

}  // namespace obvr::render

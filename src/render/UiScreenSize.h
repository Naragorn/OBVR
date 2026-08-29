#pragma once

#include "core/Types.h"

namespace obvr::render {

// The renderer's own idea of the screen, and the experiment of correcting it.
//
// The layout probe left one split standing: on an eye-sized frame the in-game
// menus lay out against the real buffer size while the mouse maps against the
// size the game asked for, so the cursor and the hit test never mean the same
// place and nothing clicks. The INI-settings route to closing that split is
// off the table - the engine persists those to disk, and the run that proved
// it also proved what that costs. This is the next candidate: NiDX9Renderer
// keeps a width and height of its own (+0xA58/+0xA5C, see GameAddresses.h),
// a value that lives and dies with the process.
//
// The hypothesis under test: the menu system reads this pair when it builds a
// menu, and setting it to the size the game believes in - after the renderer
// is initialized, before the first menu is built - makes every menu lay out
// against the same size the mouse maps against, which is the state the game
// ships in. Present is the moment: by the first presented frame the renderer
// is built and filled, and no menu exists yet.

// The decision alone, pure so every flow is testable.
enum class UiSizeLockAction {
	// The switch is off; nothing is read or written.
	NotWanted,
	// Belief and frame agree, so there is no split to close.
	NothingToDo,
	// The renderer's pair does not read as the frame's size, so the offset
	// does not mean what it is believed to mean - nothing is written.
	WrongValues,
	// The pair reads exactly the created frame size: rewrite it to the
	// believed size.
	Lock,
};

UiSizeLockAction DecideUiSizeLock(bool enabled, UInt32 believedWidth, UInt32 believedHeight,
                                  UInt32 createdWidth, UInt32 createdHeight,
                                  UInt32 readWidth, UInt32 readHeight);

// The acting half: reads the renderer's pair, logs what it found, and applies
// the decision - once. Safe to call every frame; it retries while the
// renderer does not exist yet and retires itself after the first real
// attempt. believed/created come from the resolution hook, handed in rather
// than fetched so this file stays linkable without it.
void TryLockUiScreenSize(bool enabled, UInt32 believedWidth, UInt32 believedHeight,
                         UInt32 createdWidth, UInt32 createdHeight);

}  // namespace obvr::render

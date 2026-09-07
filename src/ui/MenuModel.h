#pragma once

#include "core/Types.h"

namespace obvr::ui {

// What a settings menu is, apart from the drawing of it.
//
// Deliberately knows nothing about Config, and that is the seam. The bridge
// that reads a value out of the configuration and writes it back is a separate
// and very thin thing; everything with a decision in it - what moving down
// does at the bottom of a list, what a step does at the end of a range, how a
// number becomes the text of a number - is here, over plain values, and can be
// checked without Windows, a headset, or a running game.
//
// It also keeps the menu honest about a limit it cannot remove. Most of OBVR's
// settings are read fresh every frame, so changing one takes effect at once.
// A few are not: the render size is decided when the device is created, and
// the stereo mode with it. Those are not hidden - a menu that silently omits
// the setting somebody is looking for is worse than one that shows it greyed -
// but they are marked, and the mark is part of the model rather than a note in
// the drawing code.

enum class ItemKind {
	// On or off. Stored as a number like everything else, so the bridge back
	// to the configuration has one shape rather than three.
	Toggle,

	// A number over a range, changed in steps.
	Number,
	// A line of text with no value: an explanation in a flow such as the
	// onboarding. Not something a key changes.
	Text,
	// A button: "Next", "Back", "Finish". Left or right on it fires it; the
	// value carries nothing.
	Action,
};

struct MenuItem {
	// What the row says. Kept short enough to sit beside its value at the
	// width the menu is drawn at; the explanation goes in `help`.
	const char* label = "";

	// One line under the list explaining the selected row. The INI has
	// paragraphs on most of these settings and they are worth having, but a
	// headset is a bad place to read a paragraph.
	const char* help = "";

	ItemKind kind = ItemKind::Toggle;

	float value = 0.0f;
	float minimum = 0.0f;
	float maximum = 1.0f;

	// How far one press moves the value. Ignored for a toggle, which has only
	// two places to be.
	float step = 1.0f;

	// How many decimals the value is shown with. A head movement scale wants
	// two; a key code wants none, and showing "1.00" for a key would suggest
	// it could be 1.5.
	UInt32 decimals = 2;

	// True when the setting only takes effect on the next start - the render
	// size and what is decided with it. The menu says so on the row rather
	// than letting somebody change it and conclude OBVR is broken.
	bool needsRestart = false;

	// A picture beside the row: rows of characters, '#' in the text colour
	// and 'o' in the accent colour, anything else clear (see Canvas::DrawIcon).
	// A row with one is drawn taller, with its help beside the picture. The
	// onboarding's two ways to play are the rows that carry these.
	const char* const* icon = nullptr;
	UInt32 iconRows = 0;

	// Marked as the choice currently in force - the one of several rows that
	// is "on". Drawn as a mark before the label.
	bool chosen = false;
};

// What a press means. Kept separate from any key code: which key does which of
// these is a binding, and bindings do not belong in the part that decides what
// happens.
enum class MenuAction {
	None,
	Up,
	Down,
	Decrease,
	Increase,

	// A bigger jump through a long list, for getting past a category.
	PageUp,
	PageDown,
};

// Where the menu is: which row is selected, and which row is at the top of the
// visible window.
struct MenuState {
	UInt32 selected = 0;
	UInt32 firstVisible = 0;
};

// How far a page moves. A whole visible page would leave nothing on screen to
// orient by, so it moves by most of one and keeps a row of context.
inline constexpr UInt32 kPageContextRows = 2;

// The state after an action.
//
// itemCount of zero gives back a state at the top rather than an invalid one -
// a menu with nothing in it is a menu bug, not a reason to index into nothing.
//
// The selection wraps at both ends. In a headset, holding a key and watching
// the highlight stop dead at the bottom reads as the input having been lost;
// wrapping is unambiguous, and the list is short enough that going the long way
// round costs nothing.
//
// The window follows the selection rather than the other way about, and it
// follows by the least it can: moving down past the bottom scrolls one row, not
// a page, because a list that jumps a page under a moving highlight is a list
// nobody can track.
MenuState AdvanceMenu(MenuState state, MenuAction action, UInt32 itemCount, UInt32 visibleRows);

// What a click on a row means, for a laser or a finger rather than a key:
// a toggle flips, a number steps down on its left half and up on its right
// (xFraction is where across the row the click landed, 0 to 1), a button
// fires as Increase, and a line of text is nothing.
MenuAction ClickActionFor(const MenuItem& item, float xFraction);

// The value after an action, clamped to the item's range.
//
// Anything that is not Decrease or Increase leaves the value exactly as it
// was - moving the selection must not nudge the setting it lands on.
//
// A toggle ignores the step and simply flips, in both directions, so whichever
// key is nearer to hand does the same thing.
float AdjustValue(const MenuItem& item, MenuAction action);

// The value as text, written into `out`.
//
// Its own routine because the plugin builds without the CRT - there is no
// snprintf to reach for - and because the rounding wants to be looked at
// rather than assumed. Half away from zero, so 1.005 at two decimals is 1.01
// and -1.005 is -1.01, which is what a reader expects from a number that is
// being displayed rather than computed with.
//
// Always terminates, and never writes more than `size` bytes including the
// terminator. A value too long for the buffer is truncated rather than
// refused: a menu row that says "1.0" instead of "1.05" is a cosmetic fault,
// and one that writes past a stack buffer is not.
//
// A toggle is written as "on" or "off" rather than 1 or 0. The one thing a
// settings menu must never make somebody guess is which way round a switch is.
void FormatValue(const MenuItem& item, char* out, UInt32 size);

// A whole number as text, for the row counter and anything else that needs one.
// Same contract as FormatValue: always terminated, never overruns.
void FormatInteger(SInt32 value, char* out, UInt32 size);

// The value as it should be written into the INI, which is not the same as the
// value as it should be shown.
//
// A switch reads better as "on" in a menu and has to be written as 1 in a file
// - Config reads it with a numeric reader, and "on" would parse as nothing. The
// two spellings are deliberately different functions rather than one with a
// flag, because getting them the wrong way round produces a menu that looks
// right and a file the game cannot read.
//
// falseWord and trueWord are for the settings Config reads as a word instead:
// Menus is "cinema" or "world". When either is given, a switch is written as
// the matching word. Ignored for a number.
//
// Same contract as FormatValue: always terminated, never overruns.
void FormatValueForIni(const MenuItem& item, const char* falseWord, const char* trueWord,
                       char* out, UInt32 size);

}  // namespace obvr::ui

#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"
#include "ui/MenuModel.h"

namespace obvr::ui {

// What the settings menu looks like.
//
// A function over a canvas rather than a method on the layer that owns the
// texture, and that is the whole point: the appearance can then be produced and
// inspected in a test, and the layer is left holding a texture and a copy. The
// alternative - laying out a menu inside the code that locks a Direct3D
// surface - means every question about where a row sits costs a headset.
//
// Everything is in whole pixels at a whole-number scale. See the note on scale
// in Canvas::DrawText: at any other factor the strokes of a five-pixel-tall
// letter come out uneven, and in a headset that reads as a menu somebody forgot
// to sharpen.

// The colours are Oblivion's own menus': a dark brown ground, the text in
// parchment tan, headings and ornaments in old gold, and the chosen row lit
// rather than boxed - vanilla brightens the selected line to near white and
// leaves the rest tan.
struct MenuTheme {
	// The panel. Alpha is part of it, and it is the only place transparency
	// comes from - the text and the highlight are drawn opaque on top, so a
	// letter never fades into what is behind the menu.
	render::Pixel background{26, 19, 12, 236};

	// The outer frame, and the thinner light line inside it with the corner
	// diamonds - the double border every vanilla panel wears.
	render::Pixel frame{132, 102, 54, 255};
	render::Pixel frameLight{222, 190, 122, 255};
	render::Pixel title{240, 218, 160, 255};

	// A category heading, deliberately dimmer than a row: it is a signpost,
	// not something to be read one word at a time.
	render::Pixel category{190, 158, 100, 255};

	render::Pixel text{214, 198, 164, 255};
	render::Pixel value{244, 228, 178, 255};

	// The selected row: a warm dark bar under text lit to parchment white.
	render::Pixel highlight{88, 64, 34, 255};
	render::Pixel highlightText{255, 242, 208, 255};

	// The help line and the note on a row that needs a restart.
	render::Pixel help{168, 152, 124, 255};
	render::Pixel warning{226, 162, 108, 255};
};

// How many rows fit on a canvas of this height at this scale.
//
// Its own function because the caller needs the same number the painter uses -
// AdvanceMenu scrolls by it - and two places computing it separately is two
// places to get it wrong, with the symptom being a highlight that scrolls one
// row before it reaches the edge, or one row after it has passed it.
//
// Never returns zero: a canvas too short for even one row still gets one, drawn
// clipped, because a menu with no rows at all cannot be navigated back out of.
UInt32 VisibleRowsFor(UInt32 canvasHeight, UInt32 scale);

// Draws the whole menu: panel, title, the visible rows with their values, the
// heading above each category, and the help line for the selected row.
//
// `items` and `categories` are parallel arrays of `count` entries - the item as
// the model sees it, and the heading it belongs under. Two arrays rather than a
// field on MenuItem because the model is meant not to know what a category is.
//
// A count of zero draws the panel and the title and nothing else, rather than
// nothing at all: an empty menu that still appears is a bug somebody can see.
void PaintMenu(Canvas& canvas, const MenuItem* items, const char* const* categories, UInt32 count,
               MenuState state, UInt32 scale, const MenuTheme& theme);

// The same with a title of the caller's - the onboarding names its pages.
// Text rows are drawn dimmed and without a value; an Action row carries a
// marker where the value would be.
void PaintMenu(Canvas& canvas, const MenuItem* items, const char* const* categories, UInt32 count,
               MenuState state, UInt32 scale, const MenuTheme& theme, const char* title);

// Which row a canvas pixel lies on, for a laser pointed at the menu: the
// item's index, or -1 for the title, a heading, the help line, the margins,
// or anything past the last drawn row. Walks the same layout PaintMenu draws
// - the test pins the two together by painting each selection and reading
// the highlight bar back - so a hit is a row that is actually on screen.
SInt32 RowAtPixel(const MenuItem* items, const char* const* categories, UInt32 count,
                  MenuState state, UInt32 canvasWidth, UInt32 canvasHeight, UInt32 scale,
                  SInt32 x, SInt32 y);

}  // namespace obvr::ui

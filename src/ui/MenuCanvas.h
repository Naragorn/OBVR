#pragma once

#include "core/Types.h"
#include "render/TestPattern.h"

namespace obvr::ui {

// A rectangle of pixels and the few things OBVR needs to put on one.
//
// Kept apart from the texture it ends up in, and that is the point of it: a
// canvas is a pointer, a width and a height, so everything the menu looks like
// can be drawn and checked in a test without a device, a headset, or a running
// game. The only thing the render side then has to get right is the copy, and
// a copy either works or is obviously broken.
//
// Deliberately small. There is no clipping stack, no transform, no state - a
// settings menu is text on panels, and every feature past that is one more
// thing to keep working for no reader's benefit. What it does have is total
// clipping on every call: a rectangle half off the edge draws its half, and
// one entirely off draws nothing, rather than either being rejected or writing
// past the buffer. Menus are laid out with arithmetic, arithmetic has
// fencepost errors, and the difference between a fencepost error and a
// corrupted heap should not be a matter of luck.
//
// Integer arithmetic throughout, like TestPattern and for the same reason: the
// freestanding build has no floating point library, and a layout that cannot
// be computed there cannot be tested there.
class Canvas {
public:
	// The buffer is borrowed, not owned. It has to outlive the canvas, which
	// is trivially true for the intended use - a canvas is made, drawn on, and
	// dropped inside one function.
	Canvas(render::Pixel* pixels, UInt32 width, UInt32 height)
		: m_pixels(pixels), m_width(width), m_height(height) {}

	UInt32 Width() const { return m_width; }
	UInt32 Height() const { return m_height; }

	// The whole canvas in one colour, alpha included. This is how a menu gets
	// its transparency: fill with the panel colour at the alpha it should
	// have, then draw opaque text on top.
	void Fill(render::Pixel colour);

	// A filled rectangle, clipped. Negative positions and sizes running past
	// the edge are normal here rather than exceptional - a row highlight is
	// positioned by arithmetic that does not know where the edge is.
	//
	// A width or height of zero or less draws nothing, which is what makes
	// "draw a bar as wide as the value" work at the bottom of its range
	// without a special case at the call site.
	void FillRect(SInt32 x, SInt32 y, SInt32 width, SInt32 height, render::Pixel colour);

	// A one-pixel outline, at the given scale so it stays visible when the
	// rest of the menu is scaled up.
	void DrawFrame(SInt32 x, SInt32 y, SInt32 width, SInt32 height, SInt32 thickness,
	               render::Pixel colour);

	// Text, with (x, y) at the top left of the first glyph.
	//
	// scale is a whole number for a reason: at any other factor a glyph's
	// pixels land between screen pixels and the strokes come out uneven, one
	// row of a letter fatter than the next. In a headset that reads as a
	// blurry menu rather than as a scaling artefact, and no amount of
	// filtering fixes it - a five-pixel-tall letter has no detail to spare.
	//
	// Characters with no glyph draw as blanks. See GlyphPixel: a menu is not
	// the place to discover a stray byte.
	void DrawText(SInt32 x, SInt32 y, const char* text, SInt32 scale, render::Pixel colour);

	// One pixel, clipped. The primitive the others are built from, exposed
	// because a test that wants to know what got drawn reads it back.
	void SetPixel(SInt32 x, SInt32 y, render::Pixel colour);
	render::Pixel GetPixel(SInt32 x, SInt32 y) const;

private:
	render::Pixel* m_pixels = nullptr;
	UInt32 m_width = 0;
	UInt32 m_height = 0;
};

}  // namespace obvr::ui

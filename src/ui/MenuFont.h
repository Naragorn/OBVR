#pragma once

#include "core/Types.h"

namespace obvr::ui {

// The letters OBVR draws with, because nothing else here can draw one.
//
// Everything OBVR has put in the headset so far has been Oblivion's own
// picture, copied. A settings menu is the first thing it has to say in its own
// words, and there is no route to a letter that does not end here:
//
//   * Oblivion's font system draws into the game's 2D layer through code this
//     project would have to reverse engineer, for a menu that then inherits
//     every layout problem the flat layer already has.
//   * D3DX has a font helper, but it lives in a versioned d3dx9_NN.dll that
//     may or may not be beside the game, and the plugin builds without the
//     CRT precisely so that it depends on nothing it did not bring.
//   * LINK and MenuQue draw menus already, and that route was weighed and put
//     aside: it needs an esp, two more mods, and it produces a flat menu laid
//     out for a monitor rather than for a headset.
//
// So the glyphs are here, as data. Five by seven, which is the smallest size
// that still tells an 8 from a B, and drawn at a whole-number scale so a pixel
// of a letter is a square block of screen pixels and nothing is ever
// resampled - in a headset a half-pixel of blur on a thin stroke is the
// difference between reading a number and guessing it.
//
// Written as rows of text rather than as packed bits. It is the same data
// either way, but one of the two can be proofread, and a font that cannot be
// proofread is a font with a broken glyph in it that nobody finds until it is
// on screen. The packing happens in the accessor.
//
// All integer arithmetic, no floating point, no CRT - the same constraint
// TestPattern is written under, and for the same reason: what cannot be
// generated in the freestanding build cannot be checked there either.

// The size of one glyph's box, and the space a character advances by. The
// advance is one wider so that letters do not touch; the gap belongs to the
// character on the left, which is what makes a run of them line up.
inline constexpr UInt32 kGlyphWidth = 5;
inline constexpr UInt32 kGlyphHeight = 7;
inline constexpr UInt32 kGlyphAdvance = kGlyphWidth + 1;

// The rows a line of text advances by, glyph plus leading.
inline constexpr UInt32 kLineAdvance = kGlyphHeight + 2;

// The first and last character with a glyph. Everything printable in ASCII and
// nothing else: a menu that has to name a setting can do it in that, and a
// character set that stops at 126 is one that cannot be handed a byte it has
// no answer for.
inline constexpr char kFirstGlyph = ' ';
inline constexpr char kLastGlyph = '~';

// Whether the glyph for `character` has ink at (x, y), with (0, 0) at its top
// left.
//
// Pure, and total: out-of-range coordinates and characters with no glyph both
// answer false rather than reading past the table. A menu is not the place to
// discover a stray byte, and answering "no ink" keeps a bad character looking
// like a gap instead of a crash.
bool GlyphPixel(char character, UInt32 x, UInt32 y);

// How wide a string is, in pixels, at a scale of one.
//
// The trailing gap of the last character is not counted, so a string measured
// this way and drawn at the same place ends where it says it does - which is
// what right-aligning a number depends on.
UInt32 TextWidth(const char* text);

}  // namespace obvr::ui

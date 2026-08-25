#pragma once

#include "core/Types.h"

namespace obvr::render {

// The picture OBVR puts in the headset before it can put Oblivion there.
//
// Deliberately not a coloured rectangle. A rectangle answers one question -
// did anything arrive - and leaves every other question open, so the next
// three faults each cost their own debugging session. This pattern is built
// to answer four at a glance, each by a feature that is wrong in an obvious
// way if the thing it tests is wrong:
//
//   1. Does the whole texture arrive?  A border frame around all four edges.
//      Wrong texture bounds cut it, and a missing edge says which side.
//   2. Is it the right way up?         A vertical ramp, black at the top and
//      white at the bottom. Upside down is unmistakable, and the ramp doubles
//      as a look at whether the colour space is being handled sensibly.
//   3. Is left really left?            A marker in a different colour per
//      eye - red on the left, green on the right. Close one eye and name the
//      colour; no measurement needed.
//   4. Is it mirrored?                 That marker sits off centre
//      horizontally, on its own side. A mirrored image moves it across.
//
// All integer arithmetic. Not for speed - this runs once - but because the
// freestanding build has no floating point library, and a pattern that
// cannot be generated on the verification build is a pattern that cannot be
// checked there either.

enum class Eye { Left, Right };

// One pixel, in the byte order DXGI_FORMAT_R8G8B8A8_UNORM expects: red first.
struct Pixel {
	UInt8 r;
	UInt8 g;
	UInt8 b;
	UInt8 a;
};

// The colour at one position. Pure: the same arguments always give the same
// answer, which is what makes the whole pattern checkable without a headset,
// a device, or a window.
//
// Out-of-range coordinates are clamped rather than rejected. A pattern
// generator is not the place to discover a bad size, and returning something
// sensible keeps a fencepost error looking like a fencepost error instead of
// a crash.
Pixel PatternPixel(UInt32 x, UInt32 y, UInt32 width, UInt32 height, Eye eye);

// Fills a buffer with the pattern.
//
// rowPitch is in bytes and is the distance between the start of one row and
// the next, which Direct3D is free to make larger than width * 4 - a texture
// row is padded to whatever alignment the driver wants. Writing rows back to
// back regardless is the classic way to get a picture that shears diagonally,
// and it looks like a projection fault rather than a stride fault.
void FillPattern(UInt8* pixels, UInt32 width, UInt32 height, UInt32 rowPitch, Eye eye);

// How many bytes a buffer holding one tightly packed eye image needs, and
// whether that size is usable at all.
//
// It lives here rather than with the texture code so that it can be checked
// on any platform, which matters more than where it reads best: a headset
// asking for a size that overflows a 32-bit multiplication would otherwise
// produce an allocation far too small and a write straight past the end of
// it. In a 32-bit process that is not a theoretical worry - it is a number
// four thousand pixels square comes within reach of.
//
// Returns 0 when the size is unusable, which the caller has to treat as a
// refusal rather than as an empty buffer.
UInt32 PatternBufferBytes(UInt32 width, UInt32 height);

// How thick the border is for a given size, in pixels.
//
// Proportional rather than fixed: a fixed four pixels is a bold frame at
// 320 wide and invisible at 2000, and the headset resolutions this has to
// work at are not known in advance.
UInt32 BorderThickness(UInt32 width, UInt32 height);

}  // namespace obvr::render

#pragma once

#include <vector>

#include "core/Types.h"
#include "render/TestPattern.h"

namespace obvr::render {

// Reading a DDS file into pixels on the CPU: the game's item and spell icons
// (textures\menus\icons\...), which OBVR paints into its own quick menu.
//
// What the icons are, counted in the two vanilla texture archives on
// 2026-09-28 (Oblivion - Textures - Compressed.bsa and DLCShiveringIsles -
// Textures.bsa, every file under menus\icons): 844 DXT3 and 301 DXT5 at
// 64x64, 90 DXT3 and 41 DXT5 at 32x32, 28 DXT1, 25 uncompressed with
// alpha (16 or 32 bits a pixel, bit masks in the header) and a handful of
// other sizes. So: DXT1, DXT3, DXT5 and bit-masked RGB(A), the top level
// only; anything else (DX10 headers, luminance, volumes) is refused.
//
// The decoding follows Microsoft's block compression description
// (learn.microsoft.com/en-us/windows/uwp/graphics-concepts/block-compression
// and .../opaque-and-1-bit-alpha-textures): 5:6:5 end colours, two derived
// ones; BC1 with three colours and a transparent one when the first end
// colour is not the greater; BC2 four bits of alpha a pixel; BC3 two end
// alphas and six or four derived ones. DXT3 and DXT5 colour blocks are read
// in four-colour mode always, the way Direct3D 10 hardware decodes BC2 and
// BC3 (their alpha comes from the alpha block).
//
// Pure, so every format is checked by a test with hand-made blocks.

struct DdsInfo {
	enum class Format : UInt8 { Dxt1, Dxt3, Dxt5, Masked };
	Format format = Format::Masked;
	UInt32 width = 0;
	UInt32 height = 0;
	// Masked only: bits a pixel (16, 24 or 32) and each channel's mask; an
	// alpha mask of zero is opaque.
	UInt32 bitCount = 0;
	UInt32 redMask = 0;
	UInt32 greenMask = 0;
	UInt32 blueMask = 0;
	UInt32 alphaMask = 0;
};

inline constexpr UInt32 kDdsHeaderBytes = 128;  // "DDS " and the 124-byte header
// Bigger than any icon has a reason to be: a file claiming more is refused
// rather than decoded into a buffer of its choosing.
inline constexpr UInt32 kDdsMaxSide = 1024;

// The header: false when it is not a DDS this reader decodes, or the file is
// too short for its top level.
bool ReadDdsHeader(const UInt8* data, UInt32 size, DdsInfo& out);

// The top level as pixels, red first (Pixel's order), row by row from the
// top. False as ReadDdsHeader.
bool DecodeDds(const UInt8* data, UInt32 size, std::vector<Pixel>& out, UInt32& width,
               UInt32& height);

// A picture fitted into a square of `side` pixels: each target pixel is the
// average of the source pixels it covers (or the nearest one when the source
// is smaller), colours weighted by their alpha so a transparent pixel's
// colour does not darken the edge. A non-square source keeps its shape,
// centred, the rest transparent.
void FitSquare(const Pixel* src, UInt32 width, UInt32 height, Pixel* dst, UInt32 side);

}  // namespace obvr::render

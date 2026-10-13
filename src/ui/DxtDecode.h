#pragma once

#include "core/Types.h"
#include "render/TestPattern.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// DXT5 (BC3) blocks decoded into pixels in the canvas's own channel order
// (red and blue swapped, as the texture keeps them - SwapRedAndBlue). The
// game's own enemy health frames are DXT5, 128 by 32, no mipmaps (read
// 2026-10-13 off textures/menus/misc/sequence_dds/000001.dds); read back
// off the texture the game loaded (game/EnemyHealthModel.h). A block is
// 16 bytes: two alpha endpoints and 16 three-bit alpha indices, two RGB565
// colour endpoints and 16 two-bit colour indices (the D3D documentation's
// "Block Compression (Direct3D 10)", the same for 9).

inline constexpr UInt32 kFourCcDxt5 = 0x35545844;  // 'D','X','T','5'
inline constexpr UInt32 kFormatA8R8G8B8 = 21;      // D3DFMT_A8R8G8B8

inline render::Pixel Rgb565(UInt32 v, UInt8 alpha) {
	const UInt8 r = static_cast<UInt8>(((v >> 11) & 31) * 255 / 31);
	const UInt8 g = static_cast<UInt8>(((v >> 5) & 63) * 255 / 63);
	const UInt8 b = static_cast<UInt8>((v & 31) * 255 / 31);
	return SwapRedAndBlue(render::Pixel{r, g, b, alpha});
}

// One block into out[16], row by row.
inline void DecodeDxt5Block(const UInt8* block, render::Pixel out[16]) {
	const UInt32 a0 = block[0];
	const UInt32 a1 = block[1];
	UInt8 alphas[8];
	alphas[0] = static_cast<UInt8>(a0);
	alphas[1] = static_cast<UInt8>(a1);
	if (a0 > a1) {
		for (UInt32 i = 1; i < 7; ++i) {
			alphas[i + 1] = static_cast<UInt8>(((7 - i) * a0 + i * a1) / 7);
		}
	} else {
		for (UInt32 i = 1; i < 5; ++i) {
			alphas[i + 1] = static_cast<UInt8>(((5 - i) * a0 + i * a1) / 5);
		}
		alphas[6] = 0;
		alphas[7] = 255;
	}
	UInt64 alphaBits = 0;
	for (UInt32 i = 0; i < 6; ++i) {
		alphaBits |= static_cast<UInt64>(block[2 + i]) << (8 * i);
	}
	const UInt32 c0 = static_cast<UInt32>(block[8]) | (static_cast<UInt32>(block[9]) << 8);
	const UInt32 c1 = static_cast<UInt32>(block[10]) | (static_cast<UInt32>(block[11]) << 8);
	const render::Pixel p0 = Rgb565(c0, 255);
	const render::Pixel p1 = Rgb565(c1, 255);
	render::Pixel colours[4];
	colours[0] = p0;
	colours[1] = p1;
	if (c0 > c1) {
		colours[2] = render::Pixel{static_cast<UInt8>((2 * p0.r + p1.r) / 3), static_cast<UInt8>((2 * p0.g + p1.g) / 3),
		                           static_cast<UInt8>((2 * p0.b + p1.b) / 3), 255};
		colours[3] = render::Pixel{static_cast<UInt8>((p0.r + 2 * p1.r) / 3), static_cast<UInt8>((p0.g + 2 * p1.g) / 3),
		                           static_cast<UInt8>((p0.b + 2 * p1.b) / 3), 255};
	} else {
		colours[2] = render::Pixel{static_cast<UInt8>((p0.r + p1.r) / 2), static_cast<UInt8>((p0.g + p1.g) / 2),
		                           static_cast<UInt8>((p0.b + p1.b) / 2), 255};
		colours[3] = render::Pixel{0, 0, 0, 255};
	}
	const UInt32 colourBits = static_cast<UInt32>(block[12]) | (static_cast<UInt32>(block[13]) << 8) |
	                          (static_cast<UInt32>(block[14]) << 16) | (static_cast<UInt32>(block[15]) << 24);
	for (UInt32 i = 0; i < 16; ++i) {
		render::Pixel p = colours[(colourBits >> (2 * i)) & 3];
		p.a = alphas[(alphaBits >> (3 * i)) & 7];
		out[i] = p;
	}
}

// A whole DXT5 picture: `data` the blocks, `pitch` the bytes from one row
// of blocks to the next, `out` width x height pixels. False when the size
// is no multiple of 4 or nothing is given.
inline bool DecodeDxt5(const UInt8* data, UInt32 width, UInt32 height, UInt32 pitch, render::Pixel* out) {
	if (data == nullptr || out == nullptr || width == 0 || height == 0 || (width & 3) != 0 || (height & 3) != 0) {
		return false;
	}
	render::Pixel block[16];
	for (UInt32 by = 0; by < height / 4; ++by) {
		for (UInt32 bx = 0; bx < width / 4; ++bx) {
			DecodeDxt5Block(data + by * pitch + bx * 16, block);
			for (UInt32 y = 0; y < 4; ++y) {
				for (UInt32 x = 0; x < 4; ++x) {
					out[(by * 4 + y) * width + bx * 4 + x] = block[y * 4 + x];
				}
			}
		}
	}
	return true;
}

// An A8R8G8B8 picture (the bytes blue, green, red, alpha - the canvas's own
// order) copied row by row.
inline bool CopyA8R8G8B8(const UInt8* data, UInt32 width, UInt32 height, UInt32 pitch, render::Pixel* out) {
	if (data == nullptr || out == nullptr || width == 0 || height == 0) {
		return false;
	}
	for (UInt32 y = 0; y < height; ++y) {
		const UInt8* row = data + y * pitch;
		for (UInt32 x = 0; x < width; ++x) {
			out[y * width + x] = render::Pixel{row[x * 4], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3]};
		}
	}
	return true;
}

}  // namespace obvr::ui

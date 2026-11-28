#pragma once

// The dust on the mirror's glass (docs/mirror-spec.md 7, stage 1): a texture
// made once on the CPU, as the tablet's paper is, and drawn as one blended
// piece over the glass - no shader, no file to ship. Sparse specks and a few
// faint smudges of pale grey, premultiplied (the tablet's pieces blend one,
// inverse source alpha), the specks at most 12 percent opaque and the smudges
// at most 5. The same texels every time: no clock, no random state. Pure;
// covered by mirror_page_test.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr UInt32 kMirrorDustSide = 256;

// Of 255: the specks' most, 12 percent; the smudges' most, 5 percent.
inline constexpr UInt32 kMirrorDustSpeckMost = 30;
inline constexpr UInt32 kMirrorDustSmudgeMost = 12;

namespace mirror_dust {

inline UInt32 Hash(UInt32 x, UInt32 y) {
	UInt32 h = x * 0x9E3779B1u ^ (y + 0x7F4A7C15u) * 0x85EBCA77u;
	h ^= h >> 15;
	h *= 0x2C1B3C6Du;
	h ^= h >> 12;
	h *= 0x297A2D39u;
	h ^= h >> 15;
	return h;
}

// A texel of pale grey at `alpha` of 255, colour weighed by it.
inline UInt32 Texel(UInt32 alpha) {
	const UInt32 r = 214 * alpha / 255;
	const UInt32 g = 219 * alpha / 255;
	const UInt32 b = 224 * alpha / 255;
	return (alpha << 24) | (r << 16) | (g << 8) | b;
}

// The smudges: where, how wide (of the side), how strong.
struct Smudge {
	float x;
	float y;
	float radius;
	float strength;
};
inline constexpr Smudge kSmudges[] = {{0.18f, 0.22f, 0.14f, 1.0f}, {0.71f, 0.15f, 0.10f, 0.7f}, {0.55f, 0.48f, 0.16f, 0.8f},
                                      {0.12f, 0.70f, 0.12f, 0.7f}, {0.83f, 0.66f, 0.13f, 1.0f}, {0.40f, 0.88f, 0.09f, 0.6f}};

// The smudges' alpha at a texel, of kMirrorDustSmudgeMost: the strongest
// smudge there, falling off with the square of the distance to its middle.
inline UInt32 SmudgeAlpha(UInt32 x, UInt32 y, UInt32 side) {
	const float fx = static_cast<float>(x) / static_cast<float>(side);
	const float fy = static_cast<float>(y) / static_cast<float>(side);
	float most = 0.0f;
	for (const Smudge& s : kSmudges) {
		const float dx = (fx - s.x) / s.radius;
		const float dy = (fy - s.y) / s.radius;
		const float d = dx * dx + dy * dy;
		const float fall = d < 1.0f ? (1.0f - d) * (1.0f - d) * s.strength : 0.0f;
		most = fall > most ? fall : most;
	}
	return static_cast<UInt32>(most * static_cast<float>(kMirrorDustSmudgeMost));
}

// One speck in this many 2 by 2 blocks of texels (about a millimetre and a half on the page).
inline constexpr UInt32 kSpeckOneIn = 130;

}  // namespace mirror_dust

// `texels`: side x side of A8R8G8B8, one 32-bit word each (0xAARRGGBB), row by
// row. Nothing for no buffer or no side.
inline void FillMirrorDust(UInt32* texels, UInt32 side) {
	if (texels == nullptr || side == 0) {
		return;
	}
	for (UInt32 y = 0; y < side; ++y) {
		for (UInt32 x = 0; x < side; ++x) {
			UInt32 alpha = mirror_dust::SmudgeAlpha(x, y, side);
			const UInt32 h = mirror_dust::Hash(x >> 1, y >> 1);
			if (h % mirror_dust::kSpeckOneIn == 0) {
				// A speck, its strength from the hash's other bits: 5 to 12 percent.
				alpha = 13 + (h >> 16) % (kMirrorDustSpeckMost - 13 + 1);
			}
			texels[y * side + x] = alpha == 0 ? 0u : mirror_dust::Texel(alpha);
		}
	}
}

}  // namespace obvr::vr

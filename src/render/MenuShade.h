#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"

// The decisions behind the shaded menu background, kept apart from the device
// that applies them so every flow can be exercised without one.
//
// Two things happen to the held pair when a pause menu opens, both asked for
// after seeing them missing. The brown shade is vanilla's own idea: Oblivion
// dims the paused world behind its menus, and a frozen world shown at full
// colour reads as a freeze rather than a pause. The single border is a stereo
// fact: the two eyes' pictures are cropped at opposite edges, so each eye's
// picture ends at a different angle and the edges cannot be fused - reported
// as doubled bars at the sides while the top and bottom, whose crops match,
// show one. Blacking each picture back to the window BOTH eyes show puts all
// four edges at the same angles.
namespace obvr::render {

// Reads a colour written as six hex digits, with or without a leading '#'.
// Anything else is refused rather than guessed at: a half-parsed colour is a
// tint nobody asked for, with nothing in the file to explain it.
inline bool ParseHexColor(const char* text, UInt32& rgbOut) {
	if (text == nullptr) {
		return false;
	}
	if (*text == '#') {
		++text;
	}

	UInt32 value = 0;
	int digits = 0;
	for (; text[digits] != '\0'; ++digits) {
		if (digits == 6) {
			return false;
		}
		const char c = text[digits];
		UInt32 nibble;
		if (c >= '0' && c <= '9') {
			nibble = static_cast<UInt32>(c - '0');
		} else if (c >= 'a' && c <= 'f') {
			nibble = static_cast<UInt32>(c - 'a') + 10;
		} else if (c >= 'A' && c <= 'F') {
			nibble = static_cast<UInt32>(c - 'A') + 10;
		} else {
			return false;
		}
		value = (value << 4) | nibble;
	}

	if (digits != 6) {
		return false;
	}

	rgbOut = value;
	return true;
}

// Folds the configured colour and strength into the one ARGB value the tint
// quad draws with. Zero means "no tint", and the only way to get zero is a
// strength of zero (or less): even a black shade at full strength carries its
// alpha, so it still counts as a tint rather than as the feature being off.
inline UInt32 ComposeShadeColor(UInt32 rgb, float strength) {
	if (strength <= 0.0f) {
		return 0;
	}
	if (strength > 1.0f) {
		strength = 1.0f;
	}
	const UInt32 alpha = static_cast<UInt32>(strength * 255.0f + 0.5f);
	if (alpha == 0) {
		return 0;
	}
	return (alpha << 24) | (rgb & 0x00FFFFFFu);
}

// Where the frame region that BOTH eyes show lands inside one eye's picture.
//
// Each eye shows a slice of the game's frame (mySrc, in frame pixels) placed
// into its own picture (myDest, in texture pixels). The slices differ - the
// eyes are cropped at opposite edges - and the strip one eye shows alone is
// exactly the strip whose edge cannot be fused. The shared frame window is the
// intersection of the two source slices, mapped here through the same linear
// placement StretchRect used.
//
// A degenerate input - an empty slice, or slices that do not overlap - returns
// myDest unchanged. That only happens when the placement arithmetic upstream
// has already failed, and blacking out the whole picture on top of a fault
// would replace a wrong picture with none.
inline d3d9::Rect CommonWindowInEye(const d3d9::Rect& mySrc, const d3d9::Rect& myDest,
                                    const d3d9::Rect& otherSrc) {
	const SInt32 srcWidth = mySrc.right - mySrc.left;
	const SInt32 srcHeight = mySrc.bottom - mySrc.top;
	if (srcWidth <= 0 || srcHeight <= 0) {
		return myDest;
	}

	const SInt32 sharedLeft = otherSrc.left > mySrc.left ? otherSrc.left : mySrc.left;
	const SInt32 sharedRight = otherSrc.right < mySrc.right ? otherSrc.right : mySrc.right;
	const SInt32 sharedTop = otherSrc.top > mySrc.top ? otherSrc.top : mySrc.top;
	const SInt32 sharedBottom = otherSrc.bottom < mySrc.bottom ? otherSrc.bottom : mySrc.bottom;
	if (sharedRight <= sharedLeft || sharedBottom <= sharedTop) {
		return myDest;
	}

	const float scaleX =
		static_cast<float>(myDest.right - myDest.left) / static_cast<float>(srcWidth);
	const float scaleY =
		static_cast<float>(myDest.bottom - myDest.top) / static_cast<float>(srcHeight);

	auto mapX = [&](SInt32 frameX) {
		const float mapped =
			static_cast<float>(myDest.left) +
			static_cast<float>(frameX - mySrc.left) * scaleX;
		SInt32 pixel = static_cast<SInt32>(mapped + 0.5f);
		if (pixel < myDest.left) {
			pixel = myDest.left;
		}
		if (pixel > myDest.right) {
			pixel = myDest.right;
		}
		return pixel;
	};
	auto mapY = [&](SInt32 frameY) {
		const float mapped =
			static_cast<float>(myDest.top) +
			static_cast<float>(frameY - mySrc.top) * scaleY;
		SInt32 pixel = static_cast<SInt32>(mapped + 0.5f);
		if (pixel < myDest.top) {
			pixel = myDest.top;
		}
		if (pixel > myDest.bottom) {
			pixel = myDest.bottom;
		}
		return pixel;
	};

	d3d9::Rect common;
	common.left = mapX(sharedLeft);
	common.right = mapX(sharedRight);
	common.top = mapY(sharedTop);
	common.bottom = mapY(sharedBottom);
	return common;
}

// The parts of the picture outside the shared window, as up to four fill
// rectangles. Empty strips are left out, so the count is what the caller
// loops over and a picture already trimmed costs no fills at all.
//
// The side strips take the full height and the top and bottom strips sit
// between them; where two strips would overlap in a corner, both painting the
// same black pixel twice is cheaper than corner arithmetic.
inline int EdgeStrips(const d3d9::Rect& dest, const d3d9::Rect& common, d3d9::Rect out[4]) {
	int count = 0;
	if (common.left > dest.left) {
		out[count++] = d3d9::Rect{dest.left, dest.top, common.left, dest.bottom};
	}
	if (common.right < dest.right) {
		out[count++] = d3d9::Rect{common.right, dest.top, dest.right, dest.bottom};
	}
	if (common.top > dest.top) {
		out[count++] = d3d9::Rect{dest.left, dest.top, dest.right, common.top};
	}
	if (common.bottom < dest.bottom) {
		out[count++] = d3d9::Rect{dest.left, common.bottom, dest.right, dest.bottom};
	}
	return count;
}

}  // namespace obvr::render

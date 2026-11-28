#pragma once

// The texts the game and mods draw over and beside the menus while the
// tablet holds them - the notices top left (Basic Primary Needs' status lines
// are notices), what people say, a region's name, Ultimate Leveling's line -
// shown on the tablet above its page (docs/inventory-tablet-spec.md section
// 31.3; Nadi, headset 2026-11-28: "texte von mods oder game die über dem
// inventar kamen können wir nun also auch über das inventar an die hand
// schieben, wenn das an ist"). [Tablet] TextsOnTablet. Before, nothing of the
// layer outside the tablet's face was shown while the tablet was up: those
// texts were seen only in the frame the room's panel came back at the close.
//
// Pure; covered by tablet_texts_test. The rectangles are read from the HUD's
// tile trees (game/TabletTexts.cpp, vr::HudElementRect's elements), in the
// layer's pixels.

#include "core/Types.h"
#include "vr/HudTileTree.h"

namespace obvr::vr {

// The text elements read: the notices, the subtitles, the region's name, the
// XP line (vr::HudElement Messages, Subtitles, Region, XpText).
inline constexpr UInt32 kTabletTextsMost = 4;

// Between the face's top edge (its tabs included) and the texts, of the
// face's height.
inline constexpr float kTabletTextsGapShare = 0.02f;

// The texts beside the page: the rectangle round every text not wholly on
// the tablet's face (`face`, the layer's pixels) - one on the face is on the
// tablet already. Invalid when there is none.
inline UiRect TextsBesidePage(const UiRect* texts, UInt32 count, const UiRect& face) {
	UiRect out;
	for (UInt32 i = 0; texts != nullptr && i < count; ++i) {
		const UiRect& t = texts[i];
		if (!t.valid || !(t.right > t.left) || !(t.bottom > t.top)) {
			continue;
		}
		const bool onFace = face.valid && t.left >= face.left && t.right <= face.right && t.top >= face.top &&
		                    t.bottom <= face.bottom;
		if (onFace) {
			continue;
		}
		if (!out.valid) {
			out = t;
			continue;
		}
		out.left = t.left < out.left ? t.left : out.left;
		out.top = t.top < out.top ? t.top : out.top;
		out.right = t.right > out.right ? t.right : out.right;
		out.bottom = t.bottom > out.bottom ? t.bottom : out.bottom;
	}
	return out;
}

// Where those texts stand, in the face's own pixels (vr::RectOnTablet's
// rect): just above the face, its middle over the face's middle, at the
// page's own scale - the size it is read at - and narrowed to the face's
// width, its shape kept, when wider. Invalid without texts or a face.
inline UiRect TextsAbovePage(const UiRect& texts, const UiRect& face) {
	UiRect out;
	const float faceWidth = face.right - face.left;
	const float faceHeight = face.bottom - face.top;
	float width = texts.right - texts.left;
	float height = texts.bottom - texts.top;
	if (!texts.valid || !face.valid || !(faceWidth > 0.0f) || !(faceHeight > 0.0f) || !(width > 0.0f) ||
	    !(height > 0.0f)) {
		return out;
	}
	if (width > faceWidth) {
		height *= faceWidth / width;
		width = faceWidth;
	}
	const float middle = 0.5f * (face.left + face.right);
	out.left = middle - 0.5f * width;
	out.right = middle + 0.5f * width;
	out.bottom = face.top - kTabletTextsGapShare * faceHeight;
	out.top = out.bottom - height;
	out.valid = true;
	return out;
}

}  // namespace obvr::vr

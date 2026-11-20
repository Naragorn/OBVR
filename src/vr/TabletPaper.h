#pragma once

// The game's own page paper on the tablet's pieces of OBVR's own (docs/
// inventory-tablet-spec.md 7, "Open, to improve later"): the figure's page,
// the tabs and the paper the figure is shown on behind its tab were plain
// parchment colour; they take the texture the inventory's page is drawn
// with since 2026-11-20 - Textures\Menus\Shared\shared_main_background_1.dds,
// the `inv_background` image of menus\main\inventory_menu.xml (745 by 792
// units, the texture 1024 by 1024 drawn one texel a unit from its top left;
// read from the vanilla archives, Oblivion - Misc.bsa and Oblivion -
// Textures - Compressed.bsa, 2026-11-20). Opened through the engine's file
// opener, so a UI mod's own paper of that name is the one taken.
//
// What this header decides, in the texture's texels: where the page's
// picture lies, and which part of its plain parchment covers a piece of a
// given shape (cut to the piece's aspect, not stretched, so the paper's
// grain keeps its size in both directions). Pure; covered by tablet_test.

#include "vr/TabletLogic.h"

namespace obvr::vr {

inline constexpr const char* kTabletPaperFile = "Textures\\Menus\\Shared\\shared_main_background_1.dds";
inline constexpr float kTabletPaperSide = 1024.0f;  // the texture's width and height

// The page as the game draws it: the image's size from its top left.
inline constexpr float kTabletPaperPageWidth = 745.0f;
inline constexpr float kTabletPaperPageHeight = 792.0f;

// The plain parchment inside the page's leather frame, above its leather band
// (measured on the texture: the parchment runs x 30-710, y 50-612; a margin
// keeps the frame's shadow out).
inline constexpr float kParchmentLeft = 40.0f;
inline constexpr float kParchmentTop = 60.0f;
inline constexpr float kParchmentRight = 690.0f;
inline constexpr float kParchmentBottom = 600.0f;

// The whole page's picture, for a piece over the page's own rectangle.
inline UiRect PaperPageSource() {
	UiRect r;
	r.left = 0.0f;
	r.top = 0.0f;
	r.right = kTabletPaperPageWidth;
	r.bottom = kTabletPaperPageHeight;
	r.valid = true;
	return r;
}

// The empty page the figure stands on behind its tab: the page's picture with
// its leather band at the foot given over to parchment, down to the page's
// lower edge (Nadi, 2026-11-20: the figure wants paper behind all of it). Two
// bands: the page's top as the game draws it, down to kPaperKeptHeight, then
// the strip below it down to the parchment's foot stretched over the rest -
// its side frames run on, the parchment's grain stretched about 1.6 times
// downwards. `page` in any unit (the layer's pixels); the sources in texels.
inline constexpr float kPaperKeptHeight = 300.0f;
inline constexpr float kParchmentFoot = 605.0f;  // just above the leather band (612)

struct PaperBand {
	UiRect rect;
	UiRect source;
};

inline void FigurePaperBands(const UiRect& page, PaperBand (&out)[2]) {
	out[0] = PaperBand{};
	out[1] = PaperBand{};
	const float height = page.bottom - page.top;
	if (!page.valid || !(height > 0.0f) || !(page.right > page.left)) {
		return;
	}
	const float split = page.top + height * kPaperKeptHeight / kTabletPaperPageHeight;
	out[0].rect = page;
	out[0].rect.bottom = split;
	out[0].source = PaperPageSource();
	out[0].source.bottom = kPaperKeptHeight;
	out[1].rect = page;
	out[1].rect.top = split;
	out[1].source = PaperPageSource();
	out[1].source.top = kPaperKeptHeight;
	out[1].source.bottom = kParchmentFoot;
}

// The largest part of the parchment with `piece`'s aspect, centred in it.
// Invalid for an invalid or empty piece.
inline UiRect ParchmentSource(const UiRect& piece) {
	UiRect r;
	const float width = piece.right - piece.left;
	const float height = piece.bottom - piece.top;
	if (!piece.valid || !(width > 0.0f) || !(height > 0.0f)) {
		return r;
	}
	const float roomWidth = kParchmentRight - kParchmentLeft;
	const float roomHeight = kParchmentBottom - kParchmentTop;
	float takeWidth = roomWidth;
	float takeHeight = roomWidth * height / width;
	if (takeHeight > roomHeight) {
		takeHeight = roomHeight;
		takeWidth = roomHeight * width / height;
	}
	r.left = kParchmentLeft + 0.5f * (roomWidth - takeWidth);
	r.right = r.left + takeWidth;
	r.top = kParchmentTop + 0.5f * (roomHeight - takeHeight);
	r.bottom = r.top + takeHeight;
	r.valid = true;
	return r;
}

}  // namespace obvr::vr

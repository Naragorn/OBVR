#include "game/TabletTexts.h"

#include <cstdio>

#include "core/Log.h"
#include "game/HudTiles.h"
#include "game/MenuType.h"
#include "vr/HandHud.h"

namespace obvr::game {
namespace {

// The texts come and go with a notice; a read every few frames is soon
// enough, and the two tile walks cost about what the HUD on the hands pays
// every world frame.
constexpr UInt32 kTextsReadFrames = 5;

constexpr vr::HudElement kTexts[vr::kTabletTextsMost] = {vr::HudElement::Messages, vr::HudElement::Subtitles,
                                                         vr::HudElement::Region, vr::HudElement::XpText};

// Each element found and where, as it changes: the next run names what was
// read beside the page (Basic Primary Needs' lines are expected among the
// notices [G]).
void SayFound(TabletTextsState& s, const vr::UiRect* rects, const vr::UiRect& face) {
	UInt32 found = 0;
	for (UInt32 i = 0; i < vr::kTabletTextsMost; ++i) {
		found |= rects[i].valid ? 1u << i : 0u;
	}
	if (found == s.foundSaid || s.foundLines == 0) {
		return;
	}
	s.foundSaid = found;
	--s.foundLines;
	char line[512] = "";
	size_t at = 0;
	for (UInt32 i = 0; i < vr::kTabletTextsMost && at < sizeof(line); ++i) {
		const vr::UiRect& r = rects[i];
		if (!r.valid) {
			continue;
		}
		const bool onFace = !vr::TextsBesidePage(&r, 1, face).valid;
		const int n = std::snprintf(line + at, sizeof(line) - at, "%s%s %.0f,%.0f..%.0f,%.0f (%s, %s)", at > 0 ? "; " : "",
		                            vr::kHudElementKeys[static_cast<UInt32>(kTexts[i])], static_cast<double>(r.left),
		                            static_cast<double>(r.top), static_cast<double>(r.right),
		                            static_cast<double>(r.bottom),
		                            vr::HudElementInSubtitleMenu(kTexts[i]) ? "HUDSubtitleMenu" : "HUDMainMenu",
		                            onFace ? "on the page" : "above the page");
		at = n > 0 ? at + static_cast<size_t>(n) : at;
	}
	OBVR_LOG("Tablet: the HUD's texts while it is up - %s", found == 0 ? "none" : line);
}

// Every sized tile of the two menus beside the face, once: what a mod draws
// there by a name not read above.
void SayTilesBeside(const vr::HudTile* tiles, UInt32 count, const char* menu,
                    const vr::UiRect& face, float layerPixelsHeight) {
	char line[1024] = "";
	size_t at = 0;
	UInt32 listed = 0;
	for (UInt32 i = 0; i < count && at + 96 < sizeof(line); ++i) {
		const vr::UiRect r = vr::ToLayerPixels(vr::HudTileRect(tiles, count, static_cast<SInt32>(i)), layerPixelsHeight);
		if (!r.valid || !vr::TextsBesidePage(&r, 1, face).valid) {
			continue;
		}
		const int n = std::snprintf(line + at, sizeof(line) - at, "%s%s %.0f,%.0f..%.0f,%.0f", listed > 0 ? "; " : "",
		                            tiles[i].name, static_cast<double>(r.left), static_cast<double>(r.top),
		                            static_cast<double>(r.right), static_cast<double>(r.bottom));
		at = n > 0 ? at + static_cast<size_t>(n) : at;
		++listed;
	}
	OBVR_LOG("Tablet: %s tiles beside the page (%u of %u) - %s", menu, listed, count, listed == 0 ? "none" : line);
}

}  // namespace

void ReadTabletTexts(TabletTextsState& s, vr::TabletFrame& frame, float layerPixelsHeight, bool wanted) {
	const bool up = wanted && frame.up && layerPixelsHeight > 0.0f;
	if (!up) {
		s.age = 0;
		s.texts = vr::UiRect{};
		s.wasUp = false;
		return;
	}
	if (s.age == 0 || s.age >= kTextsReadFrames) {
		s.age = 0;
		const UInt32 mainCount = ReadHudTiles(kMenuIdHudMain, s.mainTiles, vr::kHudTilesMax);
		const UInt32 subtitleCount = ReadMenuRootTiles("HUDSubtitleMenu", s.subtitleTiles, vr::kHudTilesMax);
		vr::UiRect rects[vr::kTabletTextsMost];
		for (UInt32 i = 0; i < vr::kTabletTextsMost; ++i) {
			const bool subtitle = vr::HudElementInSubtitleMenu(kTexts[i]);
			rects[i] = vr::ToLayerPixels(vr::HudElementRect(subtitle ? s.subtitleTiles : s.mainTiles,
			                                                subtitle ? subtitleCount : mainCount, kTexts[i]),
			                             layerPixelsHeight);
		}
		s.texts = vr::TextsBesidePage(rects, vr::kTabletTextsMost, frame.crop);
		SayFound(s, rects, frame.crop);
		if (!s.wasUp && s.tileLines > 0) {
			--s.tileLines;
			SayTilesBeside(s.subtitleTiles, subtitleCount, "HUDSubtitleMenu", frame.crop, layerPixelsHeight);
			SayTilesBeside(s.mainTiles, mainCount, "HUDMainMenu", frame.crop, layerPixelsHeight);
		}
	}
	++s.age;
	s.wasUp = true;
	frame.texts = s.texts;
	frame.textsAt = vr::TextsAbovePage(s.texts, frame.crop);
}

}  // namespace obvr::game

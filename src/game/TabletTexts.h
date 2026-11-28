#pragma once

// The HUD's texts beside the tablet's page, read (vr/TabletTexts.h; spec
// section 31.3): the notices and the subtitles (HUDSubtitleMenu), the
// region's name and the XP line (HUDMainMenu), as the HUD on the hands reads
// them (vr::HudElementRect). Those not on the tablet's face are set into the
// frame to be drawn above the page.

#include "core/Types.h"
#include "vr/HudTileTree.h"
#include "vr/TabletLogic.h"
#include "vr/TabletTexts.h"

namespace obvr::game {

struct TabletTextsState {
	UInt32 age = 0;  // frames since the last read; 0 reads on the next frame
	vr::UiRect texts;     // the last read, in the layer's pixels (vr::TextsBesidePage)
	UInt32 foundSaid = 0xFFFFFFFFu;  // the elements found as last said (a bit each)
	UInt32 foundLines = 20;
	UInt32 tileLines = 2;  // the tiles beside the page listed, the first times the tablet is up
	bool wasUp = false;
	// The two trees, read on the reading frames only, kept here (never allocated).
	vr::HudTile mainTiles[vr::kHudTilesMax];
	vr::HudTile subtitleTiles[vr::kHudTilesMax];
};

// Every few frames while the tablet's menus are up and `wanted` ([Tablet]
// TextsOnTablet, drawn into the eyes): reads the texts' rectangles, keeps
// those beside the face (frame.texts) and where they stand above the page
// (frame.textsAt). The elements found are said as they change, with their
// menu and rectangle; the first times the tablet is up, every sized HUD
// tile beside the page once, so a mod's texts are named in the log.
void ReadTabletTexts(TabletTextsState& state, vr::TabletFrame& frame, float layerPixelsHeight, bool wanted);

}  // namespace obvr::game

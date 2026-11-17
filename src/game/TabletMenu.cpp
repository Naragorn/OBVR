#include "game/TabletMenu.h"

#include "core/Log.h"
#include "game/HudTiles.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// A menu's pictures down to the third level: the vanilla pages have at most
// twenty-odd (tablet-tiles.txt, 2026-11-17).
constexpr UInt32 kPaperTilesMost = 64;

constexpr UInt32 kPages[] = {kMenuIdInventory, kMenuIdStats, kMenuIdMagic, kMenuIdMap};

// The shown page: the one of the four whose root is not hidden and whose
// pictures give a page (game::ReadMenuPaper reads none under a hidden root).
vr::TabletRects ReadShownPage(float screenWidth, UInt32& pageId) {
	vr::PaperTile tiles[kPaperTilesMost];
	for (const UInt32 id : kPages) {
		const UInt32 count = ReadMenuPaper(id, tiles, kPaperTilesMost);
		const vr::TabletRects rects = vr::PaperOf(tiles, count, screenWidth, vr::kHudUiHeightUnits);
		if (rects.valid) {
			pageId = id;
			return rects;
		}
	}
	pageId = kMenuIdNone;
	return vr::TabletRects{};
}

void LogChange(TabletMenuState& state, UInt32 pageId, const vr::TabletFrame& frame, UInt32 popups) {
	const SInt32 crop[4] = {static_cast<SInt32>(frame.crop.left), static_cast<SInt32>(frame.crop.top),
	                        static_cast<SInt32>(frame.crop.right), static_cast<SInt32>(frame.crop.bottom)};
	if (pageId == state.loggedPage && crop[0] == state.loggedCrop[0] && crop[1] == state.loggedCrop[1] &&
	    crop[2] == state.loggedCrop[2] && crop[3] == state.loggedCrop[3]) {
		return;
	}
	state.loggedPage = pageId;
	for (int i = 0; i < 4; ++i) {
		state.loggedCrop[i] = crop[i];
	}
	OBVR_LOG("Tablet: the %s page read - the page at %.0f,%.0f..%.0f,%.0f layer pixels, shown with %u popup(s) "
	         "%d,%d..%d,%d",
	         MenuIdName(pageId), static_cast<double>(frame.paper.left), static_cast<double>(frame.paper.top),
	         static_cast<double>(frame.paper.right), static_cast<double>(frame.paper.bottom), popups, crop[0],
	         crop[1], crop[2], crop[3]);
}

vr::TabletFrame ReadNow(TabletMenuState& state, bool wanted, float layerPixelsWidth, float layerPixelsHeight) {
	vr::TabletFrame frame;
	UInt32 stack[kMenuStackEntries];
	const bool asked = wanted && layerPixelsWidth > 0.0f && layerPixelsHeight > 0.0f && IsMenuMode();
	const UInt32 count = asked ? MenuStackEntries(stack, kMenuStackEntries) : 0;
	if (!vr::TabletMenusUp(stack, count)) {
		state.loggedPage = kMenuIdNone;
		state.loggedMissing = false;
		return frame;
	}
	const float screenWidth = vr::kHudUiHeightUnits * layerPixelsWidth / layerPixelsHeight;
	UInt32 pageId = kMenuIdNone;
	vr::TabletRects rects = ReadShownPage(screenWidth, pageId);
	if (!rects.valid) {
		// A layout whose page cannot be told (a UI mod's): the menus stay on
		// their own panel. Said once a menu.
		if (!state.loggedMissing) {
			state.loggedMissing = true;
			OBVR_LOG("Tablet: no page read from the menus that are up - they stay on their panel");
		}
		return frame;
	}
	UInt32 popups = 0;
	vr::PaperTile tiles[kPaperTilesMost];
	for (UInt32 i = 0; i < count; ++i) {
		if (stack[i] == kMenuIdBigFour || vr::IsHudMenu(stack[i])) {
			continue;
		}
		const UInt32 read = ReadMenuPaper(stack[i], tiles, kPaperTilesMost);
		const vr::UiRect popup = vr::PopupRect(tiles, read, screenWidth, vr::kHudUiHeightUnits);
		vr::JoinPopup(rects, popup);
		popups += popup.valid ? 1u : 0u;
	}
	frame.up = true;
	frame.paper = vr::ToLayerPixels(rects.paper, layerPixelsHeight);
	frame.crop = vr::ToLayerPixels(rects.crop, layerPixelsHeight);
	LogChange(state, pageId, frame, popups);
	return frame;
}

}  // namespace

vr::TabletFrame ReadTabletMenus(TabletMenuState& state, bool wanted, float layerPixelsWidth, float layerPixelsHeight,
                                bool mark) {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long before = ReadPerformanceCounter();
	const vr::TabletFrame frame = ReadNow(state, wanted, layerPixelsWidth, layerPixelsHeight);
	if (frame.up && ticksPerSecond > 0) {
		const double seconds = static_cast<double>(ReadPerformanceCounter() - before) / static_cast<double>(ticksPerSecond);
		state.costSeconds += seconds;
		state.worstSeconds = seconds > state.worstSeconds ? seconds : state.worstSeconds;
		++state.costFrames;
	}
	if (mark && state.costFrames > 0) {
		OBVR_LOG("Tablet: %u frames up since the last mark - the read %.4f ms a frame on average, the worst %.4f ms",
		         state.costFrames, 1000.0 * state.costSeconds / static_cast<double>(state.costFrames),
		         1000.0 * state.worstSeconds);
		state.costSeconds = 0.0;
		state.worstSeconds = 0.0;
		state.costFrames = 0;
	}
	return frame;
}

}  // namespace obvr::game

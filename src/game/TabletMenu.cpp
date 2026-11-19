#include "game/TabletMenu.h"

#include "core/Log.h"
#include "game/HudTiles.h"
#include "game/MenuFigure.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// A menu's pictures down to the third level: the vanilla pages have at most
// twenty-odd (tablet-tiles.txt, 2026-11-17).
constexpr UInt32 kPaperTilesMost = 64;

constexpr UInt32 kFigureReadFrames = 15;

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
	// A new page or popup, or the cut moved by more than a tenth of the page's
	// height: the figure breathes and turns, and every frame of that is not news.
	const SInt32 slack = static_cast<SInt32>(0.1f * (frame.paper.bottom - frame.paper.top));
	bool moved = false;
	for (int i = 0; i < 4; ++i) {
		const SInt32 d = crop[i] - state.loggedCrop[i];
		moved = moved || d > slack || d < -slack;
	}
	if (pageId == state.loggedPage && popups == state.loggedPopups && !moved) {
		return;
	}
	state.loggedPopups = popups;
	state.loggedPage = pageId;
	for (int i = 0; i < 4; ++i) {
		state.loggedCrop[i] = crop[i];
	}
	OBVR_LOG("Tablet: the %s page read - the page at %.0f,%.0f..%.0f,%.0f layer pixels, shown with %u popup(s)%s, the face "
	         "%d,%d..%d,%d",
	         MenuIdName(pageId), static_cast<double>(frame.paper.left), static_cast<double>(frame.paper.top),
	         static_cast<double>(frame.paper.right), static_cast<double>(frame.paper.bottom), popups,
	         frame.figurePage ? " and the figure on its own page"
	         : frame.figureTab ? " and the figure behind a tab"
	                           : (state.figureSeen ? " and the figure" : ""),
	         crop[0],
	         crop[1], crop[2], crop[3]);
}

vr::TabletFrame ReadNow(TabletMenuState& state, bool wanted, float layerPixelsWidth, float layerPixelsHeight,
                        bool inScene, vr::TabletFigure mode) {
	vr::TabletFrame frame;
	UInt32 stack[kMenuStackEntries];
	const bool asked = wanted && layerPixelsWidth > 0.0f && layerPixelsHeight > 0.0f && IsMenuMode();
	const UInt32 count = asked ? MenuStackEntries(stack, kMenuStackEntries) : 0;
	if (!vr::TabletMenusUp(stack, count)) {
		state.figureAge = 0;
		state.loggedPage = kMenuIdNone;
		state.loggedMissing = false;
		return frame;
	}
	const float screenWidth = vr::kHudUiHeightUnits * layerPixelsWidth / layerPixelsHeight;
	UInt32 pageId = kMenuIdNone;
	vr::TabletRects rects = ReadShownPage(screenWidth, pageId);
	state.figureSeen = false;
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
	// The figure (game/MenuFigure.h). The walk over the model's nodes costs
	// about 0.015 ms; the figure only breathes and turns between two of them,
	// so it is read every kFigureReadFrames frames and the last kept between.
	if (state.figureAge == 0 || state.figureAge >= kFigureReadFrames) {
		state.figureRead = ReadMenuFigure(layerPixelsWidth / layerPixelsHeight, state.figure);
		state.figureAge = 0;
	}
	++state.figureAge;
	vr::UiRect figure;
	figure.left = state.figure.left * screenWidth;
	figure.top = state.figure.top * vr::kHudUiHeightUnits;
	figure.right = state.figure.right * screenWidth;
	figure.bottom = state.figure.bottom * vr::kHudUiHeightUnits;
	figure.valid = state.figureRead;
	frame.up = true;
	frame.page = pageId;
	frame.paper = vr::ToLayerPixels(rects.paper, layerPixelsHeight);
	frame.pageCrop = vr::ToLayerPixels(rects.crop, layerPixelsHeight);
	frame.crop = frame.pageCrop;
	// [Tablet] Figure, drawn into the eyes: on a page of its own beside the
	// page - not while a popup is up, which the game draws where that page
	// would stand - or behind a tab (the page shown while a popup is up). The
	// overlay (InScene=0) shows one cut of the layer: with the figure where
	// the game draws it, unless it is off.
	frame.popupUp = popups > 0;
	const vr::UiRect figurePixels = vr::ToLayerPixels(figure, layerPixelsHeight);
	if (inScene && mode == vr::TabletFigure::Beside && popups == 0) {
		vr::SetFigurePage(frame, figurePixels);
	} else if (inScene && mode == vr::TabletFigure::Tab) {
		vr::SetFigureTab(frame, figurePixels);
	} else if (figure.valid && !inScene && mode != vr::TabletFigure::Off) {
		frame.crop = vr::UnionRect(frame.crop, figurePixels);
	}
	state.figureSeen = frame.figurePage || frame.figureTab || (figure.valid && !inScene && mode != vr::TabletFigure::Off);
	LogChange(state, pageId, frame, popups);
	return frame;
}

}  // namespace

vr::TabletFrame ReadTabletMenus(TabletMenuState& state, bool wanted, float layerPixelsWidth, float layerPixelsHeight,
                                bool inScene, vr::TabletFigure mode, bool mark) {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long before = ReadPerformanceCounter();
	const vr::TabletFrame frame = ReadNow(state, wanted, layerPixelsWidth, layerPixelsHeight, inScene, mode);
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

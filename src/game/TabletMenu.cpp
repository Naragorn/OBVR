#include "game/TabletMenu.h"

#include "core/Log.h"
#include "game/BookMenuRead.h"
#include "game/GameAddresses.h"
#include "game/HudTiles.h"
#include "game/MenuFigure.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "platform/Win32Min.h"
#include "vr/TabletBook.h"
#include "vr/TabletClose.h"

namespace obvr::game {
namespace {

// A menu's pictures down to the third level: the vanilla pages have at most
// twenty-odd (tablet-tiles.txt, 2026-11-17).
constexpr UInt32 kPaperTilesMost = 64;

constexpr UInt32 kFigureReadFrames = 15;

constexpr UInt32 kPages[] = {kMenuIdInventory, kMenuIdStats, kMenuIdMagic, kMenuIdMap};

// The traits a box is moved by (xOBSE's GameTiles.h table, as game/HudTiles.cpp
// reads them): its x and y, and the locus the tiles under it count from.
constexpr UInt16 kTraitLocus = 0x0FA6;
constexpr UInt16 kTraitY = 0x0FAC;
constexpr UInt16 kTraitX = 0x0FAD;
// The locus trait's &true; reads 2 (game/HudTiles.cpp).
constexpr float kLocusTrue = 2.0f;

// [R in Oblivion.exe 1.2.416] Tile::UpdateFloat 0x0058CEB0 takes the trait's
// value from 0x0058B220, which makes one when the tile has none, and sets its
// number (0x0058CA00, which tells the tiles reading it). A formula the trait
// has stays: it gives its own value again when what it reads changes, and
// the next frame's shift comes again (vr::ShiftOntoPage).
void WriteTileNumber(const void* tile, UInt16 trait, float value) {
	using UpdateFloatFn = void(__thiscall*)(void* self, UInt32 trait, float value);
	reinterpret_cast<UpdateFloatFn>(addr::kTileUpdateFloat)(const_cast<void*>(tile), trait, value);
}

bool InStack(const UInt32* stack, UInt32 count, UInt32 menuId) {
	for (UInt32 i = 0; i < count; ++i) {
		if (stack[i] == menuId) {
			return true;
		}
	}
	return false;
}

// A box moved onto the page's middle (vr::TabletBoxMoves, vr::ShiftOntoPage):
// its menu's root shifted, which the tiles under it count from when it is a
// locus (game::ReadMenuPaper's rule) - the vanilla boxes' roots all are. A
// root that is no locus is left as the game has it: the tiles under it would
// not follow, and the box would come apart. The place before is kept to put
// back (vr::KeptOriginal). False when nothing was moved.
bool MoveBox(TabletMenuState& state, UInt32 menuId, const vr::BoxShift& shift, const vr::UiRect& box) {
	const void* const root = MenuRootTile(menuId);
	float locus = 0.0f;
	if (root == nullptr || !ReadTileNumber(root, kTraitLocus, locus) || locus != kLocusTrue) {
		if (!state.unmovableSaid) {
			state.unmovableSaid = true;
			OBVR_LOG("Tablet: the %s box stays where the game draws it - its menu's root is no locus, the tiles under it "
			         "would not follow a shift",
			         MenuIdName(menuId));
		}
		return false;
	}
	// No x or y of its own: the root stands at 0, where the tiles under it count from.
	float x = 0.0f;
	float y = 0.0f;
	ReadTileNumber(root, kTraitX, x);
	ReadTileNumber(root, kTraitY, y);
	TabletMovedBox* record = nullptr;
	for (UInt32 i = 0; i < state.movedCount; ++i) {
		if (state.moved[i].menuId == menuId) {
			record = &state.moved[i];
		}
	}
	const bool known = record != nullptr && record->root == root;
	if (record == nullptr) {
		if (state.movedCount >= vr::kTabletPopupsMost) {
			return false;
		}
		record = &state.moved[state.movedCount++];
	}
	record->originalX = vr::KeptOriginal(known, record->originalX, record->writtenX, x);
	record->originalY = vr::KeptOriginal(known, record->originalY, record->writtenY, y);
	record->menuId = menuId;
	record->root = root;
	record->writtenX = x + shift.dx;
	record->writtenY = y + shift.dy;
	WriteTileNumber(root, kTraitX, record->writtenX);
	WriteTileNumber(root, kTraitY, record->writtenY);
	if (state.movedLines > 0) {
		--state.movedLines;
		OBVR_LOG("Tablet: the %s box moved onto the page's middle - by %.0f,%.0f units, from %.0f,%.0f..%.0f,%.0f "
		         "(its root %.0f,%.0f to %.0f,%.0f)",
		         MenuIdName(menuId), static_cast<double>(shift.dx), static_cast<double>(shift.dy),
		         static_cast<double>(box.left), static_cast<double>(box.top), static_cast<double>(box.right),
		         static_cast<double>(box.bottom), static_cast<double>(x), static_cast<double>(y),
		         static_cast<double>(record->writtenX), static_cast<double>(record->writtenY));
	}
	return true;
}

// The boxes moved that are no longer on the tablet's page (`kept` those that
// are): put back where the game had them while the menu is still up and its
// root the one moved, and while our value still stands (vr::PutBack) - a box
// the pages closed under, or the figure set beside meanwhile. A menu gone is
// only forgotten: its tiles are no more.
void PutBackBoxes(TabletMenuState& state, const UInt32* stack, UInt32 count, const UInt32* kept, UInt32 keptCount) {
	for (UInt32 i = state.movedCount; i-- > 0;) {
		const TabletMovedBox& box = state.moved[i];
		if (InStack(kept, keptCount, box.menuId)) {
			continue;
		}
		const void* const root = InStack(stack, count, box.menuId) ? MenuRootTile(box.menuId) : nullptr;
		if (root != nullptr && root == box.root) {
			float x = 0.0f;
			float y = 0.0f;
			const bool backX = ReadTileNumber(root, kTraitX, x) && vr::PutBack(x, box.writtenX);
			const bool backY = ReadTileNumber(root, kTraitY, y) && vr::PutBack(y, box.writtenY);
			if (backX) {
				WriteTileNumber(root, kTraitX, box.originalX);
			}
			if (backY) {
				WriteTileNumber(root, kTraitY, box.originalY);
			}
			if (state.movedLines > 0) {
				--state.movedLines;
				OBVR_LOG("Tablet: the %s box put back where the game had it - no longer on the tablet's page%s",
				         MenuIdName(box.menuId), backX && backY ? "" : " (the game had set its own place meanwhile)");
			}
		} else if (state.movedLines > 0) {
			--state.movedLines;
			OBVR_LOG("Tablet: the %s box gone from the page", MenuIdName(box.menuId));
		}
		state.moved[i] = state.moved[state.movedCount - 1];
		--state.movedCount;
	}
}

// The shown page: the one of the four whose root is not hidden and whose
// pictures give a page (game::ReadMenuPaper reads none under a hidden root),
// with its own info box beside it where it has one (vr::PagePopupTileId).
vr::TabletRects ReadShownPage(float screenWidth, UInt32& pageId) {
	vr::PaperTile tiles[kPaperTilesMost];
	for (const UInt32 id : kPages) {
		const UInt32 count = ReadMenuPaper(id, tiles, kPaperTilesMost, vr::PagePopupTileId(id));
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

// An info box beside the page, said as it comes and goes: the item info
// (vr::TabletFaceMenus) or the page's own (vr::PagePopupTileId).
void LogInfoBox(bool& seen, UInt32& linesLeft, bool shown, const char* what, const vr::TabletRects& rects) {
	if (shown == seen) {
		return;
	}
	seen = shown;
	if (linesLeft == 0) {
		return;
	}
	--linesLeft;
	OBVR_LOG("Tablet: %s %s - the face %.0f..%.0f units across", what, shown ? "shown beside the page" : "gone",
	         static_cast<double>(rects.crop.left), static_cast<double>(rects.crop.right));
}

// The menus over the page joined to its face (vr::TabletFaceMenus), each
// one's pictures a popup. With the boxes on the page (vr::BoxesOnPage) a box
// that waits for an answer is moved by the game onto the page's middle
// first; this frame's read was of its place before, so it is joined where it
// now stands. A box moved before and no longer on the page is put back.
// Returns how many popups were read (more than are kept, too); `itemInfo`
// whether the item info was among them.
UInt32 JoinFaceMenus(TabletMenuState& state, const UInt32* stack, UInt32 count, float screenWidth, bool onPage,
                     vr::TabletRects& rects, bool& itemInfo) {
	UInt32 kept[kMenuStackEntries + 1];
	UInt32 keptCount = 0;
	vr::PaperTile tiles[kPaperTilesMost];
	UInt32 faces[kMenuStackEntries + 1];
	const UInt32 faceCount = vr::TabletFaceMenus(stack, count, faces, kMenuStackEntries + 1);
	UInt32 popups = 0;
	for (UInt32 i = 0; i < faceCount; ++i) {
		const UInt32 read = ReadMenuPaper(faces[i], tiles, kPaperTilesMost);
		vr::UiRect popup = vr::PopupRect(tiles, read, screenWidth, vr::kHudUiHeightUnits);
		if (onPage && popup.valid && vr::TabletBoxMoves(faces[i])) {
			kept[keptCount++] = faces[i];
			const vr::BoxShift shift = vr::ShiftOntoPage(popup, rects.paper, screenWidth, vr::kHudUiHeightUnits);
			if (shift.move && MoveBox(state, faces[i], shift, popup)) {
				popup.left += shift.dx;
				popup.right += shift.dx;
				popup.top += shift.dy;
				popup.bottom += shift.dy;
			}
		}
		const bool info = faces[i] == kMenuIdMagicPopup;
		vr::JoinPopup(rects, popup, info);
		popups += popup.valid ? 1u : 0u;
		itemInfo = itemInfo || (popup.valid && info);
	}
	PutBackBoxes(state, stack, count, kept, keptCount);
	return popups;
}

// The figure (game/MenuFigure.h), in HUD units. The walk over the model's
// nodes costs about 0.015 ms; the figure only breathes and turns between two
// of them, so it is read every kFigureReadFrames frames and the last kept
// between.
vr::UiRect ReadFigure(TabletMenuState& state, float screenWidth, float aspect) {
	if (state.figureAge == 0 || state.figureAge >= kFigureReadFrames) {
		state.figureRead = ReadMenuFigure(aspect, state.figure);
		state.figureAge = 0;
	}
	++state.figureAge;
	vr::UiRect figure;
	figure.left = state.figure.left * screenWidth;
	figure.top = state.figure.top * vr::kHudUiHeightUnits;
	figure.right = state.figure.right * screenWidth;
	figure.bottom = state.figure.bottom * vr::kHudUiHeightUnits;
	figure.valid = state.figureRead;
	return figure;
}

// The frame from the rectangles read, in the layer's pixels: the page, the
// face as the game lays it out, and each popup where the game draws it.
void FillFrame(vr::TabletFrame& frame, const vr::TabletRects& rects, UInt32 pageId, float layerPixelsHeight) {
	frame.up = true;
	frame.page = pageId;
	frame.paper = vr::ToLayerPixels(rects.paper, layerPixelsHeight);
	frame.pageCrop = vr::ToLayerPixels(rects.crop, layerPixelsHeight);
	frame.crop = frame.pageCrop;
	frame.outline = vr::ToLayerPixels(rects.outline, layerPixelsHeight);
	frame.popupCount = rects.popupCount;
	frame.popupsOverflow = rects.popupsOverflow;
	for (UInt32 i = 0; i < rects.popupCount; ++i) {
		frame.popups[i] = vr::ToLayerPixels(rects.popups[i], layerPixelsHeight);
	}
}

// A book, a note or a letter read alone - no F1-F4 entry under it
// (docs/inventory-tablet-spec.md 25.4): its own pictures are the page
// (vr::BookPaperOf), its look from its user0 (game/BookMenuRead.h); no
// figure. A box raised over it (a script's message) joins it as over the
// pages. Said as its look changes.
vr::TabletFrame ReadBookAlone(TabletMenuState& state, const UInt32* stack, UInt32 count, float screenWidth,
                              float layerPixelsHeight, bool onPage) {
	vr::TabletFrame frame;
	vr::PaperTile tiles[kPaperTilesMost];
	const UInt32 read = ReadMenuPaper(kMenuIdBook, tiles, kPaperTilesMost);
	vr::TabletRects rects = vr::BookPaperOf(tiles, read, screenWidth, vr::kHudUiHeightUnits);
	UInt32 over[kMenuStackEntries];
	UInt32 overCount = 0;
	for (UInt32 i = 0; i < count; ++i) {
		if (stack[i] != kMenuIdBook) {
			over[overCount++] = stack[i];
		}
	}
	bool itemInfo = false;
	JoinFaceMenus(state, over, overCount, screenWidth, onPage && rects.valid, rects, itemInfo);
	if (!rects.valid) {
		if (!state.loggedMissing) {
			state.loggedMissing = true;
			OBVR_LOG("Tablet: no page read from the book menu - it stays on the menus' panel");
		}
		return frame;
	}
	FillFrame(frame, rects, kMenuIdBook, layerPixelsHeight);
	frame.popupUp = vr::BoxOverPage(rects);
	frame.bookScroll = ReadBookMenu().scroll;
	if (state.loggedPage != kMenuIdBook || state.loggedBookScroll != frame.bookScroll) {
		state.loggedPage = kMenuIdBook;
		state.loggedBookScroll = frame.bookScroll;
		OBVR_LOG("Tablet: a %s read alone, held in the hand - its paper at %.0f,%.0f..%.0f,%.0f layer pixels, the face "
		         "%.0f,%.0f..%.0f,%.0f",
		         frame.bookScroll ? "letter (the book menu's scroll look)" : "book (the book menu's book look)",
		         static_cast<double>(frame.paper.left), static_cast<double>(frame.paper.top),
		         static_cast<double>(frame.paper.right), static_cast<double>(frame.paper.bottom),
		         static_cast<double>(frame.crop.left), static_cast<double>(frame.crop.top),
		         static_cast<double>(frame.crop.right), static_cast<double>(frame.crop.bottom));
	}
	return frame;
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
		PutBackBoxes(state, stack, count, nullptr, 0);
		frame.menusGone = asked && vr::OnlyHudMenus(stack, count);
		return frame;
	}
	const float screenWidth = vr::kHudUiHeightUnits * layerPixelsWidth / layerPixelsHeight;
	if (!InStack(stack, count, kMenuIdBigFour)) {
		return ReadBookAlone(state, stack, count, screenWidth, layerPixelsHeight, vr::BoxesOnPage(inScene, mode));
	}
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
		PutBackBoxes(state, stack, count, nullptr, 0);
		return frame;
	}
	// The page's own info box is a popup already (vr::PaperOf).
	const UInt32 own = rects.popupCount;
	LogInfoBox(state.ownInfoSeen, state.ownInfoLines, own > 0, "the page's own info box", rects);
	bool itemInfo = false;
	const UInt32 popups =
		own + JoinFaceMenus(state, stack, count, screenWidth, vr::BoxesOnPage(inScene, mode), rects, itemInfo);
	LogInfoBox(state.itemInfoSeen, state.itemInfoLines, itemInfo, "the item info (MagicPopup)", rects);
	const vr::UiRect figure = ReadFigure(state, screenWidth, layerPixelsWidth / layerPixelsHeight);
	FillFrame(frame, rects, pageId, layerPixelsHeight);
	// [Tablet] Figure, drawn into the eyes: on a page of its own beside the
	// page - not while a popup is up, which the game draws where that page
	// would stand - or behind a tab (the page shown while a box is up). The
	// overlay (InScene=0) shows one cut of the layer: with the figure where
	// the game draws it, unless it is off.
	frame.popupUp = vr::BoxOverPage(rects);
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
	if (mark) {
		LogBookMenuAtMark();
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

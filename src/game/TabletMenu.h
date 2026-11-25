#pragma once

// The tablet's menus as the game has them this frame
// (docs/inventory-tablet-spec.md, 4.1 and 4.2): whether the menus that are up
// are the tablet's, and the rectangle of the interface's picture they take -
// the shown page of the F1-F4 menus with whatever is raised over it - read
// from the menus' own tiles, so a UI mod's layout is followed and none is
// assumed. The decisions are vr/TabletLogic.h's; this reads.

#include "core/Types.h"
#include "render/MenuModelFit.h"
#include "vr/TabletBoxes.h"
#include "vr/TabletLogic.h"

namespace obvr::game {

// A box moved onto the page's middle (vr::TabletBoxMoves): its menu, the root
// tile shifted, the place the game had (to put back when the box is no longer
// on the page) and the one written.
struct TabletMovedBox {
	UInt32 menuId = 0;
	const void* root = nullptr;
	float originalX = 0.0f;
	float originalY = 0.0f;
	float writtenX = 0.0f;
	float writtenY = 0.0f;
};

// What a menu episode remembers of the tablet: the last page and crop
// logged, so a change is said once.
struct TabletMenuState {
	UInt32 loggedPage = 0;
	SInt32 loggedCrop[4] = {};
	UInt32 loggedPopups = 0;
	bool loggedMissing = false;
	bool figureSeen = false;  // the figure joined this frame's crop
	// The item info (MagicPopup) beside the page as last said, and the lines left to say it.
	bool itemInfoSeen = false;
	UInt32 itemInfoLines = 12;
	// The same for the page's own info box (the stats menu's, vr::PagePopupTileId).
	bool ownInfoSeen = false;
	UInt32 ownInfoLines = 12;
	// The boxes on the page (vr/TabletBoxes.h): the boxes the game was moved
	// to draw on the page's middle, the lines left to say a move, and whether
	// a box that could not be moved was said.
	TabletMovedBox moved[vr::kTabletPopupsMost];
	UInt32 movedCount = 0;
	UInt32 movedLines = 12;
	bool unmovableSaid = false;
	// The figure's rectangle as last read (game/MenuFigure.h), and how many
	// frames ago; 0 reads it on the next frame.
	render::FigureRect figure;
	bool figureRead = false;
	UInt32 figureAge = 0;
	// The read's cost between two hand-script marks (said at the mark).
	double costSeconds = 0.0;
	double worstSeconds = 0.0;
	UInt32 costFrames = 0;
};

// What the hand mode is told (vr::TabletFrame's `up`, `paper` and `crop`, in
// the layer's pixels): nothing unless `wanted`, the game is in menu mode,
// the menu stack is the tablet's (vr::TabletMenusUp) and the shown page's
// rectangle could be read. `layerPixelsWidth` by `layerPixelsHeight` is what
// the interface draws into. Read every frame the menus are up - the page
// changes with a tab, a popup comes and goes; logged at each change.
// `inScene` and `mode`: the tablet drawn into the eyes, and where the figure
// is on it ([Tablet] Figure: vr::SetFigurePage, vr::SetFigureTab); the
// overlay's cut holds it where the game draws it, unless it is off.
// `mark`: a hand-script mark this frame - the read's cost since the last is
// said. With the boxes on the page (vr::BoxesOnPage) a box that waits for an
// answer is moved by the game onto the page's middle - its menu's root tile
// shifted, and put back when the box is no longer on the page.
vr::TabletFrame ReadTabletMenus(TabletMenuState& state, bool wanted, float layerPixelsWidth,
                                float layerPixelsHeight, bool inScene, vr::TabletFigure mode,
                                bool mark = false);

}  // namespace obvr::game

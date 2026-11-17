#pragma once

// The tablet's menus as the game has them this frame
// (docs/inventory-tablet-spec.md, 4.1 and 4.2): whether the menus that are up
// are the tablet's, and the rectangle of the interface's picture they take -
// the shown page of the F1-F4 menus with whatever is raised over it - read
// from the menus' own tiles, so a UI mod's layout is followed and none is
// assumed. The decisions are vr/TabletLogic.h's; this reads.

#include "core/Types.h"
#include "render/MenuModelFit.h"
#include "vr/TabletLogic.h"

namespace obvr::game {

// What a menu episode remembers of the tablet: the last page and crop
// logged, so a change is said once.
struct TabletMenuState {
	UInt32 loggedPage = 0;
	SInt32 loggedCrop[4] = {};
	UInt32 loggedPopups = 0;
	bool loggedMissing = false;
	bool figureSeen = false;  // the figure joined this frame's crop
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
// `figurePage`: the figure on a page of its own beside the page (the tablet
// drawn into the eyes); otherwise the cut holds it where the game draws it.
// `mark`: a hand-script mark this frame - the read's cost since the last is
// said.
vr::TabletFrame ReadTabletMenus(TabletMenuState& state, bool wanted, float layerPixelsWidth,
                                float layerPixelsHeight, bool figurePage, bool mark = false);

}  // namespace obvr::game

#pragma once

// The menu's figure - the paperdoll the F1-F4 pages draw beside them - as a
// rectangle of the screen, for the tablet's cut (vr/TabletLogic.h; Nadi,
// 2026-11-17: the figure on the tablet beside the page). Read from the model
// the interface manager hangs on its holder (manager+0x60, as
// render/InterfaceRenderHook.cpp FitMenuModelHolder reads it): every node's
// place and every small bound (a weapon's), seen by the menu's own view
// (render/MenuModelFit.h FigureScreenRect).

#include "render/MenuModelFit.h"

namespace obvr::game {

// The figure's rectangle in fractions of the screen; false without a model on
// the holder or before the menu's view has been read. `aspect` is the
// screen's width over its height.
bool ReadMenuFigure(float aspect, render::FigureRect& out);

}  // namespace obvr::game

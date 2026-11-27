#pragma once

// The figure's rectangle held steady for the tablet (docs/inventory-tablet-
// spec.md 26.1; Nadi, Windows 2026-11-27: the edge round the figure on its
// own page flickers, and the figure behind its tab too).
//
// The rectangle is read from the figure's bones every 15 frames
// (game::ReadMenuFigure), and the figure breathes: that run's reads were
// 0.545..0.913, 0.544..0.913, 0.545..0.913 of the screen. The figure's page
// and its tab are laid out from the rectangle (vr::SetFigurePage: the
// page's right edge and the face's width follow its width; vr::SetFigureTab:
// the figure is fitted into the page by it), so each read that differs by a
// pixel moved the page's edge or scaled the whole figure anew, six times a
// second. Here the rectangle taken is kept, a little wider than read, for as
// long as the reads stay inside it and are not much smaller; a figure that
// is turned or changed outgrows or leaves it and is taken anew. Pure;
// tablet_figure_steady_test.

#include "vr/HudTileTree.h"

namespace obvr::vr {

// Of the read's height: added on every side when a rectangle is taken (the
// breath's room), and how far an edge may stand inside the kept one before
// the figure counts as smaller.
inline constexpr float kFigureSteadyPad = 0.015f;
inline constexpr float kFigureSteadySlack = 0.06f;

struct FigureSteadyState {
	UiRect held;
};

// `read`: the figure's rectangle as read this frame, `bounds` the screen it
// lies in (same units). The rectangle for the tablet: the kept one while the
// read fits it, else the read with its room, inside the bounds. A read that
// is not valid is passed on as it is and the kept one stays.
inline UiRect StepFigureSteady(FigureSteadyState& s, const UiRect& read, const UiRect& bounds) {
	const float height = read.bottom - read.top;
	if (!read.valid || !(height > 0.0f) || !(read.right > read.left)) {
		return read;
	}
	if (s.held.valid) {
		const float slack = kFigureSteadySlack * height;
		const bool inside = read.left >= s.held.left && read.top >= s.held.top && read.right <= s.held.right &&
		                    read.bottom <= s.held.bottom;
		const bool smaller = read.left - s.held.left > slack || read.top - s.held.top > slack ||
		                     s.held.right - read.right > slack || s.held.bottom - read.bottom > slack;
		if (inside && !smaller) {
			return s.held;
		}
	}
	const float pad = kFigureSteadyPad * height;
	UiRect taken = read;
	taken.left -= pad;
	taken.top -= pad;
	taken.right += pad;
	taken.bottom += pad;
	if (bounds.valid) {
		taken.left = taken.left < bounds.left ? bounds.left : taken.left;
		taken.top = taken.top < bounds.top ? bounds.top : taken.top;
		taken.right = taken.right > bounds.right ? bounds.right : taken.right;
		taken.bottom = taken.bottom > bounds.bottom ? bounds.bottom : taken.bottom;
	}
	s.held = taken;
	return taken;
}

}  // namespace obvr::vr

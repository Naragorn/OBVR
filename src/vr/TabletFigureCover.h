#pragma once

// Whether the page's own pictures lie over the figure's place
// (docs/inventory-tablet-spec.md 33; Nadi, 2026-11-28: "wenn ich die
// spielfigur als tab habe, und im inv das karten menü ausgewählt habe, dann
// clippt die karte in die fläche für die spielfigur hinein").
//
// The figure on the tablet - behind its tab (vr::SetFigureTab) or on its own
// page (vr::SetFigurePage) - is a cut of the game's screen where the figure
// stands (game::ReadMenuFigure). Every page but the map stays inside its
// paper, left of that place. The map's extension (567x613 HUD units beside
// the 745x792 paper, tablet-tiles.txt 2026-11-17) reaches to the right over
// it, so on the map page the cut showed the map, and the figure's tab showed
// the map where the figure belongs. Where the page lies over that place there
// is no figure to cut: none is shown there, and the figure's tab waits for a
// page that leaves it free (vr::StepTabletTab's `figureAway`). Pure;
// tablet_figure_cover_test.

#include "vr/HudTileTree.h"

namespace obvr::vr {

// The share of the figure's rectangle the page must lie over before it
// counts as covered: the map's extension lies over most of it, while a few
// pixels of a page's picture past its paper are no cover.
inline constexpr float kFigureCoverShare = 0.1f;

// The area two rectangles share; 0 when either is invalid or they do not overlap.
inline float SharedArea(const UiRect& a, const UiRect& b) {
	if (!a.valid || !b.valid) {
		return 0.0f;
	}
	const float left = a.left > b.left ? a.left : b.left;
	const float right = a.right < b.right ? a.right : b.right;
	const float top = a.top > b.top ? a.top : b.top;
	const float bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
	return right > left && bottom > top ? (right - left) * (bottom - top) : 0.0f;
}

// Whether the page reaches past its paper over the figure's place: the parts
// of `outline` (the page with what reaches beyond it, without popups:
// vr::TabletRects::outline) outside `paper` - a band on each side, the left
// and right ones the outline's full height - lie over at least
// kFigureCoverShare of `figure`. All three in the same units. False when any
// of them is invalid or empty.
inline bool PageOverFigure(const UiRect& paper, const UiRect& outline, const UiRect& figure) {
	if (!paper.valid || !outline.valid || !figure.valid) {
		return false;
	}
	const float figureArea = (figure.right - figure.left) * (figure.bottom - figure.top);
	if (!(figure.right > figure.left) || !(figure.bottom > figure.top) || !(figureArea > 0.0f)) {
		return false;
	}
	UiRect bands[4];
	for (UiRect& band : bands) {
		band = outline;
	}
	bands[0].right = paper.left;   // left of the paper
	bands[1].left = paper.right;   // right of the paper
	bands[2].left = paper.left;    // above it
	bands[2].right = paper.right;
	bands[2].bottom = paper.top;
	bands[3].left = paper.left;    // below it
	bands[3].right = paper.right;
	bands[3].top = paper.bottom;
	float covered = 0.0f;
	for (const UiRect& band : bands) {
		covered += SharedArea(band, figure);
	}
	return covered >= kFigureCoverShare * figureArea;
}

}  // namespace obvr::vr

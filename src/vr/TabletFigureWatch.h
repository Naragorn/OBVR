#pragma once

// What changes under the tablet's figure from frame to frame, for the log
// (docs/inventory-tablet-spec.md 26.1; Nadi, Windows 2026-11-27: the edge
// round the figure on its own page flickers, and the figure behind its tab
// too). Not pinned by that run's log: the figure's rectangle is read every
// 15 frames from the figure's bones (game::ReadMenuFigure) and its page and
// its tab follow it, and the log said only the first three reads. This
// counts, over kFigureWatchFrames frames with the figure wanted, the frames
// its rectangle moved and by how far, and the frames it was not read (the
// tablet showed the page alone then); said only when something moved, a
// few dozen times a session. Pure; tablet_figure_watch_test.

#include "core/Types.h"
#include "vr/TabletLogic.h"

namespace obvr::vr {

inline constexpr UInt32 kFigureWatchFrames = 90;
inline constexpr UInt32 kFigureWatchLines = 40;

struct FigureWatchState {
	UiRect last;
	bool seen = false;        // a frame with the figure wanted before this one
	UInt32 frames = 0;
	UInt32 moves = 0;         // frames its rectangle moved by a layer pixel or more
	UInt32 drops = 0;         // frames it was wanted and not read
	float mostPixels = 0.0f;  // the largest of those moves, along any edge
	UInt32 linesLeft = kFigureWatchLines;
};

struct FigureWatchReport {
	bool say = false;
	UInt32 frames = 0;
	UInt32 moves = 0;
	UInt32 drops = 0;
	float mostPixels = 0.0f;
};

inline float FigureEdgeMove(const UiRect& a, const UiRect& b) {
	const float d[4] = {a.left - b.left, a.top - b.top, a.right - b.right, a.bottom - b.bottom};
	float most = 0.0f;
	for (float v : d) {
		const float m = v < 0.0f ? -v : v;
		most = m > most ? m : most;
	}
	return most;
}

// One frame: `wanted` the figure set to show on the tablet ([Tablet] Figure
// beside or tab, drawn into the eyes) with the pages up; `figure` its
// rectangle in the layer's pixels this frame (not valid: not read).
inline FigureWatchReport StepFigureWatch(FigureWatchState& s, bool wanted, const UiRect& figure) {
	FigureWatchReport r;
	if (!wanted) {
		const UInt32 lines = s.linesLeft;
		s = FigureWatchState{};
		s.linesLeft = lines;
		return r;
	}
	++s.frames;
	if (!figure.valid) {
		++s.drops;
	} else if (s.seen && s.last.valid) {
		const float moved = FigureEdgeMove(figure, s.last);
		if (moved >= 1.0f) {
			++s.moves;
			s.mostPixels = moved > s.mostPixels ? moved : s.mostPixels;
		}
	}
	s.last = figure;
	s.seen = true;
	if (s.frames < kFigureWatchFrames) {
		return r;
	}
	r.say = (s.moves > 0 || s.drops > 0) && s.linesLeft > 0;
	r.frames = s.frames;
	r.moves = s.moves;
	r.drops = s.drops;
	r.mostPixels = s.mostPixels;
	s.linesLeft -= r.say ? 1u : 0u;
	s.frames = 0;
	s.moves = 0;
	s.drops = 0;
	s.mostPixels = 0.0f;
	return r;
}

}  // namespace obvr::vr

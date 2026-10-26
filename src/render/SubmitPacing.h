#pragma once

#include "core/Types.h"

namespace obvr::render {

// When a finished frame may be handed to the compositor.
//
// SteamVR wakes the application at "running start", about 3 ms before a
// vsync, with the poses for the frame after it. A frame that is submitted
// before that vsync is taken for the one just ahead - the low-latency path
// meant for applications that finish inside those 3 ms; one submitted after
// it is shown a vsync later, the ordinary one-frame pipeline. An application
// that finishes right at the vsync flips between the two, and every flip
// costs a frame: measured 2026-10-19 with alternate eye rendering in an
// empty cell (1.8 ms of work a frame, the submit 1 to 3 ms before the
// vsync), every other frame took two vsyncs, the compositor counted a
// mis-presented frame and a dropped one for each flip, 17 to 50 % of the
// frames; with five people in view (the submit 6 ms after the vsync) and
// under the dual pass (3.5 ms of work, the submit just after the vsync) the
// pipeline stood still. The compositor's own frame timing (compositor.csv)
// showed it - newPosesReadyMs, newFrameReadyMs, numMisPresented.
//
// So the submit is held until the vsync has passed by a margin: the frame
// costs the latency it would have had anyway in any scene with people in
// it, and the rate stops flipping.
//
// Where the vsync is: the compositor says of every finished frame how long
// before its vsync it handed the poses out (newPosesReadyMs, -3.0 to -3.3 ms
// here, -7 under load), and OBVR knows the tick WaitGetPoses returned on.
// The vsync of the frame being rendered is that tick plus the last finished
// frame's running start. Two other sources were tried first (2026-10-19):
// the record of the frame in flight, which right after WaitGetPoses is
// often still the previous frame's, so its vsync lay a period back and
// nothing was ever held; and GetFrameTimeRemaining, which at the poses
// moment reads about one period - the compositor's frame runs from running
// start to running start, so it says nothing about the vsync.
constexpr double kSubmitPacingMarginMs = 0.5;
// The hold covers the running start window and the margin, no more. A frame
// that would wait longer is early by more than a running start: nothing
// flips for it, and a hold only adds latency - or, when the vsync estimate
// is off by a period, a period. Nadi's Windows game (Pimax Dream Air, 90 Hz,
// 2026-10-25) had the cap at 10 ms and held every frame about 9.3 ms.
constexpr double kSubmitPacingMaxHoldMs = 4.0;
// A running start longer than this is not one. SteamVR's is about 3 ms, 7 ms
// was seen under load (the Beyond, 75 Hz). The Pimax Dream Air's compositor
// at 90 Hz reported 10.97 ms on every frame - the whole 11.11 ms period: it
// hands the poses out right after a vsync, so a frame is never early, and
// the old cap of 12 ms let that through (docs/performance-spec.md, the
// Windows run of 2026-10-25). Below any headset's period from 72 Hz up.
constexpr double kSubmitPacingRunningStartMaxMs = 7.5;

// Milliseconds from the poses to the vsync, from the compositor's
// newPosesReadyMs of a finished frame (negative = before its vsync);
// negative when that is not a running start - at or after the vsync, or
// longer than kSubmitPacingRunningStartMaxMs.
inline double MsToVsyncFromRunningStart(float newPosesReadyMs) {
	if (!(newPosesReadyMs < 0.0f)) {
		return -1.0;
	}
	const double runningStart = -static_cast<double>(newPosesReadyMs);
	if (runningStart > kSubmitPacingRunningStartMaxMs) {
		return -1.0;
	}
	return runningStart;
}

// Ticks to wait before submitting, or 0 to submit now: when the vsync is
// unknown, already past the margin, or the hold implausibly long.
inline UInt64 SubmitHoldTicks(UInt64 nowTick, UInt64 vsyncTick, bool vsyncKnown, UInt64 frequency,
                              double marginMs = kSubmitPacingMarginMs) {
	if (!vsyncKnown || frequency == 0) {
		return 0;
	}
	const double ticksPerMs = static_cast<double>(frequency) / 1000.0;
	const double target = static_cast<double>(vsyncTick) + marginMs * ticksPerMs;
	const double now = static_cast<double>(nowTick);
	if (now >= target) {
		return 0;
	}
	const double hold = target - now;
	if (hold > kSubmitPacingMaxHoldMs * ticksPerMs) {
		return 0;
	}
	return static_cast<UInt64>(hold);
}

}  // namespace obvr::render

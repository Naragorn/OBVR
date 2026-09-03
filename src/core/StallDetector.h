#pragma once

#include "core/Types.h"

namespace obvr::watchdog {

// The decision half of the hang watchdog, kept apart from the thread that
// runs it so every flow can be exercised without one.
//
// Why it exists: the third-person hang (task #32) ended three sessions with
// a log whose last line was whatever happened to be written last, and the
// step trace that names the blocking calls is armed for thirty frames after
// a view switch only. A thread that watches the frame counter can say, for
// any hang, how long the frames have been standing still and which step the
// render thread reached last - once, when the stall is first declared, and
// again only after frames have moved in between.
struct StallDetector {
	UInt32 lastCount = 0;
	UInt32 quietTicks = 0;
	bool everMoved = false;  // frames have ended at least once
	bool reported = false;   // the current stall was declared already
};

enum class StallVerdict {
	Quiet,    // frames have never ended yet: the game is still starting
	Moving,   // the counter advanced since the last tick
	Stalled,  // the counter has stood still for the given ticks - declare it
	Standing  // still stalled, already declared
};

// One tick of the watchdog, with the frame counter's current reading.
inline StallVerdict Tick(StallDetector& d, UInt32 count, UInt32 stallTicks) {
	if (count != d.lastCount) {
		d.lastCount = count;
		d.quietTicks = 0;
		d.everMoved = true;
		d.reported = false;
		return StallVerdict::Moving;
	}
	if (!d.everMoved) {
		return StallVerdict::Quiet;
	}
	if (d.quietTicks < stallTicks) {
		++d.quietTicks;
	}
	if (d.quietTicks >= stallTicks && !d.reported) {
		d.reported = true;
		return StallVerdict::Stalled;
	}
	return d.reported ? StallVerdict::Standing : StallVerdict::Quiet;
}

}  // namespace obvr::watchdog

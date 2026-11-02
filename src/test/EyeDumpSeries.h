#pragma once

// A run of eye pictures, one a frame, from a hand-script mark on
// ([Debug] EyeDumpFrames): the frames in a row that a picture every mark
// cannot show - what a reflection does from one frame to the next while the
// head turns (the water's "reset" of 2026-10-22). The mark's frame is the
// first of the run (f00), then as many frames as asked, named
// <mark>-f<NN>; a new mark starts a new run from its own frame.

#include <cstdio>

#include "core/Types.h"

namespace obvr::test {

struct EyeDumpSeries {
	UInt32 left = 0;   // frames still to write after this one
	UInt32 index = 0;  // the frame about to be written, from 0
	char mark[64] = "";
};

// Steps the run by one frame. True when this frame is to be written, with
// its name in nameOut. `frames` is the key: 0 writes nothing, ever.
inline bool StepEyeDumpSeries(EyeDumpSeries& series, bool marked, const char* markName,
                              UInt32 frames, char* nameOut, size_t nameSize) {
	if (marked && frames > 0 && markName != nullptr && markName[0] != '\0') {
		std::snprintf(series.mark, sizeof(series.mark), "%s", markName);
		series.index = 0;
		series.left = frames;
	}
	if (series.left == 0) {
		return false;
	}
	std::snprintf(nameOut, nameSize, "%s-f%02u", series.mark, series.index);
	++series.index;
	--series.left;
	return true;
}

}  // namespace obvr::test

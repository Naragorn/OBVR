#pragma once

// The hand fit's read measured (game/FittedCapsules.h ReadHandVerticesNow,
// every kHandFitEveryFrames with a hand's shape out): its own time, the
// skinned shapes it read and the memory probes it asked of the system, said
// once every so many reads. The read was the one part of the hands' step the
// contact cost line did not time - and it was the part that stalled a frame
// (Windows capture 1-45 of 2026-11-13: 13 to 19 ms every 15 frames,
// docs/performance-spec.md). Pure, hand_read_cost_test.

#include "core/Types.h"

namespace obvr::game {

struct HandReadCostWindow {
	UInt32 reads = 0;
	double sumMicros = 0.0;
	double maxMicros = 0.0;
	UInt32 shapes = 0;  // the skinned shapes read, in all
	UInt32 probes = 0;  // the memory probes asked of the system, in all
};

// One read added; true when the window is full and its line is due (the
// caller says it and starts a new window). A time that is no number or below
// zero is not counted; readsPerLine 0 is never due.
inline bool AddHandReadCost(HandReadCostWindow& w, double micros, UInt32 shapes, UInt32 probes, UInt32 readsPerLine) {
	if (!(micros >= 0.0)) {
		return false;
	}
	++w.reads;
	w.sumMicros += micros;
	w.maxMicros = micros > w.maxMicros ? micros : w.maxMicros;
	w.shapes += shapes;
	w.probes += probes;
	return readsPerLine > 0 && w.reads >= readsPerLine;
}

inline double HandReadCostMean(const HandReadCostWindow& w) { return w.reads > 0 ? w.sumMicros / w.reads : 0.0; }
inline double HandReadShapesMean(const HandReadCostWindow& w) {
	return w.reads > 0 ? static_cast<double>(w.shapes) / w.reads : 0.0;
}
inline double HandReadProbesMean(const HandReadCostWindow& w) {
	return w.reads > 0 ? static_cast<double>(w.probes) / w.reads : 0.0;
}

}  // namespace obvr::game

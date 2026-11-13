#pragma once

// What a hand-script mark writes as pictures: the two eye pictures
// ([Debug] EyeDump, OBVR-Eye-Left/Right-<mark>.bmp) and the 2D layer the
// headset is handed for menus and the HUD ([Debug] LayerDump,
// OBVR-Layer-<mark>.bmp) - the layer is where the inventory's paperdoll is
// drawn on a VR frame, so the eye pictures, taken behind the overlays, never
// show it. Each at a divisor-th of its size ([Debug] EyeDumpDivisor, 4 by
// default: a quarter, as the eye pictures always were; 1 is full size, for
// counting fingers on a hand held close; at most 4).

#include "core/Types.h"

namespace obvr::test {

struct MarkDumpPlan {
	bool eyes = false;
	bool layer = false;
	UInt32 divisor = 4;
};

inline constexpr UInt32 kMarkDumpDefaultDivisor = 4;
inline constexpr UInt32 kMarkDumpMostDivisor = 4;

// 0 (or a key left out) is the default quarter; past 4 held at 4, the
// smallest the BMP writer takes (render/LayoutProbe.h DumpSurfaceBmp).
inline UInt32 MarkDumpDivisor(UInt32 setting) {
	if (setting == 0) {
		return kMarkDumpDefaultDivisor;
	}
	return setting > kMarkDumpMostDivisor ? kMarkDumpMostDivisor : setting;
}

inline MarkDumpPlan PlanMarkDump(bool marked, bool eyeDump, bool layerDump, bool layerThere,
                                 UInt32 divisorSetting) {
	MarkDumpPlan plan;
	plan.divisor = MarkDumpDivisor(divisorSetting);
	if (!marked) {
		return plan;
	}
	plan.eyes = eyeDump;
	plan.layer = layerDump && layerThere;
	return plan;
}

// What the log says after "the eye pictures at the mark": "written" whenever
// the eyes were asked and written, so the scripts' old expectation holds
// with the layer asked as well.
inline const char* MarkDumpWords(const MarkDumpPlan& plan, bool eyesWritten, bool layerWritten) {
	if (!plan.eyes) {
		return layerWritten ? "not asked, the layer written" : "not asked, the layer refused";
	}
	if (!plan.layer) {
		return eyesWritten ? "written" : "refused";
	}
	if (eyesWritten) {
		return layerWritten ? "written, the layer written" : "written, the layer refused";
	}
	return layerWritten ? "refused, the layer written" : "refused, the layer refused";
}

}  // namespace obvr::test

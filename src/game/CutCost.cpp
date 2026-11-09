#include "platform/Win32Min.h"

#include "core/Log.h"
#include "game/Dismember.h"

namespace obvr::game {

void StepCutsMeasured(const CutSettings& settings, float dtSeconds) {
	constexpr UInt32 kCutCostFrames = 600;
	static CutCostWindow s_window;
	static const double s_microsPerTick = 1e6 / static_cast<double>(ReadPerformanceFrequency());
	const long long start = ReadPerformanceCounter();
	StepCuts(settings, dtSeconds);
	const double micros = static_cast<double>(ReadPerformanceCounter() - start) * s_microsPerTick;
	const UInt32 live = StandingCuts();
	if (live == 0 && s_window.mostLive == 0) {
		s_window = CutCostWindow{};
		return;
	}
	if (AddCutCost(s_window, micros, live, kCutCostFrames)) {
		OBVR_LOG("Dismember: the cuts' cost - %u frames, %.1f us a frame on the mean, %.1f the longest, at most %u "
		         "cut(s) standing (%u slots)",
		         s_window.frames, CutCostMean(s_window), s_window.maxMicros, s_window.mostLive, kCutsMost);
		s_window = CutCostWindow{};
	}
}

}  // namespace obvr::game

#include "platform/Win32Min.h"

#include "core/Log.h"
#include "game/FittedCapsules.h"
#include "game/HandReadCostLogic.h"
#include "game/RefFingerprint.h"

namespace obvr::game {

UInt32 ReadHandVerticesMeasured(const NiAVObject* root, std::vector<NiPoint3>& right, std::vector<NiPoint3>& left) {
	// a read every 15 frames: a line about every 20 s at the headset's rate
	constexpr UInt32 kReadsPerLine = 120;
	static HandReadCostWindow s_window;
	static const double s_microsPerTick = 1e6 / static_cast<double>(ReadPerformanceFrequency());
	const UInt32 probesBefore = RefProbesMade();
	const long long start = ReadPerformanceCounter();
	const UInt32 read = ReadHandVerticesNow(root, right, left);
	const double micros = static_cast<double>(ReadPerformanceCounter() - start) * s_microsPerTick;
	if (AddHandReadCost(s_window, micros, read, RefProbesMade() - probesBefore, kReadsPerLine)) {
		OBVR_LOG("Hands: the hand fit's read - %u reads, %.1f us a read on the mean, %.1f the longest, %.1f shapes "
		         "and %.1f memory probes a read",
		         s_window.reads, HandReadCostMean(s_window), s_window.maxMicros, HandReadShapesMean(s_window),
		         HandReadProbesMean(s_window));
		s_window = HandReadCostWindow{};
	}
	return read;
}

}  // namespace obvr::game

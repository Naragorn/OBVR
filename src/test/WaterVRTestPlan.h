#pragma once

#include "core/Types.h"

namespace obvr::test {

inline float WaterTestPitch(float requested) {
	// Comparisons also reject NaN and infinities. Retain the established test
	// view if an edited INI supplies an unsupported value.
	return requested >= -80.0f && requested <= 80.0f ? requested : -35.0f;
}

struct WaterShaderDumpLedger {
	const void* saved[16]{};
	bool NeedsDump(int index, const void* shader, bool replayActive) const {
		return replayActive && index >= 0 && index < 16 && shader != nullptr &&
		       saved[index] != shader;
	}
	void Record(int index, const void* shader, bool succeeded) {
		if (succeeded && index >= 0 && index < 16 && shader != nullptr)
			saved[index] = shader;
	}
};

constexpr UInt32 kWaterTestViews = 7;
constexpr UInt32 kWaterTestFramesPerView = 24;
constexpr UInt32 kWaterTestSettleFrames = 90;
constexpr UInt32 kWaterTestCaptureFrames[2] = {8, 18};
constexpr UInt32 kWaterTestExpectedMask =
	(1u << (kWaterTestViews * 2 * 2)) - 1u;

enum class WaterTestEnableAction {
	None,
	Arm,
	Disarm
};

inline bool WaterTestReplayActive(bool enabled, bool active, bool finished) {
	return enabled && active && !finished;
}

inline bool WaterTestSweepMayAdvance(UInt32 settleFrames) {
	return settleFrames >= kWaterTestSettleFrames;
}

inline bool WaterTestShouldSuppressLifecycleCapture(UInt32 frame, bool firstEye,
                                                    bool alreadySuppressed) {
	return frame == 4 && firstEye && !alreadySuppressed;
}

inline WaterTestEnableAction ChooseWaterTestEnableAction(
		bool requested, bool enabled, bool active) {
	if (requested && !enabled) return WaterTestEnableAction::Arm;
	if (!requested && enabled && !active) return WaterTestEnableAction::Disarm;
	return WaterTestEnableAction::None;
}
inline UInt32 WaterTestViewIndex(UInt32 frame) {
	const UInt32 view = frame / kWaterTestFramesPerView;
	return view < kWaterTestViews ? view : kWaterTestViews - 1;
}

inline float WaterTestViewYaw(UInt32 frame) {
	return -60.0f + 120.0f * static_cast<float>(WaterTestViewIndex(frame)) /
	                   static_cast<float>(kWaterTestViews - 1);
}

inline int WaterTestCapturePhase(bool active, bool finished, UInt32 frame) {
	if (!active || finished ||
	    frame >= kWaterTestViews * kWaterTestFramesPerView) return -1;
	const UInt32 step = frame % kWaterTestFramesPerView;
	if (step == kWaterTestCaptureFrames[0]) return 0;
	if (step == kWaterTestCaptureFrames[1]) return 1;
	return -1;
}

inline UInt32 WaterTestEyeBit(UInt32 view, unsigned phase, bool left) {
	if (view >= kWaterTestViews || phase >= 2) return 0;
	return 1u << (view * 4 + phase * 2 + (left ? 0u : 1u));
}

inline bool WaterTestEvidenceComplete(UInt32 images, UInt32 matrices,
                                      UInt32 fixedCapture, UInt32 projectionScale,
                                      UInt32 eyeReuse) {
	return images == kWaterTestExpectedMask &&
	       matrices == kWaterTestExpectedMask &&
	       fixedCapture == kWaterTestExpectedMask &&
	       projectionScale == kWaterTestExpectedMask &&
	       eyeReuse == kWaterTestExpectedMask;
}

}  // namespace obvr::test

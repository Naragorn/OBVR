#include <cstdio>
#include <limits>
#include <initializer_list>

#include "test/WaterVRTestPlan.h"

using namespace obvr;
using namespace obvr::test;

namespace {
unsigned checks = 0;
unsigned failures = 0;
void Check(bool value, const char* message) {
	++checks;
	if (!value) {
		++failures;
		std::printf("FAIL: %s\n", message);
	}
}
}

int main() {
	for (float pitch : {-80.0f, -35.0f, 0.0f, 35.0f, 80.0f})
		Check(WaterTestPitch(pitch) == pitch, "valid replay pitches preserved including endpoints");
	for (float pitch : {-81.0f, 81.0f, std::numeric_limits<float>::infinity(),
	                    -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
		Check(WaterTestPitch(pitch) == -35.0f, "invalid replay pitch retains established default");
	int firstShader = 0, replacementShader = 0;
	for (int index = -1; index <= 16; ++index)
	for (unsigned replay = 0; replay < 2; ++replay)
	for (unsigned nonnull = 0; nonnull < 2; ++nonnull)
	for (unsigned success = 0; success < 2; ++success) {
		WaterShaderDumpLedger ledger;
		const void* shader = nonnull ? &firstShader : nullptr;
		const bool valid = index >= 0 && index < 16 && nonnull;
		Check(ledger.NeedsDump(index, shader, replay != 0) == (valid && replay),
		      "dump rejects invalid slots, null shaders and inactive replay");
		ledger.Record(index, shader, success != 0);
		Check(ledger.NeedsDump(index, shader, replay != 0) == (valid && replay && !success),
		      "failed writes retry; successful writes suppress duplicates");
		Check(ledger.NeedsDump(index, &replacementShader, true) == (index >= 0 && index < 16),
		      "replacement shader at same slot is captured anew");
	}
	for (unsigned enabled = 0; enabled < 2; ++enabled)
	for (unsigned active = 0; active < 2; ++active)
	for (unsigned finished = 0; finished < 2; ++finished) {
		const bool expected = enabled == 1 && active == 1 && finished == 0;
		Check(WaterTestReplayActive(enabled != 0, active != 0, finished != 0) == expected,
		      "replay and target diagnostics run only in an enabled unfinished active test");
	}
	Check(!WaterTestSweepMayAdvance(0), "sweep waits at first world frame");
	Check(!WaterTestSweepMayAdvance(kWaterTestSettleFrames - 1),
	      "sweep waits through the last settling frame");
	Check(WaterTestSweepMayAdvance(kWaterTestSettleFrames),
	      "sweep starts at the settling boundary");
	Check(WaterTestSweepMayAdvance(kWaterTestSettleFrames + 1),
	      "sweep stays enabled after settling");
	for (UInt32 frame : {3u, 4u, 5u})
	for (unsigned first = 0; first < 2; ++first)
	for (unsigned suppressed = 0; suppressed < 2; ++suppressed) {
		const bool expected = frame == 4 && first != 0 && suppressed == 0;
		Check(WaterTestShouldSuppressLifecycleCapture(frame, first != 0,
		                                               suppressed != 0) == expected,
		      "lifecycle injection suppresses exactly one first-eye capture");
	}
	for (unsigned requested = 0; requested < 2; ++requested)
	for (unsigned enabled = 0; enabled < 2; ++enabled)
	for (unsigned active = 0; active < 2; ++active) {
		WaterTestEnableAction expected = WaterTestEnableAction::None;
		if (requested && !enabled) expected = WaterTestEnableAction::Arm;
		else if (!requested && enabled && !active)
			expected = WaterTestEnableAction::Disarm;
		Check(ChooseWaterTestEnableAction(requested != 0, enabled != 0, active != 0) == expected,
		      "all requested, enabled and active attach/disarm flows");
	}	for (unsigned active = 0; active < 2; ++active)
	for (unsigned finished = 0; finished < 2; ++finished) {
		for (UInt32 frame = 0; frame <= kWaterTestViews * kWaterTestFramesPerView; ++frame) {
			const UInt32 step = frame % kWaterTestFramesPerView;
			int expected = -1;
			if (active && !finished &&
			    frame < kWaterTestViews * kWaterTestFramesPerView) {
				if (step == kWaterTestCaptureFrames[0]) expected = 0;
				if (step == kWaterTestCaptureFrames[1]) expected = 1;
			}
			Check(WaterTestCapturePhase(active != 0, finished != 0, frame) == expected,
			      "all active, finished, range and capture-step flows");
		}
	}

	Check(WaterTestViewIndex(0) == 0, "first frame maps to first yaw view");
	Check(WaterTestViewIndex(kWaterTestFramesPerView - 1) == 0,
	      "last frame in first group stays in first yaw view");
	Check(WaterTestViewIndex(kWaterTestFramesPerView) == 1,
	      "next group advances the yaw view");
	Check(WaterTestViewIndex(100000) == kWaterTestViews - 1,
	      "out-of-range frame clamps to final yaw view");
	Check(WaterTestViewYaw(0) == -60.0f, "yaw sweep starts at minus sixty degrees");
	Check(WaterTestViewYaw((kWaterTestViews - 1) * kWaterTestFramesPerView) == 60.0f,
	      "yaw sweep ends at plus sixty degrees");

	UInt32 mask = 0;
	for (UInt32 view = 0; view < kWaterTestViews; ++view)
	for (unsigned phase = 0; phase < 2; ++phase)
	for (unsigned left = 0; left < 2; ++left) {
		const UInt32 bit = WaterTestEyeBit(view, phase, left != 0);
		Check(bit != 0 && (mask & bit) == 0,
		      "every view, wave phase and eye receives a unique evidence bit");
		mask |= bit;
	}
	Check(mask == kWaterTestExpectedMask,
	      "all evidence bits exactly fill the expected mask");
	Check(WaterTestEyeBit(kWaterTestViews, 0, true) == 0,
	      "out-of-range view is refused");
	Check(WaterTestEyeBit(0, 2, true) == 0,
	      "out-of-range wave phase is refused");

	Check(WaterTestEvidenceComplete(mask, mask, mask, mask, mask),
	      "complete evidence passes");
	const UInt32 missing[] = {
		mask & ~1u, mask & ~2u, mask & ~4u, mask & ~8u, mask & ~16u
	};
	for (unsigned field = 0; field < 5; ++field) {
		UInt32 values[5] = {mask, mask, mask, mask, mask};
		values[field] = missing[field];
		Check(!WaterTestEvidenceComplete(
		          values[0], values[1], values[2], values[3], values[4]),
		      "each missing evidence class independently fails");
	}

	std::printf("%u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;
}

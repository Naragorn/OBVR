#include "perf/ProfileLogic.h"

namespace obvr::perf {
namespace {

bool IsFinite(double value) { return value == value && value < 1.0e300 && value > -1.0e300; }

} // namespace

ProfileSettings NormalizeSettings(ProfileSettings settings) {
	if (settings.captureSeconds < kMinCaptureSeconds) settings.captureSeconds = kDefaultCaptureSeconds;
	if (settings.captureSeconds > kMaxCaptureSeconds) settings.captureSeconds = kMaxCaptureSeconds;
	if (settings.maxRecords < kMinMaxRecords) settings.maxRecords = kDefaultMaxRecords;
	if (settings.maxRecords > kMaxMaxRecords) settings.maxRecords = kMaxMaxRecords;
	const UInt64 bytesPerRecord = static_cast<UInt64>(sizeof(CpuEvent)) +
	                              sizeof(FrameRecord) + sizeof(GpuSample);
	const UInt32 memoryMax = static_cast<UInt32>(kProfilerBufferLimitBytes / bytesPerRecord);
	if (settings.maxRecords > memoryMax) settings.maxRecords = memoryMax;
	return settings;
}

bool ElapsedMilliseconds(UInt64 start, UInt64 end, UInt64 frequency, double& milliseconds) {
	if (frequency == 0 || end < start) {
		milliseconds = 0.0;
		return false;
	}
	const UInt64 ticks = end - start;
	// Keep the arithmetic wide before converting to double. This remains
	// precise enough for QPC ranges used by one capture and rejects no valid
	// 64-bit counter value.
	milliseconds = (static_cast<double>(ticks) * 1000.0) /
	               static_cast<double>(frequency);
	return IsFinite(milliseconds);
}

bool NearestRankPercentile(const double* values, UInt32 count, double percentile,
                           double& result) {
	if (values == nullptr || count == 0 || !IsFinite(percentile) || percentile < 0.0 ||
	    percentile > 100.0) {
		result = 0.0;
		return false;
	}
	// The caller supplies a sorted scratch array. This explicit contract keeps
	// this pure helper allocation-free and lets the CLI use its own copy.
	const double rank = (percentile / 100.0) * static_cast<double>(count);
	UInt32 index = rank <= 1.0 ? 0 : static_cast<UInt32>(rank + 0.999999999) - 1;
	if (index >= count) index = count - 1;
	result = values[index];
	return IsFinite(result);
}

const char* EventTypeName(EventType type) {
	switch (type) {
		case EventType::Present: return "present";
		case EventType::ScenePass: return "scene_pass";
		case EventType::BetweenPasses: return "between_passes";
		case EventType::EyeCapture: return "eye_capture";
		case EventType::HudBetween: return "hud_between";
		case EventType::EyeCameraShift: return "eye_camera_shift";
		case EventType::EyeCameraRestore: return "eye_camera_restore";
		case EventType::AfterSecondPass: return "after_second";
		case EventType::BeginFrame: return "begin_frame";
		case EventType::PosesWait: return "poses_wait";
		case EventType::InteropFlush: return "interop_flush";
		case EventType::InteropLock: return "interop_lock";
		case EventType::InteropHeld: return "interop_held";
		case EventType::SubmitLeft: return "submit_left";
		case EventType::SubmitRight: return "submit_right";
		case EventType::FrameEndCallback: return "frame_end_callback";
		case EventType::MonitorPresent: return "monitor_present";
	}
	return "unknown";
}

const char* DeliveryModeName(DeliveryMode mode) {
	switch (mode) {
		case DeliveryMode::Unknown: return "unknown";
		case DeliveryMode::WorldDual: return "world_dual";
		case DeliveryMode::WorldAer: return "world_aer";
		case DeliveryMode::Mono: return "mono";
		case DeliveryMode::LiveMenu: return "live_menu";
		case DeliveryMode::HeldMenu: return "held_menu";
		case DeliveryMode::Flat: return "flat";
		case DeliveryMode::Setup: return "setup";
	}
	return "unknown";
}

const char* GpuStatusName(GpuStatus status) {
	switch (status) {
		case GpuStatus::Ready: return "ready";
		case GpuStatus::Pending: return "pending";
		case GpuStatus::PoolFull: return "gpu_pool_full";
		case GpuStatus::Timeout: return "timeout";
		case GpuStatus::QueryError: return "query_error";
		case GpuStatus::Disjoint: return "disjoint";
		case GpuStatus::InvalidFrequency: return "invalid_frequency";
		case GpuStatus::DeviceLost: return "device_lost";
		case GpuStatus::PendingAtStop: return "pending_at_stop";
	}
	return "query_error";
}

} // namespace obvr::perf

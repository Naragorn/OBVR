#include "perf/ProfileLogic.h"

#include <cstdio>

namespace obvr::perf {
namespace {

bool IsFinite(double value) { return value == value && value < 1.0e300 && value > -1.0e300; }

} // namespace

ProfileSettings NormalizeSettings(ProfileSettings settings) {
	if (settings.captureSeconds < kMinCaptureSeconds) settings.captureSeconds = kDefaultCaptureSeconds;
	if (settings.captureSeconds > kMaxCaptureSeconds) settings.captureSeconds = kMaxCaptureSeconds;
	if (settings.maxRecords < kMinMaxRecords) settings.maxRecords = kDefaultMaxRecords;
	if (settings.maxRecords > kMaxMaxRecords) settings.maxRecords = kMaxMaxRecords;
	// The share of each table an event brings, rounded up, against the
	// budget less the tables' minimums - conservative, so CaptureBytes of
	// the result never exceeds the limit.
	const UInt64 bytesPerRecord =
	    static_cast<UInt64>(sizeof(CpuEvent)) + (sizeof(FrameRecord) + kFrameDivisor - 1) / kFrameDivisor +
	    (settings.gpuTiming ? (sizeof(GpuSample) + kGpuDivisor - 1) / kGpuDivisor : 0) +
	    (sizeof(CompositorTimingRecord) + kCompositorTimingDivisor - 1) / kCompositorTimingDivisor;
	const UInt64 floorBytes = static_cast<UInt64>(kMinMaxRecords) *
	                          (sizeof(FrameRecord) + sizeof(GpuSample) + sizeof(CompositorTimingRecord));
	const UInt32 memoryMax = static_cast<UInt32>((kProfilerBufferLimitBytes - floorBytes) / bytesPerRecord);
	if (settings.maxRecords > memoryMax) settings.maxRecords = memoryMax;
	if (settings.startAfterSeconds > kMaxStartAfterSeconds) settings.startAfterSeconds = kMaxStartAfterSeconds;
	return settings;
}

UInt32 CompositorTimingCapacity(UInt32 maxRecords) {
	const UInt32 share = maxRecords / kCompositorTimingDivisor;
	return share < kMinMaxRecords ? kMinMaxRecords : share;
}

UInt32 FrameCapacity(UInt32 maxRecords) {
	const UInt32 share = maxRecords / kFrameDivisor;
	return share < kMinMaxRecords ? kMinMaxRecords : share;
}

UInt32 GpuCapacity(UInt32 maxRecords, bool gpuTiming) {
	const UInt32 share = gpuTiming ? maxRecords / kGpuDivisor : 0;
	return share < kMinMaxRecords ? kMinMaxRecords : share;
}

UInt64 CaptureBytes(const ProfileSettings& settings) {
	return static_cast<UInt64>(settings.maxRecords) * sizeof(CpuEvent) +
	       static_cast<UInt64>(FrameCapacity(settings.maxRecords)) * sizeof(FrameRecord) +
	       static_cast<UInt64>(GpuCapacity(settings.maxRecords, settings.gpuTiming)) * sizeof(GpuSample) +
	       static_cast<UInt64>(CompositorTimingCapacity(settings.maxRecords)) * sizeof(CompositorTimingRecord);
}

bool CaptureDirectoryCandidate(const char* base, UInt32 attempt, char* out, UInt32 size) {
	if (base == nullptr || out == nullptr || size == 0) return false;
	const int written = attempt == 0 ? std::snprintf(out, size, "%s", base)
	                                 : std::snprintf(out, size, "%s-%u", base, attempt + 1);
	if (written < 0 || static_cast<UInt32>(written) >= size) {
		out[0] = '\0';
		return false;
	}
	return true;
}

DirectoryStep NextDirectoryStep(bool created, bool alreadyExists, UInt32 attempt) {
	if (created) return DirectoryStep::Write;
	if (alreadyExists && attempt + 1 < kMaxDirectoryAttempts) return DirectoryStep::TryNext;
	return DirectoryStep::GiveUp;
}

const char* const kCompositorTimingCsvHeader =
    "present_id,vr_frame_id,frame_index,num_presents,mis_presented,dropped_frames,"
    "reprojection_flags,system_time_s,pre_submit_gpu_ms,post_submit_gpu_ms,total_render_gpu_ms,"
    "compositor_render_gpu_ms,compositor_idle_cpu_ms,client_frame_interval_ms,"
    "wait_get_poses_called_ms,new_poses_ready_ms,new_frame_ready_ms,compositor_update_start_ms,"
    "compositor_render_start_ms,vsyncs_ready_for_use,vsyncs_to_first_view\n";

int CompositorTimingCsvLine(char* out, UInt32 size, const CompositorTimingRecord& r) {
	const int written = std::snprintf(
	    out, size, "%llu,%llu,%u,%u,%u,%u,%u,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u",
	    static_cast<unsigned long long>(r.presentId), static_cast<unsigned long long>(r.vrFrameId),
	    r.frameIndex, r.numFramePresents, r.numMisPresented, r.numDroppedFrames,
	    r.reprojectionFlags, r.systemTimeSeconds, static_cast<double>(r.preSubmitGpuMs),
	    static_cast<double>(r.postSubmitGpuMs), static_cast<double>(r.totalRenderGpuMs),
	    static_cast<double>(r.compositorRenderGpuMs), static_cast<double>(r.compositorIdleCpuMs),
	    static_cast<double>(r.clientFrameIntervalMs), static_cast<double>(r.waitGetPosesCalledMs),
	    static_cast<double>(r.newPosesReadyMs), static_cast<double>(r.newFrameReadyMs),
	    static_cast<double>(r.compositorUpdateStartMs), static_cast<double>(r.compositorRenderStartMs),
	    r.numVSyncsReadyForUse, r.numVSyncsToFirstView);
	if (written < 0 || static_cast<UInt32>(written) >= size) {
		return -1;
	}
	return written;
}

bool CaptureStartDue(UInt64 armedTick, UInt64 nowTick, UInt32 startAfterSeconds, UInt64 frequency) {
	if (frequency == 0 || nowTick < armedTick) return false;
	return nowTick - armedTick >= static_cast<UInt64>(startAfterSeconds) * frequency;
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
		case EventType::BoneLock: return "bone_lock";
		case EventType::SubmitHold: return "submit_hold";
		case EventType::PacingVsync: return "pacing_vsync";
		case EventType::DrawCalls: return "draw_calls";
		case EventType::VsConstantCalls: return "vs_constant_calls";
		case EventType::StateCalls: return "state_calls";
		case EventType::ShapeRender: return "shape_render";
		case EventType::CullCalls: return "cull_calls";
		case EventType::TorchScan: return "torch_scan";
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

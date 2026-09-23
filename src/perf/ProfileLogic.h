#pragma once

#include "core/Types.h"

namespace obvr::perf {

constexpr UInt32 kProfileSchemaVersion = 1;
constexpr UInt32 kDefaultCaptureSeconds = 60;
constexpr UInt32 kDefaultMaxRecords = 65536;
constexpr UInt32 kMinCaptureSeconds = 5;
constexpr UInt32 kMaxCaptureSeconds = 300;
constexpr UInt32 kMinMaxRecords = 1024;
constexpr UInt32 kMaxMaxRecords = 262144;
constexpr UInt32 kProfilerBufferLimitBytes = 32u * 1024u * 1024u;

struct ProfileSettings {
	bool enabled = false;
	bool gpuTiming = false;
	UInt32 captureSeconds = kDefaultCaptureSeconds;
	UInt32 maxRecords = kDefaultMaxRecords;
};

// Normalisation is intentionally pure. Config reload and tests use the same
// rules, so a malformed INI can never create an unbounded frame buffer.
ProfileSettings NormalizeSettings(ProfileSettings settings);

enum class EventType : UInt8 {
	Present = 0,
	ScenePass,
	BetweenPasses,
	EyeCapture,
	HudBetween,
	EyeCameraShift,
	EyeCameraRestore,
	AfterSecondPass,
	BeginFrame,
	PosesWait,
	InteropFlush,
	InteropLock,
	InteropHeld,
	SubmitLeft,
	SubmitRight,
	FrameEndCallback,
	MonitorPresent,
};

enum class DeliveryMode : UInt8 {
	Unknown = 0,
	WorldDual,
	WorldAer,
	Mono,
	LiveMenu,
	HeldMenu,
	Flat,
	Setup,
};

enum class EventStatus : UInt8 {
	Complete = 0,
	MissingEnd,
	InvalidTicks,
	DroppedCapacity,
};

enum class GpuStatus : UInt8 {
	Ready = 0,
	Pending,
	PoolFull,
	Timeout,
	QueryError,
	Disjoint,
	InvalidFrequency,
	DeviceLost,
	PendingAtStop,
};

struct EventContext {
	UInt64 presentId = 0;
	UInt64 sceneId = 0;
	UInt64 vrFrameId = 0;
	UInt32 passIndex = 0;
	UInt8 eye = 2; // 0=left, 1=right, 2=not an eye
	DeliveryMode mode = DeliveryMode::Unknown;
};

struct CpuEvent {
	UInt64 eventId = 0;
	EventType type = EventType::Present;
	EventContext context;
	UInt64 startTick = 0;
	UInt64 endTick = 0;
	EventStatus status = EventStatus::MissingEnd;
};

struct FrameRecord {
	UInt64 presentId = 0;
	UInt64 vrFrameId = 0;
	UInt64 startTick = 0;
	UInt64 endTick = 0;
	UInt64 intervalTick = 0;
	DeliveryMode requestedMode = DeliveryMode::Unknown;
	DeliveryMode deliveredMode = DeliveryMode::Unknown;
	SInt32 poseResult = -1;
	UInt8 leftCaptured = 0;
	UInt8 rightCaptured = 0;
	UInt8 passCount = 0;
	UInt8 setup = 0;
	UInt8 status = 0;
	SInt32 submitLeft = -1;
	SInt32 submitRight = -1;
};

struct GpuSample {
	UInt64 sampleId = 0;
	UInt64 presentId = 0;
	UInt64 vrFrameId = 0;
	EventType type = EventType::ScenePass;
	UInt32 passIndex = 0;
	UInt8 eye = 2;
	GpuStatus status = GpuStatus::Pending;
	UInt64 startTick = 0;
	UInt64 endTick = 0;
	UInt64 frequency = 0;
	UInt32 agePresents = 0;
};

struct SessionInfo {
	UInt64 sessionId = 0;
	UInt64 qpcFrequency = 0;
	UInt64 startTick = 0;
	UInt64 endTick = 0;
	UInt32 schemaVersion = kProfileSchemaVersion;
	UInt32 generation = 0;
};

// Converts a validated counter interval into milliseconds. Returning false
// for a zero frequency or reversed ticks keeps missing values distinguishable
// from a genuine zero-length span.
bool ElapsedMilliseconds(UInt64 start, UInt64 end, UInt64 frequency, double& milliseconds);

// nearest-rank percentile, with p in [0,100]. The input is not modified.
bool NearestRankPercentile(const double* values, UInt32 count, double percentile,
                           double& result);

const char* EventTypeName(EventType type);
const char* DeliveryModeName(DeliveryMode mode);
const char* GpuStatusName(GpuStatus status);

} // namespace obvr::perf

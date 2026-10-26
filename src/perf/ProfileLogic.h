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
constexpr UInt32 kMaxStartAfterSeconds = 3600;

struct ProfileSettings {
	bool enabled = false;
	bool gpuTiming = false;
	UInt32 captureSeconds = kDefaultCaptureSeconds;
	UInt32 maxRecords = kDefaultMaxRecords;
	// The capture armed by Enabled starts this many seconds later, so a
	// tester reaches the scene that stutters before the records fill
	// (2026-10-18: a capture from the game's start ended before the
	// tester's room). Zero starts at once.
	UInt32 startAfterSeconds = 0;
	// The render sampler beside the capture (render/RenderSampler.h): the
	// render thread's eip and stack about every millisecond, OBVR-samples.csv.
	bool sampler = false;
	// The call clocks inside the capture (draw_calls, vs_constant_calls,
	// state_calls, shape_render, cull_calls): two counter reads per hooked
	// call, which under Wine is ntdll and the vDSO each time - about 2.5 ms
	// a pass with five people in view (the render sampler, 2026-10-19). Off
	// by default so a capture measures the frame, on for the split.
	bool callClock = false;
};

// Normalisation is intentionally pure. Config reload and tests use the same
// rules, so a malformed INI can never create an unbounded frame buffer.
ProfileSettings NormalizeSettings(ProfileSettings settings);

// Whether an armed capture is due: startAfterSeconds of the counter have
// passed since armedTick. A zero frequency, or a counter behind the
// arming, is never due.
bool CaptureStartDue(UInt64 armedTick, UInt64 nowTick, UInt32 startAfterSeconds, UInt64 frequency);

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
	// The bone lock's own work inside a scene pass (InterfaceRenderHook.cpp,
	// HandleBoneUpload), summed over the pass and marked once at its end
	// (2026-10-18: seven people in the harness hall cost 4.3 ms a pass).
	BoneLock,
	// The wait before a submit that would have landed inside the compositor's
	// running start (render/SubmitPacing.h, [Render] SubmitPastVsync).
	SubmitHold,
	// The pacing's estimate of the imminent vsync: from the moment
	// WaitGetPoses returned (start) to the vsync (end).
	PacingVsync,
	// Time inside the hooked D3D9 calls of a scene pass, summed over the pass
	// and marked at its end like bone_lock (render/InterfaceRenderHook.cpp,
	// CallClock): the draws, the vertex-shader constant uploads, and the
	// other setters OBVR hooks (render states, transforms, declarations,
	// shaders, pixel constants, streams, indices). What a pass spends outside
	// these is the engine's own work and the calls OBVR does not hook
	// (textures, sampler states).
	DrawCalls,
	VsConstantCalls,
	StateCalls,
	// The engine's per-shape render (render/ShapeRenderClock.h), summed over
	// the pass: constant maps, skinning setup, streams and the draw per shape.
	ShapeRender,
	// The engine's culling (NiCullingProcess::Process, render/CullingHook.cpp),
	// summed over the pass.
	CullCalls,
	// OBVR's walk of the player's cell for the torches it lit, every 30
	// frames (game/TorchFire.cpp, ScanTorchesInPlayerCell) - asked for by the
	// performance session so a cell of several hundred references shows.
	TorchScan,
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

// One frame as the compositor measured it (Compositor_FrameTiming, read
// after WaitGetPoses for a frame finished one or two frames earlier,
// vr/OpenVRTypes.h), kept for compositor.csv. presentId and vrFrameId are
// OBVR's ids at the moment of reading - a frame or two after the one the
// row describes - so the row joins frames.csv to within that; the
// compositor's own frame index and vsync-relative moments are its clock.
struct CompositorTimingRecord {
	UInt64 presentId = 0;
	UInt64 vrFrameId = 0;
	UInt32 frameIndex = 0;
	UInt32 numFramePresents = 0;
	UInt32 numMisPresented = 0;
	UInt32 numDroppedFrames = 0;
	UInt32 reprojectionFlags = 0;
	double systemTimeSeconds = 0.0;
	float preSubmitGpuMs = 0.0f;
	float postSubmitGpuMs = 0.0f;
	float totalRenderGpuMs = 0.0f;
	float compositorRenderGpuMs = 0.0f;
	float compositorIdleCpuMs = 0.0f;
	float clientFrameIntervalMs = 0.0f;
	float waitGetPosesCalledMs = 0.0f;
	float newPosesReadyMs = 0.0f;
	float newFrameReadyMs = 0.0f;
	float compositorUpdateStartMs = 0.0f;
	float compositorRenderStartMs = 0.0f;
	UInt32 numVSyncsReadyForUse = 0;
	UInt32 numVSyncsToFirstView = 0;
};

// One compositor row a frame, where the events run to dozens a frame: the
// compositor buffer holds a quarter of maxRecords rows, never fewer than
// kMinMaxRecords, and a full buffer drops rows rather than ending the
// capture (frames.csv and the events stay whole).
constexpr UInt32 kCompositorTimingDivisor = 4;
UInt32 CompositorTimingCapacity(UInt32 maxRecords);

// The other tables are sized by what a frame writes, not each to maxRecords
// (maxRecords counts the events): about 32 events a frame (the hall,
// 2026-10-23: 143395 events in 4455 frames) and four GPU samples (two scene
// passes, two eye captures). An eighth of the events for the frames and a
// quarter for the GPU samples leave four and two times that headroom; the
// GPU table holds only the minimum without GPU timing. Each table full ends
// the capture as before (truncated_capacity). Sized each to maxRecords, the
// 32 MiB held about 60 s - the reappear run of 2026-10-24 stopped at 119 s.
constexpr UInt32 kFrameDivisor = 8;
constexpr UInt32 kGpuDivisor = 4;
UInt32 FrameCapacity(UInt32 maxRecords);
UInt32 GpuCapacity(UInt32 maxRecords, bool gpuTiming);
// The bytes a capture with these settings allocates, all four tables.
UInt64 CaptureBytes(const ProfileSettings& settings);

// The folder a capture is written to. Session ids start at 1 with every
// game, so a folder an earlier game left (a live session's capture the
// harness runner does not move) refused the export of 2026-10-24 08:45. The
// writer tries the session's own name, then "-2", "-3" ... after it.
constexpr UInt32 kMaxDirectoryAttempts = 64;
// Attempt 0 is base itself, attempt n base + "-" + (n + 1). False when the
// name does not fit size or base is missing.
bool CaptureDirectoryCandidate(const char* base, UInt32 attempt, char* out, UInt32 size);
enum class DirectoryStep : UInt8 { Write, TryNext, GiveUp };
// After one CreateDirectory try: created, or refused because the name
// exists (then the next name, up to kMaxDirectoryAttempts tries), or refused
// for another reason (no drive, no rights: give up at once).
DirectoryStep NextDirectoryStep(bool created, bool alreadyExists, UInt32 attempt);

// compositor.csv: its header, and one row (without the line break) for a
// record. Returns the characters written, or a negative value when the
// buffer is too small.
extern const char* const kCompositorTimingCsvHeader;
int CompositorTimingCsvLine(char* out, UInt32 size, const CompositorTimingRecord& record);

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

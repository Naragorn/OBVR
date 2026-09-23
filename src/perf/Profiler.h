#pragma once

#include <atomic>

#include "perf/ProfileLogic.h"
#include "perf/GpuQueries.h"
#include "platform/Win32Min.h"

namespace obvr::perf {

class Profiler {
public:
	struct Span {
		UInt32 slot = 0xFFFFFFFFu;
		UInt64 startTick = 0;
		EventType type = EventType::Present;
		EventContext context;
		bool active = false;
	};
	class ScopedSpan {
	public:
		ScopedSpan(Profiler& profiler, EventType type, const EventContext& context = EventContext{})
			: m_profiler(&profiler), m_span(profiler.Begin(type, context)) {}
		~ScopedSpan() { if (m_profiler != nullptr) m_profiler->End(m_span); }
		ScopedSpan(const ScopedSpan&) = delete;
		ScopedSpan& operator=(const ScopedSpan&) = delete;
	private:
		Profiler* m_profiler;
		Span m_span;
	};

	Profiler() = default;
	~Profiler();
	Profiler(const Profiler&) = delete;
	Profiler& operator=(const Profiler&) = delete;

	// Called after Load and every hot reload. A 0->1 transition starts one
	// bounded capture; leaving Enabled=1 does not create an endless sequence.
	void Configure(ProfileSettings settings);
	void Shutdown();

	bool IsCapturing() const { return m_capturing; }
	bool IsWriterBusy() const { return m_writerBusy.load(std::memory_order_acquire); }
	UInt64 CurrentPresentId() const { return m_presentId; }
	UInt64 CurrentVrFrameId() const { return m_vrFrameId; }

	void OnPresentBegin(DeliveryMode requested, bool setup);
	void OnPresentEnd();
	UInt64 BeginVrFrame();
	void SetFrameDetails(DeliveryMode delivered, UInt64 vrFrameId, SInt32 poseResult,
	                     UInt8 leftCaptured, UInt8 rightCaptured, UInt8 passCount);
	void SetSubmitResult(UInt8 eye, SInt32 result);

	Span Begin(EventType type, const EventContext& context = EventContext{});
	void End(Span& span);
	void Mark(EventType type, UInt64 startTick, UInt64 endTick,
	          const EventContext& context = EventContext{});

	// GPU adapters call this only after a non-blocking query result is ready.
	void AddGpuSample(const GpuSample& sample);
	void GpuBegin(EventType type, const EventContext& context, void* device);
	void GpuEnd();

	// Exposed for tests and the query adapter. It never forces a GPU flush.
	void NoteGpuCounter(GpuStatus status);

	static Profiler& Instance();

private:
	struct WriterJob;
	static DWORD __stdcall WriterThunk(void* parameter);
	static void WriteJob(WriterJob& job);
	void StopCapture(const char* reason);
	void StartCapture();
	void FinishWriter();
	bool AppendEvent(const CpuEvent& event);
	bool AppendFrame(const FrameRecord& frame);

	ProfileSettings m_settings;
	bool m_configuredEnabled = false;
	bool m_capturing = false;
	bool m_truncated = false;
	bool m_stopRequested = false;
	bool m_stopping = false;
	UInt64 m_sessionSequence = 0;
	UInt64 m_presentId = 0;
	UInt64 m_vrFrameId = 0;
	UInt64 m_nextEventId = 1;
	UInt64 m_nextSampleId = 1;
	UInt64 m_previousPresentTick = 0;
	UInt64 m_captureStartTick = 0;
	UInt64 m_qpcFrequency = 0;
	UInt32 m_generation = 0;
	UInt32 m_eventCount = 0;
	UInt32 m_frameCount = 0;
	UInt32 m_gpuCount = 0;
	UInt32 m_gpuPoolFull = 0;
	UInt32 m_gpuPending = 0;
	UInt32 m_gpuErrors = 0;
	CpuEvent* m_events = nullptr;
	FrameRecord* m_frames = nullptr;
	GpuSample* m_gpuSamples = nullptr;
	GpuQueryMachine m_gpuQueries;
	void* m_gpuDevice = nullptr;
	bool m_gpuOpen = false;
	FrameRecord m_currentFrame;
	bool m_frameOpen = false;
	Span m_presentSpan;
	WriterJob* m_job = nullptr;
	void* m_writerHandle = nullptr;
	std::atomic<bool> m_writerBusy{false};
};

} // namespace obvr::perf

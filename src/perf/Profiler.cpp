#include "perf/Profiler.h"

#include <cstdio>
#include <new>

#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace obvr::perf {
namespace {

Profiler g_profiler;
UInt64 g_nextSession = 1;

UInt64 Tick() { return static_cast<UInt64>(ReadPerformanceCounter()); }

void CopyText(char* destination, UInt32 capacity, const char* source) {
	if (capacity == 0) return;
	UInt32 i = 0;
	if (source != nullptr) {
		for (; i + 1 < capacity && source[i] != '\0'; ++i) destination[i] = source[i];
	}
	destination[i] = '\0';
}

bool WriteAll(HANDLE file, const void* data, UInt32 bytes) {
	DWORD written = 0;
	return file != InvalidHandle() && WriteFile(file, data, bytes, &written, nullptr) != 0 &&
	       written == bytes;
}

bool WriteText(HANDLE file, const char* text) {
	UInt32 length = 0;
	while (text != nullptr && text[length] != '\0') ++length;
	return WriteAll(file, text, length);
}

bool OpenOutput(const char* directory, const char* name, char* path, UInt32 pathSize,
	               HANDLE& file) {
	char partialName[128] = {};
	std::snprintf(partialName, sizeof(partialName), "%s.partial", name != nullptr ? name : "");
	if (!JoinPath(directory, partialName, path, pathSize)) return false;
	file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
	                   FILE_ATTRIBUTE_NORMAL, nullptr);
	return file != InvalidHandle();
}

bool CommitOutput(const char* directory, const char* name, const char* partialPath) {
	char finalPath[512] = {};
	return JoinPath(directory, name, finalPath, sizeof(finalPath)) &&
	       MoveFileExA(partialPath, finalPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

void CloseOutput(HANDLE file) {
	if (file != InvalidHandle()) CloseHandle(file);
}

// One record into a table: a ring overwrites its oldest when full, a window
// refuses (and the capture ends, truncated_capacity).
template <class T>
bool PutRecord(T* table, UInt32 capacity, UInt64& written, bool ring, const T& record) {
	if (table == nullptr || capacity == 0) return false;
	if (ring) {
		table[RingSlot(written, capacity)] = record;
		++written;
		return true;
	}
	if (written >= capacity) return false;
	table[written++] = record;
	return true;
}

// One CSV of a table, oldest first (RingOrderedSlot): the header, then each
// stored row `keep` accepts, formatted by `format` without its line break.
// `kept` counts the rows written. False when the file could not be made
// whole.
template <class T, class Keep, class Format>
bool WriteTable(const char* directory, const char* name, const char* header, const T* table, UInt64 written,
                UInt32 capacity, UInt32& kept, Keep keep, Format format) {
	char path[512] = {};
	HANDLE file = InvalidHandle();
	if (!OpenOutput(directory, name, path, sizeof(path), file)) return false;
	bool ok = WriteText(file, header);
	char line[512] = {};
	const UInt32 stored = RingStoredCount(written, capacity);
	for (UInt32 i = 0; i < stored; ++i) {
		const T& row = table[RingOrderedSlot(written, capacity, i)];
		if (!keep(row)) continue;
		const int n = format(row, line, sizeof(line) - 1);
		if (n < 0 || static_cast<UInt32>(n) >= sizeof(line) - 1) {
			ok = false;
			continue;
		}
		line[n] = '\n';
		line[n + 1] = '\0';
		++kept;
		ok = WriteText(file, line) && ok;
	}
	CloseOutput(file);
	return CommitOutput(directory, name, path) && ok;
}

} // namespace

struct Profiler::WriterJob {
	CpuEvent* events = nullptr;
	FrameRecord* frames = nullptr;
	GpuSample* gpuSamples = nullptr;
	CompositorTimingRecord* compositorTimings = nullptr;
	// Records put in (written) and the tables' sizes; the writer reads them
	// oldest first through RingOrderedSlot, which for a table that never
	// wrapped is the plain order.
	UInt64 eventWritten = 0;
	UInt64 frameWritten = 0;
	UInt64 gpuWritten = 0;
	UInt64 compositorWritten = 0;
	UInt32 eventCapacity = 0;
	UInt32 frameCapacity = 0;
	UInt32 gpuCapacity = 0;
	UInt32 compositorCapacity = 0;
	// A ring keeps only what started within its seconds before the end.
	UInt64 cutoffTick = 0;
	SessionInfo session;
	ProfileSettings settings;
	UInt32 gpuPoolFull = 0;
	UInt32 gpuPending = 0;
	UInt32 gpuErrors = 0;
	bool truncatedCapacity = false;
	char stopReason[32] = {};
	char directory[512] = {};
	Profiler* owner = nullptr;
};

namespace {
// The crash's and the shutdown's job: no heap in the crash filter.
Profiler::WriterJob* SyncJob() {
	static Profiler::WriterJob job;
	return &job;
}
} // namespace

Profiler& Profiler::Instance() { return g_profiler; }

Profiler::~Profiler() { Shutdown(); }

void Profiler::Configure(ProfileSettings settings) {
	settings = NormalizeSettings(settings);
	const bool captureConfigChanged = (m_capturing || m_startPending) &&
		(settings.gpuTiming != m_settings.gpuTiming ||
		 settings.captureSeconds != m_settings.captureSeconds ||
		 settings.maxRecords != m_settings.maxRecords ||
		 settings.startAfterSeconds != m_settings.startAfterSeconds ||
		 settings.ring != m_settings.ring || settings.ringSeconds != m_settings.ringSeconds);
	if (captureConfigChanged && m_capturing) StopCapture("config_changed");
	const bool rising = settings.enabled && !m_configuredEnabled;
	m_configuredEnabled = settings.enabled;
	m_settings = settings;
	if (!settings.enabled) {
		m_startPending = false;
		if (m_capturing) StopCapture("disabled");
		return;
	}
	if ((rising || captureConfigChanged) && !m_capturing) {
		// Armed with a delay, the capture begins from OnPresentBegin once the
		// seconds have passed (ProfileLogic.h, startAfterSeconds).
		if (settings.startAfterSeconds > 0) {
			if (m_qpcFrequency == 0) m_qpcFrequency = static_cast<UInt64>(ReadPerformanceFrequency());
			m_startPending = true;
			m_startArmedTick = Tick();
			OBVR_LOG("Performance: capture armed, it starts in %u seconds", settings.startAfterSeconds);
		} else if (m_writerBusy.load(std::memory_order_acquire)) {
			OBVR_LOG("Performance: capture request is busy while the previous export is writing");
		} else {
			StartCapture();
		}
	}
}

void Profiler::StartCapture() {
	FinishWriter();
	m_events = new (std::nothrow) CpuEvent[m_settings.maxRecords];
	m_frames = new (std::nothrow) FrameRecord[FrameCapacity(m_settings.maxRecords)];
	m_gpuSamples = new (std::nothrow) GpuSample[GpuCapacity(m_settings.maxRecords, m_settings.gpuTiming)];
	m_compositorTimings =
	    new (std::nothrow) CompositorTimingRecord[CompositorTimingCapacity(m_settings.maxRecords)];
	if (m_events == nullptr || m_frames == nullptr || m_gpuSamples == nullptr ||
	    m_compositorTimings == nullptr) {
		delete[] m_events;
		delete[] m_frames;
		delete[] m_gpuSamples;
		delete[] m_compositorTimings;
		m_events = nullptr;
		m_frames = nullptr;
		m_gpuSamples = nullptr;
		m_compositorTimings = nullptr;
		OBVR_LOG("Performance: capture refused, memory allocation failed");
		return;
	}
	m_sessionSequence = g_nextSession++;
	m_generation++;
	m_eventCount = m_frameCount = m_gpuCount = m_compositorCount = 0;
	m_lastCompositorFrameIndex = 0;
	m_compositorSeen = false;
	m_gpuPoolFull = m_gpuPending = m_gpuErrors = 0;
	m_nextEventId = m_nextSampleId = 1;
	m_previousPresentTick = 0;
	m_captureStartTick = Tick();
	m_qpcFrequency = static_cast<UInt64>(ReadPerformanceFrequency());
	m_presentId = 0;
	m_vrFrameId = 0;
	m_truncated = false;
	m_stopRequested = false;
	m_frameOpen = false;
	m_gpuDevice = nullptr;
	m_gpuOpen = false;
	m_gpuQueries.Shutdown();
	m_capturing = m_qpcFrequency != 0;
	if (!m_capturing) {
		OBVR_LOG("Performance: capture refused, QPC frequency is zero");
		delete[] m_events; delete[] m_frames; delete[] m_gpuSamples; delete[] m_compositorTimings;
		m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr; m_compositorTimings = nullptr;
		return;
	}
	OBVR_LOG("Performance: capture %llu started, %s %u seconds (%u events, %u frames, %u GPU samples)",
	         static_cast<unsigned long long>(m_sessionSequence),
	         m_settings.ring ? "a ring of the last" : "a window of",
	         m_settings.ring ? m_settings.ringSeconds : m_settings.captureSeconds,
	         m_settings.maxRecords, FrameCapacity(m_settings.maxRecords),
	         GpuCapacity(m_settings.maxRecords, m_settings.gpuTiming));
}

bool Profiler::PrepareJob(WriterJob& job, const char* reason) {
	m_stopping = true;
	// The folder is this capture's writer's to name; until it has, the
	// sampler (render/RenderSampler.cpp) must not find the previous one.
	m_exportDirectory[0] = '\0';
	OnPresentEnd();
	// At a crash the D3D device is not called again (its queries are left
	// as they are); the samples already read are kept.
	if (m_gpuQueries.IsUsable() && !m_crashed) {
		m_gpuQueries.Stop();
	}
	if (m_gpuQueries.IsUsable()) m_gpuCount = m_gpuQueries.WrittenCount();
	const UInt64 endTick = Tick();
	job.events = m_events; job.frames = m_frames; job.gpuSamples = m_gpuSamples;
	job.compositorTimings = m_compositorTimings;
	job.eventWritten = m_eventCount; job.frameWritten = m_frameCount; job.gpuWritten = m_gpuCount;
	job.compositorWritten = m_compositorCount;
	job.eventCapacity = m_settings.maxRecords;
	job.frameCapacity = FrameCapacity(m_settings.maxRecords);
	job.gpuCapacity = GpuCapacity(m_settings.maxRecords, m_settings.gpuTiming);
	job.compositorCapacity = CompositorTimingCapacity(m_settings.maxRecords);
	job.cutoffTick = m_settings.ring ? RingCutoffTick(endTick, m_settings.ringSeconds, m_qpcFrequency) : 0;
	job.settings = m_settings;
	job.gpuPoolFull = m_gpuPoolFull; job.gpuPending = m_gpuPending; job.gpuErrors = m_gpuErrors;
	job.truncatedCapacity = m_truncated;
	CopyText(job.stopReason, sizeof(job.stopReason), reason);
	job.session.sessionId = m_sessionSequence;
	job.session.schemaVersion = kProfileSchemaVersion;
	job.session.qpcFrequency = m_qpcFrequency;
	job.session.startTick = m_settings.ring && job.cutoffTick > m_captureStartTick ? job.cutoffTick
	                                                                              : m_captureStartTick;
	job.session.endTick = endTick;
	job.session.generation = m_generation;
	job.directory[0] = '\0';
	char base[512] = {};
	char directoryName[96] = {};
	std::snprintf(directoryName, sizeof(directoryName), "OBVR-performance-%llu",
	              static_cast<unsigned long long>(m_sessionSequence));
	if (platform::BuildGamePath("OBVR.log", base, sizeof(base))) {
		UInt32 cut = 0;
		while (base[cut] != '\0') ++cut;
		while (cut > 0 && base[cut - 1] != '\\' && base[cut - 1] != '/') --cut;
		if (cut < sizeof(job.directory)) {
			for (UInt32 i = 0; i < cut; ++i) job.directory[i] = base[i];
			job.directory[cut] = '\0';
		}
	}
	char sessionPath[512] = {};
	const bool placed = job.directory[0] != '\0' &&
	                    JoinPath(job.directory, directoryName, sessionPath, sizeof(sessionPath));
	if (placed) CopyText(job.directory, sizeof(job.directory), sessionPath);
	job.owner = this;
	// The tables are the job's now.
	m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr; m_compositorTimings = nullptr;
	if (!m_crashed) m_gpuQueries.Shutdown();
	m_gpuDevice = nullptr;
	m_gpuOpen = false;
	m_capturing = false;
	m_stopRequested = false;
	m_stopping = false;
	if (!placed) {
		OBVR_LOG("Performance: capture %llu stopped (%s), output directory unavailable",
		         static_cast<unsigned long long>(job.session.sessionId), reason);
	}
	return placed;
}

void Profiler::StopCapture(const char* reason) {
	if (!m_capturing) return;
	WriterJob* job = new (std::nothrow) WriterJob{};
	if (job == nullptr) {
		OBVR_LOG("Performance: capture stopped (%s), writer allocation failed", reason);
		delete[] m_events; delete[] m_frames; delete[] m_gpuSamples; delete[] m_compositorTimings;
		m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr; m_compositorTimings = nullptr;
		m_capturing = false;
		m_stopping = false;
		return;
	}
	if (!PrepareJob(*job, reason)) {
		delete[] job->events; delete[] job->frames; delete[] job->gpuSamples; delete[] job->compositorTimings;
		delete job;
		return;
	}
	// Busy before the capture reads as over: the sampler waits on the busy
	// flag for this capture's folder, and reads the capture's end first.
	m_writerBusy.store(true, std::memory_order_release);
	HANDLE thread = CreateThread(nullptr, 0, &Profiler::WriterThunk, job, 0, nullptr);
	if (thread == nullptr) {
		OBVR_LOG("Performance: capture %llu stopped, writer thread unavailable; export skipped",
		         static_cast<unsigned long long>(job->session.sessionId));
		delete[] job->events; delete[] job->frames; delete[] job->gpuSamples; delete[] job->compositorTimings;
		delete job;
		m_writerBusy.store(false, std::memory_order_release);
		return;
	}
	m_writerHandle = thread;
	OBVR_LOG("Performance: capture %llu stopped (%s), export queued",
	         static_cast<unsigned long long>(job->session.sessionId), reason);
}

void Profiler::SaveOnCrash() {
	if (m_crashed) return;
	m_crashed = true;
	m_startPending = false;
	// A writer still busy owns its own tables; what records now is another
	// capture's and is written beside it.
	if (!m_capturing) return;
	WriterJob& job = *SyncJob();
	if (!PrepareJob(job, "crash")) return;
	OBVR_LOG("Performance: capture %llu written at the crash, in this thread",
	         static_cast<unsigned long long>(job.session.sessionId));
	WriteToFreeFolder(job);  // the tables stay allocated: the heap is not trusted here
}

void Profiler::FinishWriter() {
	// StartCapture only calls this at a configuration edge. A running writer
	// is allowed to finish; a new request is reported as busy rather than
	// touching its immutable buffers. Shutdown handles the thread explicitly.
	if (m_writerBusy.load(std::memory_order_acquire)) return;
	if (m_writerHandle != nullptr) {
		WaitForSingleObject(m_writerHandle, 0);
		CloseHandle(m_writerHandle);
		m_writerHandle = nullptr;
	}
}

void Profiler::Shutdown() {
	if (m_capturing && !m_crashed) {
		// In this thread: at DLL detach a new writer thread would wait on the
		// loader lock this thread holds (the ring always records at the end).
		WriterJob& job = *SyncJob();
		if (PrepareJob(job, "shutdown")) {
			WriteToFreeFolder(job);
		}
		delete[] job.events; delete[] job.frames; delete[] job.gpuSamples; delete[] job.compositorTimings;
		job.events = nullptr; job.frames = nullptr; job.gpuSamples = nullptr; job.compositorTimings = nullptr;
	}
	if (m_writerHandle != nullptr) {
		// The writer only touches its detached buffers and Win32 file handles.
		// Waiting is confined to shutdown and never occurs in a render callback.
		WaitForSingleObject(m_writerHandle, 0xFFFFFFFFu);
		CloseHandle(m_writerHandle);
		m_writerHandle = nullptr;
	}
	FinishWriter();
}

bool Profiler::AppendEvent(const CpuEvent& event) {
	if (!PutRecord(m_events, m_settings.maxRecords, m_eventCount, m_settings.ring, event)) {
		m_truncated = true;
		m_stopRequested = true;
		return false;
	}
	return true;
}

void Profiler::AddCompositorTiming(CompositorTimingRecord record) {
	if (!m_capturing || m_compositorTimings == nullptr ||
	    (!m_settings.ring && m_compositorCount >= CompositorTimingCapacity(m_settings.maxRecords))) {
		return;
	}
	if (m_compositorSeen && record.frameIndex <= m_lastCompositorFrameIndex) {
		return;  // read before, or older than one read before
	}
	m_compositorSeen = true;
	m_lastCompositorFrameIndex = record.frameIndex;
	record.presentId = m_presentId;
	record.vrFrameId = m_vrFrameId;
	PutRecord(m_compositorTimings, CompositorTimingCapacity(m_settings.maxRecords), m_compositorCount,
	          m_settings.ring, record);
}

bool Profiler::AppendFrame(const FrameRecord& frame) {
	if (!PutRecord(m_frames, FrameCapacity(m_settings.maxRecords), m_frameCount, m_settings.ring, frame)) {
		m_truncated = true;
		m_stopRequested = true;
		return false;
	}
	return true;
}

void Profiler::OnPresentBegin(DeliveryMode requested, bool setup) {
	const bool busy = m_writerBusy.load(std::memory_order_acquire);
	switch (NextSaveStep(m_saveRequested, m_capturing, busy)) {
	case SaveStep::Save:
		m_saveRequested = false;
		StopCapture("saved");
		return;
	case SaveStep::Busy:
		m_saveRequested = false;
		OBVR_LOG("Performance: save asked while the last one still writes - nothing new to save yet");
		break;
	case SaveStep::Ignore:
		m_saveRequested = false;
		break;
	}
	if (RingRestartDue(m_settings.ring, m_configuredEnabled, m_capturing, busy, m_crashed || m_ringGaveUp) &&
	    !m_startPending) {
		FinishWriter();
		StartCapture();
		// A ring that cannot allocate says so once, not every frame.
		m_ringGaveUp = !m_capturing;
		return;
	}
	if (!m_capturing) {
		if (m_startPending &&
		    CaptureStartDue(m_startArmedTick, Tick(), m_settings.startAfterSeconds, m_qpcFrequency)) {
			m_startPending = false;
			if (m_writerBusy.load(std::memory_order_acquire)) {
				OBVR_LOG("Performance: capture request is busy while the previous export is writing");
			} else {
				StartCapture();
			}
		}
		return;
	}
	const UInt64 now = Tick();
	const UInt64 durationTicks = static_cast<UInt64>(m_settings.captureSeconds) * m_qpcFrequency;
	if (!m_settings.ring && now >= m_captureStartTick && now - m_captureStartTick >= durationTicks) {
		StopCapture("duration");
		return;
	}
	if (m_gpuQueries.IsUsable()) {
		m_gpuQueries.Poll(m_presentId + 1, 4);
		m_gpuCount = m_gpuQueries.WrittenCount();
		if (!m_settings.ring && m_gpuCount >= GpuCapacity(m_settings.maxRecords, m_settings.gpuTiming)) {
			m_truncated = true;
			m_stopRequested = true;
		}
	}
	++m_presentId;
	m_currentFrame = FrameRecord{};
	m_currentFrame.presentId = m_presentId;
	m_currentFrame.startTick = now;
	m_currentFrame.intervalTick = m_previousPresentTick == 0 ? 0 : now - m_previousPresentTick;
	m_currentFrame.requestedMode = requested;
	m_currentFrame.setup = setup ? 1 : 0;
	m_previousPresentTick = now;
	m_frameOpen = true;
	EventContext context{};
	context.presentId = m_presentId;
	m_presentSpan = Begin(EventType::Present, context);
}

void Profiler::SetFrameDetails(DeliveryMode delivered, UInt64 vrFrameId, SInt32 poseResult,
	                             UInt8 leftCaptured, UInt8 rightCaptured, UInt8 passCount) {
	if (!m_capturing || !m_frameOpen) return;
	m_vrFrameId = vrFrameId;
	m_currentFrame.vrFrameId = vrFrameId;
	m_currentFrame.deliveredMode = delivered;
	if (poseResult != -1) m_currentFrame.poseResult = poseResult;
	if (leftCaptured != 0xFF) m_currentFrame.leftCaptured = leftCaptured;
	if (rightCaptured != 0xFF) m_currentFrame.rightCaptured = rightCaptured;
	if (passCount != 0xFF) m_currentFrame.passCount = passCount;
}

void Profiler::SetSubmitResult(UInt8 eye, SInt32 result) {
	if (!m_capturing || !m_frameOpen) return;
	if (eye == 0) m_currentFrame.submitLeft = result;
	if (eye == 1) m_currentFrame.submitRight = result;
}

void Profiler::OnPresentEnd() {
	if (!m_capturing || !m_frameOpen) return;
	const UInt64 now = Tick();
	m_currentFrame.endTick = now;
	AppendFrame(m_currentFrame);
	End(m_presentSpan);
	m_frameOpen = false;
	if (m_stopRequested && !m_stopping) StopCapture("truncated_capacity");
}

UInt64 Profiler::BeginVrFrame() {
	if (!m_capturing) return 0;
	return ++m_vrFrameId;
}

Profiler::Span Profiler::Begin(EventType type, const EventContext& context) {
	Span span{};
	if (!m_capturing) return span;
	span.slot = static_cast<UInt32>(m_eventCount);
	span.startTick = Tick();
	span.type = type;
	span.context = context;
	span.context.presentId = context.presentId == 0 ? m_presentId : context.presentId;
	span.active = true;
	return span;
}

void Profiler::End(Span& span) {
	if (!span.active) return;
	const UInt64 end = Tick();
	CpuEvent event{};
	event.eventId = m_nextEventId++;
	event.type = span.type;
	event.context = span.context;
	event.startTick = span.startTick;
	event.endTick = end;
	event.status = end >= span.startTick ? EventStatus::Complete : EventStatus::InvalidTicks;
	AppendEvent(event);
	span.active = false;
}

void Profiler::Mark(EventType type, UInt64 startTick, UInt64 endTick,
	                  const EventContext& context) {
	if (!m_capturing) return;
	CpuEvent event{};
	event.eventId = m_nextEventId++;
	event.type = type;
	event.context = context;
	event.context.presentId = context.presentId == 0 ? m_presentId : context.presentId;
	event.startTick = startTick;
	event.endTick = endTick;
	event.status = endTick >= startTick ? EventStatus::Complete : EventStatus::InvalidTicks;
	AppendEvent(event);
}

void Profiler::AddGpuSample(const GpuSample& sample) {
	if (!m_capturing) return;
	if (!PutRecord(m_gpuSamples, GpuCapacity(m_settings.maxRecords, m_settings.gpuTiming), m_gpuCount,
	               m_settings.ring, sample)) {
		m_truncated = true;
		m_stopRequested = true;
		return;
	}
	if (sample.status == GpuStatus::Pending) ++m_gpuPending;
}

void Profiler::GpuBegin(EventType type, const EventContext& context, void* device) {
	if (!m_capturing || !m_settings.gpuTiming || m_gpuOpen || device == nullptr) return;
	if (m_gpuDevice != device) {
		m_gpuQueries.Shutdown();
		m_gpuDevice = device;
		QueryOps ops = MakeD3D9TimestampOps(device);
		if (!m_gpuQueries.Initialize(ops, m_gpuSamples, GpuCapacity(m_settings.maxRecords, m_settings.gpuTiming))) {
			++m_gpuErrors;
			OBVR_LOG("Performance: GPU timing unavailable for this D3D9 device generation");
			return;
		}
		m_gpuQueries.SetRing(m_settings.ring);
		OBVR_LOG("Performance: D3D9 timestamp timing enabled");
	}
	GpuSample identity{};
	identity.sampleId = m_nextSampleId++;
	identity.presentId = context.presentId == 0 ? m_presentId : context.presentId;
	identity.vrFrameId = context.vrFrameId;
	identity.type = type;
	identity.passIndex = context.passIndex;
	identity.eye = context.eye;
	if (!m_gpuQueries.Begin(identity)) {
		++m_gpuPoolFull;
		return;
	}
	m_gpuOpen = true;
}

void Profiler::GpuEnd() {
	if (!m_gpuOpen) return;
	if (!m_gpuQueries.End()) ++m_gpuErrors;
	m_gpuOpen = false;
}

void Profiler::NoteGpuCounter(GpuStatus status) {
	if (status == GpuStatus::PoolFull) ++m_gpuPoolFull;
	if (status == GpuStatus::Pending || status == GpuStatus::PendingAtStop) ++m_gpuPending;
	if (status != GpuStatus::Ready && status != GpuStatus::Pending && status != GpuStatus::PendingAtStop &&
	    status != GpuStatus::PoolFull) ++m_gpuErrors;
}

void Profiler::WriteToFreeFolder(WriterJob& job) {
	// The session's own name first, then "-2", "-3" ... while the name is
	// taken - an earlier game's capture keeps its folder (ProfileLogic.h).
	char base[sizeof(job.directory)] = {};
	CopyText(base, sizeof(base), job.directory);
	for (UInt32 attempt = 0;; ++attempt) {
		if (!CaptureDirectoryCandidate(base, attempt, job.directory, sizeof(job.directory))) {
			OBVR_LOG("Performance: export partial (output directory unavailable)");
			return;
		}
		const bool created = CreateDirectoryA(job.directory, nullptr) != 0;
		const bool exists = !created && GetLastError() == ERROR_ALREADY_EXISTS;
		const DirectoryStep step = NextDirectoryStep(created, exists, attempt);
		if (step == DirectoryStep::Write) {
			if (attempt > 0) {
				OBVR_LOG("Performance: an earlier capture's folder has the session's name - writing to %s",
				         job.directory);
			}
			// Published before the busy flag drops (the thunk's release): the
			// sampler's rows follow the capture into this folder.
			if (job.owner != nullptr) {
				CopyText(job.owner->m_exportDirectory, sizeof(job.owner->m_exportDirectory), job.directory);
			}
			WriteJob(job);
			return;
		}
		if (step == DirectoryStep::GiveUp) {
			OBVR_LOG("Performance: export partial (output directory unavailable: %s, %s)", job.directory,
			         exists ? "every free name tried" : "not creatable");
			return;
		}
	}
}

DWORD __stdcall Profiler::WriterThunk(void* parameter) {
	WriterJob* job = static_cast<WriterJob*>(parameter);
	WriteToFreeFolder(*job);
	Profiler* owner = job->owner;
	delete[] job->events;
	delete[] job->frames;
	delete[] job->gpuSamples;
	delete[] job->compositorTimings;
	delete job;
	if (owner != nullptr) owner->m_writerBusy.store(false, std::memory_order_release);
	return 0;
}

void Profiler::WriteJob(WriterJob& job) {
	char path[512] = {};
	HANDLE file = InvalidHandle();
	// A ring keeps what started at or after the cutoff, and the GPU and
	// compositor rows of those frames (by the first kept frame's present).
	UInt64 firstPresentKept = 0;
	const UInt32 frameStored = RingStoredCount(job.frameWritten, job.frameCapacity);
	for (UInt32 i = 0; i < frameStored; ++i) {
		const FrameRecord& f = job.frames[RingOrderedSlot(job.frameWritten, job.frameCapacity, i)];
		if (f.startTick >= job.cutoffTick) {
			firstPresentKept = f.presentId;
			break;
		}
	}
	const UInt64 cutoff = job.cutoffTick;
	const auto byPresent = [&](UInt64 presentId) { return cutoff == 0 || presentId >= firstPresentKept; };
	UInt32 eventCount = 0;
	UInt32 frameCount = 0;
	UInt32 gpuCount = 0;
	UInt32 compositorCount = 0;
	bool exportOk = WriteTable(
	    job.directory, "cpu_events.csv",
	    "event_id,event_type,present_id,scene_id,vr_frame_id,pass_index,eye,mode,start_tick,end_tick,status\n",
	    job.events, job.eventWritten, job.eventCapacity, eventCount,
	    [&](const CpuEvent& e) { return e.startTick >= cutoff; },
	    [](const CpuEvent& e, char* line, UInt32 size) {
		    return std::snprintf(line, size, "%llu,%s,%llu,%llu,%llu,%u,%u,%s,%llu,%llu,%u",
		                         static_cast<unsigned long long>(e.eventId), EventTypeName(e.type),
		                         static_cast<unsigned long long>(e.context.presentId),
		                         static_cast<unsigned long long>(e.context.sceneId),
		                         static_cast<unsigned long long>(e.context.vrFrameId), e.context.passIndex,
		                         e.context.eye, DeliveryModeName(e.context.mode),
		                         static_cast<unsigned long long>(e.startTick),
		                         static_cast<unsigned long long>(e.endTick), static_cast<UInt32>(e.status));
	    });
	exportOk = WriteTable(
	               job.directory, "frames.csv",
	               "present_id,vr_frame_id,start_tick,end_tick,interval_tick,requested_mode,delivered_mode,pose_result,"
	               "left_captured,right_captured,pass_count,setup,status,submit_left,submit_right\n",
	               job.frames, job.frameWritten, job.frameCapacity, frameCount,
	               [&](const FrameRecord& f) { return f.startTick >= cutoff; },
	               [](const FrameRecord& f, char* line, UInt32 size) {
		               return std::snprintf(line, size, "%llu,%llu,%llu,%llu,%llu,%s,%s,%d,%u,%u,%u,%u,%u,%d,%d",
		                                    static_cast<unsigned long long>(f.presentId),
		                                    static_cast<unsigned long long>(f.vrFrameId),
		                                    static_cast<unsigned long long>(f.startTick),
		                                    static_cast<unsigned long long>(f.endTick),
		                                    static_cast<unsigned long long>(f.intervalTick),
		                                    DeliveryModeName(f.requestedMode), DeliveryModeName(f.deliveredMode),
		                                    f.poseResult, f.leftCaptured, f.rightCaptured, f.passCount, f.setup,
		                                    f.status, f.submitLeft, f.submitRight);
	               }) &&
	           exportOk;
	exportOk = WriteTable(
	               job.directory, "gpu_samples.csv",
	               "sample_id,present_id,vr_frame_id,event_type,pass_index,eye,status,start_tick,end_tick,frequency,"
	               "age_presents\n",
	               job.gpuSamples, job.gpuWritten, job.gpuCapacity, gpuCount,
	               [&](const GpuSample& g) { return byPresent(g.presentId); },
	               [](const GpuSample& g, char* line, UInt32 size) {
		               return std::snprintf(line, size, "%llu,%llu,%llu,%s,%u,%u,%s,%llu,%llu,%llu,%u",
		                                    static_cast<unsigned long long>(g.sampleId),
		                                    static_cast<unsigned long long>(g.presentId),
		                                    static_cast<unsigned long long>(g.vrFrameId), EventTypeName(g.type),
		                                    g.passIndex, g.eye, GpuStatusName(g.status),
		                                    static_cast<unsigned long long>(g.startTick),
		                                    static_cast<unsigned long long>(g.endTick),
		                                    static_cast<unsigned long long>(g.frequency), g.agePresents);
	               }) &&
	           exportOk;
	exportOk = WriteTable(job.directory, "compositor.csv", kCompositorTimingCsvHeader, job.compositorTimings,
	                      job.compositorWritten, job.compositorCapacity, compositorCount,
	                      [&](const CompositorTimingRecord& c) { return byPresent(c.presentId); },
	                      [](const CompositorTimingRecord& c, char* line, UInt32 size) {
		                      return CompositorTimingCsvLine(line, size, c);
	                      }) &&
	           exportOk;

	if (OpenOutput(job.directory, "manifest.json", path, sizeof(path), file)) {
		const char* status = exportOk ? (job.truncatedCapacity ? "truncated_capacity" : "complete") : "partial";
		char text[1536] = {};
		std::snprintf(text, sizeof(text),
		              "{\n  \"schema_version\":%u,\n  \"session_id\":%llu,\n"
		              "  \"generation\":%u,\n  \"qpc_frequency\":%llu,\n"
		              "  \"start_tick\":%llu,\n  \"end_tick\":%llu,\n"
		              "  \"capture_seconds\":%u,\n  \"max_records\":%u,\n"
		              "  \"mode\":\"%s\",\n  \"ring_seconds\":%u,\n"
		              "  \"gpu_timing\":%s,\n"
		              "  \"event_count\":%u,\n  \"frame_count\":%u,\n"
		              "  \"gpu_count\":%u,\n  \"gpu_pool_full\":%u,\n"
		              "  \"gpu_pending\":%u,\n  \"gpu_errors\":%u,\n"
		              "  \"compositor_count\":%u,\n"
			              "  \"truncated_capacity\":%s,\n"
			              "  \"status\":\"%s\",\n  \"stop_reason\":\"%s\",\n"
			              "  \"runtime_measurement\":true\n}\n",
		              job.session.schemaVersion,
		              static_cast<unsigned long long>(job.session.sessionId), job.session.generation,
		              static_cast<unsigned long long>(job.session.qpcFrequency),
		              static_cast<unsigned long long>(job.session.startTick),
		              static_cast<unsigned long long>(job.session.endTick),
		              job.settings.captureSeconds, job.settings.maxRecords,
		              job.settings.ring ? "ring" : "window", job.settings.ringSeconds,
		              job.settings.gpuTiming ? "true" : "false", eventCount, frameCount,
		              gpuCount, job.gpuPoolFull, job.gpuPending, job.gpuErrors, compositorCount,
		              job.truncatedCapacity ? "true" : "false", status, job.stopReason);
		const bool ok = WriteText(file, text);
		CloseOutput(file);
		if (ok && CommitOutput(job.directory, "manifest.json", path)) {
			OBVR_LOG("Performance: export %s (%u events, %u frames, %u GPU samples)",
			         status, eventCount, frameCount, gpuCount);
		} else {
			OBVR_LOG("Performance: export partial (manifest write failed)");
		}
	} else {
		OBVR_LOG("Performance: export partial (manifest unavailable)");
	}
}

} // namespace obvr::perf

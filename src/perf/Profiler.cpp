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

bool JoinPath(const char* directory, const char* name, char* output, UInt32 capacity) {
	UInt32 at = 0;
	while (directory != nullptr && directory[at] != '\0') {
		if (at + 1 >= capacity) return false;
		output[at] = directory[at];
		++at;
	}
	if (at > 0 && output[at - 1] != '\\' && output[at - 1] != '/') {
		if (at + 1 >= capacity) return false;
		output[at++] = '\\';
	}
	for (UInt32 i = 0; name != nullptr && name[i] != '\0'; ++i) {
		if (at + 1 >= capacity) return false;
		output[at++] = name[i];
	}
	output[at] = '\0';
	return true;
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

} // namespace

struct Profiler::WriterJob {
	CpuEvent* events = nullptr;
	FrameRecord* frames = nullptr;
	GpuSample* gpuSamples = nullptr;
	UInt32 eventCount = 0;
	UInt32 frameCount = 0;
	UInt32 gpuCount = 0;
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

Profiler& Profiler::Instance() { return g_profiler; }

Profiler::~Profiler() { Shutdown(); }

void Profiler::Configure(ProfileSettings settings) {
	settings = NormalizeSettings(settings);
	const bool captureConfigChanged = m_capturing &&
		(settings.gpuTiming != m_settings.gpuTiming ||
		 settings.captureSeconds != m_settings.captureSeconds ||
		 settings.maxRecords != m_settings.maxRecords);
	if (captureConfigChanged) StopCapture("config_changed");
	const bool rising = settings.enabled && !m_configuredEnabled;
	m_configuredEnabled = settings.enabled;
	m_settings = settings;
	if (!settings.enabled) {
		if (m_capturing) StopCapture("disabled");
		return;
	}
	if ((rising || captureConfigChanged) && !m_capturing) {
		if (m_writerBusy.load(std::memory_order_acquire)) {
			OBVR_LOG("Performance: capture request is busy while the previous export is writing");
		} else {
			StartCapture();
		}
	}
}

void Profiler::StartCapture() {
	FinishWriter();
	m_events = new (std::nothrow) CpuEvent[m_settings.maxRecords];
	m_frames = new (std::nothrow) FrameRecord[m_settings.maxRecords];
	m_gpuSamples = new (std::nothrow) GpuSample[m_settings.maxRecords];
	if (m_events == nullptr || m_frames == nullptr || m_gpuSamples == nullptr) {
		delete[] m_events;
		delete[] m_frames;
		delete[] m_gpuSamples;
		m_events = nullptr;
		m_frames = nullptr;
		m_gpuSamples = nullptr;
		OBVR_LOG("Performance: capture refused, memory allocation failed");
		return;
	}
	m_sessionSequence = g_nextSession++;
	m_generation++;
	m_eventCount = m_frameCount = m_gpuCount = 0;
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
		delete[] m_events; delete[] m_frames; delete[] m_gpuSamples;
		m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr;
		return;
	}
	OBVR_LOG("Performance: capture %llu started for %u seconds (%u records)",
	         static_cast<unsigned long long>(m_sessionSequence), m_settings.captureSeconds,
	         m_settings.maxRecords);
}

void Profiler::StopCapture(const char* reason) {
	if (!m_capturing) return;
	m_stopping = true;
	OnPresentEnd();
	if (m_gpuQueries.IsUsable()) {
		m_gpuQueries.Stop();
		m_gpuCount = m_gpuQueries.OutputCount();
	}
	const UInt64 endTick = Tick();
	WriterJob* job = new (std::nothrow) WriterJob{};
	if (job == nullptr) {
		OBVR_LOG("Performance: capture stopped (%s), writer allocation failed", reason);
		delete[] m_events; delete[] m_frames; delete[] m_gpuSamples;
		m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr;
		m_capturing = false;
		m_stopping = false;
		return;
	}
	job->events = m_events; job->frames = m_frames; job->gpuSamples = m_gpuSamples;
	job->eventCount = m_eventCount; job->frameCount = m_frameCount; job->gpuCount = m_gpuCount;
	job->settings = m_settings;
	job->gpuPoolFull = m_gpuPoolFull; job->gpuPending = m_gpuPending; job->gpuErrors = m_gpuErrors;
	job->truncatedCapacity = m_truncated;
	CopyText(job->stopReason, sizeof(job->stopReason), reason);
	job->session.sessionId = m_sessionSequence;
	job->session.schemaVersion = kProfileSchemaVersion;
	job->session.qpcFrequency = m_qpcFrequency;
	job->session.startTick = m_captureStartTick;
	job->session.endTick = endTick;
	job->session.generation = m_generation;
	char base[512] = {};
	char directoryName[96] = {};
	std::snprintf(directoryName, sizeof(directoryName), "OBVR-performance-%llu",
	              static_cast<unsigned long long>(m_sessionSequence));
	if (platform::BuildGamePath("OBVR.log", base, sizeof(base))) {
		UInt32 cut = 0;
		while (base[cut] != '\0') ++cut;
		while (cut > 0 && base[cut - 1] != '\\' && base[cut - 1] != '/') --cut;
		if (cut < sizeof(job->directory)) {
			for (UInt32 i = 0; i < cut; ++i) job->directory[i] = base[i];
			job->directory[cut] = '\0';
		}
	}
	char sessionPath[512] = {};
	if (job->directory[0] == '\0' ||
	    !JoinPath(job->directory, directoryName, sessionPath, sizeof(sessionPath))) {
		OBVR_LOG("Performance: capture %llu stopped (%s), output directory unavailable",
		         static_cast<unsigned long long>(job->session.sessionId), reason);
		delete[] job->events;
		delete[] job->frames;
		delete[] job->gpuSamples;
		delete job;
		m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr;
		m_gpuQueries.Shutdown();
		m_gpuDevice = nullptr;
		m_gpuOpen = false;
		m_capturing = false;
		m_stopping = false;
		return;
	}
	CopyText(job->directory, sizeof(job->directory), sessionPath);
	job->owner = this;
	m_events = nullptr; m_frames = nullptr; m_gpuSamples = nullptr;
	m_gpuQueries.Shutdown();
	m_gpuDevice = nullptr;
	m_gpuOpen = false;
	m_capturing = false;
	m_stopRequested = false;
	m_stopping = false;
	m_writerBusy.store(true, std::memory_order_release);
	HANDLE thread = CreateThread(nullptr, 0, &Profiler::WriterThunk, job, 0, nullptr);
	if (thread == nullptr) {
		OBVR_LOG("Performance: capture %llu stopped, writer thread unavailable; export skipped",
		         static_cast<unsigned long long>(job->session.sessionId));
		delete[] job->events;
		delete[] job->frames;
		delete[] job->gpuSamples;
		delete job;
		m_writerBusy.store(false, std::memory_order_release);
		return;
	}
	m_writerHandle = thread;
	OBVR_LOG("Performance: capture %llu stopped (%s), export queued",
	         static_cast<unsigned long long>(job->session.sessionId), reason);
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
	if (m_capturing) StopCapture("shutdown");
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
	if (m_eventCount >= m_settings.maxRecords) {
		m_truncated = true;
		m_stopRequested = true;
		return false;
	}
	m_events[m_eventCount++] = event;
	return true;
}

bool Profiler::AppendFrame(const FrameRecord& frame) {
	if (m_frameCount >= m_settings.maxRecords) {
		m_truncated = true;
		m_stopRequested = true;
		return false;
	}
	m_frames[m_frameCount++] = frame;
	return true;
}

void Profiler::OnPresentBegin(DeliveryMode requested, bool setup) {
	if (!m_capturing) return;
	const UInt64 now = Tick();
	const UInt64 durationTicks = static_cast<UInt64>(m_settings.captureSeconds) * m_qpcFrequency;
	if (now >= m_captureStartTick && now - m_captureStartTick >= durationTicks) {
		StopCapture("duration");
		return;
	}
	if (m_gpuQueries.IsUsable()) {
		m_gpuQueries.Poll(m_presentId + 1, 4);
		m_gpuCount = m_gpuQueries.OutputCount();
		if (m_gpuCount >= m_settings.maxRecords) {
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
	span.slot = m_eventCount;
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
	if (m_gpuCount >= m_settings.maxRecords) {
		m_truncated = true;
		m_stopRequested = true;
		return;
	}
	m_gpuSamples[m_gpuCount++] = sample;
	if (sample.status == GpuStatus::Pending) ++m_gpuPending;
}

void Profiler::GpuBegin(EventType type, const EventContext& context, void* device) {
	if (!m_capturing || !m_settings.gpuTiming || m_gpuOpen || device == nullptr) return;
	if (m_gpuDevice != device) {
		m_gpuQueries.Shutdown();
		m_gpuDevice = device;
		QueryOps ops = MakeD3D9TimestampOps(device);
		if (!m_gpuQueries.Initialize(ops, m_gpuSamples, m_settings.maxRecords)) {
			++m_gpuErrors;
			OBVR_LOG("Performance: GPU timing unavailable for this D3D9 device generation");
			return;
		}
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

DWORD __stdcall Profiler::WriterThunk(void* parameter) {
	WriterJob* job = static_cast<WriterJob*>(parameter);
	if (CreateDirectoryA(job->directory, nullptr)) {
		WriteJob(*job);
	} else {
		OBVR_LOG("Performance: export partial (output directory unavailable)");
	}
	Profiler* owner = job->owner;
	delete[] job->events;
	delete[] job->frames;
	delete[] job->gpuSamples;
	delete job;
	if (owner != nullptr) owner->m_writerBusy.store(false, std::memory_order_release);
	return 0;
}

void Profiler::WriteJob(WriterJob& job) {
	char path[512] = {};
	HANDLE file = InvalidHandle();
	bool exportOk = true;

	if (OpenOutput(job.directory, "cpu_events.csv", path, sizeof(path), file)) {
		bool ok = WriteText(file, "event_id,event_type,present_id,scene_id,vr_frame_id,pass_index,eye,mode,start_tick,end_tick,status\n");
		char line[512] = {};
		for (UInt32 i = 0; i < job.eventCount; ++i) {
			const CpuEvent& e = job.events[i];
			std::snprintf(line, sizeof(line), "%llu,%s,%llu,%llu,%llu,%u,%u,%s,%llu,%llu,%u\n",
			              static_cast<unsigned long long>(e.eventId), EventTypeName(e.type),
			              static_cast<unsigned long long>(e.context.presentId),
			              static_cast<unsigned long long>(e.context.sceneId),
			              static_cast<unsigned long long>(e.context.vrFrameId), e.context.passIndex,
			              e.context.eye, DeliveryModeName(e.context.mode),
			              static_cast<unsigned long long>(e.startTick),
			              static_cast<unsigned long long>(e.endTick), static_cast<UInt32>(e.status));
			ok = WriteText(file, line) && ok;
		}
		CloseOutput(file);
		exportOk = CommitOutput(job.directory, "cpu_events.csv", path) && ok && exportOk;
	} else {
		exportOk = false;
	}

	if (OpenOutput(job.directory, "frames.csv", path, sizeof(path), file)) {
		bool ok = WriteText(file, "present_id,vr_frame_id,start_tick,end_tick,interval_tick,requested_mode,delivered_mode,pose_result,left_captured,right_captured,pass_count,setup,status,submit_left,submit_right\n");
		char line[512] = {};
		for (UInt32 i = 0; i < job.frameCount; ++i) {
			const FrameRecord& f = job.frames[i];
				std::snprintf(line, sizeof(line), "%llu,%llu,%llu,%llu,%llu,%s,%s,%d,%u,%u,%u,%u,%u,%d,%d\n",
			              static_cast<unsigned long long>(f.presentId),
			              static_cast<unsigned long long>(f.vrFrameId),
			              static_cast<unsigned long long>(f.startTick),
			              static_cast<unsigned long long>(f.endTick),
			              static_cast<unsigned long long>(f.intervalTick),
			              DeliveryModeName(f.requestedMode), DeliveryModeName(f.deliveredMode),
				              f.poseResult, f.leftCaptured, f.rightCaptured, f.passCount, f.setup,
				              f.status, f.submitLeft, f.submitRight);
			ok = WriteText(file, line) && ok;
		}
		CloseOutput(file);
		exportOk = CommitOutput(job.directory, "frames.csv", path) && ok && exportOk;
	} else {
		exportOk = false;
	}

	if (OpenOutput(job.directory, "gpu_samples.csv", path, sizeof(path), file)) {
		bool ok = WriteText(file, "sample_id,present_id,vr_frame_id,event_type,pass_index,eye,status,start_tick,end_tick,frequency,age_presents\n");
		char line[512] = {};
		for (UInt32 i = 0; i < job.gpuCount; ++i) {
			const GpuSample& g = job.gpuSamples[i];
			std::snprintf(line, sizeof(line), "%llu,%llu,%llu,%s,%u,%u,%s,%llu,%llu,%llu,%u\n",
			              static_cast<unsigned long long>(g.sampleId),
			              static_cast<unsigned long long>(g.presentId),
			              static_cast<unsigned long long>(g.vrFrameId), EventTypeName(g.type),
			              g.passIndex, g.eye, GpuStatusName(g.status),
			              static_cast<unsigned long long>(g.startTick),
			              static_cast<unsigned long long>(g.endTick),
			              static_cast<unsigned long long>(g.frequency), g.agePresents);
			ok = WriteText(file, line) && ok;
		}
		CloseOutput(file);
		exportOk = CommitOutput(job.directory, "gpu_samples.csv", path) && ok && exportOk;
	} else {
		exportOk = false;
	}

	if (OpenOutput(job.directory, "manifest.json", path, sizeof(path), file)) {
		const char* status = exportOk ? (job.truncatedCapacity ? "truncated_capacity" : "complete") : "partial";
		char text[1280] = {};
		std::snprintf(text, sizeof(text),
		              "{\n  \"schema_version\":%u,\n  \"session_id\":%llu,\n"
		              "  \"generation\":%u,\n  \"qpc_frequency\":%llu,\n"
		              "  \"start_tick\":%llu,\n  \"end_tick\":%llu,\n"
		              "  \"capture_seconds\":%u,\n  \"max_records\":%u,\n"
		              "  \"gpu_timing\":%s,\n"
		              "  \"event_count\":%u,\n  \"frame_count\":%u,\n"
		              "  \"gpu_count\":%u,\n  \"gpu_pool_full\":%u,\n"
		              "  \"gpu_pending\":%u,\n  \"gpu_errors\":%u,\n"
			              "  \"truncated_capacity\":%s,\n"
			              "  \"status\":\"%s\",\n  \"stop_reason\":\"%s\",\n"
			              "  \"runtime_measurement\":true\n}\n",
		              job.session.schemaVersion,
		              static_cast<unsigned long long>(job.session.sessionId), job.session.generation,
		              static_cast<unsigned long long>(job.session.qpcFrequency),
		              static_cast<unsigned long long>(job.session.startTick),
		              static_cast<unsigned long long>(job.session.endTick),
		              job.settings.captureSeconds, job.settings.maxRecords,
		              job.settings.gpuTiming ? "true" : "false", job.eventCount, job.frameCount,
		              job.gpuCount, job.gpuPoolFull, job.gpuPending, job.gpuErrors,
		              job.truncatedCapacity ? "true" : "false", status, job.stopReason);
		const bool ok = WriteText(file, text);
		CloseOutput(file);
		if (ok && CommitOutput(job.directory, "manifest.json", path)) {
			OBVR_LOG("Performance: export %s (%u events, %u frames, %u GPU samples)",
			         status, job.eventCount, job.frameCount, job.gpuCount);
		} else {
			OBVR_LOG("Performance: export partial (manifest write failed)");
		}
	} else {
		OBVR_LOG("Performance: export partial (manifest unavailable)");
	}
}

} // namespace obvr::perf

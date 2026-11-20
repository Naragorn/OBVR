// The frame's wait for its poses, and the late one ([Render] LatePoseWait,
// vr/PosePrediction.h): HeadsetRenderer's BeginFrame split at the wait
// (2026-11-20). Its own file so HeadsetRenderer.cpp keeps the size ratchet.

#include "core/Config.h"
#include "core/Log.h"
#include "perf/Profiler.h"
#include "platform/Win32Min.h"
#include "render/GameDevice.h"
#include "render/HeadsetRenderer.h"
#include "render/InteropBracket.h"
#include "render/SubmitPacing.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"
#include "vr/PosePrediction.h"

namespace obvr::render {

// The wait and what follows it: the pacing, the compositor's timing, the
// policy. keepPredicted: the poses were predicted before it (BeginFrameEarly)
// and stay the frame's.
bool HeadsetRenderer::WaitAndOpen(vr::OpenVRBackend& backend, UInt64 vrFrameId, bool keepPredicted) {
	// Blocks until the compositor wants the next frame. From here on Oblivion
	// runs on the compositor's clock.
	//
	// Under DXVK's queue lock, the same one the submits take. In the
	// compositor's default (implicit) timing mode WaitGetPoses is queue
	// work: openvr.h says the runtime records its GPU timestamp there, and
	// offers SetExplicitTimingMode for the case where "the application
	// needs to access the VkQueue from another thread while WaitGetPoses
	// is executing" - which is DXVK's case every frame, its CS thread
	// submitting on that queue whenever it likes. Two threads on one
	// VkQueue is what Vulkan forbids and what the NVIDIA driver answered
	// with "Graphics Exception: Class 0xc9c0 Subchannel 0x0 Mismatch",
	// a corrupted command stream, followed by VK_ERROR_DEVICE_LOST and a
	// game waiting for ever inside DXVK - dozens of times over a week, at
	// loading screens, in menus, after view switches. Without the lock
	// (no DXVK, or no device yet) the wait runs as it always did.
	perf::EventContext beginContext{};
	beginContext.vrFrameId = vrFrameId;
	m_frameOpen = false;
	int waited = 0;
	{
		InteropBracket poses;
		const bool held = poses.Begin(GetGameDevice(), BracketOwner::Poses);
		if (held != m_poseLockReported || !m_poseLockEverReported) {
			m_poseLockEverReported = true;
			m_poseLockReported = held;
			OBVR_LOG("Poses: WaitGetPoses runs %s", held
			         ? "under DXVK's queue lock - the compositor's timestamp cannot "
			           "collide with DXVK's own submissions"
			         : "without DXVK's queue lock - no interop device, so the "
			           "compositor's timestamp may collide with DXVK's submissions");
		}
		perf::Profiler::ScopedSpan poseWait(perf::Profiler::Instance(),
		                                perf::EventType::PosesWait, beginContext);
		waited = backend.WaitGetPoses(keepPredicted);
	}
	m_lastWaitEndTick = static_cast<UInt64>(ReadPerformanceCounter());
	NotePacing(backend, vrFrameId);
	RecordCompositorTiming(backend);
	perf::Profiler::Instance().SetFrameDetails(perf::DeliveryMode::Unknown, vrFrameId,
	                                           waited, 0xFF, 0xFF, 0xFF);
	SubmitDecision decision = m_policy.Observe(waited);

	if (decision == SubmitDecision::StopRendering) {
		OBVR_LOG("Render: stopped rendering, WaitGetPoses returned %d%s", waited,
		         waited == vr::openvr::kCompositorErrorDoNotHaveFocus
		             ? " - another application holds the scene, and waiting on that "
		               "throttles the game to 10 Hz"
		             : "");
		m_textures.Destroy();
		return false;
	}

	if (decision != SubmitDecision::Continue) {
		return false;
	}

	// From here a frame is owed to the compositor. EndFrame is what pays it.
	m_frameOpen = true;

	// Captures belong to exactly one frame. A frame that turned flat after
	// its passes ran - a menu opening between the render and Present - leaves
	// its captures unsubmitted, and they must not survive into a later frame
	// that would submit them as its own.
	m_dualCaptured[0] = false;
	m_dualCaptured[1] = false;
	return true;
}

// After the wait: where the imminent vsync is, for the submit pacing, and
// SteamVR's half-rate fallback (BeginFrame's, moved with the wait).
void HeadsetRenderer::NotePacing(vr::OpenVRBackend& backend, UInt64 vrFrameId) {
	// For the submit pacing: where the imminent vsync is. The compositor hands
	// the poses out a few milliseconds before a vsync ("running start", 3.0 to
	// 3.3 ms here, reported as newPosesReadyMs of every finished frame); the
	// frame finished one back says how far before, OBVR's tick says when.
	// render/SubmitPacing.h has the two sources tried before this one.
	m_vsyncKnown = false;
	if (GetConfig().tracker.submitPastVsync) {
		const UInt64 posesTick = static_cast<UInt64>(ReadPerformanceCounter());
		// The headset's period bounds what counts as a running start
		// (render/SubmitPacing.h: a whole-period hand-out at 144 Hz is under
		// the fixed cap); 0 while SteamVR has not said its rate.
		const double periodMs = PeriodMsFromDisplayHz(backend.DisplayFrequencyHz());
		vr::openvr::CompositorFrameTiming finished{};
		double toVsyncMs = -1.0;
		if (backend.GetFrameTiming(finished, 1)) {
			toVsyncMs = MsToVsyncFromRunningStart(finished.newPosesReadyMs, periodMs);
			// SteamVR's half-rate fallback, named for the log: every frame shown
			// twice (interleaved reprojection) after a run of dropped frames - a GPU
			// spike, a load - and kept for seconds (render/HalfRateWatch.h).
			switch (FeedHalfRate(finished.frameIndex, finished.numFramePresents, posesTick)) {
				case HalfRateWatch::Event::Entered:
					OBVR_LOG("Compositor: SteamVR shows every frame twice since its frame %u - interleaved reprojection, "
					         "the half-rate fallback it takes after dropped frames (a GPU spike, a load); the game runs "
					         "at half the headset's rate until it leaves (episode %u)",
					         m_halfRate.enteredAt, m_halfRate.episodes);
					break;
				case HalfRateWatch::Event::Left:
					OBVR_LOG("Compositor: back to full rate after %u frames at half rate (%u frames at half rate so far "
					         "in %u episodes)",
					         m_halfRate.lastEpisodeFrames, m_halfRate.halfRateFrames, m_halfRate.episodes);
					break;
				case HalfRateWatch::Event::None:
					break;
			}
		}
		m_lastToVsyncMs = toVsyncMs;
		if (toVsyncMs >= 0.0) {
			const double frequency = static_cast<double>(ReadPerformanceFrequency());
			m_vsyncTick = posesTick + static_cast<UInt64>(toVsyncMs * frequency / 1000.0);
			m_vsyncKnown = true;
			perf::EventContext pacingContext{};
			pacingContext.vrFrameId = vrFrameId;
			perf::Profiler::Instance().Mark(perf::EventType::PacingVsync, posesTick, m_vsyncTick,
			                                pacingContext);
		}
		// The first three frames and then one in 750 (ten seconds at 75 Hz).
		if (m_pacingReports < 3 || m_pacingReports % 750 == 0) {
			const double runningStart = static_cast<double>(-finished.newPosesReadyMs);
			const double cap = RunningStartCapMs(periodMs);
			const char* why = toVsyncMs >= 0.0 ? ""
			                  : runningStart > cap
			                      ? " (not a running start - the compositor hands the poses out about a whole "
			                        "period before the vsync, so the frame is never early; nothing is held)"
			                      : " (unknown - no pacing this frame)";
			OBVR_LOG("Submit pacing: the last finished frame had its poses %.2f ms before its "
			         "vsync, so this one's vsync is %.2f ms away%s (a running start counts up to %.2f ms, %s; "
			         "%u holds so far)",
			         runningStart, toVsyncMs, why, cap,
			         periodMs > 0.0 ? "55 % of the headset's period or 7.5 ms" : "the headset's period not known",
			         m_submitHolds);
		}
		++m_pacingReports;
	}
}

// The compositor's finished frames, for compositor.csv while a capture runs.
void HeadsetRenderer::RecordCompositorTiming(vr::OpenVRBackend& backend) {
	// The compositor's account of the frame just finished, for compositor.csv:
	// the only GPU timing OBVR has under DXVK, and the frame pacing seen from
	// the compositor's side (2026-10-19: alternate eye rendering dropped every
	// other frame in a cheap cell, and nothing on OBVR's side of WaitGetPoses
	// could say why). Read only while a capture runs - two calls a frame then.
	//
	// Two frames back and one frame back, not the latest: the latest is the
	// frame still in flight (the first capture read it with 0 presents and a
	// GPU time of -0.55 ms), and when the app misses a running start the
	// compositor's index steps by two, so the frame it reprojected in the gap
	// is only ever seen one or two frames later. The profiler keeps each
	// index once, so the steady state is one finished row a frame.
	if (perf::Profiler::Instance().IsCapturing()) {
		for (UInt32 framesAgo = 2; framesAgo > 0; --framesAgo) {
		vr::openvr::CompositorFrameTiming timing{};
		if (backend.GetFrameTiming(timing, framesAgo)) {
			perf::CompositorTimingRecord record{};
			record.frameIndex = timing.frameIndex;
			record.numFramePresents = timing.numFramePresents;
			record.numMisPresented = timing.numMisPresented;
			record.numDroppedFrames = timing.numDroppedFrames;
			record.reprojectionFlags = timing.reprojectionFlags;
			record.systemTimeSeconds = timing.systemTimeInSeconds;
			record.preSubmitGpuMs = timing.preSubmitGpuMs;
			record.postSubmitGpuMs = timing.postSubmitGpuMs;
			record.totalRenderGpuMs = timing.totalRenderGpuMs;
			record.compositorRenderGpuMs = timing.compositorRenderGpuMs;
			record.compositorIdleCpuMs = timing.compositorIdleCpuMs;
			record.clientFrameIntervalMs = timing.clientFrameIntervalMs;
			record.waitGetPosesCalledMs = timing.waitGetPosesCalledMs;
			record.newPosesReadyMs = timing.newPosesReadyMs;
			record.newFrameReadyMs = timing.newFrameReadyMs;
			record.compositorUpdateStartMs = timing.compositorUpdateStartMs;
			record.compositorRenderStartMs = timing.compositorRenderStartMs;
			record.numVSyncsReadyForUse = timing.numVSyncsReadyForUse;
			record.numVSyncsToFirstView = timing.numVSyncsToFirstView;
			perf::Profiler::Instance().AddCompositorTiming(record);
		}
		}
	}
}

bool HeadsetRenderer::BeginFrameEarly(vr::OpenVRBackend& backend) {
	const UInt64 vrFrameId = perf::Profiler::Instance().BeginVrFrame();
	perf::EventContext beginContext{};
	beginContext.vrFrameId = vrFrameId;
	perf::Profiler::ScopedSpan beginFrame(perf::Profiler::Instance(),
	                                     perf::EventType::BeginFrame, beginContext);
	m_waitPending = false;
	if (!PrepareFrame(backend)) {
		return false;
	}
	const double frequency = static_cast<double>(ReadPerformanceFrequency());
	const double now = static_cast<double>(ReadPerformanceCounter()) / frequency;
	const double ahead = vr::SecondsToPhotons(now, static_cast<double>(m_lastWaitEndTick) / frequency,
	                                          PeriodMsFromDisplayHz(backend.DisplayFrequencyHz()) / 1000.0,
	                                          m_lastToVsyncMs >= 0.0 ? m_lastToVsyncMs / 1000.0 : -1.0,
	                                          static_cast<double>(backend.SecondsFromVsyncToPhotons()));
	// Not said yet (the first frame, no rate) or no head to predict: the
	// wait here, as without the setting.
	if (ahead < 0.0 || !backend.PredictFramePoses(static_cast<float>(ahead))) {
		return WaitAndOpen(backend, vrFrameId, false);
	}
	if (!m_lateWaitReported) {
		m_lateWaitReported = true;
		OBVR_LOG("Poses: the late pose wait is on - the update runs on poses predicted %.1f ms ahead, WaitGetPoses "
		         "waits at the render's entry, the frame is submitted with the pose it was drawn with",
		         ahead * 1000.0);
	}
	m_waitPending = true;
	m_pendingFrameId = vrFrameId;
	m_frameOpen = true;
	m_dualCaptured[0] = false;
	m_dualCaptured[1] = false;
	return true;
}

bool HeadsetRenderer::WaitLate(vr::OpenVRBackend& backend) {
	if (!m_waitPending) {
		return m_frameOpen;
	}
	m_waitPending = false;
	return WaitAndOpen(backend, m_pendingFrameId, true);
}

}  // namespace obvr::render

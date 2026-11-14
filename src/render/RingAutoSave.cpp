// The ring saved at a slow stretch (perf/AutoSaveLogic.h), beside the
// half-rate watch the frame's timing feeds (render/HeadsetRenderer.cpp
// BeginFrame); its own file so HeadsetRenderer.cpp stays under the size
// ratchet (tests/function-sizes.txt).

#include "core/Config.h"
#include "core/Log.h"
#include "perf/Profiler.h"
#include "platform/Win32Min.h"
#include "render/HeadsetRenderer.h"

namespace obvr::render {

// [Performance] AutoSaveSlowSeconds/AutoSaveMax (perf/AutoSaveLogic.h): a
// half-rate stretch of that length saves the ring when it ends, or 30 s into
// it - the slow places a tester did not stop at to press "Save the last
// minute". Fed only while the submit pacing reads the frame timing
// ([Render] SubmitPastVsync, on by default).
HalfRateWatch::Event HeadsetRenderer::FeedHalfRate(UInt32 frameIndex, UInt32 presents, UInt64 tick) {
	const HalfRateWatch::Event event = m_halfRate.Feed(frameIndex, presents);
	const perf::ProfileSettings& p = GetConfig().performance;
	perf::AutoSaveSettings s;
	s.slowSeconds = p.autoSaveSlowSeconds;
	s.maxSaves = p.autoSaveMax;
	const perf::HalfRateEvent mapped = event == HalfRateWatch::Event::Entered ? perf::HalfRateEvent::Entered
	                                   : event == HalfRateWatch::Event::Left  ? perf::HalfRateEvent::Left
	                                                                          : perf::HalfRateEvent::None;
	const double frequency = static_cast<double>(ReadPerformanceFrequency());
	const double now = frequency > 0.0 ? static_cast<double>(tick) / frequency : 0.0;
	const bool ringRecords = p.enabled && p.ring && perf::Profiler::Instance().IsCapturing();
	const perf::AutoSaveStep step = perf::StepAutoSave(m_autoSave, s, ringRecords, mapped, now);
	if (step.reason == perf::AutoSaveReason::None) {
		return event;
	}
	perf::Profiler::Instance().RequestSave();
	OBVR_LOG("Performance: the ring saved by itself (%u of %u this session) - %.1f s at half rate %s",
	         m_autoSave.saves, s.maxSaves, step.stretchSeconds,
	         step.reason == perf::AutoSaveReason::StretchEnded ? "that just ended" : "and still going");
	return event;
}

}  // namespace obvr::render

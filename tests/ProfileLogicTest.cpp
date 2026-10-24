#include <cmath>
#include <cstdio>
#include <cstring>

#include "perf/ProfileLogic.h"

namespace {
int failures = 0;
void Check(bool condition, const char* text) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", text);
	if (!condition) ++failures;
}

void TestSettings() {
	using namespace obvr::perf;
	ProfileSettings s{};
	s.captureSeconds = 1;
	s.maxRecords = 1;
	s = NormalizeSettings(s);
	Check(s.captureSeconds == kDefaultCaptureSeconds, "too-short captures use the default");
	Check(s.maxRecords == kDefaultMaxRecords, "too-small buffers use the default");
	s.captureSeconds = 999;
	s.maxRecords = 999999;
	s = NormalizeSettings(s);
	Check(s.captureSeconds == kMaxCaptureSeconds, "capture duration is capped");
	Check(s.maxRecords <= kMaxMaxRecords && CaptureBytes(s) <= kProfilerBufferLimitBytes,
	      "record capacity is capped by the 32 MiB buffer limit");
	s.startAfterSeconds = 99999;
	s = NormalizeSettings(s);
	Check(s.startAfterSeconds == kMaxStartAfterSeconds, "the start delay is capped at an hour");
	s.startAfterSeconds = 0;
	Check(NormalizeSettings(s).startAfterSeconds == 0, "no delay stays no delay");
}

void TestStartDelay() {
	using obvr::perf::CaptureStartDue;
	Check(!CaptureStartDue(1000, 1000 + 59 * 10, 60, 10), "a second short of the delay: not due");
	Check(CaptureStartDue(1000, 1000 + 60 * 10, 60, 10), "the delay passed to the tick: due");
	Check(CaptureStartDue(1000, 1000 + 61 * 10, 60, 10), "past the delay: due");
	Check(CaptureStartDue(1000, 1000, 0, 10), "no delay: due at once");
	Check(!CaptureStartDue(1000, 900, 0, 10), "a counter behind the arming: never due");
	Check(!CaptureStartDue(1000, 2000, 1, 0), "no frequency: never due");
}

void TestTime() {
	using obvr::perf::ElapsedMilliseconds;
	double ms = 0.0;
	Check(ElapsedMilliseconds(100, 200, 1000, ms) && ms == 100.0,
	      "counter ticks convert to wall milliseconds");
	Check(!ElapsedMilliseconds(200, 100, 1000, ms), "reversed ticks are invalid");
	Check(!ElapsedMilliseconds(0, 1, 0, ms), "zero frequency is invalid");
	}

void TestPercentiles() {
	using obvr::perf::NearestRankPercentile;
	const double sorted[] = {1.0, 2.0, 4.0, 8.0};
	double result = 0.0;
	Check(NearestRankPercentile(sorted, 4, 0.0, result) && result == 1.0,
	      "zero percentile uses the first rank");
	Check(NearestRankPercentile(sorted, 4, 50.0, result) && result == 2.0,
	      "median uses nearest rank");
	Check(NearestRankPercentile(sorted, 4, 100.0, result) && result == 8.0,
	      "maximum uses the last rank");
	Check(!NearestRankPercentile(nullptr, 0, 95.0, result), "empty percentiles are missing");
}
void TestCompositorTiming() {
	using namespace obvr::perf;
	Check(CompositorTimingCapacity(65536) == 16384, "the compositor buffer is a quarter of the records");
	Check(CompositorTimingCapacity(1024) == kMinMaxRecords, "and never below the minimum");
	CompositorTimingRecord r{};
	r.presentId = 7; r.vrFrameId = 8; r.frameIndex = 1234; r.numFramePresents = 2;
	r.numMisPresented = 1; r.numDroppedFrames = 1; r.reprojectionFlags = 0x24;
	r.systemTimeSeconds = 1.5; r.preSubmitGpuMs = 2.25f; r.postSubmitGpuMs = 0.5f;
	r.totalRenderGpuMs = 4.0f; r.compositorRenderGpuMs = 1.0f; r.compositorIdleCpuMs = 9.0f;
	r.clientFrameIntervalMs = 13.25f; r.waitGetPosesCalledMs = -2.0f; r.newPosesReadyMs = -1.5f;
	r.newFrameReadyMs = 0.75f; r.compositorUpdateStartMs = 10.0f; r.compositorRenderStartMs = 11.0f;
	r.numVSyncsReadyForUse = 1; r.numVSyncsToFirstView = 2;
	char line[512] = {};
	const int n = CompositorTimingCsvLine(line, sizeof(line), r);
	const char* want = "7,8,1234,2,1,1,36,1.500000,2.250,0.500,4.000,1.000,9.000,13.250,-2.000,-1.500,"
	                   "0.750,10.000,11.000,1,2";
	bool same = n > 0;
	for (int i = 0; same && want[i] != '\0'; ++i) same = line[i] == want[i];
	same = same && n > 0 && line[n] == '\0' && want[n] == '\0';
	Check(same, "a compositor row writes every field in the header's order");
	char small[16] = {};
	Check(CompositorTimingCsvLine(small, sizeof(small), r) < 0, "a buffer too small is refused");
	const char* h = kCompositorTimingCsvHeader;
	int commas = 0;
	for (int i = 0; h[i] != '\0'; ++i) commas += h[i] == ',';
	Check(commas == 20, "the header names twenty-one columns, as the row writes");
}
} // namespace

// The tables are sized by what a frame writes, not each to maxRecords: the
// events run to about 32 a frame (the hall, 2026-10-23: 143395 events in
// 4455 frames), so the 32 MiB used to hold only about 60 s - the reappear
// run of 2026-10-24 stopped at 119 s of a 150 s window.
void TestBudget() {
	using namespace obvr::perf;
	Check(FrameCapacity(65536) == 65536 / kFrameDivisor, "the frame table is an eighth of the events");
	Check(FrameCapacity(2048) == kMinMaxRecords, "and never below the minimum");
	Check(GpuCapacity(65536, true) == 65536 / kGpuDivisor, "with GPU timing the GPU table is a quarter");
	Check(GpuCapacity(65536, false) == kMinMaxRecords, "without it only the minimum is held");
	ProfileSettings s{};
	s.maxRecords = kMaxMaxRecords;
	s.gpuTiming = false;
	ProfileSettings off = NormalizeSettings(s);
	Check(CaptureBytes(off) <= kProfilerBufferLimitBytes, "without GPU timing the capture fits 32 MiB");
	Check(off.maxRecords >= 230000, "and holds about 110 s of events at 32 a frame and 75 Hz");
	s.gpuTiming = true;
	ProfileSettings on = NormalizeSettings(s);
	Check(CaptureBytes(on) <= kProfilerBufferLimitBytes, "with GPU timing the capture fits 32 MiB");
	Check(on.maxRecords <= off.maxRecords, "and GPU timing never buys more events");
	s.maxRecords = kDefaultMaxRecords;
	Check(NormalizeSettings(s).maxRecords == kDefaultMaxRecords, "a budget that fits stays as asked");
}

// The folder a capture is written to. Session ids start at 1 with every
// game, so a folder left by an earlier game (a live session's capture the
// runner does not move) refused the export of 2026-10-24 08:45 outright.
void TestDirectory() {
	using namespace obvr::perf;
	char name[64] = {};
	Check(CaptureDirectoryCandidate("C:\\Game\\OBVR-performance-1", 0, name, sizeof(name)) &&
	          std::strcmp(name, "C:\\Game\\OBVR-performance-1") == 0,
	      "the first try is the session's own name");
	Check(CaptureDirectoryCandidate("C:\\Game\\OBVR-performance-1", 1, name, sizeof(name)) &&
	          std::strcmp(name, "C:\\Game\\OBVR-performance-1-2") == 0,
	      "the second try counts on from it");
	Check(CaptureDirectoryCandidate("OBVR-performance-1", 11, name, sizeof(name)) &&
	          std::strcmp(name, "OBVR-performance-1-12") == 0,
	      "and keeps counting");
	char tiny[8] = {};
	Check(!CaptureDirectoryCandidate("OBVR-performance-1", 0, tiny, sizeof(tiny)), "a name that does not fit is refused");
	Check(!CaptureDirectoryCandidate("OBVR-perf", 1, tiny, sizeof(tiny)), "and its suffix too");
	Check(!CaptureDirectoryCandidate(nullptr, 0, name, sizeof(name)), "no base is no name");
	Check(NextDirectoryStep(true, false, 0) == DirectoryStep::Write, "a created folder is written");
	Check(NextDirectoryStep(true, false, 5) == DirectoryStep::Write, "on any try");
	Check(NextDirectoryStep(false, true, 0) == DirectoryStep::TryNext, "an existing one sends to the next name");
	Check(NextDirectoryStep(false, true, kMaxDirectoryAttempts - 2) == DirectoryStep::TryNext,
	      "up to the last try");
	Check(NextDirectoryStep(false, true, kMaxDirectoryAttempts - 1) == DirectoryStep::GiveUp,
	      "after the last try the export gives up");
	Check(NextDirectoryStep(false, false, 0) == DirectoryStep::GiveUp,
	      "any other failure (no drive, no rights) gives up at once");
}

int main() {
	TestSettings();
	TestTime();
	TestStartDelay();
	TestPercentiles();
	TestCompositorTiming();
	TestBudget();
	TestDirectory();
	return failures == 0 ? 0 : 1;
}

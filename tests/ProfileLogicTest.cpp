#include <cmath>
#include <cstdio>

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
	Check(s.maxRecords <= kMaxMaxRecords &&
	          static_cast<UInt64>(s.maxRecords) *
	              (sizeof(CpuEvent) + sizeof(FrameRecord) + sizeof(GpuSample)) <=
	              kProfilerBufferLimitBytes,
	      "record capacity is capped by the 32 MiB buffer limit");
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
} // namespace

int main() {
	TestSettings();
	TestTime();
	TestPercentiles();
	return failures == 0 ? 0 : 1;
}

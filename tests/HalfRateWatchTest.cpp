// Checks the half-rate watch (render/HalfRateWatch.h): when SteamVR's run of
// twice-shown frames is called half rate, when it is called over, what a
// repeated frame index and a short burst do, and the counts kept.

#include <cstdio>

#include "render/HalfRateWatch.h"

using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using Event = HalfRateWatch::Event;

void TestFullRate() {
	std::printf("Full rate\n");
	HalfRateWatch w;
	bool quiet = true;
	for (UInt32 i = 1; i <= 200; ++i) {
		quiet = quiet && w.Feed(1000 + i, 1) == Event::None;
	}
	Check(quiet && !w.halfRate && w.episodes == 0, "two hundred frames shown once: nothing");
	Check(w.Feed(1200, 1) == Event::None && w.singles == 200, "the same index again counts once");
}

void TestEnterAndLeave() {
	std::printf("Entering and leaving\n");
	HalfRateWatch w;
	for (UInt32 i = 1; i <= 5; ++i) w.Feed(100 + i, 1);
	Event e = Event::None;
	UInt32 index = 200;
	for (UInt32 i = 1; i <= HalfRateWatch::kEnterFrames; ++i) {
		e = w.Feed(index, 2);
		if (i < HalfRateWatch::kEnterFrames) {
			Check(e == Event::None, i == 1 ? "the first twice-shown frames say nothing yet" : "");
		}
		index += 2;   // half rate steps the index by two
	}
	Check(e == Event::Entered && w.halfRate && w.episodes == 1, "the tenth consecutive twice-shown frame: entered");
	Check(w.enteredAt == 200, "the episode is dated to its first twice-shown frame");
	for (UInt32 i = 0; i < 300; ++i) {
		Check(w.Feed(index, 2) == Event::None || false, i == 0 ? "inside the episode: quiet" : "");
		index += 2;
	}
	e = Event::None;
	for (UInt32 i = 1; i <= HalfRateWatch::kLeaveFrames; ++i) {
		e = w.Feed(index++, 1);
		if (i < HalfRateWatch::kLeaveFrames) Check(e == Event::None, i == 1 ? "the first single frames say nothing yet" : "");
	}
	Check(e == Event::Left && !w.halfRate, "the tenth consecutive single frame: left");
	Check(w.lastEpisodeFrames == HalfRateWatch::kEnterFrames + 300, "the episode counted its twice-shown frames, not the way out");
	Check(w.halfRateFrames == w.lastEpisodeFrames, "the total over all episodes");

	// A single reprojected frame inside full rate changes nothing.
	for (UInt32 i = 0; i < 3; ++i) w.Feed(index++, 2);
	for (UInt32 i = 0; i < 20; ++i) w.Feed(index++, 1);
	Check(!w.halfRate && w.episodes == 1, "a burst of three twice-shown frames is not half rate");

	// A second episode counts on top.
	for (UInt32 i = 0; i < HalfRateWatch::kEnterFrames; ++i) { w.Feed(index, 2); index += 2; }
	for (UInt32 i = 0; i < 50; ++i) { w.Feed(index, 3); index += 2; }
	for (UInt32 i = 0; i < HalfRateWatch::kLeaveFrames; ++i) w.Feed(index++, 1);
	Check(w.episodes == 2 && w.lastEpisodeFrames == HalfRateWatch::kEnterFrames + 50 &&
	          w.halfRateFrames == HalfRateWatch::kEnterFrames + 300 + HalfRateWatch::kEnterFrames + 50,
	      "a second episode, three presents counting as twice or more, the totals summed");
}

void TestStutterInside() {
	std::printf("A single frame inside an episode\n");
	HalfRateWatch w;
	UInt32 index = 10;
	for (UInt32 i = 0; i < HalfRateWatch::kEnterFrames; ++i) { w.Feed(index, 2); index += 2; }
	Check(w.halfRate, "entered");
	w.Feed(index++, 1);
	for (UInt32 i = 0; i < 5; ++i) { w.Feed(index, 2); index += 2; }
	Check(w.halfRate && w.singles == 0, "one single frame inside does not end the episode");
}

}  // namespace

int main() {
	TestFullRate();
	TestEnterAndLeave();
	TestStutterInside();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

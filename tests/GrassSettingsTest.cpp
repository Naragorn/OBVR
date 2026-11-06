// Checks the grass settings' decisions (game/GrassSettings.h): the end and
// start distances for the wanted reach, the game's iMinGrassSize for a
// density, whether the grass's buffers are still unmade, what is written
// for what is wanted against what the game holds now, and whether a setting
// object is the one its name says.

#include <cstdio>
#include <limits>

#include "game/GrassSettings.h"

namespace {

using namespace obvr;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

const float kNan = std::numeric_limits<float>::quiet_NaN();

void TestDistances() {
	std::printf("the distances for a reach\n");
	Check(!GrassDistancesFor(0.0f).set, "0: the game's own, nothing set");
	Check(!GrassDistancesFor(-500.0f).set, "below 0: nothing set");
	Check(!GrassDistancesFor(kNan).set, "no number: nothing set");
	const GrassDistances mid = GrassDistancesFor(5000.0f);
	Check(mid.set && mid.end == 5000.0f && mid.start == 4000.0f, "5000: the end there, the fade a thousand before it");
	const GrassDistances low = GrassDistancesFor(300.0f);
	Check(low.set && low.end == kGrassDistanceLeast && low.start == kGrassDistanceLeast - kGrassFadeBand,
	      "less than the least: the least (under 400.5 the game takes the grass away)");
	const GrassDistances high = GrassDistancesFor(20000.0f);
	Check(high.set && high.end == kGrassDistanceMost && high.start == kGrassDistanceMost - kGrassFadeBand,
	      "more than the most: 8000, the most the game's own slider sets");
	Check(GrassDistancesFor(kGrassDistanceLeast).start >= 0.0f, "the fade's start is never below 0");
}

void TestMinSize() {
	std::printf("iMinGrassSize for a density\n");
	Check(GrassMinSizeFor(0.0f) == 0, "0: the game's own, nothing set");
	Check(GrassMinSizeFor(-10.0f) == 0, "below 0: nothing set");
	Check(GrassMinSizeFor(kNan) == 0, "no number: nothing set");
	Check(GrassMinSizeFor(100.0f) == 80, "100 %: the game's default, 80");
	Check(GrassMinSizeFor(400.0f) == 40, "400 %: half the spacing, four times the places");
	Check(GrassMinSizeFor(25.0f) == 160, "25 %: twice the spacing");
	Check(GrassMinSizeFor(200.0f) == 57, "200 %: 57 (80 over the root of 2, rounded)");
	Check(GrassMinSizeFor(50.0f) == 113, "50 %: 113");
	Check(GrassMinSizeFor(1000.0f) == 40, "more than the most: 400 %");
	Check(GrassMinSizeFor(5.0f) == 160, "less than the least: 25 %");
	bool falls = true;
	SInt32 before = GrassMinSizeFor(kGrassDensityLeast);
	for (float percent = kGrassDensityLeast; percent <= kGrassDensityMost; percent += 25.0f) {
		const SInt32 size = GrassMinSizeFor(percent);
		falls = falls && size <= before && size >= 40 && size <= 160;
		before = size;
	}
	Check(falls, "denser never means a larger spacing, and the spacing stays within 40 and 160");
}

void TestBuffers() {
	std::printf("the grass's buffers\n");
	Check(GrassBuffersUnmade(0, 0, 0), "no user and neither buffer: unmade");
	Check(!GrassBuffersUnmade(1, 0, 0), "a user: made");
	Check(!GrassBuffersUnmade(0, 0x12345678, 0), "the first buffer there: made");
	Check(!GrassBuffersUnmade(0, 0, 0x12345678), "the second buffer there: made");
}

GrassNow Now(float end, float start, SInt32 minSize, bool unmade) {
	GrassNow now;
	now.end = end;
	now.start = start;
	now.minSize = minSize;
	now.buffersUnmade = unmade;
	return now;
}

void TestWrite() {
	std::printf("what is written\n");
	{
		const GrassWrite w = GrassWriteFor(0.0f, 0.0f, Now(3000.0f, 2000.0f, 80, false));
		Check(!w.distance && !w.minSize && !w.minSizeWaits, "nothing wanted: nothing written");
	}
	{
		const GrassWrite w = GrassWriteFor(6000.0f, 0.0f, Now(3000.0f, 2000.0f, 80, false));
		Check(w.distance && w.end == 6000.0f && w.start == 5000.0f && !w.minSize, "a reach wanted: both distances");
	}
	{
		const GrassWrite w = GrassWriteFor(6000.0f, 0.0f, Now(6000.0f, 5000.0f, 80, false));
		Check(!w.distance, "the game holds them already: not written again");
	}
	{
		const GrassWrite w = GrassWriteFor(6000.0f, 0.0f, Now(6000.0f, 2000.0f, 80, false));
		Check(w.distance && w.start == 5000.0f, "only the fade's start differs: written");
	}
	{
		const GrassWrite w = GrassWriteFor(0.0f, 200.0f, Now(3000.0f, 2000.0f, 80, true));
		Check(w.minSize && w.size == 57 && !w.minSizeWaits && !w.distance, "a density wanted, the buffers unmade: written");
	}
	{
		const GrassWrite w = GrassWriteFor(0.0f, 200.0f, Now(3000.0f, 2000.0f, 80, false));
		Check(!w.minSize && w.minSizeWaits && w.size == 57, "the buffers made for another spacing: not written, it waits");
	}
	{
		const GrassWrite w = GrassWriteFor(0.0f, 200.0f, Now(3000.0f, 2000.0f, 57, false));
		Check(!w.minSize && !w.minSizeWaits, "the game holds the spacing already: nothing to do");
	}
	{
		const GrassWrite w = GrassWriteFor(8000.0f, 100.0f, Now(3000.0f, 2000.0f, 120, true));
		Check(w.distance && w.end == 8000.0f && w.minSize && w.size == 80, "both wanted: both written");
	}
}

void TestSettingName() {
	std::printf("the setting objects\n");
	Check(SettingNameIs("fGrassEndDistance:Grass", "fGrassEndDistance:Grass"), "the name it should carry: believed");
	Check(!SettingNameIs("fGrassEndDistance:Grass", "fGrassStartFadeDistance:Grass"), "another name: refused");
	Check(!SettingNameIs("fGrassEndDistance:GrassX", "fGrassEndDistance:Grass"), "a longer name: refused");
	Check(!SettingNameIs("fGrassEndDistance", "fGrassEndDistance:Grass"), "a shorter name: refused");
	Check(!SettingNameIs(nullptr, "fGrassEndDistance:Grass"), "no name: refused");
	Check(GrassValuesPlausible(Now(3000.0f, 2000.0f, 80, true)), "the game's defaults: plausible");
	Check(GrassValuesPlausible(Now(0.0f, 0.0f, 80, true)), "the grass off (both 0): plausible");
	Check(!GrassValuesPlausible(Now(kNan, 2000.0f, 80, true)), "no number for the end: not");
	Check(!GrassValuesPlausible(Now(3000.0f, -5.0f, 80, true)), "a start below 0: not");
	Check(!GrassValuesPlausible(Now(3.0e6f, 2000.0f, 80, true)), "an end past a million units: not");
	Check(!GrassValuesPlausible(Now(3000.0f, 2000.0f, 0, true)), "a spacing of 0 (the game divides by it): not");
	Check(!GrassValuesPlausible(Now(3000.0f, 2000.0f, 5000, true)), "a spacing past the 2048 it divides: not");
}

}  // namespace

int main() {
	TestDistances();
	TestMinSize();
	TestBuffers();
	TestWrite();
	TestSettingName();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

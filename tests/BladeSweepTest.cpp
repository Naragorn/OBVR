// The strike's blade swept between two frames (game/BladeSweepLogic.h,
// docs/weapon-collision-spec.md 26): a blade that crosses a body within one
// frame touches it at a pose between; the sweep only from the frame before
// of the same swing in the same world, not from a carried blade; the steps
// bounded; the swing's summary.

#include <cstdio>
#include <limits>

#include "game/BladeSweepLogic.h"

using namespace obvr::game;
using obvr::NiPoint3;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kActor = 0x1234u;
constexpr UInt32 kOther = 0x5678u;

// One person: a torso upright at x = 0, y = 50, radius 10 (the strike's
// torso), from z 0 to 60.
BladeBodies& Person() {
	static BladeBodies bodies;
	bodies.Clear();
	bodies.count = 1;
	bodies.cap[0].a = NiPoint3{0.0f, 50.0f, 0.0f};
	bodies.cap[0].b = NiPoint3{0.0f, 50.0f, 60.0f};
	bodies.cap[0].radius = 10.0f;
	bodies.cap[0].actor = kActor;
	bodies.cap[0].part = BodyPart::Torso;
	return bodies;
}

// A blade at height z, its base at the player (y 0) and its tip 60 units
// ahead along y, swung sideways: at x.
NiPoint3 Base(float x) { return NiPoint3{x * 0.2f, 0.0f, 30.0f}; }
NiPoint3 Tip(float x) { return NiPoint3{x, 60.0f, 30.0f}; }

BladeSweepMemory Seen(UInt32 frame, UInt32 world, float x) {
	BladeSweepMemory m;
	RememberBlade(m, frame, world, Base(x), Tip(x));
	return m;
}

void TestStart() {
	std::printf("the sweep's start\n");
	BladeSweepMemory none;
	Check(SweepStart(none, 100, 1, Base(0), Tip(0)) == SweepFrom::FirstFrame, "nothing remembered: the first frame");
	const BladeSweepMemory m = Seen(100, 1, -40.0f);
	Check(SweepStart(m, 101, 1, Base(40), Tip(40)) == SweepFrom::Swept,
	      "the frame before, a swing or not: swept (its first frame sweeps from the blade before it)");
	Check(SweepStart(m, 101, 2, Base(40), Tip(40)) == SweepFrom::NewWorld, "a load since: not swept");
	Check(SweepStart(m, 100, 1, Base(40), Tip(40)) == SweepFrom::SameFrame, "the same frame again: nothing between");
	Check(SweepStart(m, 103, 1, Base(40), Tip(40)) == SweepFrom::Gap,
	      "frames without a strike between (through a wall, a menu): not swept");
	Check(SweepStart(m, 99, 1, Base(40), Tip(40)) == SweepFrom::Gap, "a stamp from before: not swept");
	Check(SweepStart(m, 101, 1, Base(400), Tip(400)) == SweepFrom::Carried,
	      "the tip 440 units on in one frame: carried, not swung");
	Check(SweepStart(m, 101, 1, Base(-40.0f + 299.0f), Tip(-40.0f + 299.0f)) == SweepFrom::Swept,
	      "299 units: still a swing");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	Check(SweepStart(m, 101, 1, Base(40), NiPoint3{nan, 0.0f, 0.0f}) == SweepFrom::NotANumber, "a NaN tip: no number");
	Check(SweepStart(m, 101, 1, NiPoint3{inf, 0.0f, 0.0f}, Tip(40)) == SweepFrom::NotANumber,
	      "an endless base: no number");
	Check(SweepStart(none, 100, 1, Base(0), NiPoint3{nan, 0.0f, 0.0f}) == SweepFrom::NotANumber,
	      "no number before anything else");
	BladeSweepMemory wrapped = Seen(0xFFFFFFFFu, 1, 0.0f);
	Check(SweepStart(wrapped, 0, 1, Base(1), Tip(1)) == SweepFrom::Swept, "the stamp wrapping over: the frame before");
	for (int s = 0; s <= 6; ++s) {
		Check(SweepFromName(static_cast<SweepFrom>(s))[0] != '?', "every start has a name");
	}
	Check(SweepFromName(static_cast<SweepFrom>(7))[0] == '?', "an unknown start: ?");
}

void TestSteps() {
	std::printf("the steps\n");
	const BladeSweepMemory m = Seen(1, 1, 0.0f);
	Check(SweepSteps(m, Base(0), Tip(0)) == 1, "no way: one step, no pose between");
	Check(SweepSteps(m, Base(5), Tip(5)) == 1, "under a step: one");
	Check(SweepSteps(m, Base(6), Tip(6)) == 1, "exactly a step: one");
	Check(SweepSteps(m, Base(12), Tip(12)) == 2, "two steps' way: two");
	Check(SweepSteps(m, Base(13), Tip(13)) == 3, "a little over two: three");
	Check(SweepSteps(m, Base(43), Tip(43)) == 8, "the measured half-rate mean (43 units): 8");
	Check(SweepSteps(m, Base(1000), Tip(1000)) == kBladeSweepMostSteps, "a huge way: at most 32");
	// The base moving farther than the tip (a stab driven from the shoulder).
	BladeSweepMemory s;
	RememberBlade(s, 1, 1, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 60.0f, 0.0f});
	Check(SweepSteps(s, NiPoint3{0.0f, 30.0f, 0.0f}, NiPoint3{0.0f, 62.0f, 0.0f}) == 5,
	      "the base's way decides when it is the longer");
}

void TestTouch() {
	std::printf("the touch between frames\n");
	BladeBodies& bodies = Person();
	NiPoint3 atBase{0.0f, 0.0f, 0.0f}, atTip{0.0f, 0.0f, 0.0f};
	// The tip from x -40 to x +40 in one frame: past the torso (radius 10 +
	// margin 4) at both ends, through it between.
	Check(CapsulesTouched(bodies, kActor, Base(-40), Tip(-40), 4.0f) == 0, "the frame before: beside him");
	Check(CapsulesTouched(bodies, kActor, Base(40), Tip(40), 4.0f) == 0,
	      "this frame: past him - the one-frame test misses (the bug)");
	const BladeSweepMemory m = Seen(10, 1, -40.0f);
	const UInt32 steps = SweepSteps(m, Base(40), Tip(40));
	Check(SweptCapsulesTouched(bodies, kActor, m.base, m.tip, Base(40), Tip(40), steps, 4.0f, atBase, atTip) == 1,
	      "swept: touched between");
	Check(atTip.x > -14.0f && atTip.x < 14.0f, "the pose it touched at is the one through him");
	// A swing that stays beside him.
	Check(SweptCapsulesTouched(bodies, kActor, Base(-80), Tip(-80), Base(-30), Tip(-30), 9, 4.0f, atBase, atTip) == 0,
	      "beside him all the way: missed");
	// Someone not in the list.
	Check(SweptCapsulesTouched(bodies, kOther, m.base, m.tip, Base(40), Tip(40), steps, 4.0f, atBase, atTip) == -1,
	      "no capsule of his in the list: -1");
	// One step: no pose between.
	Check(SweptCapsulesTouched(bodies, kActor, Base(-1), Tip(-1), Base(40), Tip(40), 1, 4.0f, atBase, atTip) == 0,
	      "one step: nothing between, in the list: 0");
	Check(SweptCapsulesTouched(bodies, kOther, Base(-1), Tip(-1), Base(40), Tip(40), 1, 4.0f, atBase, atTip) == -1,
	      "one step, not in the list: -1");
	// A huge jump at the most steps still finds the torso (32 steps of 15
	// units over 480: one lands within 14 of the axis).
	Check(SweptCapsulesTouched(bodies, kActor, Base(-240), Tip(-240), Base(240), Tip(240), kBladeSweepMostSteps, 4.0f,
	                           atBase, atTip) == 1,
	      "a huge jump at 32 steps: touched");
}

void TestSummary() {
	std::printf("the swing's summary\n");
	SwingSweepSummary s;
	StartSummary(s, 9, 1);
	Check(s.serial == 9 && s.hand == 1 && s.frames == 0, "started for its swing and hand");
	Check(SummaryFramesPerSecond(s) == 0.0f, "no frame: no rate");
	NoteSummaryFrame(s, SweepFrom::FirstFrame, 0.0f, 1.0f / 45.0f);
	NoteSummaryFrame(s, SweepFrom::Swept, 43.0f, 1.0f / 45.0f);
	NoteSummaryFrame(s, SweepFrom::Swept, 20.0f, 1.0f / 30.0f);
	NoteSummaryFrame(s, SweepFrom::Carried, 500.0f, 0.0f);   // carried: not the swing's jump; untimed
	NoteSummaryFrame(s, SweepFrom::SameFrame, 0.0f, 2.0f);   // a nonsense clock: not counted
	Check(s.frames == 5 && s.sweptFrames == 2, "frames and swept frames counted");
	Check(s.mostJump == 43.0f, "the largest swept jump kept, the carried one not");
	Check(s.longestFrame > 0.033f && s.longestFrame < 0.034f, "the longest timed frame");
	const float fps = SummaryFramesPerSecond(s);
	Check(fps > 38.0f && fps < 39.0f, "the rate over the timed frames only (3 in 0.078 s)");
	StartSummary(s, 10, 0);
	Check(s.frames == 0 && s.mostJump == 0.0f && s.serial == 10, "a new swing starts empty");
}

}  // namespace

int main() {
	TestStart();
	TestSteps();
	TestTouch();
	TestSummary();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

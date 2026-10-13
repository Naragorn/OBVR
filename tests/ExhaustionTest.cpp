// Checks the exhaustion instead of the collapse (game/ExhaustionLogic.h):
// the settings as used, when the player's knock state step is skipped, every
// flow of the state - begun by low fatigue or the paralysis, held through the
// hysteresis, over when recovered and not paralysed, eased in and out, off
// mid-state - and the breath: its pacing and its cues.

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/ExhaustionLogic.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 1e-3f) { return std::fabs(a - b) <= within; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestRules() {
	std::printf("The rules\n");
	const ExhaustionSettings g;
	Check(g.enabled && Near(g.belowPercent, 12.0f) && Near(g.abovePercent, 25.0f) && Near(g.weight, 4.0f) && g.sounds,
	      "the defaults: on, 12 %, 25 %, x4, sounds");
	Check(Near(ExhaustionBelowShare(12.0f), 0.12f) && Near(ExhaustionBelowShare(1.0f), 0.01f) &&
	          Near(ExhaustionBelowShare(50.0f), 0.5f),
	      "the lower edge as set, 1 to 50 %");
	Check(Near(ExhaustionBelowShare(0.0f), 0.12f) && Near(ExhaustionBelowShare(51.0f), 0.12f) &&
	          Near(ExhaustionBelowShare(kNaN), 0.12f),
	      "out of range or not a number: 12 %");
	Check(Near(ExhaustionAboveShare(0.12f, 25.0f), 0.25f) && Near(ExhaustionAboveShare(0.12f, 90.0f), 0.9f),
	      "the upper edge as set");
	Check(Near(ExhaustionAboveShare(0.12f, 0.0f), 0.25f) && Near(ExhaustionAboveShare(0.12f, kNaN), 0.25f),
	      "the upper edge out of range: 25 %");
	Check(Near(ExhaustionAboveShare(0.3f, 20.0f), 0.4f), "an upper edge under the lower: a tenth above the lower");
	Check(Near(ExhaustionAboveShare(0.3f, 30.0f), 0.4f), "an upper edge at the lower: a tenth above it");
	Check(Near(ExhaustionWeightFactor(4.0f), 4.0f) && Near(ExhaustionWeightFactor(1.0f), 1.0f) &&
	          Near(ExhaustionWeightFactor(10.0f), 10.0f),
	      "the weight factor as set, 1 to 10");
	Check(Near(ExhaustionWeightFactor(0.5f), 4.0f) && Near(ExhaustionWeightFactor(11.0f), 4.0f) &&
	          Near(ExhaustionWeightFactor(kNaN), 4.0f),
	      "out of range or not a number: 4");

	Check(CollapseSkipped(true, true, 0, -1.0f, false), "the player standing, fatigue below 0: skipped");
	Check(CollapseSkipped(true, true, 0, 50.0f, true), "the player standing, paralysed: skipped");
	Check(!CollapseSkipped(true, true, 0, 0.0f, false), "fatigue at 0, not paralysed: run (the engine's own edge)");
	Check(!CollapseSkipped(true, true, 0, 50.0f, false), "fatigue left, not paralysed: run");
	Check(!CollapseSkipped(true, true, 2, -1.0f, false), "a knockdown under way (state 2): run");
	Check(!CollapseSkipped(true, true, 1, -1.0f, true), "lying (state 1): run");
	Check(!CollapseSkipped(true, true, 5, -1.0f, true), "getting up (state 5): run");
	Check(!CollapseSkipped(false, true, 0, -1.0f, true), "the setting off: run");
	Check(!CollapseSkipped(true, false, 0, -1.0f, true), "not the player: run");
	Check(!CollapseSkipped(true, true, 0, kNaN, false), "fatigue not a number: run");
}

ExhaustionVerdict Step(ExhaustionState& s, const ExhaustionSettings& g, float now, float base, bool paralyzed,
                       float dt = 0.02f) {
	return StepExhaustion(s, g, true, now, base, paralyzed, dt);
}

void TestStateFlows() {
	std::printf("The state\n");
	const ExhaustionSettings g;
	ExhaustionState s;
	ExhaustionVerdict v = Step(s, g, 100.0f, 200.0f, false);
	Check(!v.began && !s.exhausted && Near(v.ease, 0.0f) && Near(v.factor, 1.0f) && Near(v.share, 0.5f),
	      "half the fatigue: fresh, the factor 1");
	v = Step(s, g, 23.0f, 200.0f, false);
	Check(v.began && s.exhausted && !s.byParalysis, "under 12 %: begun, by the fatigue");
	Check(Near(v.ease, 0.04f) && Near(v.factor, 1.12f), "eased in a frame's worth (0.02 of 0.5 s): the factor 1.12");
	for (int i = 0; i < 30; ++i) {
		v = Step(s, g, 23.0f, 200.0f, false);
	}
	Check(!v.began && Near(v.ease, 1.0f) && Near(v.factor, 4.0f) && Near(s.seconds, 0.62f),
	      "half a second later: fully in, the factor 4, the time counted");
	v = Step(s, g, 40.0f, 200.0f, false);
	Check(!v.ended && s.exhausted, "back to 20 %: still exhausted (under the upper edge)");
	v = Step(s, g, 50.0f, 200.0f, false);
	Check(!v.ended && s.exhausted, "at 25 % exactly: still (over, not at)");
	v = Step(s, g, 52.0f, 200.0f, false);
	// 33 frames of 0.02 s held (the frame it ends on is not counted).
	Check(v.ended && !s.exhausted && Near(s.seconds, 0.0f) && Near(v.seconds, 0.66f),
	      "over 25 %: over, the verdict with the whole time held (the state cleared)");
	Check(Near(v.ease, 1.0f - 0.02f / 1.5f) && v.factor < 4.0f, "easing out: a frame's worth of 1.5 s");
	for (int i = 0; i < 100; ++i) {
		v = Step(s, g, 52.0f, 200.0f, false);
	}
	Check(Near(v.ease, 0.0f) && Near(v.factor, 1.0f), "two seconds later: fresh, the factor 1");

	// The paralysis: begun by it alone, held through full fatigue, over when it lifts.
	s = ExhaustionState{};
	v = Step(s, g, 200.0f, 200.0f, true);
	Check(v.began && s.byParalysis, "paralysed at full fatigue: begun, by the paralysis");
	v = Step(s, g, 200.0f, 200.0f, true);
	Check(!v.ended && s.exhausted, "still paralysed: held");
	v = Step(s, g, 200.0f, 200.0f, false);
	Check(v.ended, "the paralysis lifted at full fatigue: over");
	s = ExhaustionState{};
	Step(s, g, 10.0f, 200.0f, true);
	v = Step(s, g, 10.0f, 200.0f, false);
	Check(!v.ended && s.exhausted, "the paralysis lifted at 5 % fatigue: still exhausted by the fatigue");

	// Unknown fatigue: never by the fatigue, and recovered at once.
	s = ExhaustionState{};
	v = StepExhaustion(s, g, false, 0.0f, 0.0f, false, 0.02f);
	Check(!v.began && Near(v.share, 1.0f), "fatigue unknown: fresh");
	v = StepExhaustion(s, g, false, 0.0f, 0.0f, true, 0.02f);
	Check(v.began, "fatigue unknown, paralysed: begun");
	v = StepExhaustion(s, g, false, 0.0f, 0.0f, false, 0.02f);
	Check(v.ended, "fatigue unknown, the paralysis lifted: over (nothing holds it)");
	s = ExhaustionState{};
	v = Step(s, g, 10.0f, 0.0f, false);
	Check(!v.began, "a base of 0: the share unknown, fresh");
	v = Step(s, g, kNaN, 200.0f, false);
	Check(!v.began, "fatigue not a number: fresh");

	// Negative fatigue (the collapse's own range) is under the edge.
	s = ExhaustionState{};
	v = Step(s, g, -30.0f, 200.0f, false);
	Check(v.began && v.share < 0.0f, "fatigue below 0: begun");

	// Switched off mid-state: over at once, eased out.
	ExhaustionSettings off;
	off.enabled = false;
	s = ExhaustionState{};
	Step(s, g, 10.0f, 200.0f, false);
	v = Step(s, off, 10.0f, 200.0f, true);
	Check(v.ended && !s.exhausted && v.ease < 1.0f, "the setting off: over, whatever the fatigue and the paralysis");
	v = Step(s, off, 10.0f, 200.0f, true);
	Check(!v.began, "and not begun again while off");

	// The edges as set.
	ExhaustionSettings wide;
	wide.belowPercent = 30.0f;
	wide.abovePercent = 60.0f;
	wide.weight = 2.0f;
	s = ExhaustionState{};
	v = Step(s, wide, 58.0f, 200.0f, false);
	Check(v.began, "under 30 %: begun");
	for (int i = 0; i < 30; ++i) {
		v = Step(s, wide, 58.0f, 200.0f, false);
	}
	Check(Near(v.factor, 2.0f), "the weight x2 at full");
	v = Step(s, wide, 118.0f, 200.0f, false);
	Check(!v.ended, "59 %: still");
	v = Step(s, wide, 122.0f, 200.0f, false);
	Check(v.ended, "61 %: over");

	// No time: nothing eases, nothing counts.
	s = ExhaustionState{};
	v = Step(s, g, 10.0f, 200.0f, false, 0.0f);
	Check(v.began && Near(v.ease, 0.0f) && Near(s.seconds, 0.0f), "a frame without time: begun, not eased");
	v = Step(s, g, 10.0f, 200.0f, false, -1.0f);
	Check(Near(v.ease, 0.0f), "a negative dt: as 0");
}

void TestBreathRules() {
	std::printf("The breath's pacing\n");
	Check(Near(ExhaustionMissing(true, 0.12f, false), 0.88f) && Near(ExhaustionMissing(true, 0.0f, false), 1.0f),
	      "the fatigue missing: one less the share");
	Check(Near(ExhaustionMissing(true, -0.7f, false), 1.0f) && Near(ExhaustionMissing(true, 1.5f, false), 0.0f),
	      "below 0 all of it, above the base none");
	Check(Near(ExhaustionMissing(true, 0.9f, true), 1.0f), "paralysed: all of it, whatever the fatigue");
	Check(Near(ExhaustionMissing(false, 0.5f, false), 1.0f) && Near(ExhaustionMissing(true, kNaN, false), 1.0f),
	      "the share unknown or not a number: all of it");
	Check(Near(HeartbeatSeconds(0.0f), 1.0f) && Near(HeartbeatSeconds(1.0f), 0.3334f) &&
	          Near(HeartbeatSeconds(0.5f), 1.0f - 0.6666f * 0.25f),
	      "the heartbeat: a second at rest, a third at everything missing (Realistic Fatigue's curve)");
	Check(Near(HeartbeatSeconds(2.0f), 0.3334f) && Near(HeartbeatSeconds(-1.0f), 1.0f), "missing clamped to 0..1");
	Check(Near(PantIntervalSeconds(1.0f), 4.0f * 0.3334f) && Near(PantIntervalSeconds(0.75f), 4.0f * (1.0f - 0.6666f * 0.5625f)),
	      "a pant every four beats: 1.33 s at nothing left, 2.5 s at a quarter");
	Check(Near(PantVolume(1.0f), 1.0f) && Near(PantVolume(0.75f), 0.5625f) && Near(PantVolume(0.88f), 0.7744f),
	      "the volume the missing share squared");
	Check(Near(PantVolume(0.1f), kPantLeastVolume) && Near(PantVolume(0.0f), kPantLeastVolume),
	      "never under the least volume");
}

void TestBreath() {
	std::printf("The breath\n");
	BreathState b;
	BreathVerdict v = StepBreath(b, true, false, false, 1.0f, 0.02f);
	Check(v.cue == BreathCue::None && !b.on, "fresh, not exhausted: nothing");
	// Exhausted without a fall, everything missing: the first pant half a
	// second in, then one every 1.33 s, at full volume.
	v = StepBreath(b, true, true, false, 1.0f, 0.02f);
	Check(v.cue == BreathCue::None && b.on && Near(v.interval, 1.3336f), "the spell begun: the interval named, no pant yet");
	int frames = 1;
	while (v.cue != BreathCue::Pant && frames < 400) {
		v = StepBreath(b, true, true, false, 1.0f, 0.02f);
		++frames;
	}
	Check(v.cue == BreathCue::Pant && Near(v.volume, 1.0f) && b.pants == 1 && Near(frames * 0.02f, 0.5f, 0.03f),
	      "half a second in: the first pant, full volume");
	v = StepBreath(b, true, true, false, 1.0f, 0.02f);
	frames = 1;
	while (v.cue != BreathCue::Pant && frames < 400) {
		v = StepBreath(b, true, true, false, 1.0f, 0.02f);
		++frames;
	}
	Check(v.cue == BreathCue::Pant && b.pants == 2 && Near(frames * 0.02f, 1.34f, 0.03f),
	      "the second pant 1.33 s after the first");
	// A quarter of the fatigue left: slower and quieter.
	v = StepBreath(b, true, true, false, 0.75f, 0.02f);
	Check(v.cue == BreathCue::None && Near(v.interval, 2.5f, 0.01f), "a quarter left: a pant every 2.5 s");
	frames = 1;
	while (v.cue != BreathCue::Pant && frames < 400) {
		v = StepBreath(b, true, true, false, 0.75f, 0.02f);
		++frames;
	}
	Check(v.cue == BreathCue::Pant && Near(frames * 0.02f, 2.5f, 0.03f) && Near(v.volume, 0.5625f),
	      "and it comes at 2.5 s, at 56% volume");
	// Over: the spell ends, nothing to stop.
	v = StepBreath(b, true, false, false, 0.75f, 0.02f);
	Check(v.cue == BreathCue::None && !b.on && b.pants == 0, "recovered: the spell over, the counts cleared");

	// A fall spared: the gasp at once, full volume; the pants a whole
	// interval later.
	b = BreathState{};
	v = StepBreath(b, true, true, true, 1.0f, 0.02f);
	Check(v.cue == BreathCue::Gasp && Near(v.volume, 1.0f) && b.gasps == 1 && b.on, "a fall spared: the gasp");
	frames = 0;
	v = StepBreath(b, true, true, false, 1.0f, 0.02f);
	++frames;
	while (v.cue != BreathCue::Pant && frames < 400) {
		v = StepBreath(b, true, true, false, 1.0f, 0.02f);
		++frames;
	}
	Check(v.cue == BreathCue::Pant && Near(frames * 0.02f, 1.34f, 0.03f), "the first pant a whole interval after the gasp");
	v = StepBreath(b, true, true, true, 1.0f, 0.02f);
	Check(v.cue == BreathCue::Gasp && b.gasps == 2 && Near(b.sincePant, 0.0f), "another fall spared: the gasp again, the pants wait anew");

	// Sounds off: nothing, and a spell cut short.
	b = BreathState{};
	v = StepBreath(b, false, true, true, 1.0f, 0.02f);
	Check(v.cue == BreathCue::None && !b.on, "sounds off: not even the gasp");
	StepBreath(b, true, true, false, 1.0f, 0.02f);
	v = StepBreath(b, false, true, false, 1.0f, 0.02f);
	Check(v.cue == BreathCue::None && !b.on, "sounds switched off mid-spell: the spell over");

	// No time: nothing comes due.
	b = BreathState{};
	StepBreath(b, true, true, false, 1.0f, 0.02f);
	for (int i = 0; i < 100; ++i) {
		v = StepBreath(b, true, true, false, 1.0f, 0.0f);
	}
	Check(v.cue == BreathCue::None && b.pants == 0, "frames without time: no pant comes due");
}

}  // namespace

int main() {
	TestRules();
	TestStateFlows();
	TestBreathRules();
	TestBreath();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

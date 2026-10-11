// Checks the attack tempo's decisions (game/AttackTempoLogic.h): the
// settings from the percentages, which groups count, the offset moved, the
// key window kept continuous, the hold and the chance multiplier scaled,
// and the speed read back.

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/AttackTempoLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestSettings() {
	std::printf("The settings\n");
	AttackTempoSettings s = AttackTempoFrom(true, 75.0f, 100.0f);
	Check(s.enabled && Near(s.speed, 0.75f) && Near(s.chance, 1.0f) && Near(s.hold, 1.0f),
	      "75% tempo, 100% frequency: on, the time at three quarters, the chance and hold the game's own");
	s = AttackTempoFrom(true, 100.0f, 50.0f);
	Check(s.enabled && Near(s.speed, 1.0f) && Near(s.chance, 0.5f) && Near(s.hold, 2.0f),
	      "50% frequency: half the chance, twice the hold");
	s = AttackTempoFrom(true, 100.0f, 200.0f);
	Check(s.enabled && Near(s.chance, 2.0f) && Near(s.hold, 0.5f), "200% frequency: twice the chance, half the hold");
	s = AttackTempoFrom(true, 100.0f, 100.0f);
	Check(s.active && !s.enabled && Near(s.speed, 1.0f) && Near(s.chance, 1.0f) && Near(s.hold, 1.0f),
	      "both at 100: nothing to change, off - but watched");
	s = AttackTempoFrom(false, 50.0f, 50.0f);
	Check(!s.active && !s.enabled && Near(s.speed, 1.0f) && Near(s.chance, 1.0f) && Near(s.hold, 1.0f),
	      "the mode off: not watched, the game's own whatever the percentages");
	Check(AttackTempoFrom(true, 75.0f, 100.0f).active, "the mode on: watched");
	Check(Near(AttackTempoFrom(true, 20.0f, 100.0f).speed, 0.2f), "20% tempo: the floor, taken");
	Check(Near(AttackTempoFrom(true, 200.0f, 100.0f).speed, 2.0f), "200% tempo: the ceiling, taken");
	Check(Near(AttackTempoFrom(true, 19.0f, 100.0f).speed, 1.0f), "19% tempo: out of range, the game's own");
	Check(Near(AttackTempoFrom(true, 201.0f, 100.0f).speed, 1.0f), "201% tempo: out of range, the game's own");
	Check(Near(AttackTempoFrom(true, 0.0f, 100.0f).speed, 1.0f), "0 (a word in the INI): the game's own");
	Check(Near(AttackTempoFrom(true, kNaN, 100.0f).speed, 1.0f), "NaN tempo: the game's own");
	Check(Near(AttackTempoFrom(true, 100.0f, 9.0f).chance, 1.0f), "9% frequency: out of range, the game's own");
	Check(Near(AttackTempoFrom(true, 100.0f, 10.0f).chance, 0.1f) && Near(AttackTempoFrom(true, 100.0f, 10.0f).hold, 10.0f),
	      "10% frequency: the floor, a tenth the chance and ten times the hold");
	Check(Near(AttackTempoFrom(true, 100.0f, kNaN).chance, 1.0f), "NaN frequency: the game's own");
	s = AttackTempoFrom(true, 50.0f, 100.0f);
	Check(s.enabled && Near(s.speed, 0.5f), "the tempo alone switches it on");
	s = AttackTempoFrom(true, 100.0f, 75.0f);
	Check(s.enabled && Near(s.chance, 0.75f), "the frequency alone switches it on");
}

void TestGroups() {
	std::printf("Which groups\n");
	Check(IsAttackGroup(19) && IsAttackGroup(20) && IsAttackGroup(21) && IsAttackGroup(22),
	      "AttackBow, AttackLeft, AttackRight, AttackPower");
	Check(IsAttackGroup(23) && IsAttackGroup(24) && IsAttackGroup(25) && IsAttackGroup(26),
	      "the four directional power attacks");
	Check(!IsAttackGroup(18) && !IsAttackGroup(27), "neither Unequip before them nor BlockIdle after");
	Check(!IsAttackGroup(0) && !IsAttackGroup(3) && !IsAttackGroup(17) && !IsAttackGroup(255),
	      "idle, walking, the draw, garbage: no");
}

void TestOffset() {
	std::printf("The offset\n");
	const float step = 1.0f / 90.0f;
	Check(Near(TempoOffset(2.0f, step, 0.5f, 1.0f), 2.0f - 0.5f * step), "half speed: half the step taken back");
	Check(Near(TempoOffset(2.0f, step, 0.75f, 1.0f), 2.0f - 0.25f * step), "three quarters: a quarter taken back");
	Check(Near(TempoOffset(2.0f, step, 2.0f, 1.0f), 2.0f + step), "double: a step added on");
	Check(Near(TempoOffset(2.0f, step, 0.5f, 0.9f), 2.0f - 0.45f * step),
	      "half speed at a weapon's 0.9: half of the engine's 0.9 taken back");
	Check(Near(TempoOffset(2.0f, step, 2.0f, 1.2f), 2.0f + 1.2f * step), "double at 1.2: the engine's 1.2 added on");
	Check(TempoOffset(2.0f, step, 1.0f, 0.9f) == 2.0f, "the game's own speed: untouched, whatever the rate");
	Check(TempoOffset(2.0f, 0.0f, 0.5f, 1.0f) == 2.0f, "no step: untouched");
	Check(TempoOffset(2.0f, -step, 0.5f, 1.0f) == 2.0f, "a step back (a new clock): untouched");
	Check(TempoOffset(2.0f, step, 0.0f, 1.0f) == 2.0f && TempoOffset(2.0f, step, -1.0f, 1.0f) == 2.0f,
	      "no speed: untouched");
	Check(TempoOffset(2.0f, step, 0.5f, 0.0f) == 2.0f && TempoOffset(2.0f, step, 0.5f, kNaN) == 2.0f,
	      "no rate: untouched");
	Check(TempoOffset(2.0f, step, kNaN, 1.0f) == 2.0f && TempoOffset(2.0f, kNaN, 0.5f, 1.0f) == 2.0f,
	      "NaN: untouched");
	// Over a second of frames at half speed the sequence's time runs half of
	// what the engine would have run it: the engine runs it on by its rate
	// times each step, this takes half of that back.
	const float rates[] = {1.0f, 0.9f, 1.2f};
	for (float rate : rates) {
		float offset = -3.0f;
		float time = 0.0f;  // the engine's own run of the sequence's time
		for (int i = 0; i < 90; ++i) {
			time += rate * step;
			offset = TempoOffset(offset, step, 0.5f, rate);
		}
		Check(Near(time + offset + 3.0f, 0.5f * rate), "90 frames at half speed: half the engine's second of it");
	}
}

void TestKeyWindow() {
	std::printf("The key window\n");
	// Half speed: the engine's window starts a half step before the last one
	// ended; it is started where that one ended.
	const float step = 1.0f / 90.0f;
	Check(Near(ContinuousKeyStart(1.0f - 0.5f * step, 1.0f + 0.5f * step, 1.0f, true), 1.0f),
	      "started where the last window ended (the offset moved back)");
	// Double speed: the engine's window starts a step after the last one
	// ended; the keys in between are not skipped.
	Check(Near(ContinuousKeyStart(1.0f + step, 1.0f + 2.0f * step, 1.0f, true), 1.0f),
	      "started where the last window ended (the offset moved on)");
	Check(ContinuousKeyStart(1.0f, 1.0f + step, 1.0f, true) == 1.0f, "exactly continuous: as the engine has it");
	Check(ContinuousKeyStart(0.7f, 0.7f + step, 1.0f, false) == 0.7f, "no last end known: the engine's own");
	Check(ContinuousKeyStart(0.0f, step, 1.2f, true) == 0.0f, "another play from 0 after one ended at 1.2: its own");
	Check(ContinuousKeyStart(2.0f, 2.0f + step, 1.0f, true) == 2.0f, "a jump of a second on: its own");
	Check(ContinuousKeyStart(0.9f, 0.95f, 1.0f, true) == 0.9f, "the last end past this window's end: its own");
	Check(Near(ContinuousKeyStart(0.9f, 1.0f, 1.0f, true), 1.0f), "the last end exactly at the window's end: from there");
}

void TestScaling() {
	std::printf("The hold and the chance\n");
	Check(Near(ScaledHold(1.2f, 2.0f), 2.4f), "a 1.2 s hold at half the frequency: 2.4 s");
	Check(Near(ScaledHold(1.2f, 0.5f), 0.6f), "at twice: 0.6 s");
	Check(ScaledHold(1.2f, 1.0f) == 1.2f, "the game's own frequency: as drawn");
	Check(ScaledHold(0.0f, 2.0f) == 0.0f, "no hold stays none");
	Check(ScaledHold(1.2f, 0.0f) == 1.2f && ScaledHold(1.2f, -1.0f) == 1.2f, "no factor: as drawn");
	Check(std::isnan(ScaledHold(kNaN, 2.0f)), "a draw that is no number is left");
	Check(Near(ScaledChanceMult(0.75f, 0.5f, true), 0.375f), "the under-attack multiplier at half: 0.375");
	Check(ScaledChanceMult(0.75f, 0.5f, false) == 0.75f, "out of scope: the game's own");
	Check(ScaledChanceMult(1.0f, 1.0f, true) == 1.0f, "a factor of 1: the game's own");
	Check(ScaledChanceMult(1.0f, 0.0f, true) == 1.0f, "no factor: the game's own");
}

void TestMeasured() {
	std::printf("The speed read back\n");
	Check(Near(MeasuredSpeed(0.6f, 1.2f, 1.0f), 0.5f), "0.6 s of animation in 1.2 s at rate 1: half speed");
	Check(Near(MeasuredSpeed(0.72f, 1.2f, 1.2f), 0.5f),
	      "0.72 s of animation in 1.2 s at a weapon's rate 1.2: half speed too");
	Check(Near(MeasuredSpeed(0.48f, 1.2f, 0.8f), 0.5f), "0.48 s in 1.2 s at a slow weapon's 0.8: half speed too");
	Check(MeasuredSpeed(0.6f, 0.0f, 1.0f) == 0.0f, "nothing passed: 0");
	Check(MeasuredSpeed(0.6f, 1.2f, 0.0f) == 0.0f, "no rate: 0");
	Check(SequenceRate(1.2f) == 1.2f && SequenceRate(0.8f) == 0.8f && SequenceRate(1.0f) == 1.0f,
	      "a rate as measured");
	Check(SequenceRate(0.0f) == 1.0f && SequenceRate(-1.0f) == 1.0f && SequenceRate(kNaN) == 1.0f,
	      "no rate: the game's 1");
	Check(SequenceRate(0.2f) == 1.0f && SequenceRate(27.0f) == 1.0f, "a reading no weapon gives (a jump): the game's 1");
	Check(SequenceRate(0.25f) == 0.25f && SequenceRate(4.0f) == 4.0f, "the edges, taken");
}

void TestSwings() {
	std::printf("A swing begun\n");
	Check(SwingBegins(0, 0x3F0762F4, 0.0f, 0.11f), "an attack sequence in a slot that held none");
	Check(SwingBegins(0x3F07621C, 0x3F0762F4, 0.9f, 0.11f), "another attack sequence in the slot");
	Check(!SwingBegins(0x3F0762F4, 0x3F0762F4, 0.11f, 0.12f), "the same one playing on: no");
	Check(SwingBegins(0x3F0762F4, 0x3F0762F4, 0.9f, 0.0f), "the same one from the start again");
	Check(!SwingBegins(0x3F0762F4, 0x3F0762F4, 0.12f, 0.10f), "a frame's jitter back: no");
	Check(!SwingBegins(0x3F0762F4, 0, 0.9f, 0.0f) && !SwingBegins(0, 0, 0.0f, 0.0f), "no sequence: no");
}

}  // namespace

int main() {
	TestSettings();
	TestGroups();
	TestOffset();
	TestKeyWindow();
	TestScaling();
	TestMeasured();
	TestSwings();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

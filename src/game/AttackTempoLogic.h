#pragma once

// Enemy attacks slower and rarer (docs/combat-comfort-spec.md 4, items 1
// and 2; the tester, 2026-10-10: "gegner angriff tempo bei % slider in
// settings verringern können. dann wird die animation verlangsamt, sodass
// blocken und kampf in vr einfacher ist. und auch die schnelligkeit/
// häufigkeit mit der die gegner angreifen").
//
// The tempo ([Hands] EnemyAttackSpeed, a percentage): the attack sequences
// of those fighting the player - the anim groups AttackBow to
// AttackRightPower, in any of the actor's five sequence slots - have their
// time run that much as fast while they play. The engine keeps a sequence's
// time as the anim data's clock plus the sequence's own offset
// (NiControllerSequence +0x48), as the draw speed found (WeaponDrawSpeed.h):
// each frame the offset is moved by (speed - 1) times the clock's step, so
// the pose, the keys - the blow's "hit" among them - and the group's end all
// come `speed` times as fast, in their order. The engine fires a sequence's
// keys over the window from its time at the clock before the step to its
// time after, both read with the offset as it is then (0x00476F4D,
// 0x00477A4B); an offset moved back between two updates starts the next
// window before the last one ended, and a key in the overlap would fire
// twice - a blow landing twice. So the one call that fires them
// (0x00477AB1 -> 0x0051AF70) goes through a wrapper that starts a followed
// sequence's window where the last one ended (ContinuousKeyStart): every key
// once, whichever way the offset went.
//
// The frequency ([Hands] EnemyAttackFrequency, a percentage): the melee
// decision rolls against the attack chance the combat style computes
// (0x00546800: the style's attack chance plus its bonuses, times the
// style's "attack while under attack" or "attack not under attack"
// multiplier - the CS wiki's Combat Style page), and a decision to hold
// waits a random hold timer (0x00546B20 between the style's min and max,
// 0.5-1.5 s by the defaults fAIDefaultHoldMinTime/MaxTime) before the next.
// The two multipliers' getters (0x004AA0B0, 0x004AA130) are detoured to
// return theirs times the setting, and the two hold draws' call sites
// (0x00619176, 0x00620BFB) to draw theirs divided by it - only while the
// style was fetched for an actor fighting the player (Actor::GetCombatStyle
// 0x005E0F50, called right before each: the scope). Half the frequency:
// half the chance an attack is chosen, twice the hold between decisions.
//
// Pure: the decisions here, the engine side in game/AttackTempo.cpp;
// attack_tempo_test.

#include "core/Types.h"

namespace obvr::game {

struct AttackTempoSettings {
	bool active = false;   // Full VR running: the attacks of those fighting the player are watched
	bool enabled = false;  // and something to change
	float speed = 1.0f;    // the attack sequences' time runs this times as fast
	float chance = 1.0f;   // the attack chance's multiplier
	float hold = 1.0f;     // the hold between melee decisions' multiplier
};

// A percentage as a factor: 100 the game's own; out of its range, or not a
// number, the game's own too (0 is what a word in the INI reads as).
inline float PercentFactor(float percent, float lowest, float highest) {
	if (!(percent >= lowest) || !(percent <= highest)) {
		return 1.0f;
	}
	return percent / 100.0f;
}

inline constexpr float kAttackSpeedPercentMin = 20.0f;
inline constexpr float kAttackSpeedPercentMax = 200.0f;
inline constexpr float kAttackFrequencyPercentMin = 10.0f;
inline constexpr float kAttackFrequencyPercentMax = 200.0f;

// The settings from the INI's percentages. With the mode off, or both at
// 100, nothing is enabled and the engine is left alone.
inline AttackTempoSettings AttackTempoFrom(bool fullVr, float speedPercent, float frequencyPercent) {
	AttackTempoSettings s;
	if (!fullVr) {
		return s;
	}
	s.active = true;
	s.speed = PercentFactor(speedPercent, kAttackSpeedPercentMin, kAttackSpeedPercentMax);
	s.chance = PercentFactor(frequencyPercent, kAttackFrequencyPercentMin, kAttackFrequencyPercentMax);
	s.hold = 1.0f / s.chance;
	s.enabled = s.speed != 1.0f || s.chance != 1.0f;
	return s;
}

// The attack groups, xOBSE NiNodes.h's list: 19 AttackBow, 20 AttackLeft,
// 21 AttackRight, 22 AttackPower, 23-26 the directional power attacks.
inline constexpr UInt8 kAnimGroupAttackBow = 19;
inline constexpr UInt8 kAnimGroupAttackRightPower = 26;

inline bool IsAttackGroup(UInt8 code) { return code >= kAnimGroupAttackBow && code <= kAnimGroupAttackRightPower; }

// A playing sequence's offset moved so that its time ran `speed` times as
// fast over this step of the clock: the engine ran it on by `rate` times
// the step - its own rate for the sequence, the weapon's speed, which the
// engine adds in the same way (harness 2026-10-11: at the game's own speed
// a swing's time ran 0.9 of the clock for the bandit's weapon, and with a
// plain half step taken back it ran 0.4, not 0.45 - the engine's share and
// this one add) - and this takes the difference back (or adds more, past
// 1). No step, or a speed of 1, leaves it; a speed or a rate that is none
// (0, negative, NaN) leaves it too.
inline float TempoOffset(float offset, float step, float speed, float rate) {
	if (!(speed > 0.0f) || !(step > 0.0f) || !(rate > 0.0f)) {
		return offset;
	}
	return offset + (speed - 1.0f) * rate * step;
}

// Where a followed sequence's key window starts: at the end of the last one,
// so no key fires twice (the offset moved back) and none is skipped (moved
// on). A last end further than this from the engine's start is another play
// of the same sequence (an attack re-used after a restart begins at 0 again),
// and the engine's own start stands; so does one past the window's end.
inline constexpr float kKeyWindowGapMax = 0.5f;

inline float ContinuousKeyStart(float from, float to, float lastTo, bool haveLast) {
	if (!haveLast) {
		return from;
	}
	const float gap = from - lastTo;
	if (gap > kKeyWindowGapMax || gap < -kKeyWindowGapMax) {
		return from;
	}
	return lastTo <= to ? lastTo : from;
}

// The hold the melee decision drew, scaled: seconds times the hold factor;
// nothing below 0, and a draw that is no number is left.
inline float ScaledHold(float seconds, float hold) {
	if (!(seconds >= 0.0f) || !(hold > 0.0f)) {
		return seconds;
	}
	return seconds * hold;
}

// The chance multiplier the combat style's getter hands back, scaled: only
// in scope (the style fetched for someone fighting the player) and with a
// factor to apply.
inline float ScaledChanceMult(float mult, float chance, bool inScope) {
	return inScope && chance > 0.0f ? mult * chance : mult;
}

// The measured speed of a followed attack, for the log: the sequence's time
// it ran through, over the clock that passed at the sequence's own rate.
// At the game's own speed an attack sequence's time runs at the weapon's
// speed - 0.8 or 1.2 of the clock for the harness bandit's weapons, read
// in the harness on 2026-10-11 as the engine advanced it over the first
// frame followed, before the offset was touched (its frequency at +0x28
// reads 1 throughout; where the engine applies the weapon's speed is not
// located). So `rate` seconds of the sequence per second of the clock is
// the game's own; this is what fraction of that ran. 0 when nothing
// passed, or no rate.
inline float MeasuredSpeed(float sequenceTimeRun, float clockRun, float rate) {
	return clockRun > 0.0f && rate > 0.0f ? sequenceTimeRun / (clockRun * rate) : 0.0f;
}

// A sequence's rate as measured over one frame: anything that is no rate
// (0, negative, NaN, or past what a weapon's speed can be - a sequence
// restarted or jumped between two readings) counts as the game's 1.
inline constexpr float kSequenceRateMin = 0.25f;
inline constexpr float kSequenceRateMax = 4.0f;

inline float SequenceRate(float rate) {
	return rate >= kSequenceRateMin && rate <= kSequenceRateMax ? rate : 1.0f;
}

// A swing begun, for the count the log gives whatever the settings: an
// attack sequence playing in a slot that held none, or another one, last
// frame - or the same one from the start again (its time behind the last
// read by more than a frame's jitter; the engine re-uses one sequence object
// for every play of the attack).
inline constexpr float kSwingRestartSlack = 0.05f;

inline bool SwingBegins(UInt32 previous, UInt32 sequence, float previousTime, float time) {
	if (sequence == 0) {
		return false;
	}
	return previous != sequence || time + kSwingRestartSlack < previousTime;
}

}  // namespace obvr::game

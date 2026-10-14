#pragma once

// Exhaustion instead of the collapse, and instead of the paralysis fall
// (docs/combat-comfort-spec.md; the tester, 2026-10-12: "default an, neuer
// toggle: wenn fatigue low geht gibts statt knockdown werden die waffen
// super schwer wie in walking dead saints n sinners. man kann damit kaum
// schwingen. und der player keucht und es gibt einen sound. selbes für
// paralyse").
//
// Vanilla puts the player on the ground when their fatigue falls below 0
// (the fatigue collapse) and when they are paralysed (the fall, in third
// person) - both from the knock state's step (0x006545E0, the process
// vtable's +0x2F4; game/PlayerStagger.h), which OBVR skips for the player
// while this is on (CollapseSkipped): they stay on their feet, the fatigue
// and the paralysis themselves stay the game's. Instead the EXHAUSTED state:
// begun under a share of the base fatigue or while paralysed, over above a
// higher share and not paralysed (so the edge does not flicker; vanilla
// regenerates fatigue), eased in over half a second and out over one and a
// half. While it holds the drawn weapon's weight is multiplied
// (vr::WeaponLagExhausted - the arms heavy, the swings slow), and the player
// breathes hard with the game's own gasps (game/GameSound.h): one at full
// volume the moment a fall was replaced, and the pants after, paced as
// Realistic Fatigue paces its panting. Pure, exhaustion_test.

#include "core/Types.h"

namespace obvr::game {

struct ExhaustionSettings {
	bool enabled = true;         // [Look] Exhaustion
	float belowPercent = 12.0f;  // [Look] ExhaustionBelowPercent: exhausted under this share of the base fatigue
	float abovePercent = 25.0f;  // [Look] ExhaustionAbovePercent: recovered above this
	float weight = 2.5f;         // [Look] ExhaustionWeight: the weapon's lag times this, at full (the
	                             // tester, 2026-10-13: at x4 "attacken sind verboten", x2.5 "wirkt ok")
	bool sounds = true;          // [Look] ExhaustionSounds: the breath and the gasp
};

inline constexpr float kExhaustionInSeconds = 0.5f;   // the ease into the state
inline constexpr float kExhaustionOutSeconds = 1.5f;  // and out of it

// The settings as used: the lower edge 1 to 50 % (else 12), the upper 1 to
// 90 % and above the lower (else 25, or a tenth above the lower).
inline float ExhaustionBelowShare(float percent) {
	return percent >= 1.0f && percent <= 50.0f ? percent * 0.01f : 0.12f;
}
inline float ExhaustionAboveShare(float belowShare, float percent) {
	const float share = percent >= 1.0f && percent <= 90.0f ? percent * 0.01f : 0.25f;
	return share > belowShare ? share : belowShare + 0.1f;
}
// The weight factor 1 to 10, else 2.5.
inline float ExhaustionWeightFactor(float setting) {
	return setting >= 1.0f && setting <= 10.0f ? setting : 2.5f;
}

// Whether the player's knock state step is skipped this frame - the step
// that would begin the collapse (fatigue below 0) or the paralysis fall:
// only with the state 0 (standing; a knockdown under way, the lying and the
// get-up keep running) and only when the step would begin one.
inline bool CollapseSkipped(bool enabled, bool isPlayer, SInt32 knockState, float fatigueNow, bool paralyzed) {
	return enabled && isPlayer && knockState == 0 && (fatigueNow < 0.0f || paralyzed);
}

struct ExhaustionState {
	bool exhausted = false;
	float ease = 0.0f;    // 0 fresh, 1 fully exhausted
	float seconds = 0.0f;  // how long the state has held
	bool byParalysis = false;  // begun by the paralysis, not the fatigue
};

struct ExhaustionVerdict {
	bool began = false;
	bool ended = false;
	float ease = 0.0f;
	float factor = 1.0f;   // the weapon's lag factor this frame (1 + (weight - 1) * ease)
	float share = 1.0f;    // fatigue over base, when known
	float seconds = 0.0f;  // how long the state has held; at its end, the whole of it
};

inline ExhaustionVerdict StepExhaustion(ExhaustionState& s, const ExhaustionSettings& g, bool fatigueKnown,
                                        float fatigueNow, float fatigueBase, bool paralyzed, float dtSeconds) {
	ExhaustionVerdict v;
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	const bool shareKnown = fatigueKnown && fatigueBase > 0.0f && fatigueNow == fatigueNow;
	const float share = shareKnown ? fatigueNow / fatigueBase : 1.0f;
	v.share = share;
	const float below = ExhaustionBelowShare(g.belowPercent);
	const float above = ExhaustionAboveShare(below, g.abovePercent);
	const bool low = shareKnown && share < below;
	const bool recovered = !shareKnown || share > above;
	if (!g.enabled) {
		if (s.exhausted) {
			s.exhausted = false;
			v.ended = true;
		}
	} else if (!s.exhausted && (low || paralyzed)) {
		s.exhausted = true;
		s.seconds = 0.0f;
		s.byParalysis = paralyzed && !low;
		v.began = true;
	} else if (s.exhausted && !paralyzed && recovered) {
		s.exhausted = false;
		v.ended = true;
	}
	if (s.exhausted) {
		s.seconds += dt;
		s.ease += dt / kExhaustionInSeconds;
		if (s.ease > 1.0f) {
			s.ease = 1.0f;
		}
	} else {
		s.ease -= dt / kExhaustionOutSeconds;
		if (s.ease < 0.0f) {
			s.ease = 0.0f;
		}
	}
	v.seconds = s.seconds;
	if (v.ended) {
		s.seconds = 0.0f;
	}
	v.ease = s.ease;
	v.factor = 1.0f + (ExhaustionWeightFactor(g.weight) - 1.0f) * s.ease;
	return v;
}

// The breath and the gasp, as cues for the wave player (one wave at a
// time through winmm: the gasp first, the breath's loop after it).
// ------------------------------------------------------------- the breath
// The tester (2026-10-13): "die schnaufer und keuchen gefallen mir nicht,
// wie machen andere mods das? ... so nah wie möglich an vanilla". Vanilla
// has no breathing for low fatigue, only the collapse; its human gasps -
// NPCHumanGaspMale and NPCHumanGaspFemale, the folders
// sound/fx/npc/human/gasp/male and /female of four files each (1.0 to 2.4
// s), one picked by the engine each play - are the nearest it has, and
// they are what is played, by the player's sex, through the game's own
// sound system (its volume sliders apply; game::PlaySoundFormVolume).
// Paced as Realistic Fatigue (abo, Nexus 10925; its readme, read
// 2026-10-13) paces its panting: a heartbeat every 1.0 to 0.333 s by the
// fatigue missing (blurInterval = 1 - 0.6666 * level^2), a pant every four
// beats (pantInterval = 4 * blurInterval), louder the more is missing
// (pantLevel = level^2). The moment a fall was spared: one gasp at full
// volume, the pants a whole interval after it. A spell begun without a
// fall: the first pant half a second in.
inline constexpr UInt32 kSoundFormGaspMale = 0x000C80ED;
inline constexpr UInt32 kSoundFormGaspFemale = 0x000C80EC;
inline constexpr float kHeartbeatRestSeconds = 1.0f;
inline constexpr float kHeartbeatSpan = 0.6666f;
inline constexpr float kPantBeats = 4.0f;
inline constexpr float kPantFirstSeconds = 0.5f;
inline constexpr float kPantLeastVolume = 0.25f;

// How much of the fatigue is missing, 0 to 1: paralysed, or the share not
// known, counts as all of it.
inline float ExhaustionMissing(bool shareKnown, float share, bool paralyzed) {
	if (paralyzed || !shareKnown || !(share == share)) {
		return 1.0f;
	}
	const float missing = 1.0f - share;
	return missing < 0.0f ? 0.0f : (missing > 1.0f ? 1.0f : missing);
}

inline float HeartbeatSeconds(float missing) {
	const float m = missing < 0.0f ? 0.0f : (missing > 1.0f ? 1.0f : missing);
	return kHeartbeatRestSeconds - kHeartbeatSpan * m * m;
}

inline float PantIntervalSeconds(float missing) { return kPantBeats * HeartbeatSeconds(missing); }

inline float PantVolume(float missing) {
	const float m = missing < 0.0f ? 0.0f : (missing > 1.0f ? 1.0f : missing);
	const float volume = m * m;
	return volume < kPantLeastVolume ? kPantLeastVolume : volume;
}

enum class BreathCue : UInt8 { None, Gasp, Pant };

struct BreathState {
	bool on = false;          // in a spell of breathing
	float sincePant = 0.0f;   // seconds since the last pant (or the gasp)
	UInt32 pants = 0;         // this spell
	UInt32 gasps = 0;
};

struct BreathVerdict {
	BreathCue cue = BreathCue::None;
	float volume = 0.0f;    // for the cue, 0 to 1
	float interval = 0.0f;  // seconds between the pants now
};

// `gaspNow`: a fall was replaced this frame; `missing` as ExhaustionMissing.
// Sounds off, or the state over: the spell ends, nothing to stop (the
// gasps are one-shots).
inline BreathVerdict StepBreath(BreathState& b, bool sounds, bool exhausted, bool gaspNow, float missing,
                                float dtSeconds) {
	BreathVerdict v;
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	if (!sounds || !exhausted) {
		b = BreathState{};
		return v;
	}
	v.interval = PantIntervalSeconds(missing);
	if (!b.on) {
		b.on = true;
		b.sincePant = v.interval - kPantFirstSeconds;
	}
	if (gaspNow) {
		b.sincePant = 0.0f;
		++b.gasps;
		v.cue = BreathCue::Gasp;
		v.volume = 1.0f;
		return v;
	}
	b.sincePant += dt;
	if (b.sincePant >= v.interval) {
		b.sincePant = 0.0f;
		++b.pants;
		v.cue = BreathCue::Pant;
		v.volume = PantVolume(missing);
	}
	return v;
}

}  // namespace obvr::game

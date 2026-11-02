#pragma once

// One speed for every blunt blow, and vanilla's chance of a knockdown (Nadi,
// 2026-11-02: "Mach b mit der Option die Formel auszuschalten, default an.
// Und mach alle Geschwindigkeiten einheitlich egal wie die Methode. Nimm
// realistische Werte").
//
// The speeds. Every blow by a body - the open hand's shove and slap, the off
// hand's punch, the torch, the shield bash, the pommel, the half-sword's
// hilt, the grapple's throw, a thrown thing - staggers from StaggerSpeed and
// knocks down from KnockdownSpeed (m/s at the contact), whatever struck.
// Measured punches (fist speed in m/s): trained boxers peak at 7.9 (jab and
// cross) and 6.9 (hook), the untrained at 6.9 and 7.6 (Menzel & Potthast
// 2021, Sensors 21(23) 7882, table 6); at the contact 5.6 elite and 4.9
// junior (Liu et al. 2022, PMC9798280). So a knockdown asks for a full,
// committed blow at its contact - 6 m/s, above an amateur's contact speed -
// and a stagger for a solid deliberate hit, half of that. Nadi's hilt blows
// in the headset ran 5.3 to 5.7 m/s (2026-11-01) and knocked down too
// easily at 4.
//
// The chance (game/Shove.cpp rolls it). The engine's own "knockdown" roll
// for a hit - the stagger, by the CS wiki ("'Knockdown' refers to what is
// more commonly known as 'staggering'") - read in Oblivion.exe at
// 0x005475D0 with its helpers 0x00547B90 and 0x00547F00 (OBNG
// knowledge/hit-reactions.md):
//   p = (damage * fKnockdownDamageMult + fKnockdownDamageBase)
//       / (fKnockdownAgilMult * A * F + fKnockdownAgilBase),  at most fKnockdownChance
//   A = clamp(Agility + Luck * fActorLuckSkillMult + iActorLuckSkillBase, 0, 100)
//   F = fFatigueBase - (1 - fatigue / base fatigue) * fFatigueMult
// With [Hands] VanillaKnockdown a blow fast enough to knock down does so
// with that chance, else it staggers; the damage is the player's fist's
// (FistDamage) - every one of these blows is a fist's in the engine
// (game::KickStrike). The stagger stays the speed's.
//
// Pure, covered by impact_test.

#include "core/Types.h"

namespace obvr::game {

struct ImpactSettings {
	float staggerSpeed = 3.0f;    // [Hands] StaggerSpeed, m/s
	float knockdownSpeed = 6.0f;  // [Hands] KnockdownSpeed, m/s
	bool vanillaKnockdown = true; // [Hands] VanillaKnockdown
};

// The settings the roll reads, as the game holds them. The defaults are
// vanilla's: the EXE's constructors (fKnockdownAgilBase 0, AgilMult 1,
// DamageBase 0, iActorLuckSkillBase -20, fActorLuckSkillMult 0.4) and
// Oblivion.esm's GMST records over them (fKnockdownDamageMult 0.3,
// fKnockdownChance 0.3, fFatigueBase 1, fFatigueMult 0.5).
struct KnockdownGmst {
	float agilBase = 0.0f;
	float agilMult = 1.0f;
	float damageBase = 0.0f;
	float damageMult = 0.3f;
	float chance = 0.3f;
	SInt32 luckBase = -20;
	float luckMult = 0.4f;
	float fatigueBase = 1.0f;
	float fatigueMult = 0.5f;
};

// 0x00547B90: the attribute with the luck, held to 0..100.
inline float KnockdownAgility(const KnockdownGmst& g, float agility, float luck) {
	const float a = agility + luck * g.luckMult + static_cast<float>(g.luckBase);
	return a <= 0.0f ? 0.0f : (a >= 100.0f ? 100.0f : a);
}

// 0x00547F00: the fatigue's factor, 1 rested (with fFatigueBase 1).
inline float KnockdownFatigue(const KnockdownGmst& g, float fatigueRatio) {
	return g.fatigueBase - (1.0f - fatigueRatio) * g.fatigueMult;
}

// 0x005475D0's chance (its roll is `rand() % 100 / 100 < p`). The damage
// is taken whole, as the engine casts it to an int. A denominator of 0 or
// less (settings changed by a mod) gives the cap.
inline float VanillaKnockdownChance(const KnockdownGmst& g, float agility, float luck, float fatigueRatio,
                                    float damage) {
	const float whole = static_cast<float>(static_cast<SInt32>(damage));
	const float below =
		g.agilMult * KnockdownAgility(g, agility, luck) * KnockdownFatigue(g, fatigueRatio) + g.agilBase;
	if (!(below > 0.0f)) {
		return g.chance;
	}
	const float p = (whole * g.damageMult + g.damageBase) / below;
	return p > g.chance ? g.chance : (p < 0.0f ? 0.0f : p);
}

// A fist's health damage (UESP, Oblivion:Hand_to_Hand: "Health_Damage = 1 +
// 10.5 * (Strength/100) * (HandToHandSkill/100)", both held to 100), a power
// blow 2.5 times that (the Novice's power attack, the same page). A blow
// fast enough to knock down is struck as a power one (game::KickStrike).
inline constexpr float kPowerBlowDamageMult = 2.5f;

inline float FistDamage(float strength, float handToHand, bool power) {
	const float s = strength < 0.0f ? 0.0f : (strength > 100.0f ? 100.0f : strength);
	const float h = handToHand < 0.0f ? 0.0f : (handToHand > 100.0f ? 100.0f : handToHand);
	const float d = 1.0f + 10.5f * (s / 100.0f) * (h / 100.0f);
	return power ? d * kPowerBlowDamageMult : d;
}

// The blow's kind by its speed alone: 0 none, 1 a stagger, 2 a knockdown.
inline UInt8 ImpactTier(const ImpactSettings& s, float speed) {
	if (!(speed >= s.staggerSpeed)) {
		return 0;
	}
	return speed >= s.knockdownSpeed ? 2 : 1;
}

// Whether a knockdown stands after the roll: without VanillaKnockdown
// always; with it when `roll` (0..1) falls under `chance`.
inline bool KnockdownStands(bool vanillaKnockdown, float chance, float roll) {
	return !vanillaKnockdown || roll < chance;
}

}  // namespace obvr::game

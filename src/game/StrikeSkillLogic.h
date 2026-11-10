#pragma once

#include "core/Types.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

// Which skill a VR strike deals its damage with and trains, and whether the
// struck one reacts to it as to an attack (Nadi, 2026-11-03: every strike a
// proper hit, "1) schaden + training: blocken 2) schaden: blunt 3) schaden :
// blunt 4) schaden + training: h2h ... 9) sollte kein crime sein").
//
// Every strike below is the engine's own fist blow (game::KickStrike): the
// damage is the hand-to-hand formula of AttackHandling 0x005FEBF0 with the
// chosen skill's value in place of hand-to-hand's - the damage call at
// 0x005FF419 (0x00547280, cdecl, the skill's value its first argument) and
// the power factor at 0x005FF44D (0x00546BA0, cdecl, the same) - and the use
// it trains is credited to the chosen skill (game/SkillCreditLogic.h).
// Read 2026-11-03 in Oblivion.exe (dumpbin /disasm).
//
// The reaction is the struck one's vtable +0x3A8 (0x005FE380, thiscall
// (attacker, 0), ret 8), the call at 0x006005B1..0x006005BF that ends every
// landed hit: hostility, the assault, the alarm - the crime. A shove alone
// leaves it out; its hard form still counts as a hit through the shove's own
// ShoveCountsAsHit (game/ShoveLogic.h), as before.
//
// Pure: game/MeleeHits.cpp reads and patches; covered by strike_skill_test.

// Actor value codes (the engine's skill list; xOBSE GameForms.h kActorVal_*).
inline constexpr UInt32 kSkillBlade = 0x0E;
inline constexpr UInt32 kSkillBlock = 0x0F;
inline constexpr UInt32 kSkillBlunt = 0x10;
inline constexpr UInt32 kSkillHandToHand = 0x11;
inline constexpr UInt32 kSkillMarksman = 0x1C;

enum class BlowKind : UInt8 {
	Kick,          // the kick (unchanged: hand to hand)
	ShieldBash,    // the shield's bash
	Pommel,        // the pommel's blow
	Hilt,          // the half-sword's hilt
	Torch,         // the torch's blow
	OffHandPunch,  // the free hand's fist with a weapon drawn
	Slap,          // the open hand
	Shove,         // the push
	GrappleThrow,  // one held standing, thrown
	ThrownWeapon,  // a weapon thrown at someone
	ThrownThing,   // anything else thrown at someone
};

struct StrikeRule {
	UInt32 skill = kSkillHandToHand;  // the damage's and the training's skill
	bool reacts = true;               // the struck one told of an attack (the crime)
	bool quietHit = false;            // the engine's hit without its own sound (a slap)
};

// The weapon type's skill (WeaponTypeCode, game/MeleeHit.h): the blades
// Blade, the blunt ones Blunt, the bow Marksman. The staff has no skill of
// its own in the game; thrown it is a stick, so Blunt. No weapon: hand to
// hand.
inline UInt32 WeaponSkillOf(SInt32 weaponType) {
	switch (weaponType) {
		case 0:
		case 1:
			return kSkillBlade;
		case 2:
		case 3:
		case 4:
			return kSkillBlunt;
		case 5:
			return kSkillMarksman;
		default:
			return kSkillHandToHand;
	}
}

// `weaponType` is read only for a thrown weapon: the weapon's own type.
inline StrikeRule StrikeRuleFor(BlowKind kind, SInt32 weaponType = -1) {
	StrikeRule r;
	switch (kind) {
		case BlowKind::ShieldBash:
			r.skill = kSkillBlock;
			break;
		case BlowKind::Pommel:
		case BlowKind::Hilt:
			r.skill = kSkillBlunt;
			break;
		case BlowKind::ThrownWeapon:
			r.skill = WeaponSkillOf(weaponType);
			break;
		case BlowKind::Shove:
			r.reacts = false;
			break;
		case BlowKind::Slap:
			r.quietHit = true;
			break;
		default:
			break;
	}
	return r;
}

// Whether the engine's hit is struck without its own sound (its weapon
// sound 0x006AF880, which AttackHandling calls for the hit on flesh;
// game/StrikeRules.cpp, its detour): the slap alone - its noise is the slap's (ShoveLogic.h, SlapPlaysOwnSound),
// the fist's hit on top of it read as a punch (Nadi, 2026-11-10).
inline bool StrikeQuietsEngineSound(BlowKind kind) { return StrikeRuleFor(kind).quietHit; }

// Whether the weapon sound's detour skips a call: only while a quiet hit runs
// (`quietThread`, its thread's id, 0 for none) and only on that thread - a
// sound another thread plays meanwhile is played.
inline bool WeaponSoundSkipped(UInt32 quietThread, UInt32 callingThread) {
	return quietThread != 0 && quietThread == callingThread;
}

inline const char* BlowKindName(BlowKind kind) {
	switch (kind) {
		case BlowKind::Kick:
			return "kick";
		case BlowKind::ShieldBash:
			return "shield bash";
		case BlowKind::Pommel:
			return "pommel";
		case BlowKind::Hilt:
			return "hilt";
		case BlowKind::Torch:
			return "torch";
		case BlowKind::OffHandPunch:
			return "off-hand punch";
		case BlowKind::Slap:
			return "slap";
		case BlowKind::Shove:
			return "shove";
		case BlowKind::GrappleThrow:
			return "grapple throw";
		case BlowKind::ThrownWeapon:
			return "thrown weapon";
		case BlowKind::ThrownThing:
			return "thrown thing";
	}
	return "?";
}

// Which strike an open or closed hand at someone is (game::StrikeAndShove):
// the torch's blow, the off hand's fist, an open hand in the face a slap,
// elsewhere a shove.
inline BlowKind HandStrikeKind(bool torch, bool punch, bool inTheFace) {
	if (torch) {
		return BlowKind::Torch;
	}
	if (punch) {
		return BlowKind::OffHandPunch;
	}
	return inTheFace ? BlowKind::Slap : BlowKind::Shove;
}

// The shove's settings after the engine's hit: told of the attack by the hit
// already, the struck one is not told twice (the shield bash's rule,
// BashShoveSettings) - fatigue and liking stay the shove's. A quiet hit (a
// shove) or none leaves the shove's own rule: the hard one a hit.
inline ShoveSettings SettingsAfterStrike(const ShoveSettings& shove, bool engineHit, bool reacted) {
	ShoveSettings s = shove;
	if (engineHit && reacted) {
		s.countsAsHit = false;
		s.lightCountsAsHit = false;
	}
	return s;
}

// The bonus a cut or a crushed head trains on top of the killing blow
// (Nadi, 2026-11-03: "decaps und zerstückelungen geben extra erfahrung"):
// the weapon's skill for a head or a hand off, Blunt for a crushed head.
// Not for the debug cuts or the cuts done again after a load - those never
// come here.
enum class DismemberBonus : UInt8 { HeadOff, HandOff, HeadCrushed };

struct DismemberBonusInput {
	bool enabled = false;          // [Hands] StrikesTrainSkills
	bool targetExcepted = false;   // the target's base in the exception list
	DismemberBonus what = DismemberBonus::HeadOff;
	SInt32 weaponType = -1;        // the drawn weapon's type
	SInt32 headUses = 5;           // [Hands] DismemberHeadSkillUses
	SInt32 handUses = 2;           // [Hands] DismemberHandSkillUses
};

struct DismemberBonusPlan {
	UInt32 skill = kSkillHandToHand;
	UInt32 uses = 0;
};

// More than this many uses at once is no setting anyone meant.
inline constexpr SInt32 kMaxDismemberBonusUses = 50;

inline UInt32 ClampBonusUses(SInt32 uses) {
	if (uses <= 0) {
		return 0;
	}
	return static_cast<UInt32>(uses < kMaxDismemberBonusUses ? uses : kMaxDismemberBonusUses);
}

inline DismemberBonusPlan PlanDismemberBonus(const DismemberBonusInput& in) {
	DismemberBonusPlan plan;
	if (!in.enabled || in.targetExcepted) {
		return plan;
	}
	switch (in.what) {
		case DismemberBonus::HeadOff:
			plan.skill = WeaponSkillOf(in.weaponType);
			plan.uses = ClampBonusUses(in.headUses);
			break;
		case DismemberBonus::HandOff:
			plan.skill = WeaponSkillOf(in.weaponType);
			plan.uses = ClampBonusUses(in.handUses);
			break;
		case DismemberBonus::HeadCrushed:
			plan.skill = kSkillBlunt;
			plan.uses = ClampBonusUses(in.headUses);
			break;
	}
	return plan;
}

}  // namespace obvr::game

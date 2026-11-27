#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/BlockAngleLogic.h"
#include "game/GameAddresses.h"
#include "game/NiMath.h"

namespace obvr::game {

// The parry (docs/weapon-collision-spec.md, 4 E; the tester, 2026-10-09,
// decision 3: a parry stops the whole blow - "gar keiner", no damage comes
// through - and costs fatigue).
//
// Someone attacks; the player's drawn blade meets theirs while their attack
// is in flight. Their blow is then resolved as blocked by the engine's own
// path in the hit handler 0x005FEBF0 (Oblivion.exe 1.2.0.416, read
// 2026-10-09 with dumpbin):
//
//   0x005FF7DF  call 005E5670   thiscall(target) - is the target blocking
//                               (its process action Block, 6)
//   0x005FF83E  call 006131D0   cdecl(target, attacker or its arrow, angle*)
//                               - the attacker in front (already rerouted,
//                               game/BlockCone.h)
//   0x005FF8C7  call 005474A0   cdecl(Block skill (actor value 0x0F), Luck
//                               (7), a factor, two flags) - the share of the
//                               blow blocked, in st0
//
// For a parried blow the first answers yes, the second yes, the third 1 -
// all of it. The engine does the rest as for any blocked blow: the block
// reaction, Block's experience, the attacker's recoil (0x005F4F00 at
// 0x00600565). A block forced for a parry and met by another attacker is
// answered "not in front", so it stays a hit.
//
// Pure: the decisions here, the calls in game/Parry.cpp; parry_test.

// An attack in flight, by the actor's process action (GameAddresses.h's
// kAction_ enum): the swing and its follow-through.
inline bool IsAttackAction(SInt32 action) {
	return action == addr::kActionAttack || action == addr::kActionAttackFollowThrough;
}

// Their blade from their drawn "Weapon" node, in the world: from the node
// along its own y axis to its bound's far end on that axis - the player's
// own blade's reading (BladeSpanFromNode), taken in the world. False for no
// weapon in the hand (no bound, or a reach under 10 units or over 200).
inline bool TheirBladeFromNode(const NiPoint3& attach, const NiPoint3& axis, const NiPoint3& boundCentre,
                               float boundRadius, NiPoint3& a, NiPoint3& b) {
	const float axisLength = axis.LengthSquared();
	if (!(axisLength > 0.81f && axisLength < 1.21f) || !(boundRadius > 0.0f)) {
		return false;
	}
	const NiPoint3 toCentre = boundCentre - attach;
	const float along = DotOf3(toCentre, axis) + boundRadius;
	if (!(along >= 10.0f && along <= 200.0f)) {
		return false;
	}
	a = attach;
	b = attach + axis * along;
	return true;
}

// Someone with a blade near the player's: theirs now and the frame before.
// Or with a shield on the arm (its disc in the world, read as the player's
// own is - game/ShieldShape): a foe is listed with either.
struct BladeFoe {
	UInt32 actor = 0;
	SInt32 action = addr::kActionNone;
	bool bladeValid = true;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	bool haveLast = false;
	NiPoint3 lastA{0.0f, 0.0f, 0.0f};
	NiPoint3 lastB{0.0f, 0.0f, 0.0f};
	bool shieldValid = false;
	NiPoint3 shieldCentre{0.0f, 0.0f, 0.0f};
	NiPoint3 shieldNormal{0.0f, 0.0f, 1.0f};
	float shieldRadius = 0.0f;
	float shieldHalfThick = 0.0f;
};

inline constexpr UInt32 kBladeFoesMax = 8;

struct BladeFoes {
	UInt32 count = 0;
	BladeFoe foe[kBladeFoesMax];
};

// The foes' blades and shields as stoppers for the player's blade and hands
// (BladeContactLogic.h, the stoppers): each blade a capsule a little wider
// than the blade's own body, each shield its disc, by the foe's ids. Answers
// how many were added.
inline constexpr float kFoeBladeStopRadiusUnits = 2.5f;

inline UInt32 AddFoeStoppers(const BladeFoes& foes, BladeBodies& stoppers, BladeDiscs& discs) {
	UInt32 added = 0;
	for (UInt32 i = 0; i < foes.count; ++i) {
		const BladeFoe& f = foes.foe[i];
		if (f.actor == 0) {
			continue;
		}
		if (f.bladeValid && stoppers.Add(f.a, f.b, kFoeBladeStopRadiusUnits, FoeBladeId(f.actor))) {
			++added;
		}
		if (f.shieldValid &&
		    discs.Add(f.shieldCentre, f.shieldNormal, f.shieldRadius, f.shieldHalfThick, FoeShieldId(f.actor))) {
			++added;
		}
	}
	return added;
}

// Two blades meet within this: the player's blade's half-width and theirs.
inline constexpr float kParryReachUnits = 4.0f;
// The frame is looked at in this many steps, both blades moving at once.
inline constexpr UInt32 kParrySteps = 4;

// Whether two blades, each from where it was to where it is, came within
// `reach` of each other during the frame; the share of the frame and a point
// between them where they did.
inline bool BladesMeet(const NiPoint3& oursLastA, const NiPoint3& oursLastB, const NiPoint3& oursA,
                       const NiPoint3& oursB, const NiPoint3& theirsLastA, const NiPoint3& theirsLastB,
                       const NiPoint3& theirsA, const NiPoint3& theirsB, float reach, float& share, NiPoint3& point) {
	for (UInt32 k = 0; k <= kParrySteps; ++k) {
		const float s = static_cast<float>(k) / static_cast<float>(kParrySteps);
		const NiPoint3 oa = oursLastA + (oursA - oursLastA) * s;
		const NiPoint3 ob = oursLastB + (oursB - oursLastB) * s;
		const NiPoint3 ta = theirsLastA + (theirsA - theirsLastA) * s;
		const NiPoint3 tb = theirsLastB + (theirsB - theirsLastB) * s;
		if (SegmentSegmentDistance(oa, ob, ta, tb) <= reach) {
			share = s;
			// Where they met: on ours, nearest the middle of theirs.
			const NiPoint3 mid = (ta + tb) * 0.5f;
			point = oa + (ob - oa) * ShareOnSegment(mid, oa, ob);
			return true;
		}
	}
	return false;
}

// The player's shield as a ball: the bound of the node it hangs on ("Bip01
// L ForearmTwist", its Prn - camera/FrameLogic.h), a little smaller than the
// bound (a shield's disc, not its corners). Only a worn shield, and only a
// bound a shield's size (10 to 80 units round): a bare forearm's is a few.
inline bool ShieldBall(bool worn, float boundRadius, float& ballRadius) {
	if (!worn || !(boundRadius >= 10.0f && boundRadius <= 80.0f)) {
		return false;
	}
	ballRadius = boundRadius * 0.8f;
	return true;
}

// Their blade, from where it was to where it is, within `radius` of the
// ball's centre at any of the frame's steps.
inline bool BladeMeetsBall(const NiPoint3& lastA, const NiPoint3& lastB, const NiPoint3& a, const NiPoint3& b,
                           const NiPoint3& centre, float radius, float& share) {
	for (UInt32 k = 0; k <= kParrySteps; ++k) {
		const float s = static_cast<float>(k) / static_cast<float>(kParrySteps);
		if (PointSegmentDistance(centre, lastA + (a - lastA) * s, lastB + (b - lastB) * s) <= radius) {
			share = s;
			return true;
		}
	}
	return false;
}

// What of the player's met their blade: the drawn blade, the shield's ball,
// or the torch's shaft (game/TorchShaftLogic.h; the tester, 2026-10-25:
// "mach das man mit torch auch blocken kann").
enum class ParryBy : UInt8 { None, Blade, Shield, Torch };

// The player's guards this frame, each from where it was the frame before
// to where it is (the ball stands still for the frame).
struct ParryGuards {
	bool blade = false;
	NiPoint3 bladeLastA{0.0f, 0.0f, 0.0f};
	NiPoint3 bladeLastB{0.0f, 0.0f, 0.0f};
	NiPoint3 bladeA{0.0f, 0.0f, 0.0f};
	NiPoint3 bladeB{0.0f, 0.0f, 0.0f};
	bool shield = false;
	NiPoint3 shieldCentre{0.0f, 0.0f, 0.0f};
	float shieldRadius = 0.0f;
	bool torch = false;
	NiPoint3 torchLastA{0.0f, 0.0f, 0.0f};
	NiPoint3 torchLastB{0.0f, 0.0f, 0.0f};
	NiPoint3 torchA{0.0f, 0.0f, 0.0f};
	NiPoint3 torchB{0.0f, 0.0f, 0.0f};
};

struct ParryMeet {
	ParryBy by = ParryBy::None;
	float share = 0.0f;                  // of the frame, where they met
	NiPoint3 point{0.0f, 0.0f, 0.0f};    // on the guard that met it
};

// A foe's blade in their attack against the player's guards: the blade
// first, then the shield, then the torch - a shield and a torch are never
// worn together, and the blade is the one the swing is aimed with. The
// torch meets within the blades' reach: its shaft is 5 units thick, about a
// blade's width.
inline ParryMeet WhatMeetsTheirBlade(const ParryGuards& g, const BladeFoe& foe) {
	ParryMeet m;
	if (!IsAttackAction(foe.action) || !foe.bladeValid) {
		return m;
	}
	if (g.blade && BladesMeet(g.bladeLastA, g.bladeLastB, g.bladeA, g.bladeB, foe.lastA, foe.lastB, foe.a, foe.b,
	                          kParryReachUnits, m.share, m.point)) {
		m.by = ParryBy::Blade;
		return m;
	}
	if (g.shield && BladeMeetsBall(foe.lastA, foe.lastB, foe.a, foe.b, g.shieldCentre, g.shieldRadius, m.share)) {
		m.by = ParryBy::Shield;
		m.point = g.shieldCentre;
		return m;
	}
	if (g.torch && BladesMeet(g.torchLastA, g.torchLastB, g.torchA, g.torchB, foe.lastA, foe.lastB, foe.a, foe.b,
	                          kParryReachUnits, m.share, m.point)) {
		m.by = ParryBy::Torch;
		return m;
	}
	return ParryMeet{};
}

// A parry is felt in the hand that met it: the weapon hand for the blade,
// the other hand for the shield and the torch.
inline bool ParryFeltInWeaponHand(ParryBy by) { return by == ParryBy::Blade; }

// And heard by what met it (Oblivion.esm's SOUN, read 2026-10-09):
// WPNBlockBladeX for an edge, WPNBlockBluntX for a mace or an axe's haft and
// for the torch's wood, WPNBlockShieldLightX for a shield.
inline constexpr UInt32 kParrySoundBlade = 0x0000C3CB;
inline constexpr UInt32 kParrySoundBlunt = 0x0000C3CC;
inline constexpr UInt32 kParrySoundShield = 0x0000C3D0;

inline UInt32 ParrySoundForm(ParryBy by, bool bluntWeapon) {
	switch (by) {
	case ParryBy::Blade: return bluntWeapon ? kParrySoundBlunt : kParrySoundBlade;
	case ParryBy::Shield: return kParrySoundShield;
	case ParryBy::Torch: return kParrySoundBlunt;
	default: return 0;
	}
}

// The hit handler's hand-to-hand branch (read 2026-10-25, Oblivion.exe
// 1.2.0.416, objdump): at 0x005FF87E `cmp byte [esp+74h], 0` - the flag set
// at 0x005FF81D for a block made with neither a shield nor a weapon - and
// `je 005FF894` to the share; else, against an attacker with a weapon
// ([esp+2Ch], the attacker's equipped weapon's form, 0x005FEFA9) or an
// arrow (ebp), `fldz` at 0x005FF890: nothing blocked (UESP, Oblivion:Block:
// "hand-to-hand blocking against weapons has no effect"). A blow stopped by
// a parry or by the off hand's hold is stopped whatever the hands carry -
// the torch with the fists, someone held with no weapon equipped - so the
// compare goes through a stub: with a stopped blow it answers "equal",
// taking the handler to the share (OnBlockShare gives StoppedShare);
// otherwise it compares as before, the handler's slot one return address
// further down the stack.
inline constexpr UInt32 kHandToHandCompare = 0x005FF87E;
inline constexpr UInt8 kHandToHandCompareBytes[7] = {0x80, 0x7C, 0x24, 0x74, 0x00, 0x74, 0x0F};
inline constexpr UInt32 kHandToHandStubSize = 18;

// `cmp byte [flag], 0; jne stopped; cmp byte [esp+78h], 0; ret;
// stopped: cmp esp, esp; ret` - the flag a byte at `flagAddress`. Answers
// the bytes written, 0 without room.
inline UInt32 BuildHandToHandStub(UInt8* out, UInt32 capacity, UInt32 flagAddress) {
	if (out == nullptr || capacity < kHandToHandStubSize) {
		return 0;
	}
	const UInt8 code[kHandToHandStubSize] = {
		0x80, 0x3D, static_cast<UInt8>(flagAddress & 0xFF), static_cast<UInt8>((flagAddress >> 8) & 0xFF),
		static_cast<UInt8>((flagAddress >> 16) & 0xFF), static_cast<UInt8>((flagAddress >> 24) & 0xFF), 0x00,
		0x75, 0x06,
		0x80, 0x7C, 0x24, 0x78, 0x00,
		0xC3,
		0x39, 0xE4,
		0xC3,
	};
	for (UInt32 i = 0; i < kHandToHandStubSize; ++i) {
		out[i] = code[i];
	}
	return kHandToHandStubSize;
}

// Those whose blow is parried, each for a while after the blades met: the
// first blow of theirs that lands within it is blocked whole.
inline constexpr float kParryWindowSeconds = 0.8f;
inline constexpr UInt32 kParryLedgerMax = 8;

struct ParryLedger {
	UInt32 count = 0;
	UInt32 actor[kParryLedgerMax] = {};
	float left[kParryLedgerMax] = {};

	bool Has(UInt32 a) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (actor[i] == a) {
				return true;
			}
		}
		return false;
	}

	// Answers whether they were not parried already.
	bool Add(UInt32 a, float seconds) {
		for (UInt32 i = 0; i < count; ++i) {
			if (actor[i] == a) {
				left[i] = seconds;
				return false;
			}
		}
		if (count >= kParryLedgerMax) {
			return false;
		}
		actor[count] = a;
		left[count] = seconds;
		++count;
		return true;
	}

	// Their parried blow has come: no longer parried.
	bool Take(UInt32 a) {
		for (UInt32 i = 0; i < count; ++i) {
			if (actor[i] == a) {
				actor[i] = actor[count - 1];
				left[i] = left[count - 1];
				--count;
				return true;
			}
		}
		return false;
	}

	void Step(float dtSeconds) {
		UInt32 kept = 0;
		for (UInt32 i = 0; i < count; ++i) {
			const float l = left[i] - (dtSeconds > 0.0f ? dtSeconds : 0.0f);
			if (l > 0.0f) {
				actor[kept] = actor[i];
				left[kept] = l;
				++kept;
			}
		}
		count = kept;
	}
};

// The block check for a blow: the engine's answer, or yes when the target
// is the player, the engine says no, and someone's blow is parried - forced
// (the next check, the cone, then decides whose blow it is).
inline bool ForceBlockForParry(bool targetIsPlayer, bool engineSaysBlocking, bool anyParried) {
	return targetIsPlayer && !engineSaysBlocking && anyParried;
}

// The block check for a blow while the torch is raised as a guard with the
// weapons away (Nadi, 2026-11-03: "blocken" with a torch and no weapon
// drawn): the engine takes no block with nothing readied (torch-block.txt,
// 2026-11-01 08:41: "with nothing readied the block key does nothing"), so
// the check says yes for the player and the cone - the gaze's and the angle
// rule with the shaft as the blade (game/BlockCone) - decides each blow, as
// for the engine's own block. With a weapon or the fists readied the engine
// blocks itself and is left alone.
inline bool ForceBlockForTorch(bool targetIsPlayer, bool engineSaysBlocking, bool torchRaised, bool weaponReadied) {
	return targetIsPlayer && !engineSaysBlocking && torchRaised && !weaponReadied;
}

// The block check for a blow while the shield guards by contact
// (BlockAngleLogic.h, ShieldGuardPose; [Hands] ShieldBlockByContact): the
// game's Block control is not held for the shield then, so the engine says
// no; the check says yes for the player and the cone decides each blow by
// the disc alone (ShieldConeFor). With the engine blocking already (a
// weapon's guard, the keyboard) it is left alone, as for the torch.
inline bool ForceBlockForShield(bool targetIsPlayer, bool engineSaysBlocking, bool shieldGuards) {
	return targetIsPlayer && !engineSaysBlocking && shieldGuards;
}

// Which of the block check's forces holds at a blow. The torch's and the
// shield's leave each blow to the cone; the parry's lets every blow but the
// parried one through (ConeForParry), so it is taken only when neither of
// the others is - a parried attacker is still blocked under them, by the
// cone's parry check that comes first.
//
// The weapon by contact (BlockAngleLogic.h, WeaponGuardPose; [Hands]
// WeaponBlockByContact) is forced the same way as the shield, while its
// blade is read and not swinging (ForceBlockForWeapon); both may hold, and
// the cone asks both (ContactConeFor). The torch's needs nothing readied, so
// it never meets a drawn weapon's.
inline bool ForceBlockForWeapon(bool targetIsPlayer, bool engineSaysBlocking, bool weaponGuards) {
	return targetIsPlayer && !engineSaysBlocking && weaponGuards;
}

struct BlockForces {
	bool torch = false;
	bool shield = false;
	bool weapon = false;
	bool parry = false;
	bool torchContact = false;  // the torch by contact (its shaft judged at the cone)
	bool fistsContact = false;  // the fists by contact (their forearms judged at the cone)
	bool Any() const { return torch || shield || weapon || torchContact || fistsContact || parry; }
};

inline BlockForces BlockForcesFor(bool targetIsPlayer, bool engineSaysBlocking, bool torchRaised, bool weaponReadied,
                                  bool shieldGuards, bool anyStopped, bool weaponGuards = false,
                                  bool torchGuards = false, bool fistGuards = false) {
	BlockForces f;
	f.torch = ForceBlockForTorch(targetIsPlayer, engineSaysBlocking, torchRaised, weaponReadied);
	f.shield = !f.torch && ForceBlockForShield(targetIsPlayer, engineSaysBlocking, shieldGuards);
	f.weapon = !f.torch && ForceBlockForWeapon(targetIsPlayer, engineSaysBlocking, weaponGuards);
	f.torchContact = !f.torch && ForceBlockForWeapon(targetIsPlayer, engineSaysBlocking, torchGuards);
	f.fistsContact = !f.torch && ForceBlockForWeapon(targetIsPlayer, engineSaysBlocking, fistGuards);
	f.parry = !f.torch && !f.shield && !f.weapon && !f.torchContact && !f.fistsContact &&
	          ForceBlockForParry(targetIsPlayer, engineSaysBlocking, anyStopped);
	return f;
}

// The cone's answer for a blow under the shield's forced block, from the
// angle rule's verdict against the disc (BlockAngleFor with the shield):
// in its way, blocked; not, it lands - whichever way the player looks, the
// disc's place is the whole test, as in Blade & Sorcery. A blow the disc
// cannot be judged against (an arrow, an attacker not placed, the disc
// lost this frame) goes to the game's own cone while the shield hand is
// raised as the old pose rule had it, and lands otherwise - unless it is a
// projectile whose impact was judged against the disc already
// (game/ArrowShieldLogic.h): that verdict, kept for that projectile, decides
// its damage too.
enum class ShieldCone : UInt8 {
	Original,    // not the shield's forced block, or the pose's fallback: the cone decides
	Blocked,     // the shield is in the blow's way
	NotBlocked,  // it is not: the blow lands
};

// A projectile's verdict kept from its impact (ArrowVerdicts::Take).
enum class KeptArrow : UInt8 { None, Blocked, Through };

inline ShieldCone ShieldConeFor(bool targetIsPlayer, bool shieldForced, BlockAngle verdict, bool raised,
                                KeptArrow arrow = KeptArrow::None) {
	if (!targetIsPlayer || !shieldForced) {
		return ShieldCone::Original;
	}
	if (arrow != KeptArrow::None) {
		return arrow == KeptArrow::Blocked ? ShieldCone::Blocked : ShieldCone::NotBlocked;
	}
	if (verdict == BlockAngle::Blocked) {
		return ShieldCone::Blocked;
	}
	if (verdict == BlockAngle::Through) {
		return ShieldCone::NotBlocked;
	}
	return raised ? ShieldCone::Original : ShieldCone::NotBlocked;
}

// The cone's answer under a forced block by contact, with the shield's and
// the weapon's verdicts for this blow (BlockAngleFor with each guard; the
// weapon asked only when not swinging): either in the way - blocked (an
// arrow kept on the shield counts as the shield's); none in the way and none
// that cannot judge with its pose up - it lands; a guard that cannot judge
// (no blade or disc this frame, an attacker not placed, an arrow) with its
// old pose up - the game's own cone, as the pose's block had it.
struct ContactVerdicts {
	bool shieldAsked = false;
	BlockAngle shield = BlockAngle::Engine;
	bool shieldRaised = false;
	bool weaponAsked = false;
	BlockAngle weapon = BlockAngle::Engine;
	bool weaponRaised = false;
	bool torchAsked = false;  // the torch by contact, its hand not swinging
	BlockAngle torch = BlockAngle::Engine;
	bool torchRaised = false;
	// The fists by contact: their forearms' verdict (FistsVerdict), their pose up,
	// and the attacker armed or an arrow - a hand-to-hand block then stops
	// nothing (0x005FF890), so the fists have no say at all.
	bool fistsAsked = false;
	BlockAngle fists = BlockAngle::Engine;
	bool fistsRaised = false;
	bool fistsArmed = false;
	KeptArrow arrow = KeptArrow::None;
};

// The fists' verdict from each forearm's (a punching arm not asked): either in
// the way - blocked; else either judged not in the way - through; else
// nothing judged.
inline BlockAngle FistsVerdict(bool rightAsked, BlockAngle right, bool leftAsked, BlockAngle left) {
	if ((rightAsked && right == BlockAngle::Blocked) || (leftAsked && left == BlockAngle::Blocked)) {
		return BlockAngle::Blocked;
	}
	if ((rightAsked && right == BlockAngle::Through) || (leftAsked && left == BlockAngle::Through)) {
		return BlockAngle::Through;
	}
	return BlockAngle::Engine;
}

// The torch could not be judged and is raised: the game's cone stands in, and a
// block it gives is marked for the hand-to-hand stub as the torch's (with
// nothing readied the engine would block none of an armed attacker's blow).
inline bool TorchFallsBack(const ContactVerdicts& c) {
	return c.torchAsked && c.torch == BlockAngle::Engine && c.torchRaised;
}

inline ShieldCone ContactConeFor(bool targetIsPlayer, bool forced, const ContactVerdicts& c) {
	if (!targetIsPlayer || !forced) {
		return ShieldCone::Original;
	}
	const bool shieldBlocks = c.arrow == KeptArrow::Blocked || (c.shieldAsked && c.arrow == KeptArrow::None &&
	                                                             c.shield == BlockAngle::Blocked);
	if (shieldBlocks || (c.weaponAsked && c.weapon == BlockAngle::Blocked) ||
	    (c.torchAsked && c.torch == BlockAngle::Blocked) ||
	    (c.fistsAsked && !c.fistsArmed && c.fists == BlockAngle::Blocked)) {
		return ShieldCone::Blocked;
	}
	const bool shieldFalls = c.shieldAsked && c.arrow == KeptArrow::None && c.shield == BlockAngle::Engine && c.shieldRaised;
	const bool weaponFalls = c.weaponAsked && c.weapon == BlockAngle::Engine && c.weaponRaised;
	const bool torchFalls = TorchFallsBack(c);
	const bool fistsFall = c.fistsAsked && !c.fistsArmed && c.fists == BlockAngle::Engine && c.fistsRaised;
	return shieldFalls || weaponFalls || torchFalls || fistsFall ? ShieldCone::Original : ShieldCone::NotBlocked;
}

enum class ParryCone : UInt8 {
	Original,   // the engine's own test (the gaze's, game/BlockCone.h)
	Blocked,    // this attacker's blow was parried: in front, blocked
	NotBlocked, // the block was forced for another's parry: this one hits
};

inline ParryCone ConeForParry(bool targetIsPlayer, bool attackerParried, bool blockForced) {
	if (!targetIsPlayer) {
		return ParryCone::Original;
	}
	if (attackerParried) {
		return ParryCone::Blocked;
	}
	return blockForced ? ParryCone::NotBlocked : ParryCone::Original;
}

// The share blocked: all of a parried blow with ParryStopsAll, else the
// engine's.
inline float ParriedShare(bool parriedBlow, bool stopsAll, float engineShare) {
	return parriedBlow && stopsAll ? 1.0f : engineShare;
}

// What a parry costs: [Hands] ParryFatigue, never under 0 nor a number that
// is not one.
inline float ParryFatigueCost(float setting) {
	return setting > 0.0f && setting < 1000.0f ? setting : 0.0f;
}

// Someone the player's off hand holds by the head, the neck or the weapon
// arm (GrappleLogic.h, GrappleStopsBlows): their blows do not land - the
// same forced block and cone as a parry, with or without a weapon drawn or
// the parries on, and all of the blow stopped whatever ParryStopsAll says.
// Held, nothing is owed in fatigue for it either: the grab paid.
struct BlowStop {
	bool parried = false;  // within a parry's window (the parry on)
	bool held = false;     // held by a part that stops their blows
};

inline BlowStop BlowStopFor(bool parriesOn, bool attackerParried, UInt32 heldFoe, UInt32 attacker) {
	BlowStop s;
	s.held = heldFoe != 0 && attacker == heldFoe;
	s.parried = !s.held && parriesOn && attackerParried;
	return s;
}

// Whether anything stops a blow at the player this time: a parry in its
// window, or someone held.
inline bool AnyBlowStopped(bool parriesOn, UInt32 parriedCount, UInt32 heldFoe) {
	return (parriesOn && parriedCount > 0) || heldFoe != 0;
}

// The share blocked of a blow that was stopped: a held one's all of it, a
// parried one's by ParriedShare.
inline float StoppedShare(const BlowStop& s, bool stopsAll, float engineShare) {
	return s.held ? 1.0f : ParriedShare(s.parried, stopsAll, engineShare);
}

}  // namespace obvr::game

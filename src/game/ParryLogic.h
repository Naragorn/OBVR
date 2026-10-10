#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
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
struct BladeFoe {
	UInt32 actor = 0;
	SInt32 action = addr::kActionNone;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	bool haveLast = false;
	NiPoint3 lastA{0.0f, 0.0f, 0.0f};
	NiPoint3 lastB{0.0f, 0.0f, 0.0f};
};

inline constexpr UInt32 kBladeFoesMax = 8;

struct BladeFoes {
	UInt32 count = 0;
	BladeFoe foe[kBladeFoesMax];
};

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

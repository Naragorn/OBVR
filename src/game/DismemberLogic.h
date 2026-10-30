#pragma once

// The head comes off (docs/dismemberment-spec.md; the tester, 2026-10-25:
// "decapitations ... B erstmal nur spec"; Nadi, 2026-10-30: "go"). Health
// decides whether someone dies, the blade only how: a blade or an axe swept
// fast enough through the neck of someone dead - a corpse, or the one the
// blow just killed - cuts the head off. The living keep their parts
// (CutKills=1 drops that, never for the essential).
//
// The cut itself (game/Dismember.cpp) releases the head's one ragdoll joint
// to Spine2 and redirects bone pointers in the skins, as the arm stump does:
// in the body's shapes the head bone collapses at the neck, in the head's
// shapes everything but the head collapses under it. Pure, covered by
// dismember_test.

#include <cmath>

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// TESObjectWEAP's type (+0x90): blades 0 and 1, blunt 2 and 3 (axes among
// them), staff 4, bow 5; -1 none.
inline constexpr SInt32 kCutWeaponBladeOneHand = 0;
inline constexpr SInt32 kCutWeaponBladeTwoHand = 1;
inline constexpr SInt32 kCutWeaponBluntOneHand = 2;
inline constexpr SInt32 kCutWeaponBluntTwoHand = 3;
inline constexpr SInt32 kCutWeaponStaff = 4;
inline constexpr SInt32 kCutWeaponBow = 5;

inline bool CutPathHasAxe(const char* path) {
	if (path == nullptr) {
		return false;
	}
	for (const char* p = path; p[0] != '\0'; ++p) {
		const char a = static_cast<char>(p[0] | 0x20);
		if (a != 'a' || p[1] == '\0') {
			continue;
		}
		const char x = static_cast<char>(p[1] | 0x20);
		if (x == 'x' && p[2] != '\0' && static_cast<char>(p[2] | 0x20) == 'e') {
			return true;
		}
	}
	return false;
}

// An edge: a blade, or a blunt weapon whose model is an axe - the engine has
// no axe type (read 2026-10-30: "axe" in the model path catches every vanilla
// axe; seven enchanted ones are typed Blade, caught either way).
inline bool WeaponHasEdge(SInt32 type, const char* modelPath) {
	if (type == kCutWeaponBladeOneHand || type == kCutWeaponBladeTwoHand) {
		return true;
	}
	return (type == kCutWeaponBluntOneHand || type == kCutWeaponBluntTwoHand) && CutPathHasAxe(modelPath);
}

// Whether the blade, from where it was the frame before to where it is now,
// passed through the neck (Spine2's origin to the head's, `radius` round):
// the blade now, and the paths of nine points along it - the swept face of
// the blade, as far as a neck's width can slip between them.
inline constexpr int kNeckSweepPoints = 9;

inline bool NeckTouched(const NiPoint3& neckLow, const NiPoint3& neckHigh, float radius, const NiPoint3& gripBefore,
                        const NiPoint3& tipBefore, const NiPoint3& gripNow, const NiPoint3& tipNow) {
	if (SegmentSegmentDistance(gripNow, tipNow, neckLow, neckHigh) <= radius) {
		return true;
	}
	for (int i = 0; i < kNeckSweepPoints; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(kNeckSweepPoints - 1);
		const NiPoint3 before = gripBefore + (tipBefore - gripBefore) * t;
		const NiPoint3 now = gripNow + (tipNow - gripNow) * t;
		if (SegmentSegmentDistance(before, now, neckLow, neckHigh) <= radius) {
			return true;
		}
	}
	return false;
}

struct CutSettings {
	bool enabled = false;         // [Hands] Dismemberment (off until the headset passes it)
	float minSpeedMetres = 2.5f;  // [Hands] DismemberMinSpeed: the blade's tip, m/s
	float neckRadiusUnits = 6.0f;
	bool cutKills = false;        // [Hands] DismemberCutKills: the living too
};

struct CutInput {
	bool enabled = false;
	bool alreadyCut = false;
	bool dead = false;       // a corpse, or killed by this blow
	bool essential = false;
	bool edge = false;
	bool onNeck = false;
	float speedMetres = 0.0f;
};

enum class CutVerdict : UInt8 { Cut, Off, AlreadyCut, Living, Essential, NoEdge, NotOnNeck, TooSlow };

inline const char* CutVerdictName(CutVerdict v) {
	switch (v) {
		case CutVerdict::Cut: return "cut";
		case CutVerdict::Off: return "off";
		case CutVerdict::AlreadyCut: return "the head is off already";
		case CutVerdict::Living: return "alive - the living keep their parts";
		case CutVerdict::Essential: return "essential";
		case CutVerdict::NoEdge: return "no edge";
		case CutVerdict::NotOnNeck: return "not the neck";
		case CutVerdict::TooSlow: return "too slow";
	}
	return "?";
}

inline CutVerdict DecideCut(const CutSettings& s, const CutInput& in) {
	if (!in.enabled) {
		return CutVerdict::Off;
	}
	if (in.alreadyCut) {
		return CutVerdict::AlreadyCut;
	}
	if (!in.dead) {
		if (!s.cutKills) {
			return CutVerdict::Living;
		}
		if (in.essential) {
			return CutVerdict::Essential;
		}
	}
	if (!in.edge) {
		return CutVerdict::NoEdge;
	}
	if (!in.onNeck) {
		return CutVerdict::NotOnNeck;
	}
	if (in.speedMetres < s.minSpeedMetres) {
		return CutVerdict::TooSlow;
	}
	return CutVerdict::Cut;
}

// The killing blow. The engine's hit lands at the touch and its death comes
// a few frames later, so a living neck the blade went through - with
// everything else a cut wants - waits for the death: dead within
// kKillingBlowSeconds, the head comes off; alive past it, or dead only
// after it (another's blow), the touch is forgotten.
inline constexpr float kKillingBlowSeconds = 0.4f;

inline bool ArmsKillingBlow(const CutSettings& s, const CutInput& in) {
	if (in.dead || s.cutKills || in.essential) {
		return false;
	}
	CutInput asDead = in;
	asDead.dead = true;
	return DecideCut(s, asDead) == CutVerdict::Cut;
}

struct PendingCut {
	bool armed = false;
	UInt32 actor = 0;
	float age = 0.0f;
	NiPoint3 push{0.0f, 0.0f, 0.0f};
};

enum class PendingStep : UInt8 { None, Wait, Cut, Expired };

inline void ArmPendingCut(PendingCut& p, UInt32 actor, const NiPoint3& push) {
	p.armed = true;
	p.actor = actor;
	p.age = 0.0f;
	p.push = push;
}

inline PendingStep StepPendingCut(PendingCut& p, float dt, bool nowDead) {
	if (!p.armed) {
		return PendingStep::None;
	}
	p.age += dt > 0.0f ? dt : 0.0f;
	if (p.age > kKillingBlowSeconds) {
		p.armed = false;
		return PendingStep::Expired;
	}
	if (nowDead) {
		p.armed = false;
		return PendingStep::Cut;
	}
	return PendingStep::Wait;
}

// The killing strike: the engine's hit kills in the call (the strike's
// health line, 2026-10-30: 74 -> -401), and the body flies off before the
// blade's sweep reaches the neck - so the cut is decided at the touch, by the
// capsule the blade met: the neck takes the head, nothing else does.
inline bool StrikeKilled(float healthBefore, float healthAfter) {
	return healthBefore == healthBefore && healthAfter == healthAfter && healthBefore > 0.0f && healthAfter <= 0.0f;
}

inline bool PartTakesHead(BodyPart part) { return part == BodyPart::Neck; }

// The capsules do not tell the neck well (a slash at its height meets the
// shoulder first, a thrust at it the torso's top: the harness, 2026-10-30),
// so the killing strike is also cut when the blade's sweep - the frame before
// to now, and on the way it was going for kKillingSweepSeconds more, the time
// the flung body took from it - passes through the neck as it stood the frame
// before the blow.
inline constexpr float kKillingSweepSeconds = 0.08f;

inline bool KillingSweepMeetsNeck(const NiPoint3& neckLow, const NiPoint3& neckHigh, float radius,
                                  const NiPoint3& gripBefore, const NiPoint3& tipBefore, const NiPoint3& gripNow,
                                  const NiPoint3& tipNow, float frameSeconds, float aheadSeconds) {
	if (!(frameSeconds > 0.0f)) {
		return false;
	}
	const float scale = aheadSeconds > 0.0f ? aheadSeconds / frameSeconds : 0.0f;
	const NiPoint3 gripOn = gripNow + (gripNow - gripBefore) * scale;
	const NiPoint3 tipOn = tipNow + (tipNow - tipBefore) * scale;
	return NeckTouched(neckLow, neckHigh, radius, gripBefore, tipBefore, gripOn, tipOn);
}

// The head is sent off the blade's way at half its speed, at most
// kCutPushMostUnits (5 m/s; a harness thrust's 1300 units/s threw it 1040
// units, 2026-10-30).
inline constexpr float kCutPushMostUnits = 350.0f;

inline NiPoint3 CutPush(const NiPoint3& push) {
	const float sq = push.LengthSquared();
	if (!(sq > kCutPushMostUnits * kCutPushMostUnits)) {
		return sq == sq ? push : NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return push * (kCutPushMostUnits / std::sqrt(sq));
}

// The body's shapes give up the head bone and what hangs under it; the
// head's shapes everything else.
inline bool SwapBoneOnBodySide(bool isHead, bool underHead) { return isHead || underHead; }
inline bool SwapBoneOnHeadSide(bool isHead, bool underHead) { return !isHead && !underHead; }

// A shape outside the head node is the head's when a third or more of its
// bones are the head's - the head, what hangs under it, Neck and Neck1 -
// whatever the pose (the bound's middle against the head and Spine2 took a
// standing beggar's shirt for the head's, 2026-10-30). Measured that day on
// the beggar: FaceGenFace 2 of 5, its shirt's two shapes 2 of 10 and 2 of 19,
// its feet 2 of 20.
inline bool ShapeIsHeadSide(UInt32 headBones, UInt32 bones) { return bones > 0 && headBones * 3 >= bones; }

}  // namespace obvr::game

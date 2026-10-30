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

// The body's shapes give up the head bone and what hangs under it; the
// head's shapes everything else.
inline bool SwapBoneOnBodySide(bool isHead, bool underHead) { return isHead || underHead; }
inline bool SwapBoneOnHeadSide(bool isHead, bool underHead) { return !isHead && !underHead; }

}  // namespace obvr::game

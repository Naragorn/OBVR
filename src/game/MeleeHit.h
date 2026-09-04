#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The arithmetic of a blade meeting a body.
//
// In the hand-tracked mode the swing of the controller is the attack: no
// animation decides when the weapon lands or on whom. The blade is a segment
// in the world - from the hand along the controller's pointing axis for the
// weapon's reach - and each actor is the sphere the engine keeps around its
// model. The blade strikes when it passes close enough to the sphere's
// centre; how close is a fraction of the sphere's radius plus a pad, because
// the sphere is drawn round the whole model, arms and all, and a sword
// touching its rim is still in the air.
//
// Pure, so every decision is checked with made-up numbers.

struct Blade {
	NiPoint3 base{0.0f, 0.0f, 0.0f};
	NiPoint3 tip{0.0f, 0.0f, 0.0f};
};

// The blade in the world: the hand's offset from the eyes carried through
// the camera's world transform is the hilt; the controller's forward (y in
// the game's axes, the way HandMode answers the hand's rotation), turned by
// the head and then by the camera, is the direction the blade points; the
// reach is how far it goes.
inline Blade BladeInWorld(const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                          const NiMatrix33& handRot, const NiPoint3& handOffsetUnits,
                          float reachUnits) {
	Blade blade;
	blade.base = cameraPos + cameraRot * handOffsetUnits;
	const NiPoint3 forward = cameraRot * (handRot * NiPoint3{0.0f, 1.0f, 0.0f});
	blade.tip = blade.base + forward * reachUnits;
	return blade;
}

// The distance from a point to the nearest point of the segment a..b.
inline float SegmentPointDistance(const NiPoint3& a, const NiPoint3& b, const NiPoint3& p) {
	const NiPoint3 ab = b - a;
	const float lengthSquared = ab.LengthSquared();
	float t = 0.0f;
	if (lengthSquared > 0.0f) {
		const NiPoint3 ap = p - a;
		t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / lengthSquared;
		if (t < 0.0f) {
			t = 0.0f;
		} else if (t > 1.0f) {
			t = 1.0f;
		}
	}
	const NiPoint3 nearest = a + ab * t;
	return math::Sqrt((p - nearest).LengthSquared());
}

// Whether the blade strikes a body whose bound sphere is given: within
// factor * radius + pad of the centre. A bound with no radius - a model the
// engine has not measured - takes the pad alone, so a blade has to pass
// through the centre to count rather than never counting at all.
inline bool BladeStrikes(const Blade& blade, const NiPoint3& centre, float radius, float factor,
                         float padUnits) {
	if (radius < 0.0f) {
		radius = 0.0f;
	}
	if (factor < 0.0f) {
		factor = 0.0f;
	}
	if (padUnits < 0.0f) {
		padUnits = 0.0f;
	}
	const float within = radius * factor + padUnits;
	return SegmentPointDistance(blade.base, blade.tip, centre) <= within;
}

// One strike per body per swing: the ledger remembers whom this swing has
// already struck and starts afresh with the next swing's serial. Full is
// treated as struck - a swing does not hit a ninth body.
struct SwingLedger {
	static constexpr UInt32 kCapacity = 8;
	UInt32 serial = 0;
	UInt32 count = 0;
	const void* struck[kCapacity] = {};
};

inline bool LedgerAdmits(SwingLedger& ledger, UInt32 serial, const void* target) {
	if (ledger.serial != serial) {
		ledger.serial = serial;
		ledger.count = 0;
	}
	for (UInt32 at = 0; at < ledger.count; ++at) {
		if (ledger.struck[at] == target) {
			return false;
		}
	}
	if (ledger.count >= SwingLedger::kCapacity) {
		return false;
	}
	ledger.struck[ledger.count++] = target;
	return true;
}

// Which weapons are swung. The engine's weapon types, from xOBSE's
// GameForms.h: blade and blunt, one and two handed, are melee; a staff is
// cast with and a bow is drawn. No weapon at all is a fist, which is melee
// too.
enum class WeaponTypeCode : SInt32 {
	None = -1,
	BladeOneHand = 0,
	BladeTwoHand = 1,
	BluntOneHand = 2,
	BluntTwoHand = 3,
	Staff = 4,
	Bow = 5,
};

inline bool WeaponIsSwung(SInt32 type) {
	return type == static_cast<SInt32>(WeaponTypeCode::None) ||
	       (type >= static_cast<SInt32>(WeaponTypeCode::BladeOneHand) &&
	        type <= static_cast<SInt32>(WeaponTypeCode::BluntTwoHand));
}

}  // namespace obvr::game

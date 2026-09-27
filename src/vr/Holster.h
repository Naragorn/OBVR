#pragma once

// Drawing a weapon by reaching for it (docs/controls-spec.md 4.1, 4.2): the
// right hand at the left hip and a grip draws the sword, the left hand at the
// left shoulder and a grip draws the bow, and the same reach sheathes it
// again. Full VR only.
//
// The zones are in the body's frame: metres from the eyes along the head's
// heading alone, so a head tipped down to look at the hip does not move the
// hip. The game's axes, as the gestures use them: x right, y forward, z up.
//
// A grip that closes in a zone belongs to the holster until it opens: it does
// not grab, and it does not cancel a teleport. A grip already closed when the
// hand arrives stays a grab, so a held object is never dropped by passing the
// hip.
//
// The other kind drawn, the reach does nothing: it goes back first, by its
// own reach (the tester, 2026-09-27: first sheathe the bow, then draw the
// sword, and the other way round). Raised fists are not in the way.
//
// Oblivion has one weapon slot. When the weapon of the other kind is
// equipped, the remembered weapon of this kind is equipped first (the game
// glue does that), and it is drawn once the game shows it in the hand.
//
// Pure, covered by holster_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

enum class HolsterKind : UInt8 { None, Sword, Bow };

// What the weapon slot holds: nothing (fists), a weapon swung or a staff
// (drawn from the hip), a bow (from the shoulder).
enum class EquippedKind : UInt8 { Nothing, Melee, Bow };

struct HolsterSettings {
	bool enabled = true;
	// The left hip, low at the side: for the right hand. A standing adult's
	// eyes are about 1.6 m up and the hip about 0.95 m; seated, the hip comes
	// nearer the eyes, which the radius takes up.
	NiPoint3 swordZone{-0.18f, 0.02f, -0.62f};
	float swordRadius = 0.22f;
	// Behind the left shoulder: for the left hand.
	NiPoint3 bowZone{-0.18f, -0.12f, -0.15f};
	float bowRadius = 0.18f;
	// How long a weapon just equipped may take to show in the hand before
	// the draw is given up.
	float drawWaitSeconds = 2.0f;
};

// The hand relative to the eyes along the head's heading only, in the game's
// axes. OpenVR's tracking axes in (x right, y up, z back), the heading from
// where the head's forward points on the level.
inline NiPoint3 BodyRelative(const Quaternion& head, const NiPoint3& headPosition,
                             const NiPoint3& hand) {
	const NiPoint3 forward = ToMatrix(head) * NiPoint3{0.0f, 0.0f, -1.0f};
	float fx = forward.x;
	float fz = forward.z;
	const float length = math::Sqrt(fx * fx + fz * fz);
	if (length < 1e-4f) {
		fx = 0.0f;
		fz = -1.0f;
	} else {
		fx /= length;
		fz /= length;
	}
	const NiPoint3 d = hand - headPosition;
	// Forward on the level is (fx, fz); right is forward turned a quarter
	// clockwise seen from above: (-fz, fx).
	const float right = d.x * -fz + d.z * fx;
	const float ahead = d.x * fx + d.z * fz;
	return NiPoint3{right, ahead, d.y};
}

inline bool InZone(const NiPoint3& relative, const NiPoint3& centre, float radius) {
	const NiPoint3 d = relative - centre;
	return d.LengthSquared() <= radius * radius;
}

struct HolsterInput {
	bool allowed = false;  // Full VR, in the world, no menu
	bool rightValid = false;
	bool leftValid = false;
	NiPoint3 rightRelative{0.0f, 0.0f, 0.0f};  // BodyRelative
	NiPoint3 leftRelative{0.0f, 0.0f, 0.0f};
	bool rightGrip = false;  // as it is now
	bool leftGrip = false;
	WeaponSeen seen = WeaponSeen::Unknown;
	EquippedKind equipped = EquippedKind::Nothing;
	bool haveSword = false;  // a remembered sword is still in the pack
	bool haveBow = false;
	bool leftHanded = false;  // the zones mirror: the sword at the right hip
	float dt = 0.0f;
};

struct HolsterState {
	bool rightGripWas = false;
	bool leftGripWas = false;
	bool rightClaimed = false;
	bool leftClaimed = false;
	HolsterKind drawAfterEquip = HolsterKind::None;
	float drawWait = 0.0f;
};

struct HolsterVerdict {
	bool rightInZone = false;
	bool leftInZone = false;
	bool rightClaimed = false;  // these grips are the holster's: no grab
	bool leftClaimed = false;
	bool readyClick = false;    // the ready-weapon click: draw or sheathe
	HolsterKind equip = HolsterKind::None;    // equip the remembered one now
	HolsterKind gesture = HolsterKind::None;  // the zone that fired, for the log
	bool refused = false;       // fired with nothing of that kind to draw
	bool otherDrawn = false;    // fired while the other kind is drawn: nothing done
	bool gaveUp = false;        // the equipped weapon never showed
};

inline EquippedKind KindFor(HolsterKind kind) {
	return kind == HolsterKind::Bow ? EquippedKind::Bow : EquippedKind::Melee;
}

inline HolsterVerdict StepHolster(HolsterState& s, const HolsterInput& in,
                                  const HolsterSettings& settings) {
	HolsterVerdict v;
	const float mirror = in.leftHanded ? -1.0f : 1.0f;
	const NiPoint3 sword{settings.swordZone.x * mirror, settings.swordZone.y, settings.swordZone.z};
	const NiPoint3 bow{settings.bowZone.x * mirror, settings.bowZone.y, settings.bowZone.z};
	v.rightInZone = in.rightValid && InZone(in.rightRelative, sword, settings.swordRadius);
	v.leftInZone = in.leftValid && InZone(in.leftRelative, bow, settings.bowRadius);

	const bool rightPress = in.rightGrip && !s.rightGripWas;
	const bool leftPress = in.leftGrip && !s.leftGripWas;
	s.rightGripWas = in.rightGrip;
	s.leftGripWas = in.leftGrip;
	if (!in.rightGrip) {
		s.rightClaimed = false;
	}
	if (!in.leftGrip) {
		s.leftClaimed = false;
	}

	if (!settings.enabled || !in.allowed) {
		s.drawAfterEquip = HolsterKind::None;
		v.rightClaimed = s.rightClaimed;
		v.leftClaimed = s.leftClaimed;
		return v;
	}

	// A weapon equipped by the last reach: drawn once the game shows it.
	if (s.drawAfterEquip != HolsterKind::None) {
		s.drawWait -= in.dt;
		if (in.equipped == KindFor(s.drawAfterEquip) && in.seen != WeaponSeen::Unknown) {
			v.readyClick = in.seen == WeaponSeen::Sheathed;
			s.drawAfterEquip = HolsterKind::None;
		} else if (s.drawWait <= 0.0f) {
			v.gaveUp = true;
			s.drawAfterEquip = HolsterKind::None;
		}
	}

	HolsterKind fired = HolsterKind::None;
	if (rightPress && v.rightInZone) {
		s.rightClaimed = true;
		fired = HolsterKind::Sword;
	} else if (leftPress && v.leftInZone) {
		s.leftClaimed = true;
		fired = HolsterKind::Bow;
	}
	if (fired != HolsterKind::None) {
		v.gesture = fired;
		const bool have = fired == HolsterKind::Sword ? in.haveSword : in.haveBow;
		if (in.equipped == KindFor(fired)) {
			// Drawn or sheathed, the ready key turns it the other way.
			v.readyClick = true;
			s.drawAfterEquip = HolsterKind::None;
		} else if (in.equipped != EquippedKind::Nothing && in.seen == WeaponSeen::Drawn) {
			// The other weapon is out: it has to go back first, by its own
			// reach (the tester, 2026-09-27). Fists up are not in the way.
			v.otherDrawn = true;
		} else if (have) {
			v.equip = fired;
			s.drawAfterEquip = fired;
			s.drawWait = settings.drawWaitSeconds;
		} else {
			v.refused = true;
		}
	}
	v.rightClaimed = s.rightClaimed;
	v.leftClaimed = s.leftClaimed;
	return v;
}

}  // namespace obvr::vr

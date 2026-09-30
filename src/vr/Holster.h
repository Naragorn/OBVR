#pragma once

// Drawing a weapon by reaching for it (docs/controls-spec.md 4.1, 4.2), for
// every kind of weapon (the tester, 2026-09-27):
//
//   one-handed blade or blunt   the weapon hand's grip at the left hip
//   two-handed blade or blunt,  the weapon hand's grip at the right shoulder,
//   and the staff               over it, as from the back
//   bow                         the other hand's grip at the left shoulder
//
// The same reach sheathes it again. Full VR only.
//
// The zones are in the body's frame: metres from the eyes along the head's
// heading alone, so a head tipped down to look at the hip does not move the
// hip. The game's axes, as the gestures use them: x right, y forward, z up.
// Left-handed they mirror: the one-handed weapon at the right hip, and so on.
//
// A grip that closes in a zone belongs to the holster until it opens: it does
// not grab, and it does not cancel a teleport. A grip already closed when the
// hand arrives stays a grab, so a held object is never dropped by passing the
// hip.
//
// Another kind drawn, the reach does nothing: it goes back first, by its own
// reach (the tester, 2026-09-27: first sheathe the bow, then draw the sword,
// and the other way round). Raised fists are not in the way.
//
// Oblivion has one weapon slot. When another kind is equipped, the
// remembered weapon of this kind is equipped first (the game glue does that),
// and it is drawn once the game shows it in the hand.
//
// Pure, covered by holster_test.

#include "core/ChoiceWord.h"
#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

enum class HolsterKind : UInt8 { None, OneHand, TwoHand, Bow };

// What the weapon slot holds: nothing (fists), a one-handed blade or blunt
// weapon, a two-handed one or a staff, a bow. One kind per holster.
enum class EquippedKind : UInt8 { Nothing, OneHand, TwoHand, Bow };

// The weapon types as the game numbers them (TESObjectWEAP +0x90, xOBSE
// GameForms.h:2669-2682: BladeOneHand 0, BladeTwoHand 1, BluntOneHand 2,
// BluntTwoHand 3, Staff 4, Bow 5) sorted into the holsters. Anything else
// (no weapon) is Nothing.
inline EquippedKind KindOfWeaponType(SInt32 type) {
	switch (type) {
	case 0:
	case 2:
		return EquippedKind::OneHand;
	case 1:
	case 3:
	case 4:
		return EquippedKind::TwoHand;
	case 5:
		return EquippedKind::Bow;
	default:
		return EquippedKind::Nothing;
	}
}

// A staff: a two-hander whose shaft is a handle on both sides of the right
// hand (vr::HandleSpan::above).
inline bool IsStaffWeaponType(SInt32 type) { return type == 4; }

// Which side of the body a weapon hangs on (the tester, 2026-09-30: "links
// händer wollen whl alles links, kann aber auch n paar geben die wollen dann
// mixen und matchen. soll möglich sein"). Auto: where a right-hander has it,
// mirrored for a left-hander (LeftHanded) - what every place did before.
// Right or Left: that side, whatever the handedness; the place's distance to
// the side ([Hands] Holster*X) is kept, only its side is set.
enum class HolsterSide : UInt8 { Auto = 0, Right = 1, Left = 2 };

inline constexpr UInt32 kHolsterSideCount = 3;

// The INI's words, in the order of the values, and the settings row's.
inline constexpr const char* kHolsterSideNames[kHolsterSideCount] = {"auto", "right", "left"};

// A word from the INI, any case. False, and `out` left alone, for anything
// else.
inline bool ParseHolsterSide(const char* text, HolsterSide& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kHolsterSideNames, kHolsterSideCount, index)) {
		return false;
	}
	out = static_cast<HolsterSide>(index);
	return true;
}

// The settings row's value (0..2) as a side; out of range is Auto.
inline HolsterSide HolsterSideFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kHolsterSideCount) - 0.5f)) {
		return HolsterSide::Auto;
	}
	return static_cast<HolsterSide>(static_cast<int>(value + 0.5f));
}

struct HolsterSettings {
	bool enabled = true;
	// The left hip, low at the side: for the weapon hand. A standing adult's
	// eyes are about 1.6 m up and the hip about 0.95 m; seated, the hip comes
	// nearer the eyes, which the radius takes up.
	NiPoint3 oneHandZone{-0.18f, 0.02f, -0.62f};
	float oneHandRadius = 0.22f;
	// Over the right shoulder, behind it: for the weapon hand.
	NiPoint3 twoHandZone{0.18f, -0.12f, -0.15f};
	float twoHandRadius = 0.18f;
	// Behind the left shoulder: for the other hand.
	NiPoint3 bowZone{-0.18f, -0.12f, -0.15f};
	float bowRadius = 0.18f;
	// Which side each hangs on (HolsterSide).
	HolsterSide oneHandSide = HolsterSide::Auto;
	HolsterSide twoHandSide = HolsterSide::Auto;
	HolsterSide bowSide = HolsterSide::Auto;
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

inline NiPoint3 Mirrored(const NiPoint3& zone, bool leftHanded) {
	return NiPoint3{leftHanded ? -zone.x : zone.x, zone.y, zone.z};
}

// A place as it is on the body this frame: its setting (a right-hander's),
// put on its side.
inline NiPoint3 ZoneFor(const NiPoint3& zone, HolsterSide side, bool leftHanded) {
	const float across = zone.x < 0.0f ? -zone.x : zone.x;
	switch (side) {
	case HolsterSide::Right:
		return NiPoint3{across, zone.y, zone.z};
	case HolsterSide::Left:
		return NiPoint3{-across, zone.y, zone.z};
	default:
		return Mirrored(zone, leftHanded);
	}
}

// The side a place has after the fitting run put it at `fittedX` (stored the
// right-handed way, vr::RightHanded): Auto stays Auto - the fit is mirrored
// with the hands - and a chosen side becomes the side the hand was really on,
// so fitting a place never moves it to the other shoulder.
inline HolsterSide FittedSide(HolsterSide side, float fittedX, bool leftHanded) {
	if (side == HolsterSide::Auto) {
		return side;
	}
	const float onBody = leftHanded ? -fittedX : fittedX;
	return onBody >= 0.0f ? HolsterSide::Right : HolsterSide::Left;
}

struct HolsterInput {
	bool allowed = false;     // Full VR, in the world, no menu
	bool rightValid = false;  // the weapon hand (roles already swapped for left-handed)
	bool leftValid = false;
	NiPoint3 rightRelative{0.0f, 0.0f, 0.0f};  // BodyRelative
	NiPoint3 leftRelative{0.0f, 0.0f, 0.0f};
	bool rightGrip = false;  // as it is now
	bool leftGrip = false;
	WeaponSeen seen = WeaponSeen::Unknown;
	EquippedKind equipped = EquippedKind::Nothing;
	bool haveOneHand = false;  // a weapon of that kind has been seen this session
	bool haveTwoHand = false;
	bool haveBow = false;
	bool leftHanded = false;  // the zones mirror
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
	bool rightInZone = false;   // the weapon hand in one of its two zones
	bool leftInZone = false;
	bool rightClaimed = false;  // these grips are the holster's: no grab
	bool leftClaimed = false;
	bool readyClick = false;    // the ready-weapon click: draw or sheathe
	HolsterKind equip = HolsterKind::None;    // equip the remembered one now
	HolsterKind gesture = HolsterKind::None;  // the zone that fired, for the log
	bool refused = false;       // fired with nothing of that kind to draw
	bool otherDrawn = false;    // fired while another kind is drawn: nothing done
	bool gaveUp = false;        // the equipped weapon never showed
};

inline EquippedKind KindFor(HolsterKind kind) {
	switch (kind) {
	case HolsterKind::OneHand:
		return EquippedKind::OneHand;
	case HolsterKind::TwoHand:
		return EquippedKind::TwoHand;
	case HolsterKind::Bow:
		return EquippedKind::Bow;
	default:
		return EquippedKind::Nothing;
	}
}

inline HolsterVerdict StepHolster(HolsterState& s, const HolsterInput& in,
                                  const HolsterSettings& settings) {
	HolsterVerdict v;
	const bool atHip = in.rightValid && InZone(in.rightRelative,
	                                           ZoneFor(settings.oneHandZone, settings.oneHandSide, in.leftHanded),
	                                           settings.oneHandRadius);
	const bool atBack = in.rightValid && InZone(in.rightRelative,
	                                            ZoneFor(settings.twoHandZone, settings.twoHandSide, in.leftHanded),
	                                            settings.twoHandRadius);
	v.rightInZone = atHip || atBack;
	v.leftInZone = in.leftValid && InZone(in.leftRelative,
	                                      ZoneFor(settings.bowZone, settings.bowSide, in.leftHanded),
	                                      settings.bowRadius);

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
	if (rightPress && atHip) {
		s.rightClaimed = true;
		fired = HolsterKind::OneHand;
	} else if (rightPress && atBack) {
		s.rightClaimed = true;
		fired = HolsterKind::TwoHand;
	} else if (leftPress && v.leftInZone) {
		s.leftClaimed = true;
		fired = HolsterKind::Bow;
	}
	if (fired != HolsterKind::None) {
		v.gesture = fired;
		const bool have = fired == HolsterKind::OneHand   ? in.haveOneHand
		                  : fired == HolsterKind::TwoHand ? in.haveTwoHand
		                                                  : in.haveBow;
		if (in.equipped == KindFor(fired)) {
			// Drawn or sheathed, the ready key turns it the other way.
			v.readyClick = true;
			s.drawAfterEquip = HolsterKind::None;
		} else if (in.equipped != EquippedKind::Nothing && in.seen == WeaponSeen::Drawn) {
			// Another weapon is out: it has to go back first, by its own
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

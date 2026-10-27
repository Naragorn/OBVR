#pragma once

// Drawing a weapon by reaching for it, and putting it away at a place on the
// body (docs/controls-spec.md 4.1, 4.2; docs/free-weapons-spec.md 5.12). Four
// places since 2026-10-27, each keeping one item (below, "The four places"):
// the hips, left and right, for one-handed weapons, the torch and the shield;
// the back, left and right, for two-handed weapons, staffs, the bow and the
// shield. The weapon hand reaches for weapons, the other hand for the bow,
// the shield and the torch - the engine's own split (its one weapon slot is
// the right hand's, the bow is held in the left, the shield and the torch are
// the left hand's). Full VR only.
//
// The zones are in the body's frame: metres from the eyes along the head's
// heading alone, so a head tipped down to look at the hip does not move the
// hip. The game's axes, as the gestures use them: x right, y forward, z up.
//
// A grip that closes at a place and does something there belongs to the
// holster until it opens: it does not grab, and it does not cancel a
// teleport. A grip already closed when the hand arrives stays a grab, so a
// held object is never dropped by passing the hip.
//
// Another weapon drawn, a reach for one kept elsewhere does nothing: it goes
// back first (the tester, 2026-09-27). Raised fists are not in the way.
//
// Oblivion has one weapon slot: a weapon kept at a place but not equipped is
// in the pack, and the reach equips it first (the glue does that), then draws
// it once the game shows it. A shield or a torch put at a place is taken off
// into the pack and kept there; the other hand empty, its reach takes it back
// (EquipItem). A take waits while a two-hander or a bow is drawn; the
// engine's own equip rule decides the rest (a shield taken back with the fists
// or a two-hander out stays off the hand until a one-hander is - vanilla,
// UESP Oblivion:Armor). The glue keeps the forms per place, in the co-save.
//
// Pure, covered by holster_test.

#include "core/ChoiceWord.h"
#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/Quaternion.h"

namespace obvr::vr {


// What the other hand carries in its own slots: a shield on the arm, a torch
// in the hand (the process's GetEquippedShieldData and GetEquippedLightData,
// worn), or nothing.
enum class OffHandItem : UInt8 { Nothing, Shield, Torch };

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

// The draw and sheathe sounds OBVR plays itself (the tester, 2026-09-30: "stab
// und bogen keine equip sounds bekommen im vergleich zu 1 hand oder 2hand").
// The game plays a weapon's WPN<type>Equip/Unequip at the "Enum: Equip" and
// "Enum: Unequip" keys of its draw animation (0x006B07F0, the name built from
// the type). The staff's first-person draw and sheathe have no such key
// (staffequip.kf, staffunequip.kf: only Attach/Detach): silent in the game
// itself. The bow's have them, and the harness showed the key's span fired
// through the game's handler (0.435 to 0.536 s round the key at 0.5) - and
// with OBVR's own added the tester found the bow's "war da whl schon vorher"
// (2026-09-30): the game plays it, so OBVR does not. For the staff OBVR plays
// the game's own sound forms (Oblivion.esm): WPNStaffEquip 00029BB2,
// WPNStaffUnequip 00029BB3. 0 for any other weapon: the game plays its own.
inline constexpr UInt32 kStaffEquipSound = 0x00029BB2;
inline constexpr UInt32 kStaffUnequipSound = 0x00029BB3;

inline UInt32 DrawSoundForm(SInt32 type, bool drawn) {
	if (type == 4) {
		return drawn ? kStaffEquipSound : kStaffUnequipSound;
	}
	return 0;
}

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
	// the draw is given up; a shield or a torch stowed or taken back the same.
	float drawWaitSeconds = 2.0f;
	// [Hands] HolsterShieldTorch: the other hand stows its shield or torch at
	// the weapon hand's places and takes it back from there.
	bool offHandItems = true;
	// [Hands] HolsterLabels: a small sign at a place while a hand is at it -
	// what the place keeps, or a hint after a reach that did nothing (Nadi,
	// 2026-10-27: "Mit kleiner Anzeigetafel wie in B&S").
	bool labels = true;
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

// BodyRelative's way back: a point in the body's frame (right, forward, up,
// metres from the eyes) in tracking space - where a sign at a place hangs.
inline NiPoint3 BodyToTracking(const Quaternion& head, const NiPoint3& headPosition, const NiPoint3& relative) {
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
	return NiPoint3{headPosition.x + relative.x * -fz + relative.y * fx, headPosition.y + relative.z,
	                headPosition.z + relative.x * fx + relative.y * fz};
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


// ---------------------------------------------------------------------------
// The four places (Nadi, 2026-10-27: "Waffen, schilde und torch kann ich nun
// überall holstern wie in B&S. 2händer und staff und Bogen aber nur rücken
// rechts oder links. Torch, one hander und pfeile linke oder rechte Hüfte",
// shields at all four; docs/free-weapons-spec.md section 5.12). Each place
// keeps one item, whatever its kind, as far as the place takes that kind:
//
//   the hips, left and right     one-handed weapons, the torch, the shield
//   the back, left and right     two-handed weapons and staffs, the bow,
//                                the shield
//
// Which hand: the engine's own split, told to the players - the weapon hand
// for weapons (and the arrows), the other hand for the bow, the shield and
// the torch. The wrong hand at a place does nothing but say so (`wrongHand`).
// A place keeps the item's base form in the glue (and in the co-save); the
// weapon there is the equipped one or one in the pack. Taking a weapon kept
// at a place equips it and draws it; putting the drawn weapon there sheathes
// it and keeps it there. A weapon equipped in the inventory, kept at no
// place, goes to its kind's place on its side (DefaultSlotFor) when that is
// free - so the sword at the left hip and the claymore over the right
// shoulder work as before without a first stow.
// ---------------------------------------------------------------------------

enum class HolsterSlot : UInt8 { None, HipLeft, HipRight, BackLeft, BackRight };

inline constexpr UInt32 kHolsterSlotCount = 5;

// What a place keeps, or what a hand would put there.
enum class SlotItem : UInt8 { Nothing, OneHand, TwoHand, Bow, Shield, Torch };

inline bool SlotIsHip(HolsterSlot slot) { return slot == HolsterSlot::HipLeft || slot == HolsterSlot::HipRight; }

inline bool SlotIsBack(HolsterSlot slot) { return slot == HolsterSlot::BackLeft || slot == HolsterSlot::BackRight; }

inline bool SlotAccepts(HolsterSlot slot, SlotItem item) {
	switch (item) {
	case SlotItem::OneHand:
	case SlotItem::Torch:
		return SlotIsHip(slot);
	case SlotItem::TwoHand:
	case SlotItem::Bow:
		return SlotIsBack(slot);
	case SlotItem::Shield:
		return SlotIsHip(slot) || SlotIsBack(slot);
	default:
		return false;
	}
}

// The weapon hand's items; the bow, the shield and the torch are the other
// hand's.
inline bool WeaponHandItem(SlotItem item) { return item == SlotItem::OneHand || item == SlotItem::TwoHand; }

inline SlotItem SlotItemOf(EquippedKind kind) {
	switch (kind) {
	case EquippedKind::OneHand:
		return SlotItem::OneHand;
	case EquippedKind::TwoHand:
		return SlotItem::TwoHand;
	case EquippedKind::Bow:
		return SlotItem::Bow;
	default:
		return SlotItem::Nothing;
	}
}

inline SlotItem SlotItemOf(OffHandItem item) {
	return item == OffHandItem::Shield ? SlotItem::Shield
	       : item == OffHandItem::Torch ? SlotItem::Torch
	                                    : SlotItem::Nothing;
}

// The places on the body this frame, in BodyRelative's frame, and their
// radii: the hips from the one-hander's setting on both sides, the back's
// right from the two-hander's and its left from the bow's.
struct SlotZones {
	NiPoint3 at[kHolsterSlotCount] = {};
	float radius[kHolsterSlotCount] = {};
};

inline SlotZones SlotZonesFor(const HolsterSettings& s) {
	SlotZones z;
	z.at[static_cast<UInt32>(HolsterSlot::HipLeft)] = ZoneFor(s.oneHandZone, HolsterSide::Left, false);
	z.at[static_cast<UInt32>(HolsterSlot::HipRight)] = ZoneFor(s.oneHandZone, HolsterSide::Right, false);
	z.at[static_cast<UInt32>(HolsterSlot::BackLeft)] = ZoneFor(s.bowZone, HolsterSide::Left, false);
	z.at[static_cast<UInt32>(HolsterSlot::BackRight)] = ZoneFor(s.twoHandZone, HolsterSide::Right, false);
	z.radius[static_cast<UInt32>(HolsterSlot::HipLeft)] = s.oneHandRadius;
	z.radius[static_cast<UInt32>(HolsterSlot::HipRight)] = s.oneHandRadius;
	z.radius[static_cast<UInt32>(HolsterSlot::BackLeft)] = s.bowRadius;
	z.radius[static_cast<UInt32>(HolsterSlot::BackRight)] = s.twoHandRadius;
	return z;
}

// The place a hand is at: the nearest whose zone it is in, None outside them.
inline HolsterSlot SlotAt(const SlotZones& z, const NiPoint3& relative) {
	HolsterSlot best = HolsterSlot::None;
	float bestDistance = 0.0f;
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		const NiPoint3 d = relative - z.at[i];
		const float distance = d.LengthSquared();
		if (distance <= z.radius[i] * z.radius[i] && (best == HolsterSlot::None || distance < bestDistance)) {
			best = static_cast<HolsterSlot>(i);
			bestDistance = distance;
		}
	}
	return best;
}

// The place a weapon equipped in the inventory goes to when no place keeps
// it: its kind's place on the side the settings give it (HolsterSide, the
// hand's mirror on Auto) - the left hip, over the right shoulder, behind the
// left shoulder for a right-hander. None for anything else.
inline HolsterSlot DefaultSlotFor(SlotItem item, const HolsterSettings& s, bool leftHanded) {
	switch (item) {
	case SlotItem::OneHand:
		return ZoneFor(s.oneHandZone, s.oneHandSide, leftHanded).x < 0.0f ? HolsterSlot::HipLeft : HolsterSlot::HipRight;
	case SlotItem::TwoHand:
		return ZoneFor(s.twoHandZone, s.twoHandSide, leftHanded).x < 0.0f ? HolsterSlot::BackLeft
		                                                                  : HolsterSlot::BackRight;
	case SlotItem::Bow:
		return ZoneFor(s.bowZone, s.bowSide, leftHanded).x < 0.0f ? HolsterSlot::BackLeft : HolsterSlot::BackRight;
	default:
		return HolsterSlot::None;
	}
}

// Where a weapon equipped in the inventory and kept at no place goes: its
// home (DefaultSlotFor) when that is free; else another free place that takes
// it, so a weapon put at its home by hand stays kept there; else its home,
// in the place of what is kept there (it is in the pack, kept nowhere now).
// None for a kind no place takes.
inline HolsterSlot PlaceForUnkept(SlotItem item, HolsterSlot home, const SlotItem (&kept)[kHolsterSlotCount]) {
	if (home == HolsterSlot::None || !SlotAccepts(home, item)) {
		return HolsterSlot::None;
	}
	if (kept[static_cast<UInt32>(home)] == SlotItem::Nothing) {
		return home;
	}
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		const HolsterSlot slot = static_cast<HolsterSlot>(i);
		if (slot != home && SlotAccepts(slot, item) && kept[i] == SlotItem::Nothing) {
			return slot;
		}
	}
	return home;
}

// The side a place is on, for the body's sheath nodes (vr/SheathMirror.h).
inline HolsterSide SideOfSlot(HolsterSlot slot) {
	return slot == HolsterSlot::HipLeft || slot == HolsterSlot::BackLeft     ? HolsterSide::Left
	       : slot == HolsterSlot::HipRight || slot == HolsterSlot::BackRight ? HolsterSide::Right
	                                                                         : HolsterSide::Auto;
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
	// The place that keeps the equipped weapon, None when none does.
	HolsterSlot equippedSlot = HolsterSlot::None;
	// The other hand's shield or torch (worn).
	OffHandItem offHand = OffHandItem::Nothing;
	// What each place keeps (the glue's memory), and whether it is worn now -
	// the equipped weapon, or a shield or torch equipped - which is what the
	// watch of a stow or a take reads. Not the hand: the game keeps an
	// equipped shield off the hand while the fists, a two-hander or a bow are
	// in use, and a torch while a shield is (UESP Oblivion:Armor; measured
	// 2026-10-24, shield-equip-probe).
	SlotItem slotItem[kHolsterSlotCount] = {};
	bool slotWorn[kHolsterSlotCount] = {};
	bool leftHanded = false;
	float dt = 0.0f;
};

// What a reach at a place did, for the log and the panel.
enum class SlotOutcome : UInt8 {
	None,
	Draw,         // the weapon kept there drawn (it is the one equipped)
	EquipDraw,    // the weapon kept there equipped, then drawn
	Sheathe,      // the drawn weapon put there
	Stow,         // the shield or torch put there
	Take,         // the shield or torch kept there taken back
	Empty,        // nothing there for this hand: nothing done
	Full,         // the place keeps something else
	DoesNotFit,   // the place does not take this kind
	WrongHand,    // what is kept there is the other hand's
	OtherDrawn,   // another weapon is drawn: it goes back first
	Quiver,       // the bow drawn: the weapon hand's reach is the quiver's
};

struct HolsterState {
	bool rightGripWas = false;
	bool leftGripWas = false;
	bool rightClaimed = false;
	bool leftClaimed = false;
	// A weapon equipped by the last reach: drawn once the game shows it.
	HolsterSlot drawAfterEquip = HolsterSlot::None;
	float drawWait = 0.0f;
	// A stow or a take of the other hand's shield or torch, watched until
	// the place's item shows it.
	HolsterSlot offHandWatch = HolsterSlot::None;
	bool offHandTaking = false;
	float offHandWait = 0.0f;
};

struct HolsterVerdict {
	HolsterSlot rightSlot = HolsterSlot::None;  // the place each hand is at
	HolsterSlot leftSlot = HolsterSlot::None;
	bool rightInZone = false;   // the weapon hand at a place
	bool leftInZone = false;
	bool rightClaimed = false;  // these grips are the holster's: no grab
	bool leftClaimed = false;
	bool readyClick = false;    // the ready-weapon click: draw or sheathe
	HolsterSlot equip = HolsterSlot::None;   // equip the weapon kept at this place now
	HolsterSlot assign = HolsterSlot::None;  // the equipped weapon is kept at this place from now
	// The reach that fired: where, with which hand, and what it did.
	HolsterSlot gesture = HolsterSlot::None;
	bool gestureLeft = false;
	SlotOutcome outcome = SlotOutcome::None;
	bool gaveUp = false;  // the equipped weapon never showed; the glue forgets `gaveUpAt`
	HolsterSlot gaveUpAt = HolsterSlot::None;
	// The other hand's shield or torch: stow it at this place (keep it
	// there, then take it off), take back the one kept there, or leave it
	// because a two-hander or a bow is drawn. Then the watch: the item shows
	// the stow (or the take, offHandDoneTaking) done, or it never did and
	// the glue forgets the place.
	HolsterSlot offHandStow = HolsterSlot::None;
	HolsterSlot offHandTake = HolsterSlot::None;
	HolsterSlot offHandRefused = HolsterSlot::None;
	HolsterSlot offHandDone = HolsterSlot::None;
	HolsterSlot offHandGaveUp = HolsterSlot::None;
	bool offHandDoneTaking = false;
	// A grip that closed at no place (the holster on and allowed): where the
	// hand was from the head (right, forward, up, metres), the nearest place
	// and how far off it - the log's word when a reach finds nothing.
	bool rightMissed = false;
	bool leftMissed = false;
	NiPoint3 missedAt{0.0f, 0.0f, 0.0f};
	HolsterSlot missedNearest = HolsterSlot::None;
	float missedOff = 0.0f;
};

namespace detail {

inline HolsterSlot NearestSlot(const SlotZones& z, const NiPoint3& relative, float& off) {
	HolsterSlot best = HolsterSlot::None;
	off = 0.0f;
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		const float d = math::Sqrt((relative - z.at[i]).LengthSquared());
		if (best == HolsterSlot::None || d < off) {
			best = static_cast<HolsterSlot>(i);
			off = d;
		}
	}
	return best;
}

}  // namespace detail

inline HolsterVerdict StepHolster(HolsterState& s, const HolsterInput& in, const HolsterSettings& settings) {
	HolsterVerdict v;
	const SlotZones zones = SlotZonesFor(settings);
	v.rightSlot = in.rightValid ? SlotAt(zones, in.rightRelative) : HolsterSlot::None;
	v.leftSlot = in.leftValid ? SlotAt(zones, in.leftRelative) : HolsterSlot::None;
	v.rightInZone = v.rightSlot != HolsterSlot::None;
	v.leftInZone = v.leftSlot != HolsterSlot::None;

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
		s.drawAfterEquip = HolsterSlot::None;
		s.offHandWatch = HolsterSlot::None;
		v.rightClaimed = s.rightClaimed;
		v.leftClaimed = s.leftClaimed;
		return v;
	}
	const float dt = in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;

	// A weapon equipped by the last reach: drawn once the game shows it.
	if (s.drawAfterEquip != HolsterSlot::None) {
		s.drawWait -= dt;
		const UInt32 at = static_cast<UInt32>(s.drawAfterEquip);
		if (in.slotWorn[at] && in.seen != WeaponSeen::Unknown) {
			v.readyClick = in.seen == WeaponSeen::Sheathed;
			s.drawAfterEquip = HolsterSlot::None;
		} else if (s.drawWait <= 0.0f) {
			v.gaveUp = true;
			v.gaveUpAt = s.drawAfterEquip;
			s.drawAfterEquip = HolsterSlot::None;
		}
	}

	// The other hand's stow or take, watched until the place's item shows it.
	if (s.offHandWatch != HolsterSlot::None) {
		const UInt32 at = static_cast<UInt32>(s.offHandWatch);
		if (in.slotItem[at] == SlotItem::Nothing) {
			// The glue keeps nothing there (the stow's unequip was refused, or
			// the take's equip): nothing to watch.
			s.offHandWatch = HolsterSlot::None;
		} else {
			s.offHandWait -= dt;
			const bool shows = s.offHandTaking ? in.slotWorn[at] : !in.slotWorn[at];
			if (shows) {
				v.offHandDone = s.offHandWatch;
				v.offHandDoneTaking = s.offHandTaking;
				s.offHandWatch = HolsterSlot::None;
			} else if (s.offHandWait <= 0.0f) {
				v.offHandGaveUp = s.offHandWatch;
				v.offHandDoneTaking = s.offHandTaking;
				s.offHandWatch = HolsterSlot::None;
			}
		}
	}

	const bool drawn = in.seen == WeaponSeen::Drawn;
	const SlotItem equippedItem = SlotItemOf(in.equipped);
	const bool bowDrawn = drawn && in.equipped == EquippedKind::Bow;
	const bool weaponDrawn = drawn && WeaponHandItem(equippedItem);
	const bool bothHandsDrawn = drawn && (in.equipped == EquippedKind::TwoHand || in.equipped == EquippedKind::Bow);
	// The weapon kept at this place, or the equipped one kept at none, may be
	// put there by a draw from an empty place (the inventory's equip).
	const auto mayKeep = [&](HolsterSlot slot, SlotItem item) {
		const UInt32 at = static_cast<UInt32>(slot);
		return SlotAccepts(slot, item) && (in.slotItem[at] == SlotItem::Nothing || slot == in.equippedSlot);
	};
	const auto watch = [&](HolsterSlot slot, bool taking) {
		s.offHandWatch = slot;
		s.offHandTaking = taking;
		s.offHandWait = settings.drawWaitSeconds;
	};
	const auto equipAndDraw = [&](HolsterSlot slot) {
		v.equip = slot;
		s.drawAfterEquip = slot;
		s.drawWait = settings.drawWaitSeconds;
		return SlotOutcome::EquipDraw;
	};

	// The weapon hand at a place.
	SlotOutcome right = SlotOutcome::None;
	if (rightPress && v.rightInZone) {
		const HolsterSlot slot = v.rightSlot;
		const UInt32 at = static_cast<UInt32>(slot);
		const SlotItem kept = in.slotItem[at];
		if (bowDrawn) {
			right = SlotOutcome::Quiver;
		} else if (weaponDrawn) {
			// A place keeping another weapon of the weapon hand's: that one is
			// wanted, and the drawn one goes back first.
			if (!SlotAccepts(slot, equippedItem)) {
				right = WeaponHandItem(kept) ? SlotOutcome::OtherDrawn : SlotOutcome::DoesNotFit;
			} else if (!mayKeep(slot, equippedItem)) {
				right = WeaponHandItem(kept) ? SlotOutcome::OtherDrawn : SlotOutcome::Full;
			} else {
				v.readyClick = true;
				v.assign = slot;
				s.drawAfterEquip = HolsterSlot::None;
				right = SlotOutcome::Sheathe;
			}
		} else if (WeaponHandItem(kept)) {
			if (in.slotWorn[at]) {
				v.readyClick = true;
				s.drawAfterEquip = HolsterSlot::None;
				right = SlotOutcome::Draw;
			} else {
				right = equipAndDraw(slot);
			}
		} else if (kept != SlotItem::Nothing) {
			right = SlotOutcome::WrongHand;
		} else if (WeaponHandItem(equippedItem) && in.equippedSlot == HolsterSlot::None &&
		           SlotAccepts(slot, equippedItem)) {
			// The equipped weapon kept at no place: drawn from here, kept here.
			v.readyClick = true;
			v.assign = slot;
			right = SlotOutcome::Draw;
		} else {
			right = SlotOutcome::Empty;
		}
		if (right != SlotOutcome::Quiver && right != SlotOutcome::Empty) {
			s.rightClaimed = true;
		}
		v.gesture = slot;
		v.gestureLeft = false;
		v.outcome = right;
	}

	// The other hand at a place.
	SlotOutcome left = SlotOutcome::None;
	if (leftPress && v.leftInZone) {
		const HolsterSlot slot = v.leftSlot;
		const UInt32 at = static_cast<UInt32>(slot);
		const SlotItem kept = in.slotItem[at];
		const SlotItem carried = SlotItemOf(in.offHand);
		if (bowDrawn) {
			if (!SlotAccepts(slot, SlotItem::Bow)) {
				left = SlotOutcome::DoesNotFit;
			} else if (!mayKeep(slot, SlotItem::Bow)) {
				left = SlotOutcome::Full;
			} else {
				v.readyClick = true;
				v.assign = slot;
				s.drawAfterEquip = HolsterSlot::None;
				left = SlotOutcome::Sheathe;
			}
		} else if (carried != SlotItem::Nothing && settings.offHandItems) {
			// The place keeping this very item (worn now - equipped again from
			// the pack) is its own.
			if (!SlotAccepts(slot, carried)) {
				left = SlotOutcome::DoesNotFit;
			} else if (kept != SlotItem::Nothing && !in.slotWorn[at]) {
				left = SlotOutcome::Full;
			} else {
				v.offHandStow = slot;
				watch(slot, false);
				left = SlotOutcome::Stow;
			}
		} else if (kept == SlotItem::Bow) {
			if (weaponDrawn) {
				left = SlotOutcome::OtherDrawn;
			} else if (in.slotWorn[at]) {
				v.readyClick = true;
				s.drawAfterEquip = HolsterSlot::None;
				left = SlotOutcome::Draw;
			} else {
				left = equipAndDraw(slot);
			}
		} else if ((kept == SlotItem::Shield || kept == SlotItem::Torch) && settings.offHandItems) {
			if (bothHandsDrawn) {
				v.offHandRefused = slot;
				left = SlotOutcome::OtherDrawn;
			} else {
				v.offHandTake = slot;
				watch(slot, true);
				left = SlotOutcome::Take;
			}
		} else if (WeaponHandItem(kept)) {
			left = SlotOutcome::WrongHand;
		} else if (in.equipped == EquippedKind::Bow && in.equippedSlot == HolsterSlot::None &&
		           SlotAccepts(slot, SlotItem::Bow) && kept == SlotItem::Nothing) {
			v.readyClick = true;
			v.assign = slot;
			left = SlotOutcome::Draw;
		} else {
			left = SlotOutcome::Empty;
		}
		if (left != SlotOutcome::Empty) {
			s.leftClaimed = true;
		}
		// Both hands in one frame: the weapon hand's reach is the one said.
		if (right == SlotOutcome::None) {
			v.gesture = slot;
			v.gestureLeft = true;
			v.outcome = left;
		}
	}

	// A grip closing at no place: where it was and how far off the nearest.
	const bool rightMiss = rightPress && in.rightValid && !v.rightInZone;
	const bool leftMiss = leftPress && in.leftValid && !v.leftInZone;
	if (rightMiss || leftMiss) {
		v.rightMissed = rightMiss;
		v.leftMissed = !rightMiss && leftMiss;
		v.missedAt = rightMiss ? in.rightRelative : in.leftRelative;
		v.missedNearest = detail::NearestSlot(zones, v.missedAt, v.missedOff);
	}
	v.rightClaimed = s.rightClaimed;
	v.leftClaimed = s.leftClaimed;
	return v;
}

// The words for the log and the panel.
inline const char* SlotWords(HolsterSlot slot) {
	switch (slot) {
	case HolsterSlot::HipLeft:
		return "the left hip";
	case HolsterSlot::HipRight:
		return "the right hip";
	case HolsterSlot::BackLeft:
		return "the left shoulder";
	case HolsterSlot::BackRight:
		return "the right shoulder";
	default:
		return "no place";
	}
}

inline const char* SlotOutcomeWords(SlotOutcome o) {
	switch (o) {
	case SlotOutcome::Draw:
		return "drawn";
	case SlotOutcome::EquipDraw:
		return "equipped, then drawn";
	case SlotOutcome::Sheathe:
		return "sheathed and kept there";
	case SlotOutcome::Stow:
		return "stowed there";
	case SlotOutcome::Take:
		return "taken back";
	case SlotOutcome::Empty:
		return "nothing there for this hand";
	case SlotOutcome::Full:
		return "the place keeps something else";
	case SlotOutcome::DoesNotFit:
		return "this place does not take it";
	case SlotOutcome::WrongHand:
		return "that is the other hand's";
	case SlotOutcome::OtherDrawn:
		return "another weapon is drawn - it goes back first";
	case SlotOutcome::Quiver:
		return "the bow is drawn - the quiver's";
	default:
		return "nothing";
	}
}

// The panel's hint for a reach with the wrong hand or the wrong place
// (Nadi, 2026-10-27: "Hinweis bei falscher Hand"), short enough for it.
inline const char* SlotHint(SlotOutcome o, SlotItem kept, SlotItem carried) {
	switch (o) {
	case SlotOutcome::WrongHand:
		return WeaponHandItem(kept) ? "Weapons: weapon hand" : "Bow, shield, torch: other hand";
	case SlotOutcome::DoesNotFit:
		return carried == SlotItem::OneHand || carried == SlotItem::Torch ? "Goes at the hip"
		       : carried == SlotItem::TwoHand || carried == SlotItem::Bow ? "Goes on the back"
		                                                                  : "Does not fit here";
	case SlotOutcome::Full:
		return "Place taken";
	case SlotOutcome::OtherDrawn:
		return "Put the weapon away first";
	default:
		return nullptr;
	}
}

// The sign's hint while a hand is at a place, before any reach: what would
// go wrong if it closed there. `carried` is what that hand holds (the drawn
// weapon for the weapon hand; the drawn bow, the shield or the torch for the
// other), `kept` what the place keeps and `keptIsCarried` whether that is the
// very item in the hand (its own place). None when all is well.
inline SlotOutcome SignOutcomeFor(bool otherHand, HolsterSlot slot, SlotItem carried, SlotItem kept, bool keptIsCarried) {
	if (slot == HolsterSlot::None) {
		return SlotOutcome::None;
	}
	if (carried != SlotItem::Nothing) {
		// Another weapon of the weapon hand's kept there, one drawn: as the
		// reach says it (StepHolster), it goes back first.
		if (!keptIsCarried && WeaponHandItem(carried) && WeaponHandItem(kept)) {
			return SlotOutcome::OtherDrawn;
		}
		if (!SlotAccepts(slot, carried)) {
			return SlotOutcome::DoesNotFit;
		}
		return kept != SlotItem::Nothing && !keptIsCarried ? SlotOutcome::Full : SlotOutcome::None;
	}
	if (kept == SlotItem::Nothing) {
		return SlotOutcome::None;
	}
	return WeaponHandItem(kept) == otherHand ? SlotOutcome::WrongHand : SlotOutcome::None;
}

// Where a place's sign hangs, in BodyRelative's frame: a little above the
// place and, for the back's, in front of the shoulder, where the eyes can
// read it while the hand reaches behind.
inline NiPoint3 SignPlaceFor(HolsterSlot slot, const SlotZones& zones) {
	NiPoint3 at = zones.at[static_cast<UInt32>(slot)];
	if (SlotIsBack(slot)) {
		at.y = 0.20f;
		at.z += 0.05f;
	} else {
		at.y += 0.06f;
		at.z += 0.10f;
	}
	return at;
}

}  // namespace obvr::vr

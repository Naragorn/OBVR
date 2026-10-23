#pragma once

// A weapon lying in the world, taken at its grip: into the inventory,
// equipped, drawn - Blade & Sorcery's way (docs/free-weapons-spec.md section
// 5, decisions 1, 2, 8 and 9; the tester, 2026-10-23: "grab any weapon by
// the handle and its equipped"). The grip is the vanilla hold point, one
// point on the model: where the game's own animation puts the weapon hand's
// palm when the weapon is equipped. The ring sits on it; a touch elsewhere
// on the weapon carries it as any object does today; a weapon is never
// pulled from afar. The other hand only carries.
//
// The take is the game's own activation (game::TakeIntoInventory: its
// owner's rules, a theft when seen), the equip its EquipItem without the
// sound (game::EquipWeaponForm), the draw the ready-weapon click once the
// game shows the weapon equipped and sheathed - as a holster's reach draws
// a remembered weapon (vr::StepHolster, drawAfterEquip). The equip picks
// its stack itself: with two of the same form in the pack it may equip the
// other (section 2.1's risk, open).
//
// Pure, covered by weapon_grab_test; CameraHook reads the world and sends
// the calls.

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Holster.h"

namespace obvr::game {

// Where the vanilla hold sits on a weapon's model: along its +y axis from
// its origin, in game units, negative towards the pommel (vr/TwoHandLogic.h
// and controls-spec 4.10: the right palm at -0.9 on blades, blunts and the
// claymore, measured live; at -37.6 on a staff). The bow is unmeasured and
// taken as the hand's attach like a blade. The weapon type is the game's
// (TESObjectWEAP +0x90: 0 blade one hand, 1 blade two hand, 2 blunt one
// hand, 3 blunt two hand, 4 staff, 5 bow).
inline constexpr float kGripAxialBlade = -0.9f;
inline constexpr float kGripAxialStaff = -37.6f;
inline constexpr SInt32 kWeaponTypeStaff = 4;

inline float GripAxialFor(SInt32 weaponType) {
	return weaponType == kWeaponTypeStaff ? kGripAxialStaff : kGripAxialBlade;
}

// The grip point in the world from the model's world transform.
inline NiPoint3 GripPointOf(const NiMatrix33& rot, const NiPoint3& pos, float axial) {
	return pos + rot * NiPoint3{0.0f, axial, 0.0f};
}

struct WeaponGrabSettings {
	bool enabled = true;            // [Hands] GrabEquipsWeapons
	float gripReachMetres = 0.12f;  // [Hands] WeaponGripReachMetres: the palm this near the grip takes it
	float waitSeconds = 2.0f;       // how long the equip may take to show before the take is given up
};

enum class WeaponGrabChoice : UInt8 {
	NotAWeapon,  // the thing is no weapon: the grab as it is today
	Equip,       // the weapon hand at the grip: take, equip, draw
	Carry,       // a touch elsewhere, or the other hand at the grip: the held object as today
	Nothing,     // a weapon out of the hand's touch: no grab, no pull
};

// What a closing grip does on the weapon the pick holds. `toGripUnits` is
// the palm's distance to the grip point, `toMeshUnits` the hand's to the
// model's nearest vertex; negative when unknown, which counts as far.
inline WeaponGrabChoice ChooseWeaponGrab(bool isWeapon, bool weaponHand, bool allowed, float toGripUnits,
                                         float toMeshUnits, float gripReachUnits, float touchUnits) {
	if (!isWeapon) {
		return WeaponGrabChoice::NotAWeapon;
	}
	const bool atGrip = toGripUnits >= 0.0f && toGripUnits <= gripReachUnits;
	const bool touching = toMeshUnits >= 0.0f && toMeshUnits <= touchUnits;
	if (weaponHand && allowed && atGrip) {
		return WeaponGrabChoice::Equip;
	}
	if (touching || (!weaponHand && atGrip)) {
		return WeaponGrabChoice::Carry;
	}
	return WeaponGrabChoice::Nothing;
}

// The take's steps after the activation: the equip sent once, then the
// wait for the game to show the weapon equipped - sheathed, and the
// ready-weapon click draws it; drawn already, nothing more to do.
enum class WeaponTakePhase : UInt8 { Idle, Equipping };

struct WeaponTakeState {
	WeaponTakePhase phase = WeaponTakePhase::Idle;
	UInt32 form = 0;  // the weapon's base form, read before the take (the reference goes)
	vr::EquippedKind kind = vr::EquippedKind::Nothing;
	bool equipSent = false;
	float wait = 0.0f;
};

struct WeaponTakeInput {
	bool allowed = false;  // in the world, no menu: a menu opening cancels the take
	vr::EquippedKind equipped = vr::EquippedKind::Nothing;
	vr::WeaponSeen seen = vr::WeaponSeen::Unknown;
	float dt = 0.0f;
};

struct WeaponTakeVerdict {
	bool equipNow = false;   // send EquipItem for `form` this frame
	UInt32 form = 0;
	bool readyClick = false; // the weapon shows sheathed: the ready click draws it
	bool done = false;       // the weapon is equipped (and drawn, or being drawn by the click)
	bool gaveUp = false;     // the weapon never showed as equipped in time
	bool cancelled = false;  // a menu or the like interrupted the take
};

inline void BeginWeaponTake(WeaponTakeState& s, UInt32 form, vr::EquippedKind kind, float waitSeconds) {
	s = WeaponTakeState{};
	s.phase = WeaponTakePhase::Equipping;
	s.form = form;
	s.kind = kind;
	s.wait = waitSeconds == waitSeconds && waitSeconds > 0.0f ? waitSeconds : 2.0f;
}

inline WeaponTakeVerdict StepWeaponTake(WeaponTakeState& s, const WeaponTakeInput& in) {
	WeaponTakeVerdict v;
	if (s.phase == WeaponTakePhase::Idle) {
		return v;
	}
	if (!in.allowed) {
		s = WeaponTakeState{};
		v.cancelled = true;
		return v;
	}
	if (!s.equipSent) {
		s.equipSent = true;
		v.equipNow = true;
		v.form = s.form;
		return v;
	}
	s.wait -= in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (in.equipped == s.kind && in.seen != vr::WeaponSeen::Unknown) {
		v.readyClick = in.seen == vr::WeaponSeen::Sheathed;
		v.done = true;
		s = WeaponTakeState{};
		return v;
	}
	if (s.wait <= 0.0f) {
		v.gaveUp = true;
		s = WeaponTakeState{};
	}
	return v;
}

}  // namespace obvr::game

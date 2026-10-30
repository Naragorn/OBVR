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
	// [Hands] GrabEquipsTorches: a torch taken at its grip by the other hand,
	// or pulled to it, is equipped there (game/TorchGrab.h).
	bool torches = true;
	// [Hands] WeaponGripReachMetres: the hand this near the grip takes it. 0.12
	// was tight on the tester's first round (2026-10-23: a hand touching a
	// sword on the floor keeps its controller 0.12 to 0.14 m above the grip).
	float gripReachMetres = 0.18f;
	// [Hands] PullToHand: a weapon out of reach floats to the hand on a grip,
	// as every item did before (the tester: "wie es vorher war da
	// comfortabler"), and is equipped the moment it arrives.
	bool pullToHand = true;
	float waitSeconds = 2.0f;       // how long the equip may take to show before the take is given up
	// [Hands] TakeDrawsAtOnce: the taken weapon straight into the hand, no draw
	// (StepWeaponTake below).
	bool drawAtOnce = true;
};

enum class WeaponGrabChoice : UInt8 {
	NotAWeapon,  // the thing is no weapon: the grab as it is today
	Equip,       // the weapon hand at the grip: take, equip, draw
	Carry,       // a touch elsewhere, or the other hand at the grip: the held object as today
	Pull,        // out of the hand's touch, the pull on: floats to the hand as today, equipped on arrival
	Nothing,     // out of the hand's touch, the pull off: no grab
};

// What a closing grip does on the weapon the pick holds. `toGripUnits` is
// the hand's distance to the grip point, `toMeshUnits` the hand's to the
// model's nearest vertex; negative when unknown, which counts as far.
inline WeaponGrabChoice ChooseWeaponGrab(bool isWeapon, bool weaponHand, bool allowed, float toGripUnits,
                                         float toMeshUnits, float gripReachUnits, float touchUnits,
                                         bool pullToHand) {
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
	return pullToHand ? WeaponGrabChoice::Pull : WeaponGrabChoice::Nothing;
}

// Whether a take makes the game's pickup sound: at the grip it does (the
// tester: a sound there is "video gamey", fine); off the grip - a pull, a
// carry by the blade - it is silent.
inline bool WeaponTakeIsLoud(WeaponGrabChoice how) { return how == WeaponGrabChoice::Equip; }

// The take fires while the hold is closed at the grip, once per closing: a
// hand that closed on the way and arrives closed takes the weapon too (the
// tester's third try, 2026-10-23, closed before it was there and found no
// edge). After a take the hand has to open before the next.
struct WeaponHoldLatch {
	bool latched = false;
};

inline bool StepWeaponTakeTrigger(WeaponHoldLatch& l, bool holdClosed, bool canTake) {
	if (!holdClosed) {
		l.latched = false;
		return false;
	}
	if (canTake && !l.latched) {
		l.latched = true;
		return true;
	}
	return false;
}

// A take by the pull latches the hold the same way.
inline void LatchWeaponHold(WeaponHoldLatch& l) { l.latched = true; }

// While the hold that took a weapon stays closed, the grip grabs nothing
// else: the run of 2026-10-24 08:03 had the hand, still closed after the
// pull's take, grab a potion lying by the sword while the sword was being
// drawn, and the grab restarted the draw every frame for seconds.
inline bool WeaponHoldBlocksGrab(const WeaponHoldLatch& l) { return l.latched; }

// A pulled weapon, arrived in the weapon hand with the hold closed: the
// engine's grab is let go of (the grab key up), and once the engine has let
// go the weapon is taken and equipped like one grabbed at its grip. Only a
// pull: a weapon carried by its blade stays a carry.
enum class WeaponPullPhase : UInt8 { Idle, Releasing };

struct WeaponPullState {
	WeaponPullPhase phase = WeaponPullPhase::Idle;
	UInt32 ref = 0;
	UInt32 form = 0;
	SInt32 type = -1;
	float wait = 0.0f;
};

struct WeaponPullInput {
	bool allowed = false;
	UInt32 heldRef = 0;          // what the engine's grab holds, 0 none
	bool heldIsWeapon = false;
	UInt32 heldForm = 0;         // its base form, read while it is still there
	SInt32 heldType = -1;
	bool weaponHandHolds = false;
	bool arrived = false;        // floated all the way in (game::HeldObjectArrived)
	bool holdClosed = false;     // the weapon hand's hold
	bool pullGrab = false;       // the grab began as a pull, not a touch
	float dt = 0.0f;
};

struct WeaponPullVerdict {
	bool forceGrabUp = false;  // the grab key up this frame, so the engine lets go
	UInt32 takeNow = 0;        // the reference to take and equip this frame
	UInt32 form = 0;
	SInt32 type = -1;
	bool gaveUp = false;
	bool cancelled = false;
};

inline constexpr float kWeaponPullReleaseSeconds = 1.0f;

inline WeaponPullVerdict StepWeaponPull(WeaponPullState& s, const WeaponPullInput& in) {
	WeaponPullVerdict v;
	if (s.phase == WeaponPullPhase::Idle) {
		if (in.allowed && in.heldRef != 0 && in.heldIsWeapon && in.weaponHandHolds && in.arrived &&
		    in.holdClosed && in.pullGrab) {
			s.phase = WeaponPullPhase::Releasing;
			s.ref = in.heldRef;
			s.form = in.heldForm;
			s.type = in.heldType;
			s.wait = kWeaponPullReleaseSeconds;
			v.forceGrabUp = true;
		}
		return v;
	}
	if (!in.allowed || (in.heldRef != 0 && in.heldRef != s.ref)) {
		s = WeaponPullState{};
		v.cancelled = true;
		return v;
	}
	if (in.heldRef == 0) {
		v.takeNow = s.ref;
		v.form = s.form;
		v.type = s.type;
		s = WeaponPullState{};
		return v;
	}
	v.forceGrabUp = true;
	s.wait -= in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (s.wait <= 0.0f) {
		v.gaveUp = true;
		s = WeaponPullState{};
	}
	return v;
}

// The take's steps after the activation: the equip sent once, then the
// wait for the game to show the weapon equipped - sheathed, and the
// ready-weapon click draws it; drawn already, nothing more to do.
//
// Straight into the hand (`atOnce`, [Hands] TakeDrawsAtOnce; Nadi, Windows,
// 2026-10-24: "eine Splitsekunde Pause zwischen Waffe schwebt an und wird
// ausgerüstet"): the draw kept a taken weapon out of sight seven frames, 93
// ms at 75 Hz (weapon-pull and weapon-grab, 2026-10-25 12:13 and 12:14) -
// equipped to the sheath, then the Equip group, and on the hand only at its
// Attach key (onehandequip.kf 0.300 of 0.633 s). The game puts a weapon
// equipped while its weapon is out on the hand at once, without the group
// (weapon-equip-drawn-probe.txt, 2026-10-25 12:20: the fists out, the sword
// equipped, on the hand in the same frame, no Equip group played). So in the
// take's frame the weapon on the hip comes off first, the weapon-out flag is
// set (HighProcess +0x114, what the process's setter 0x0064AD20 writes), and
// the equip follows. The flag set by hand is not the fists' stance: the
// engine then plays the Equip group itself, but at once - 5 frames instead
// of 7 (weapon-pull 12:26), and 3 with the draw moved on to its Attach key
// (game/WeaponDrawSpeed.h JumpNextDrawToAttach; weapon-pull 12:31). A weapon shown sheathed after all is drawn by the click
// as before; one shown out but not on the hand within kTakeAtOnceWaitSeconds
// is sheathed by a click and drawn by the next. A frame counts toward that
// wait at most kTakeAtOnceMaxStepSeconds: the frame after the inventory
// closes is long (inventory-grab-weapon, 2026-10-30: its step 0.250 s), and
// counted whole it used the wait up before the weapon could reach the hand.
enum class WeaponTakePhase : UInt8 { Idle, Equipping };

inline constexpr float kTakeAtOnceWaitSeconds = 0.25f;
inline constexpr float kTakeAtOnceMaxStepSeconds = 0.05f;

struct WeaponTakeState {
	WeaponTakePhase phase = WeaponTakePhase::Idle;
	UInt32 form = 0;  // the weapon's base form, read before the take (the reference goes)
	vr::EquippedKind kind = vr::EquippedKind::Nothing;
	bool equipSent = false;
	float wait = 0.0f;
	bool atOnce = false;       // straight into the hand, while it holds
	float offHand = 0.0f;      // atOnce: how long it has shown out but not on the hand
	bool sheathing = false;    // the fallback's click sent: wait for the sheath, then draw
};

struct WeaponTakeInput {
	bool allowed = false;  // in the world, no menu: a menu opening cancels the take
	vr::EquippedKind equipped = vr::EquippedKind::Nothing;
	vr::WeaponSeen seen = vr::WeaponSeen::Unknown;
	bool onHand = false;   // the weapon hangs on its hand's bone (game::HandHoldsItem)
	float dt = 0.0f;
};

struct WeaponTakeVerdict {
	bool unequipFirst = false;  // atOnce: the weapon equipped now off first, this frame
	bool weaponOut = false;     // atOnce: the weapon-out flag set before the equip, this frame
	bool fellBack = false;      // atOnce did not hold: the draw by the click instead
	bool equipNow = false;   // send EquipItem for `form` this frame
	UInt32 form = 0;
	bool readyClick = false; // the weapon shows sheathed: the ready click draws it
	bool done = false;       // the weapon is equipped (and drawn, or being drawn by the click)
	bool gaveUp = false;     // the weapon never showed as equipped in time
	bool cancelled = false;  // a menu or the like interrupted the take
};

inline void BeginWeaponTake(WeaponTakeState& s, UInt32 form, vr::EquippedKind kind, float waitSeconds,
                            bool atOnce = false) {
	s = WeaponTakeState{};
	s.phase = WeaponTakePhase::Equipping;
	s.form = form;
	s.kind = kind;
	s.wait = waitSeconds == waitSeconds && waitSeconds > 0.0f ? waitSeconds : 2.0f;
	s.atOnce = atOnce;
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
		if (s.atOnce) {
			v.unequipFirst = in.equipped != vr::EquippedKind::Nothing;
			v.weaponOut = true;
		}
		return v;
	}
	const float dt = in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	s.wait -= dt;
	if (in.equipped == s.kind && s.atOnce && in.seen == vr::WeaponSeen::Drawn) {
		if (in.onHand) {
			v.done = true;
			s = WeaponTakeState{};
			return v;
		}
		s.offHand += dt < kTakeAtOnceMaxStepSeconds ? dt : kTakeAtOnceMaxStepSeconds;
		if (s.offHand >= kTakeAtOnceWaitSeconds - 1e-4f) {
			// Out without the weapon on the hand: sheathed by the click, and
			// drawn by the next once it shows sheathed.
			v.readyClick = true;
			v.fellBack = true;
			s.atOnce = false;
			s.sheathing = true;
		}
		return v;
	}
	if (in.equipped == s.kind && s.sheathing && in.seen == vr::WeaponSeen::Drawn) {
		return v;  // the fallback's sheathe still running
	}
	if (in.equipped == s.kind && in.seen != vr::WeaponSeen::Unknown) {
		v.readyClick = in.seen == vr::WeaponSeen::Sheathed;
		v.fellBack = s.atOnce && v.readyClick;
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

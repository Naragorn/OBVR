#pragma once

// The two hands' sides (free-weapons-spec 5.16, step 3): which hand holds the
// weapon apart from which side the controls are on, and what the hand mode
// starts fresh when the weapon goes to the other hand.
//
// Pure, covered by hand_mode_test.

#include "core/Types.h"
#include "vr/HandInput.h"
#include "vr/HeadOffset.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// A hand's own buttons, which move with the hand: the grip and the trigger.
inline constexpr UInt64 kHandOwnButtons = (1ull << openvr::kButtonIndexGrip) | (1ull << openvr::kButtonTrigger);

// The controls of two hands swapped: their sticks and their other buttons.
inline void SwapControls(HandPose& a, HandPose& b) {
	const UInt64 aControls = a.buttonsPressed & ~kHandOwnButtons;
	const UInt64 bControls = b.buttonsPressed & ~kHandOwnButtons;
	a.buttonsPressed = (a.buttonsPressed & kHandOwnButtons) | bControls;
	b.buttonsPressed = (b.buttonsPressed & kHandOwnButtons) | aControls;
	const float ax = a.thumbX;
	const float ay = a.thumbY;
	const bool aJoystick = a.thumbFromJoystickAxis;
	a.thumbX = b.thumbX;
	a.thumbY = b.thumbY;
	a.thumbFromJoystickAxis = b.thumbFromJoystickAxis;
	b.thumbX = ax;
	b.thumbY = ay;
	b.thumbFromJoystickAxis = aJoystick;
}

// Which hand holds the weapon: [Debug] WeaponSide - 0 Auto (the side in play,
// `playLeft`: WeaponSideNow, stepped from the left-handed choice and the
// handovers), 1 the right hand, 2 the left - a lever for the harness.
inline bool ThingsLeftFor(UInt32 weaponSide, bool playLeft) {
	return weaponSide == 1 ? false : (weaponSide == 2 ? true : playLeft);
}

// The things' side apart from the controls' side (free-weapons-spec 5.16,
// step 3): `thingsLeft` - the weapon in the left hand - swaps the roles as
// vr::AssignHandRoles; then, when the controls' side (`controlsLeft`, the left-handed
// choice) differs, the controls go back to it. A hand's own input moves with
// the hand: its pose, its trigger, its grip and its fingers act on what it
// holds. The controls do not: the sticks and the buttons but the grip and
// the trigger stay where the left-handed choice puts them. True when the
// roles (the poses) swapped.
inline bool AssignHandRoles(HandPose& right, HandPose& left, bool thingsLeft, bool controlsLeft) {
	AssignHandRoles(right, left, thingsLeft);
	if (thingsLeft != controlsLeft) {
		SwapControls(right, left);
	}
	return thingsLeft;
}

// The weapon's side in play (free-weapons-spec 5.16, step 4): the left-handed
// choice's at first and whenever the choice changes; a handover asks for a
// flip (`flip`), taken at the next frame's roles so the frame stays one side.
struct WeaponSideState {
	bool seen = false;
	bool leftHandedWas = false;
	bool left = false;
	bool flip = false;
};

inline bool StepWeaponSide(WeaponSideState& s, bool leftHanded) {
	if (!s.seen || leftHanded != s.leftHandedWas) {
		s.left = leftHanded;
	}
	s.seen = true;
	s.leftHandedWas = leftHanded;
	if (s.flip) {
		s.left = !s.left;
		s.flip = false;
	}
	return s.left;
}

// The co-save's record of it ('OBWS', game/QuickKeyPages.cpp): 1 the right
// hand, 2 the left. Read back under the choice of the load; no record (an
// older save) leaves the choice's side.
inline UInt32 WeaponSideRecordValue(const WeaponSideState& s) { return s.left ? 2u : 1u; }

inline bool WeaponSideOfRecordValue(UInt32 value, bool& left) {
	if (value != 1 && value != 2) {
		return false;
	}
	left = value == 2;
	return true;
}

inline void RestoreWeaponSide(WeaponSideState& s, bool found, bool savedLeft, bool leftHanded) {
	s = WeaponSideState{};
	if (found) {
		s.seen = true;
		s.leftHandedWas = leftHanded;
		s.left = savedLeft;
	}
}

// The one the game plays with (game/QuickKeyPages.cpp saves and loads it,
// camera/CameraHook.cpp steps it, a handover flips it).
inline WeaponSideState& WeaponSideNow() {
	static WeaponSideState s;
	return s;
}

// Where the left hand's swing serials start, apart from the right's.
inline constexpr UInt32 kLeftSwingSerialBase = 0x80000000u;

// A hand's place from the head, metres in the game's axes (what every gesture
// reads); nothing for a hand not tracked.
inline NiPoint3 HandFromHead(const Quaternion& head, const NiPoint3& headPosition, const HandPose& hand) {
	return hand.valid ? OffsetFromPose(head, hand.position, headPosition, 1.0f) : NiPoint3{0.0f, 0.0f, 0.0f};
}

// The roles' swap seen last frame (HandMode::StepRolesSwitch).
struct RolesSwitchState {
	bool seen = false;
	bool swappedWas = false;
};

// Whether the roles switched since the last frame; the state carried on.
inline bool StepRolesSeen(RolesSwitchState& s, bool rolesSwapped) {
	const bool switched = s.seen && rolesSwapped != s.swappedWas;
	s.seen = true;
	s.swappedWas = rolesSwapped;
	return switched;
}

}  // namespace obvr::vr

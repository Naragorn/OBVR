#pragma once

// A pull to the hand under the grip toggle ([Hands] GripToggle; Nadi, Windows
// 2026-11-27: "some of them shot out past my hand, so they were not equipped
// but shot on" - free-weapons-spec, "Windows run of 2026-11-27").
//
// The grip toggle (vr/GripToggle.h) keeps a hand's hold closed after its grip
// opens only for what the hand keeps - a drawn weapon, a bow, a torch. A pull
// of a weapon, a bow or a torch to the hand was the engine's grab on the
// grip's click: the grip opened during the flight let the grab go, the pulled
// thing was not taken, and the release left it its flight's speed (see
// GrabPhysics.h ReleaseSpeedFor). With the toggle, one grip is the whole take:
// what a pull brings to a hand is kept by that hand from the grab key's going
// down on it until it hangs there, so
// - the toggle's hold of that hand closes at the pull and stays closed
//   (PullHandsOf joins vr::GripKeepsOf), the take on arrival reads it;
// - the grab stays on while that hold is closed (PullKeepsGrab), though the
//   grip is open; the next grip opens the hold and gives the pull up.
// Off, nothing changes: the pull is the grip's, as before.
//
// Pure, covered by pull_hold_test; the hand mode reads the hands for the
// toggle (vr::GripToggleFrameOf), the frame keeps the grab
// (camera/hands/GrabKeep.cpp).

namespace obvr::game {

// The last grab step's pull, as the grab's state has it (camera/hands/Grab.h).
struct PullHoldInput {
	bool keyDown = false;         // the grab key was down (GrabState::keyWas)
	bool weaponPullGrab = false;  // that grab began on a weapon or a bow for its hand (weaponPullGrab)
	bool weaponGrabLeft = false;  // ... with the other hand: a bow (weaponHoldLeft)
	bool torchPullGrab = false;   // that grab began on a torch for the other hand (torchPullGrab)
	bool weaponArrived = false;   // a pulled weapon arrived, the engine's grab being let go (WeaponPullPhase)
	bool torchArrived = false;    // the same for a torch
	bool weaponTaking = false;    // a weapon's take until it hangs on the hand (WeaponTakePhase not Idle)
	bool weaponTakingBow = false; // ... a bow's: the other hand's
	bool torchTaking = false;     // a torch's take (TorchTakePhase not Idle)
};

struct PullHands {
	bool right = false;  // the weapon hand
	bool left = false;   // the other hand
};

// The hands a pull keeps: the grab's hand while the key is down on a pull,
// then through the arrival and the take until the thing hangs on the hand.
inline PullHands PullHandsOf(const PullHoldInput& in) {
	PullHands h;
	const bool weaponGrab = in.keyDown && in.weaponPullGrab;
	const bool weaponHeld = weaponGrab || in.weaponArrived;
	h.right = (weaponHeld && !in.weaponGrabLeft) || (in.weaponTaking && !in.weaponTakingBow);
	h.left = (weaponHeld && in.weaponGrabLeft) || (in.weaponTaking && in.weaponTakingBow) ||
	         (in.keyDown && in.torchPullGrab) || in.torchArrived || in.torchTaking;
	return h;
}

// The grab a pull keeps on: which hand's, while the key is down on a pull.
struct PullGrab {
	bool pulling = false;
	bool left = false;
};

inline PullGrab PullGrabOf(const PullHoldInput& in) {
	PullGrab g;
	if (in.keyDown && in.weaponPullGrab) {
		g.pulling = true;
		g.left = in.weaponGrabLeft;
	} else if (in.keyDown && in.torchPullGrab) {
		g.pulling = true;
		g.left = true;
	}
	return g;
}

// Whether the grab stays on with the grip open: the toggle on, a pull's grab
// under way, and the pulling hand's hold - the toggle's - still closed.
inline bool PullKeepsGrab(bool toggleOn, const PullGrab& grab, bool rightHold, bool leftHold) {
	return toggleOn && grab.pulling && (grab.left ? leftHold : rightHold);
}

}  // namespace obvr::game

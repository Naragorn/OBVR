#include "camera/hands/GrabKeep.h"

#include "core/Log.h"
#include "game/TakeItem.h"

namespace obvr::camera::hands {

game::PullHoldInput PullHoldOf(const GrabState& grab) {
	game::PullHoldInput in;
	in.keyDown = grab.keyWas;
	in.weaponPullGrab = grab.weaponPullGrab;
	in.weaponGrabLeft = grab.weaponHoldLeft;
	in.torchPullGrab = grab.torchPullGrab;
	in.weaponArrived = grab.weaponPull.phase != game::WeaponPullPhase::Idle;
	in.torchArrived = grab.torchPull.phase != game::WeaponPullPhase::Idle;
	in.weaponTaking = grab.weaponTake.phase != game::WeaponTakePhase::Idle;
	in.weaponTakingBow = grab.weaponTake.kind == vr::EquippedKind::Bow;
	in.torchTaking = grab.torchTake.phase != game::TorchTakePhase::Idle;
	return in;
}

void NoteGrabHands(vr::HandModeFrame& frame, const InventoryHold& inventoryHold, const GrabState& grab, bool active,
                   bool toggleOn) {
	const game::PullHoldInput in = PullHoldOf(grab);
	const game::PullHands pull = game::PullHandsOf(in);
	frame.pullRight = active && pull.right;
	frame.pullLeft = active && pull.left;
	// The grab's line from the pulling hand while the toggle keeps its grab, the grip open.
	const game::PullGrab grabbing = game::PullGrabOf(in);
	frame.grabHand = inventoryHold.ref != 0          ? (inventoryHold.left ? 1 : 0)
	                 : toggleOn && grabbing.pulling ? (grabbing.left ? 1 : 0)
	                                                : -1;
}

void KeepGrabs(InventoryHold& inventoryHold, const GrabState& grab, vr::HandModeResult& hand, bool active,
               bool menuIsUp, float dt, bool toggleOn) {
	if (inventoryHold.ref != 0) {
		const bool hold = inventoryHold.left ? hand.leftHold : hand.rightHold;
		inventoryHold.seconds += dt == dt && dt > 0.0f ? dt : 0.0f;
		if (!hold || !active || menuIsUp) {
			OBVR_LOG("Inventory grab: the %s hand opened after %.2f s - %s", inventoryHold.left ? "other" : "weapon",
			         static_cast<double>(inventoryHold.seconds),
			         inventoryHold.grabbedSaid ? "let go as any held thing" : "it was never held");
			inventoryHold = InventoryHold{};
		} else {
			hand.grabWanted = true;
			hand.grabWithLeftHand = inventoryHold.left;
			if (!inventoryHold.grabbedSaid && game::GrabbedRef() == inventoryHold.ref) {
				inventoryHold.grabbedSaid = true;
				OBVR_LOG("Inventory grab: %08X in the %s hand's grab after %.2f s", inventoryHold.ref,
				         inventoryHold.left ? "other" : "weapon", static_cast<double>(inventoryHold.seconds));
			}
		}
		return;
	}
	const game::PullGrab pull = game::PullGrabOf(PullHoldOf(grab));
	if (active && !menuIsUp && game::PullKeepsGrab(toggleOn, pull, hand.rightHold, hand.leftHold)) {
		hand.grabWanted = true;
		hand.grabWithLeftHand = pull.left;
	}
}

}  // namespace obvr::camera::hands

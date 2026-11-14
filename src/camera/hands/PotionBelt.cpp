#include "camera/hands/PotionBelt.h"

#include "core/Log.h"
#include "game/HolsterSlots.h"
#include "game/MeleeHits.h"
#include "game/TakeItem.h"
#include "vr/PotionBelt.h"

namespace obvr::camera::hands {

namespace {

// One hand's reach this frame: its potion dropped into it and held, or the
// place emptied when none is left in the pack.
void ReachWith(PotionBeltState& state, const HandShared& shared, bool left, bool closedNow, bool allowed,
               bool busy, InventoryHold& hold) {
	const vr::HandModeResult& hand = shared.hand;
	const vr::HolsterVerdict& h = hand.holster;
	vr::PotionReachInput in;
	in.allowed = allowed;
	in.closedNow = closedNow;
	in.slot = left ? h.leftSlot : h.rightSlot;
	in.handBusy = busy || (h.gesture == in.slot && h.gestureLeft == left && vr::HolsterTookTheReach(h.outcome));
	UInt32 (&potions)[vr::kHolsterSlotCount] = game::PotionPlaceIds();
	const UInt32 id = vr::PotionReachAt(in, potions);
	if (id == 0) {
		return;
	}
	const NiPoint3 at =
		shared.cyclopeanCamera.pos + shared.cyclopeanCamera.rot * (left ? hand.leftHandOffsetUnits : hand.rightHandOffsetUnits);
	UInt8* const form = game::HolsterSlotForm(id);
	const UInt32 ref = form != nullptr && shared.cyclopeanCameraValid ? game::DropItemForm(form, at) : 0;
	if (ref == 0) {
		potions[static_cast<UInt32>(in.slot)] = 0;
		if (state.lines > 0) {
			--state.lines;
			OBVR_LOG("Potion belt: the %s hand reached for %08X at %s - none left in the pack, the place forgets it",
			         left ? "other" : "weapon", id, vr::SlotWords(in.slot));
		}
		return;
	}
	hold = InventoryHold{};
	hold.ref = ref;
	hold.left = left;
	++state.draws;
	if (state.lines > 0) {
		--state.lines;
		OBVR_LOG("Potion belt: the %s hand drew %08X from %s - %08X dropped into it, held while it stays closed",
		         left ? "other" : "weapon", id, vr::SlotWords(in.slot), ref);
	}
}

}  // namespace

void StepPotionBelt(PotionBeltState& state, const HandStep& step, const HandShared& shared,
                    const vr::HandModeFrame& frame, InventoryHold& hold) {
	const bool rightGrip = frame.right.valid && vr::GripDown(frame.right.buttonsPressed);
	const bool leftGrip = frame.left.valid && vr::GripDown(frame.left.buttonsPressed);
	const bool rightClosed = rightGrip && !state.rightGripWas;
	const bool leftClosed = leftGrip && !state.leftGripWas;
	state.rightGripWas = rightGrip;
	state.leftGripWas = leftGrip;
	// A weapon kept at a place takes it over: its potion stays in the pack.
	UInt32 (&potions)[vr::kHolsterSlotCount] = game::PotionPlaceIds();
	if (vr::ForgetPotionsUnderItems(potions, game::HolsterSlotIds()) > 0 && state.lines > 0) {
		--state.lines;
		OBVR_LOG("Potion belt: a weapon, shield or torch now kept where a potion was - the place forgets the potion");
	}
	const bool allowed = step.active && step.config.hands.potionBelt && !step.menuIsUp && frame.inWorld;
	const bool busy = hold.ref != 0 || game::GrabbedRef() != 0;
	ReachWith(state, shared, false, rightClosed, allowed, busy, hold);
	ReachWith(state, shared, true, leftClosed, allowed, busy || hold.ref != 0, hold);
}

}  // namespace obvr::camera::hands

#include "camera/hands/Handover.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/InventoryGrab.h"
#include "game/MeleeHits.h"
#include "game/NearbyItems.h"
#include "game/TakeItem.h"

namespace obvr::camera::hands {

void StepHandover(HandoverState& state, const HandStep& step, const HandShared& shared, HandoverLinks& links) {
	const Config& config = step.config;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const float dt = step.dt;
	vr::HandModeResult& handResult = shared.hand;
	const bool rightClosedNow = handResult.rightHold && !state.rightHoldWas;
	const bool leftClosedNow = handResult.leftHold && !state.leftHoldWas;
	state.rightHoldWas = handResult.rightHold;
	state.leftHoldWas = handResult.leftHold;
	const bool usable = active && !menuIsUp;
	// The ordinary grab's thing, the other hand closing: ours from here.
	if (usable && links.inventoryHold.ref == 0 && state.handBack.ref == 0 && !links.weaponPullGrab && !links.weaponPullTaking) {
		const UInt32 grabbed = game::GrabbedRef();
		const bool grabLeft = handResult.grabWithLeftHand;
		if (grabbed != 0 && (grabLeft ? rightClosedNow : leftClosedNow)) {
			links.inventoryHold = InventoryHold{};
			links.inventoryHold.ref = grabbed;
			links.inventoryHold.left = grabLeft;
			links.inventoryHold.grabbedSaid = true;
		}
	}
	const float upm = config.tracker.unitsPerMetre;
	const float reach = config.hands.handoverReachMetres * upm;
	// The shield worn on the other hand, the weapon hand closing on it (a
	// ball of HandoverThingMetres round that hand), not at a holster
	// place: taken off and held by the weapon hand as an object. The
	// shield has no let go of its own - the arm wears it - so the closing
	// is the hand-over.
	if (usable && links.inventoryHold.ref == 0 && state.handBack.ref == 0 && rightClosedNow && game::GrabbedRef() == 0 &&
	    handResult.holster.rightSlot == vr::HolsterSlot::None && handResult.rightHandValid && handResult.leftHandValid &&
	    shared.cyclopeanCameraValid && reach > 0.0f) {
		UInt8* const shield = game::WornShieldForm();
		if (shield != nullptr) {
			const NiPoint3 leftWorld =
				shared.cyclopeanCamera.pos + shared.cyclopeanCamera.rot * handResult.leftHandOffsetUnits;
			const NiPoint3 rightWorld =
				shared.cyclopeanCamera.pos + shared.cyclopeanCamera.rot * handResult.rightHandOffsetUnits;
			const float toThing = game::NearHeldInHandUnits(math::Sqrt((rightWorld - leftWorld).LengthSquared()),
			                                                config.hands.handoverThingMetres * upm);
			if (game::HandOverAtLetGo(true, true, toThing, reach)) {
				const UInt32 ref = game::DropItemForm(shield, leftWorld);
				if (ref != 0) {
					links.inventoryHold = InventoryHold{};
					links.inventoryHold.ref = ref;
					links.inventoryHold.left = false;
				}
				OBVR_LOG("Handover: the weapon hand closed %.1f units from the worn shield - %s", static_cast<double>(toThing),
				         ref != 0 ? "taken off and held by the weapon hand" : "the drop was refused, it stays worn");
			}
		}
	}
	const auto handToRef = [&](UInt32 ref, bool left) {
		NiPoint3 centre{};
		float radius = 0.0f;
		const bool valid = left ? handResult.leftHandValid : handResult.rightHandValid;
		if (ref == 0 || !valid || !shared.cyclopeanCameraValid || !game::RefWorldBound(ref, centre, radius)) {
			return -1.0f;
		}
		const NiPoint3 hand = shared.cyclopeanCamera.pos +
		                      shared.cyclopeanCamera.rot *
		                          (left ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
		const float gap = math::Sqrt((centre - hand).LengthSquared()) - radius;
		return gap > 0.0f ? gap : 0.0f;
	};
	game::HandBackInput hb;
	hb.reachUnits = reach;
	hb.grabbedRef = game::GrabbedRef();
	hb.dt = dt;
	if (state.handBack.ref == 0 && usable && links.inventoryHold.ref != 0) {
		const bool closingLeft = !links.inventoryHold.left;
		const bool closedNow = closingLeft ? leftClosedNow : rightClosedNow;
		const float toThing = closedNow ? handToRef(links.inventoryHold.ref, closingLeft) : -1.0f;
		// reach 0: the handover off, here too (a hand exactly on the
		// thing's line measures 0).
		if (closedNow && reach > 0.0f && toThing >= 0.0f && toThing <= reach) {
			SInt32 weaponType = -1;
			const UInt32 base = game::RefBaseForm(links.inventoryHold.ref);
			const game::InventoryItemKind kind = game::InventoryItemKindOfForm(base, weaponType);
			const game::HandoverAct act = game::ActOnClosing(game::ChooseInventoryTake(
				kind, closingLeft ? game::InventoryHand::Left : game::InventoryHand::Right));
			if (act == game::HandoverAct::Pass) {
				links.inventoryHold.left = closingLeft;
				links.inventoryHold.seconds = 0.0f;
				OBVR_LOG("Handover: %08X passed to the %s hand (it closed %.1f units from it) - held there", links.inventoryHold.ref,
				         closingLeft ? "other" : "weapon", static_cast<double>(toThing));
			} else if (act == game::HandoverAct::EquipBack) {
				hb.heldRef = links.inventoryHold.ref;
				hb.wearHandClosedNow = true;
				hb.wearHandToThingUnits = toThing;
				state.handBackLeft = closingLeft;
				state.handBackKind = kind;
			}
		}
	}
	if (hb.heldRef != 0 || state.handBack.ref != 0) {
		const game::HandBackVerdict hv = game::StepHandBack(state.handBack, hb);
		links.handBackTaking = state.handBack.ref != 0 || hv.takeRef != 0;
		// The taking hand's own grab kept off it too, till it is taken: a
		// grab started on it makes the activation refuse it (the shield,
		// handover-shield.txt 2026-10-28 12:01: "refused by the game").
		if (hv.releaseHold || hv.takeRef != 0) {
			handResult.grabWanted = false;
			handResult.controls.grab = false;
		}
		if (hv.releaseHold) {
			if (links.inventoryHold.ref != 0) {
				OBVR_LOG("Handover: the %s hand closed %.1f units from %08X - the other hand lets go of it, the %s "
				         "hand takes it",
				         state.handBackLeft ? "other" : "weapon", static_cast<double>(hb.wearHandToThingUnits),
				         links.inventoryHold.ref, state.handBackLeft ? "other" : "weapon");
			}
			links.inventoryHold = InventoryHold{};
			handResult.grabWanted = false;
			handResult.controls.grab = false;
		}
		if (hv.takeRef != 0) {
			const UInt32 base = game::RefBaseForm(hv.takeRef);
			const SInt32 type = game::RefWeaponType(hv.takeRef);
			UInt32 owner = 0;
			links.forgetTaken(hv.takeRef);
			const game::TakeResult taken = game::TakeIntoInventoryQuietly(hv.takeRef, &owner);
			bool equip = false;
			if (taken == game::TakeResult::Taken && base != 0) {
				if (state.handBackKind == game::InventoryItemKind::Weapon || state.handBackKind == game::InventoryItemKind::Bow) {
					if (type >= 0 && links.weaponTake.phase == game::WeaponTakePhase::Idle) {
						const game::WeaponGrabSettings& wg = config.hands.weaponGrab;
						game::BeginWeaponTake(links.weaponTake, base, vr::KindOfWeaponType(type), wg.waitSeconds,
						                      wg.drawAtOnce);
						game::LatchWeaponHold(links.weaponHoldLatch);
						equip = true;
					}
				} else {
					equip = game::EquipItemForm(reinterpret_cast<UInt8*>(base), false);
				}
			}
			OBVR_LOG("Handover: %08X %s by the %s hand (%s; the engine's grab on %08X)%s", hv.takeRef,
			         taken == game::TakeResult::Taken ? "taken back" : "NOT taken back",
			         state.handBackLeft ? "other" : "weapon", game::TakeResultName(taken), game::GrabbedRef(),
			         equip ? " - equipping" : " - not equipped");
		}
		if (hv.gaveUp) {
			OBVR_LOG("Handover: the engine's grab kept it a second - not taken back");
		}
	}
}

}  // namespace obvr::camera::hands

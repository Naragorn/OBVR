#include "camera/hands/Doors.h"

#include "core/Log.h"
#include "game/AimAtSource.h"
#include "game/PlayerAim.h"

namespace obvr::camera::hands {

namespace {

// The grip took the handle: a short, light tick in that hand.
void TickHandle(vr::OpenVRBackend& backend, bool left) { backend.Pulse(!left, 0.02f, 160.0f, 0.35f); }

// A hand script's mark: the doors' state, and the box of the door nearest the
// right hand, with that hand in its frame (for placing a scenario's hand at a handle).
void LogDoorMark(OpenByHandState& state, const HandShared& shared, const vr::DoorGripFrame& f,
                 const game::DoorAtHand& at, const game::ReachHands& tracked, float perMetre) {
	OBVR_LOG("Door: state - %s, phase %u door %08X (%s hand, start %.3f m, now %.3f m, known %d); at a handle "
	         "%08X (%s hand, %.3f m off the middle plane, item first %d); free hands right %d left %d, grips %d/%d",
	         f.enabled ? "on" : "off", static_cast<unsigned>(state.door.phase), state.door.ref,
	         state.door.left ? "left" : "right", static_cast<double>(state.door.startNormalMetres),
	         static_cast<double>(f.heldNormalMetres), f.heldKnown ? 1 : 0, at.ref, at.left ? "left" : "right",
	         static_cast<double>(at.normalMetres), f.itemFirst ? 1 : 0, f.handFree[0] ? 1 : 0,
	         f.handFree[1] ? 1 : 0, f.gripDown[0] ? 1 : 0, f.gripDown[1] ? 1 : 0);
	if (shared.cyclopeanCameraValid) {
		UInt32 nearest = 0;
		NiPoint3 local{};
		vr::DoorBoxMetres box;
		if (game::NearestDoorView(tracked.position[0], 3.0f * perMetre, tracked.position[0], perMetre,
		                          state.doorBoxes, nearest, local, box)) {
			OBVR_LOG("Door: state - the nearest door %08X, its box %.2f..%.2f x %.2f..%.2f x %.2f..%.2f m (thin "
			         "axis %d); the right hand in its frame at %.2f %.2f %.2f m",
			         nearest, static_cast<double>(box.low.x), static_cast<double>(box.high.x),
			         static_cast<double>(box.low.y), static_cast<double>(box.high.y), static_cast<double>(box.low.z),
			         static_cast<double>(box.high.z), vr::DoorThinAxis(box), static_cast<double>(local.x),
			         static_cast<double>(local.y), static_cast<double>(local.z));
		} else {
			OBVR_LOG("Door: state - no door within 3 m of the right hand (or its model did not read as a box)");
		}
	}
}

}  // namespace

void StepDoorGrip(OpenByHandState& state, const HandStep& step, const HandShared& shared, const DoorLinks& links) {
	const vr::DoorGripSettings& settings = step.config.hands.doorGrip;
	const float perMetre = step.config.tracker.unitsPerMetre;
	const bool cameraKnown = shared.cyclopeanCameraValid;
	const vr::HandModeResult& hand = shared.hand;

	vr::DoorGripFrame f;
	f.enabled = settings.enabled && step.active && links.handsInWorld && cameraKnown;
	f.menuUp = step.menuIsUp;
	f.gripDown[0] = hand.rightGripDown;
	f.gripDown[1] = hand.leftGripDown;
	game::ReachHands tracked;
	if (cameraKnown) {
		const bool weaponReadied = game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
		const bool holding = game::PlayerHoldsGrab();
		for (int side = 0; side < 2; ++side) {
			const bool left = side == 1;
			const bool valid = left ? hand.leftHandValid : hand.rightHandValid;
			tracked.position[side] = shared.cyclopeanCamera.pos + shared.cyclopeanCamera.rot * links.controllersAt[side];
			tracked.valid[side] = valid;
			f.handFree[side] = vr::HandFreeForDoor(valid, holding && hand.grabWithLeftHand == left, weaponReadied);
		}
	}

	// A door at a free hand's handle: only looked for while none is held, and
	// for the free hands alone.
	game::DoorAtHand at;
	if (f.enabled && !f.menuUp && state.door.phase == vr::DoorGripPhase::Idle) {
		game::ReachHands free = tracked;
		free.valid[0] = f.handFree[0];
		free.valid[1] = f.handFree[1];
		at = game::FindDoorAtHandle(free, perMetre, settings, state.doorBoxes);
	}
	state.atHandle = at.ref != 0;
	state.atHandleLeft = at.left;
	if (at.ref != 0) {
		f.candidate = at.ref;
		f.candidateLeft = at.left;
		f.candidateNormalMetres = at.normalMetres;
		// An item under the same hand goes first (the grip would take it).
		f.itemFirst = links.nearItem.valid && links.nearItem.left == at.left;
	}
	// The held door: the holding hand's coordinate along its normal now.
	if (state.door.phase == vr::DoorGripPhase::Holding) {
		if (!game::RefStillThere(state.doorPrint)) {
			f.heldGone = true;
		} else {
			const int side = state.door.left ? 1 : 0;
			bool gone = false;
			f.heldKnown = tracked.valid[side] &&
			              game::DoorHandNormal(state.door.ref, tracked.position[side], perMetre, state.doorBoxes,
			                                   f.heldNormalMetres, gone);
			f.heldGone = gone;
		}
	}

	if (step.scriptMarked) {
		LogDoorMark(state, shared, f, at, tracked, perMetre);
	}
	const float startBefore = state.door.startNormalMetres;
	const vr::DoorGripVerdict v = vr::StepDoorGrip(state.door, settings, f);
	if (v.action == vr::DoorGripAction::Take) {
		state.doorPrint = game::TakeRefFingerprint(v.ref);
		TickHandle(step.backend, v.left);
	} else if (v.action == vr::DoorGripAction::Open) {
		// As A opens it (game::ActivateByPlayer): a load door loads, a locked
		// one gets the engine's own lock handling. Not one gone since.
		if (game::RefStillThere(state.doorPrint)) {
			game::ActivateByPlayer(v.ref);
			++state.opens;
		}
	}
	if (v.action != vr::DoorGripAction::None && state.doorLines > 0) {
		--state.doorLines;
		OBVR_LOG("Door: %08X %s by the %s hand (moved %.3f m along its normal, the threshold %.2f m%s%s)", v.ref,
		         v.action == vr::DoorGripAction::Take   ? "taken at the handle"
		         : v.action == vr::DoorGripAction::Open ? "opened by the grip, as A"
		                                                : "let go before it opened",
		         v.left ? "left" : "right", static_cast<double>(f.heldNormalMetres - startBefore),
		         static_cast<double>(settings.moveMetres), f.heldGone ? ", gone" : "",
		         game::RefIsLocked(v.ref) && v.action == vr::DoorGripAction::Open ? ", locked" : "");
	}
}

}  // namespace obvr::camera::hands

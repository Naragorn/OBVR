#include "camera/hands/HandoverHold.h"

#include "core/Log.h"
#include "game/HolsterSlots.h"
#include "vr/HandoverHoldSave.h"

namespace obvr::camera::hands {

void EndHoldsAtSwitch(TwoHandState& s, bool mirrored) {
	if (!vr::StepRolesSeen(s.mirrorSeen, mirrored)) {
		return;
	}
	if (vr::SwitchIsHandover(true, vr::HandoverHoldNow().restore)) {
		// The hand that took the weapon keeps it where it held it
		// (vr::HandoverHoldAxial), read from the frame before the switch.
		// The places were read from the palm as it held the weapon then, itself
		// moved by an earlier handover: added to that.
		const float along = vr::HandoverHoldAxial(s.drawnFormWas != nullptr, s.twoHand.active, s.twoHand.handAxial,
		                                          s.otherClosedWas, s.otherAxialWas, s.handleWas);
		s.handoverAxial = s.drawnFormWas != nullptr ? s.handoverAxial + along : 0.0f;
		s.handoverForm = s.handoverAxial != 0.0f ? s.drawnFormWas : nullptr;
		s.handoverMeasured = s.handoverAxial == 0.0f;
		s.handoverTraceFrames = s.handoverAxial != 0.0f ? 6 : 0;
		if (s.drawnFormWas != nullptr) {
			OBVR_LOG(
				"Hands: the weapon handed over - the hand that took it holds it %.1f units %s the old hand's palm%s",
				static_cast<double>(s.handoverAxial < 0.0f ? -s.handoverAxial : s.handoverAxial),
				s.handoverAxial < 0.0f ? "below" : "above",
				s.handoverAxial != 0.0f ? ", where it held it" : " (at the grip)");
		}
	} else {
		OBVR_LOG("Hands: the sides switched with the load - no handover, the hold is the co-save's");
	}
	if (s.twoHand.active) {
		OBVR_LOG("Hands: the sides switched - the two-hand hold ends, the other hand let go");
	}
	s.twoHand = vr::TwoHandState{};
	s.twoHandBlend = 0.0f;
	s.twoHandPreshape = 0.0f;
	s.rightHold = vr::RightHoldState{};
	s.rightHoldPending = vr::RightHoldEvent::None;
}

void RestoreHoldAfterLoad(TwoHandState& s, const TwoHandEnv& env, const UInt8* weaponForm,
                          game::WeaponState weaponState) {
	// The weapon kept drawn: held where it was saved; else at the grip. Either
	// way the hold the frames before the load had ends here (vr::StepHoldRestore).
	vr::HoldRestore& restore = vr::HandoverHoldNow().restore;
	const UInt32 keptId = restore.formId;
	const float keptAxial = restore.axial;
	const bool inWorld =
		env.hand.rightHandValid && env.hand.leftHandValid && weaponState != game::WeaponState::Unknown;
	const bool out = weaponState == game::WeaponState::Drawn;
	const vr::HoldRestoreStep step =
		vr::StepHoldRestore(restore, inWorld, out, out ? game::FormIdOf(weaponForm) : 0);
	if (step == vr::HoldRestoreStep::None || step == vr::HoldRestoreStep::Wait) {
		return;
	}
	const bool apply = step == vr::HoldRestoreStep::Apply;
	if (apply) {
		OBVR_LOG("Hands: after the load the weapon is held where it was saved - %.1f units %s its grip (%08X)",
		         static_cast<double>(keptAxial < 0.0f ? -keptAxial : keptAxial), keptAxial < 0.0f ? "below" : "above",
		         keptId);
	} else if (keptId != 0) {
		OBVR_LOG("Hands: after the load the weapon the co-save kept a hold for (%08X) is not the one drawn - the hand "
		         "at the grip",
		         keptId);
	} else if (s.handoverAxial != 0.0f) {
		OBVR_LOG("Hands: after the load no hold from the co-save - the one from before the load ends, the hand at "
		         "the grip");
	}
	s.handoverAxial = apply ? keptAxial : 0.0f;
	s.handoverForm = apply ? weaponForm : nullptr;
	// Where the palm is along the weapon, said once as after a switch
	// (SayHandoverHold); no pose written for a hold before the load is taken
	// for the engine's.
	s.handoverMeasured = !apply;
	s.handoverSettleFrames = 0;
	s.handoverTraceFrames = 0;
	s.handoverWrittenValid = false;
}

const UInt8* EndHandoverHoldWithWeapon(TwoHandState& s, const UInt8* weaponForm) {
	const UInt8* const drawnForm =
		weaponForm != nullptr && game::ReadPlayerWeaponState() == game::WeaponState::Drawn ? weaponForm : nullptr;
	if (s.handoverAxial == 0.0f || drawnForm == s.handoverForm) {
		return drawnForm;
	}
	OBVR_LOG("Hands: the weapon handed over is %s - the hand holds the next one at its grip",
	         drawnForm == nullptr ? "no longer drawn" : "another one");
	s.handoverAxial = 0.0f;
	s.handoverForm = nullptr;
	return drawnForm;
}

void NoteHoldForSave(const TwoHandState& s, const UInt8* drawnForm) {
	vr::HandoverHoldNow().now =
		vr::HoldRecordOf(game::FormIdOf(drawnForm), game::FormIdOf(s.handoverForm), s.handoverAxial);
}

void NoteFrameForSwitch(TwoHandState& s, const UInt8* drawnForm, bool otherClosed, float otherAxial,
                        const vr::HandleSpan& handle) {
	s.drawnFormWas = drawnForm;
	s.otherClosedWas = otherClosed;
	s.otherAxialWas = otherAxial;
	s.handleWas = handle;
}

}  // namespace obvr::camera::hands

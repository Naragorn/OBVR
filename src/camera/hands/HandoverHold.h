#pragma once

// The weapon handed to the other hand (free-weapons-spec 5.16), the two-hand
// step's part of it: the holds that end at a switch of the sides, where the
// hand that took the weapon holds it along it (TwoHandState::handoverAxial),
// that hold's end with its weapon, and its way through a save and a load
// (vr/HandoverHoldSave.h, game/QuickKeyPages.cpp 'OBWH'). StepTwoHands calls
// them in this order each frame: EndHoldsAtSwitch first, then
// RestoreHoldAfterLoad, EndHandoverHoldWithWeapon and NoteHoldForSave, and
// NoteFrameForSwitch at its end.
//
// Moved out of camera/hands/TwoHands.cpp on 2026-11-25, when the co-save's
// hold took it past the size table's 1000 lines (tests/function-sizes.txt).
// Harness: handover-two-hander.txt, handover-staff.txt, handover-cosave.txt,
// handover-cosave-hold.txt.

#include "camera/hands/TwoHands.h"
#include "game/PlayerAim.h"

namespace obvr::camera::hands {

// The sides switched: the hand that held the handle holds the weapon now, the
// other hand let go - the two-hand hold and the right hand's half-sword hold
// end at once. A switch while a load's hold waits is the side read from the
// co-save, no handover: the hold is the load's to give.
void EndHoldsAtSwitch(TwoHandState& s, bool mirrored);

// After a load: the hold along the weapon the co-save kept, taken at the first
// frame the hands are there and the weapon's state is read.
void RestoreHoldAfterLoad(TwoHandState& s, const TwoHandEnv& env, const UInt8* weaponForm,
                          game::WeaponState weaponState);

// The hold along the weapon from a handover ends with that weapon: put away,
// dropped or another drawn. Answers the drawn weapon's form (none while it is
// not drawn).
const UInt8* EndHandoverHoldWithWeapon(TwoHandState& s, const UInt8* weaponForm);

// The hold as a save would keep it (vr::HandoverHoldNow().now).
void NoteHoldForSave(const TwoHandState& s, const UInt8* drawnForm);

// What the next frame's switch reads of this one (EndHoldsAtSwitch).
void NoteFrameForSwitch(TwoHandState& s, const UInt8* drawnForm, bool otherClosed, float otherAxial,
                        const vr::HandleSpan& handle);

}  // namespace obvr::camera::hands

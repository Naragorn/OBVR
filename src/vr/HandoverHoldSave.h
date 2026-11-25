#pragma once

// The hold along the weapon a handover left (free-weapons-spec 5.16; camera/
// hands/TwoHands.h handoverAxial) kept in the co-save, so that after a load the
// hand holds the weapon where it held it when the game was saved, not at its
// grip. game/QuickKeyPages.cpp writes the record and reads it back beside the
// weapon's side ('OBWS'); camera/hands/TwoHands.cpp notes the hold each frame
// and takes the one read back once the world is there after the load.
//
// A load ends whatever hold the frames before it had: with no record, or one
// that keeps none, the hand is at the grip after it. A side switch seen while
// the load's hold waits is the load's (the side read from the co-save), not a
// handover.
//
// Pure, covered by handover_hold_save_test.

#include "core/Types.h"

namespace obvr::vr {

// 'OBWH': the drawn weapon's base form id (TESForm +0x0C, 0 for none) and the
// hold along it, units from where its grip has the hand (below negative).
inline constexpr UInt32 kHandoverHoldRecord = 0x4857424F;  // 'OBWH'
inline constexpr UInt32 kHandoverHoldVersion = 1;
// Longer than any weapon: a hold this far from the grip is a record gone
// wrong, not a hold.
inline constexpr float kHandoverHoldMostUnits = 1000.0f;

struct HandoverHoldRecord {
	UInt32 formId = 0;
	float axial = 0.0f;
};

// A hold worth keeping: a number, off the grip, on a weapon's length.
inline bool HoldAxialKept(float axial) {
	return axial == axial && axial != 0.0f && axial < kHandoverHoldMostUnits && axial > -kHandoverHoldMostUnits;
}

// The record of the hold now: the drawn weapon (`drawnFormId`, 0 for none
// drawn) held along it by a handover whose hold is for `holdFormId`; none for
// anything else.
inline HandoverHoldRecord HoldRecordOf(UInt32 drawnFormId, UInt32 holdFormId, float axial) {
	if (drawnFormId == 0 || drawnFormId != holdFormId || !HoldAxialKept(axial)) {
		return HandoverHoldRecord{};
	}
	return HandoverHoldRecord{drawnFormId, axial};
}

// What a load (or a new game) leaves to take once the world is there: the
// weapon the hold is for (0: none kept, the hand at the grip) and the hold.
struct HoldRestore {
	bool pending = false;
	UInt32 formId = 0;
	float axial = 0.0f;
};

// No record (an older save) or a new game: none kept - the load still ends
// the hold the frames before it had.
inline HoldRestore HoldRestoreNone() { return HoldRestore{true, 0, 0.0f}; }

// The record read back: none kept for another length or version, a hold that
// is no hold, or an id the co-save's ResolveRefID (`resolve(id, out)`) cannot
// move to this load order (the weapon's plugin gone).
template <typename Resolve>
HoldRestore HoldRestoreOfRecord(const void* data, UInt32 length, UInt32 version, Resolve resolve) {
	HoldRestore r = HoldRestoreNone();
	if (data == nullptr || length != sizeof(HandoverHoldRecord) || version != kHandoverHoldVersion) {
		return r;
	}
	const auto* const record = static_cast<const HandoverHoldRecord*>(data);
	UInt32 resolved = 0;
	if (record->formId == 0 || !HoldAxialKept(record->axial) || !resolve(record->formId, resolved) ||
	    resolved == 0) {
		return r;
	}
	r.formId = resolved;
	r.axial = record->axial;
	return r;
}

enum class HoldRestoreStep : UInt8 {
	None,   // nothing waits
	Wait,   // the world not there yet, or the drawn weapon not read yet
	Apply,  // the weapon kept is drawn: held where it was saved
	Drop,   // none kept, or another weapon or none drawn: at the grip
};

// One frame of the load's hold: `inWorld` the hands there and the player's
// weapon state read; `drawn` the weapon out; `drawnFormId` the drawn weapon's
// id (0 while none is read). Decided once, at the first such frame; a weapon
// out whose form is not read yet is waited for when a hold is kept.
inline HoldRestoreStep StepHoldRestore(HoldRestore& r, bool inWorld, bool drawn, UInt32 drawnFormId) {
	if (!r.pending) {
		return HoldRestoreStep::None;
	}
	if (!inWorld) {
		return HoldRestoreStep::Wait;
	}
	if (r.formId != 0 && drawn && drawnFormId == 0) {
		return HoldRestoreStep::Wait;
	}
	r.pending = false;
	return r.formId != 0 && drawn && drawnFormId == r.formId ? HoldRestoreStep::Apply : HoldRestoreStep::Drop;
}

// A switch of the sides seen this frame is a handover - unless a load's hold
// waits: then it is the side read from the co-save.
inline bool SwitchIsHandover(bool switched, const HoldRestore& r) { return switched && !r.pending; }

// The hold now, noted each frame (`now`), and the load's hold until it is
// taken (`restore`).
struct HandoverHoldSave {
	HandoverHoldRecord now;
	HoldRestore restore;
};

// What a save writes: a load's hold not taken yet as it was read (the frames
// before the load had another), else the hold now.
inline HandoverHoldRecord HoldRecordToSave(const HandoverHoldSave& s) {
	return s.restore.pending ? HandoverHoldRecord{s.restore.formId, s.restore.axial} : s.now;
}

// The one the game plays with (game/QuickKeyPages.cpp saves and loads it,
// camera/hands/TwoHands.cpp notes and takes it).
inline HandoverHoldSave& HandoverHoldNow() {
	static HandoverHoldSave s;
	return s;
}

}  // namespace obvr::vr

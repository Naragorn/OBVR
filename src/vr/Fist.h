#pragma once

// Fists by making a fist (docs/controls-spec.md 4.3). The weapon hand closed
// into a fist makes hand to hand active whenever no weapon is drawn. With a
// weapon in the slot but sheathed, that weapon is taken off first (the
// holster remembers it, so a reach to the hip brings it back); then the
// fists are raised. The hand opened lowers them again. While the fists are
// up, a swing strikes only with the hand closed. With a weapon drawn, a fist
// does nothing: it is the hand round the handle. Full VR only; [Hands] Fists
// switches it off.
//
// The finger curls come from SteamVR's skeletal summary (IVRInput_011
// GetSkeletalSummaryData, VRSkeletalSummaryData_t.flFingerCurl[5]: thumb,
// index, middle, ring, pinky; 0 open, 1 curled). On an Index and on Touch
// controllers they come from the touch sensing. The thumb is left out: on
// an Index it rests on the stick and the buttons whether the hand is a fist
// or not.
//
// A fist is the four fingers all past closeCurl; the hand counts as open
// again only once they are all below openCurl, so a hand between the two
// keeps what it was. Either change has to hold for holdSeconds. A finger
// whose sensor has never read is left out (vr/FingersRead.h; the tester's
// left little finger, 2026-10-12).
//
// The close limit is a row in the settings ("Fist tightness", the tester,
// 2026-09-28: a hand whose little finger reads low never made a fist). The
// open limit is kept at least kFistOpenGap below it: were it at or above,
// a hand resting between the two would count as a fist and as open by
// turns, a ready click every holdSeconds.
//
// A fist made while the hand holds something, or while its grip is the
// holster's (a reach to the hip curls the fingers too), counts for nothing.
//
// On a controller that is not an Index the fist is the trigger (FistFingersOf,
// 2026-11-25), and while it is held the grip grabs nothing
// (FistKeepsGripFromGrab).
//
// Pure, covered by fist_test.

#include "core/Types.h"
#include "vr/FingersRead.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::vr {

inline constexpr float kFistOpenGap = 0.15f;

struct FistSettings {
	bool enabled = true;
	float closeCurl = 0.90f;
	float openCurl = 0.35f;
	// [Hands] FistGripForce: a grip squeezed this hard (0..1) is a fist as
	// well - for a controller whose fingers do not read (the tester's left
	// Index, 2026-10-15: three fingers at 0, so no fist ever came). 0 off.
	float gripForce = 0.6f;
	float holdSeconds = 0.25f;
	// How long a weapon being taken off may take to leave the slot before
	// the fists are given up.
	float unequipWaitSeconds = 2.0f;
};

// The open limit as used: the setting, or kFistOpenGap below the close limit
// if it is not that far below, never under 0.
inline float FistOpenLimit(const FistSettings& settings) {
	const float highest = settings.closeCurl - kFistOpenGap;
	const float limit = settings.openCurl < highest ? settings.openCurl : highest;
	return limit > 0.0f ? limit : 0.0f;
}

struct FistState {
	bool closed = false;
	float timer = 0.0f;
	bool raiseAfterUnequip = false;
	float unequipWait = 0.0f;
};

struct FistInput {
	bool allowed = false;    // Full VR, in the world, no menu
	bool curlValid = false;  // the skeleton gave curls this frame
	float curl[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	// The hand holds an object, or its grip is the holster's: a fist now is
	// not a fist raised to fight.
	bool busy = false;
	EquippedKind equipped = EquippedKind::Nothing;
	WeaponSeen seen = WeaponSeen::Unknown;
	float dt = 0.0f;
	// The fingers this hand's sensors have read (vr/FingersRead.h): one that
	// never has is left out. Null: all four count.
	const FingersRead* read = nullptr;
	// The curls are the trigger's (FistFingersOf): a controller that is not an Index.
	bool byTrigger = false;
};

struct FistVerdict {
	bool known = false;       // there are curls to judge by
	bool closed = false;      // the weapon hand is a fist
	bool changed = false;     // it became one, or stopped being one, this frame
	bool unequip = false;     // take the sheathed weapon off, for the fists
	bool readyClick = false;  // the ready-weapon click: fists up or down
	bool gaveUp = false;      // the weapon never left the slot
	bool byTrigger = false;   // the fist is the trigger (FistInput::byTrigger)
	bool keepsGrip = false;   // the hand's grip is kept from the grab this frame (FistKeepsGripFromGrab)
};

inline FistVerdict StepFist(FistState& s, const FistInput& in, const FistSettings& settings) {
	FistVerdict v;
	v.byTrigger = in.byTrigger;
	if (!settings.enabled || !in.curlValid) {
		s = FistState{};
		return v;
	}
	v.known = true;

	// A weapon taken off for the fists: raised once the slot is empty.
	if (s.raiseAfterUnequip) {
		s.unequipWait -= in.dt;
		if (!in.allowed) {
			s.raiseAfterUnequip = false;
		} else if (in.equipped == EquippedKind::Nothing && in.seen != WeaponSeen::Unknown) {
			v.readyClick = in.seen == WeaponSeen::Sheathed;
			s.raiseAfterUnequip = false;
		} else if (s.unequipWait <= 0.0f) {
			v.gaveUp = true;
			s.raiseAfterUnequip = false;
		}
	}

	float lowest = 0.0f;
	float highest = 0.0f;
	FistCurlSpan(in.curl, in.read, lowest, highest);
	if (in.busy) {
		s.timer = 0.0f;
		v.closed = s.closed;
		return v;
	}
	const bool wanted = s.closed ? !(highest <= FistOpenLimit(settings)) : lowest >= settings.closeCurl;
	if (wanted != s.closed) {
		s.timer += in.dt;
		if (s.timer >= settings.holdSeconds) {
			s.closed = wanted;
			s.timer = 0.0f;
			v.changed = true;
		}
	} else {
		s.timer = 0.0f;
	}
	v.closed = s.closed;
	if (!v.changed || !in.allowed) {
		return v;
	}
	if (s.closed) {
		if (in.equipped == EquippedKind::Nothing) {
			v.readyClick = in.seen == WeaponSeen::Sheathed;
		} else if (in.seen == WeaponSeen::Sheathed) {
			v.unequip = true;
			s.raiseAfterUnequip = true;
			s.unequipWait = settings.unequipWaitSeconds;
		}
	} else {
		s.raiseAfterUnequip = false;
		v.readyClick = in.equipped == EquippedKind::Nothing && in.seen == WeaponSeen::Drawn;
	}
	return v;
}

// Whether a swing may strike by motion with what is in the hand: a weapon
// always; fists only as a fist, when the curls are known.
inline bool FistAllowsStrike(bool fists, const FistVerdict& fist) {
	return !fists || !fist.known || fist.closed;
}

// The fingers the fist goes by (Nadi, 2026-11-25: "trigger gehalten zählt bei
// quest als faust rdy. loslassen faust weg"). On an Index they are the
// fingers as its handle reads them, apart from the grip's squeeze. On any
// other controller - an Oculus Touch, a Pico, a Vive wand - they are the
// trigger: a Touch reads the middle, ring and little finger from the grip
// itself (a Quest 2 tester's log of 2026-11-19, 120 curl lines: the three
// alike to 0.01 at every step - 0.57 0.57 0.57, 0.91 0.91 0.92, 1.00 1.00
// 1.00 - while the index went its own way with the trigger), so a fist there
// was always the grip squeezed, and the grip is the grab. The trigger held
// past the close limit is the fist, let go below the open limit it is open;
// the grip plays no part. A controller of unknown kind keeps its fingers.
inline void FistFingersOf(FistInput& in, const HandPose& hand) {
	in.byTrigger = hand.kind == ControllerKind::Other;
	in.curlValid = hand.valid && (in.byTrigger || hand.curlValid);
	for (int finger = 0; finger < 5; ++finger) {
		in.curl[finger] = in.byTrigger ? hand.trigger : hand.curl[finger];
	}
}

// Whether the weapon hand's grip is kept from the grab because that hand is
// a fist made with the trigger: the trigger held past the close limit, or
// the fist still closed by it. The same tester's log, again and again in the
// same moment: "grab - right grip took ... key 5A down" and "Fist: the
// weapon hand closed" - a Touch fist pressed the grip, the grip grabbed what
// the punch met, and a training dummy's frame came into the hand that way.
// Not while that hand holds something (the grab keeps it) nor with a weapon
// drawn in it (that hand is no fist), and never with the fists off ([Hands]
// Fists) or on an Index, whose fist does not need the grip. Let go of the
// trigger, and the grip grabs again.
inline bool FistKeepsGripFromGrab(const FistVerdict& fist, float trigger, const FistSettings& settings,
                                  bool holding, WeaponSeen seen, EquippedKind equipped) {
	const bool weaponInHand = seen == WeaponSeen::Drawn && equipped != EquippedKind::Nothing;
	const bool fistHeld = fist.closed || trigger >= settings.closeCurl;
	return settings.enabled && fist.byTrigger && fistHeld && !holding && !weaponInHand;
}

}  // namespace obvr::vr

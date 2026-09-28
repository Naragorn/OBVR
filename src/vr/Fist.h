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
// keeps what it was. Either change has to hold for holdSeconds.
//
// The close limit is a row in the settings ("Fist at", the tester,
// 2026-09-28: a hand whose little finger reads low never made a fist). The
// open limit is kept at least kFistOpenGap below it: were it at or above,
// a hand resting between the two would count as a fist and as open by
// turns, a ready click every holdSeconds.
//
// A fist made while the hand holds something, or while its grip is the
// holster's (a reach to the hip curls the fingers too), counts for nothing.
//
// Pure, covered by fist_test.

#include "core/Types.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::vr {

inline constexpr float kFistOpenGap = 0.15f;

struct FistSettings {
	bool enabled = true;
	float closeCurl = 0.80f;
	float openCurl = 0.35f;
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
};

struct FistVerdict {
	bool known = false;       // there are curls to judge by
	bool closed = false;      // the weapon hand is a fist
	bool changed = false;     // it became one, or stopped being one, this frame
	bool unequip = false;     // take the sheathed weapon off, for the fists
	bool readyClick = false;  // the ready-weapon click: fists up or down
	bool gaveUp = false;      // the weapon never left the slot
};

inline FistVerdict StepFist(FistState& s, const FistInput& in, const FistSettings& settings) {
	FistVerdict v;
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

	float lowest = in.curl[1];
	float highest = in.curl[1];
	for (int finger = 2; finger < 5; ++finger) {
		lowest = in.curl[finger] < lowest ? in.curl[finger] : lowest;
		highest = in.curl[finger] > highest ? in.curl[finger] : highest;
	}
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

}  // namespace obvr::vr

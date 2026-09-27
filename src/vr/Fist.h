#pragma once

// Fists by making a fist (docs/controls-spec.md 4.3): with no weapon in the
// slot, the weapon hand closed into a fist readies hand to hand, and opened
// puts it away. While the fists are up, a swing strikes only with the hand
// closed. Full VR only.
//
// The finger curls come from SteamVR's skeletal summary (IVRInput_011
// GetSkeletalSummaryData, VRSkeletalSummaryData_t.flFingerCurl[5]: thumb,
// index, middle, ring, pinky; 0 open, 1 curled). The thumb is left out: on
// an Index it rests on the stick and the buttons whether the hand is a fist
// or not.
//
// A fist is the four fingers all past closeCurl; the hand counts as open
// again only once they are all below openCurl, so a hand between the two
// keeps what it was. Either change has to hold for holdSeconds.
//
// Pure, covered by fist_test.

#include "core/Types.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::vr {

struct FistSettings {
	bool enabled = true;
	float closeCurl = 0.80f;
	float openCurl = 0.35f;
	float holdSeconds = 0.25f;
};

struct FistState {
	bool closed = false;
	float timer = 0.0f;
};

struct FistInput {
	bool allowed = false;    // Full VR, in the world, no menu
	bool curlValid = false;  // the skeleton gave curls this frame
	float curl[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	// The grip squeezed: that is the grab's, and a fist made while it is
	// down changes nothing - a hand holding a cup is not raising its fists.
	bool gripDown = false;
	EquippedKind equipped = EquippedKind::Nothing;
	WeaponSeen seen = WeaponSeen::Unknown;
	float dt = 0.0f;
};

struct FistVerdict {
	bool known = false;       // there are curls to judge by
	bool closed = false;      // the weapon hand is a fist
	bool changed = false;     // it became one, or stopped being one, this frame
	bool readyClick = false;  // the ready-weapon click: fists up or down
};

inline FistVerdict StepFist(FistState& s, const FistInput& in, const FistSettings& settings) {
	FistVerdict v;
	if (!settings.enabled || !in.curlValid) {
		s = FistState{};
		return v;
	}
	v.known = true;
	float lowest = in.curl[1];
	float highest = in.curl[1];
	for (int finger = 2; finger < 5; ++finger) {
		lowest = in.curl[finger] < lowest ? in.curl[finger] : lowest;
		highest = in.curl[finger] > highest ? in.curl[finger] : highest;
	}
	if (in.gripDown) {
		s.timer = 0.0f;
		v.closed = s.closed;
		return v;
	}
	const bool wanted = s.closed ? !(highest <= settings.openCurl) : lowest >= settings.closeCurl;
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
	if (v.changed && in.allowed && in.equipped == EquippedKind::Nothing) {
		v.readyClick = s.closed ? in.seen == WeaponSeen::Sheathed : in.seen == WeaponSeen::Drawn;
	}
	return v;
}

// Whether a swing may strike by motion with what is in the hand: a weapon
// always; fists only as a fist, when the curls are known.
inline bool FistAllowsStrike(bool fists, const FistVerdict& fist) {
	return !fists || !fist.known || fist.closed;
}

}  // namespace obvr::vr

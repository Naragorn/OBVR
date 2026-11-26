#pragma once

// One part of a body a swing (the tester, 2026-11-26: "teilweise fliegt auch
// kopf UND arm ab obwohl nur kopf sollte" - sometimes the head and an arm fly
// off where only the head should). Pure, covered by cut_stroke_test;
// camera/hands/Dismember.cpp keeps it.
//
// Read in that run's OBVR.log (lines 21491-21526): one slash killed the
// zombie 2B001A50 and took its head (the killing strike, its sweep on through
// the neck); in the same frame the blade's sweep through the now dead body's
// right shoulder took the right arm, and a frame later the left arm - the
// corpse's rules (the neck, the wrists, the limbs' joints) apply to a body as
// soon as it is dead, the swing that killed it included. A slash at the
// neck's base sweeps a plane through the neck and both shoulders, each within
// its reach (6 units round the neck and a shoulder). The goblins of that run
// lost two or three parts to one swing the same way (2B009630: the head, the
// right forearm, the left hand; 2B009528: the head and both arms).
//
// So a swing - the hand mode's swing serial, the one a body is struck once by
// (vr/HandMode.h swingSerial) - takes one part of each body it cuts: the first
// the blade reaches. In a frame the head is decided first (the killing strike,
// then the necks), so a swing through the neck and a shoulder takes the head.
// A new swing may take another part.

#include "core/Types.h"

namespace obvr::game {

struct SwingCuts {
	static constexpr UInt32 kMost = 8;  // bodies one swing has cut; past it the rest are not held back
	UInt32 serial = 0;
	UInt32 actor[kMost] = {};
	UInt32 count = 0;
};

// Whether swing `serial` has cut a part of `actor` already.
inline bool SwingTookPartOf(const SwingCuts& s, UInt32 serial, UInt32 actor) {
	if (actor == 0 || s.serial != serial) {
		return false;
	}
	for (UInt32 i = 0; i < s.count && i < SwingCuts::kMost; ++i) {
		if (s.actor[i] == actor) {
			return true;
		}
	}
	return false;
}

// A part of `actor` cut by swing `serial`: kept, the swings before forgotten.
inline void NoteSwingCut(SwingCuts& s, UInt32 serial, UInt32 actor) {
	if (actor == 0) {
		return;
	}
	if (s.serial != serial) {
		s = SwingCuts{};
		s.serial = serial;
	}
	if (SwingTookPartOf(s, serial, actor) || s.count >= SwingCuts::kMost) {
		return;
	}
	s.actor[s.count++] = actor;
}

}  // namespace obvr::game

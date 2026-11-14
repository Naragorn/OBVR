#pragma once

// What the hands' and the blade's contact with the people near costs, by how
// many there are (Nadi, 2026-11-14, "A": measure before a tighter filter is
// built - OBNG's Linux horde read about 1 ms a scene call in the contact's
// functions, Nadi's Windows horde 0.03 to 0.07 ms). Every collect of the
// living near (game/BladeBodies.h) counts its people; the contact steps'
// time is taken once a frame (CameraHook StepWeaponContact and
// StepHandContacts). A line every kContactCostFrames frames in which anyone
// was near, and at every hand script mark.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kContactCostFrames = 600;

// One window of frames with anyone near. Pure; contact_cost_test.
struct ContactCostWindow {
	UInt32 frames = 0;
	UInt32 people = 0;
	UInt32 mostPeople = 0;
	double micros = 0.0;
	double longest = 0.0;
};

// A frame's contact time and the people its collects took. A frame with no
// one near is not counted. True when the window is due for its line: full,
// or a mark with something counted.
inline bool AddContactFrame(ContactCostWindow& w, UInt32 people, double micros, bool marked) {
	if (people > 0) {
		++w.frames;
		w.people += people;
		w.mostPeople = people > w.mostPeople ? people : w.mostPeople;
		const double us = micros == micros && micros > 0.0 ? micros : 0.0;
		w.micros += us;
		w.longest = us > w.longest ? us : w.longest;
	}
	return w.frames > 0 && (w.frames >= kContactCostFrames || marked);
}

inline double ContactMicrosPerFrame(const ContactCostWindow& w) { return w.frames > 0 ? w.micros / w.frames : 0.0; }
inline double ContactMicrosPerPerson(const ContactCostWindow& w) { return w.people > 0 ? w.micros / w.people : 0.0; }

void NoteCollectedPeople(UInt32 people);
void NoteContactFrame(long long ticks, bool marked);

}  // namespace obvr::game

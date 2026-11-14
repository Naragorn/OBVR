#include "game/ContactCost.h"

#include "core/Log.h"
#include "platform/Win32Min.h"

namespace obvr::game {

namespace {

struct ContactCostState {
	UInt32 peopleThisFrame = 0;
	ContactCostWindow window;
	UInt32 lines = 40;
};

ContactCostState& State() {
	static ContactCostState s;
	return s;
}

}  // namespace

void NoteCollectedPeople(UInt32 people) { State().peopleThisFrame += people; }

void NoteContactFrame(long long ticks, bool marked) {
	ContactCostState& s = State();
	static const double s_microsPerTick = 1e6 / static_cast<double>(ReadPerformanceFrequency());
	const UInt32 people = s.peopleThisFrame;
	s.peopleThisFrame = 0;
	if (!AddContactFrame(s.window, people, static_cast<double>(ticks) * s_microsPerTick, marked)) {
		return;
	}
	const ContactCostWindow& w = s.window;
	if (s.lines > 0) {
		--s.lines;
		OBVR_LOG("Contact cost: %u frames with people near - the contact steps %.1f us a frame on the mean, %.1f the "
		         "longest; %.1f people collected a frame (all collects), %u the most; %.1f us a person collected",
		         w.frames, ContactMicrosPerFrame(w), w.longest, static_cast<double>(w.people) / w.frames, w.mostPeople,
		         ContactMicrosPerPerson(w));
	}
	s.window = ContactCostWindow{};
}

}  // namespace obvr::game

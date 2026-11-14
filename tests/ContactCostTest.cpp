#include <cmath>
#include <cstdio>

#include "game/ContactCost.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::game;

void TestWindow() {
	std::printf("the contact's cost counted over frames with people near\n");
	ContactCostWindow w;
	Check(!AddContactFrame(w, 0, 50.0, false) && w.frames == 0, "no one near: not counted");
	Check(!AddContactFrame(w, 0, 50.0, true), "a mark with nothing counted: no line");
	Check(!AddContactFrame(w, 4, 100.0, false) && w.frames == 1 && w.people == 4, "four near: counted, not due");
	Check(AddContactFrame(w, 2, 300.0, true), "a mark with something counted: due");
	Check(w.mostPeople == 4 && w.longest == 300.0, "the most people and the longest frame kept");
	Check(ContactMicrosPerFrame(w) == 200.0 && ContactMicrosPerPerson(w) == 400.0 / 6.0, "per frame and per person");
	ContactCostWindow full;
	bool due = false;
	for (UInt32 i = 0; i < kContactCostFrames; ++i) {
		due = AddContactFrame(full, 1, 10.0, false);
	}
	Check(due && full.frames == kContactCostFrames, "a full window: due");
	ContactCostWindow bad;
	AddContactFrame(bad, 1, std::nan(""), false);
	AddContactFrame(bad, 1, -5.0, false);
	Check(bad.micros == 0.0 && bad.frames == 2, "NaN and negative time count as none");
	ContactCostWindow empty;
	Check(ContactMicrosPerFrame(empty) == 0.0 && ContactMicrosPerPerson(empty) == 0.0, "empty: 0, no division");
}

}  // namespace

int main() {
	TestWindow();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

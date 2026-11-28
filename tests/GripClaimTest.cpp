// Checks who gets a closing grip where the tablet's gesture and a handover
// overlap (vr/GripClaim.h): the gesture first in its zone, the handover at
// once elsewhere, late by the window when no second grip comes, and nobody
// till both grips are open after the gesture took the pair.

#include <cstdio>

#include "vr/GripClaim.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kWindow = 0.5f;

GripClaimFrame Frame(bool zone, bool wanted, bool gripsOpen = false) {
	GripClaimFrame f;
	f.usable = true;
	f.zone = zone;
	f.handoverWanted = wanted;
	f.gripsOpen = gripsOpen;
	f.dt = 0.125f;
	return f;
}

void TestOutsideTheZone() {
	std::printf("Outside the gesture's zone:\n");
	GripClaimState s;
	Check(StepGripClaim(s, Frame(false, true), kWindow) == GripClaimer::HandoverNow,
	      "the grips far apart, the tablet off or a hand holding: the handover at once, as before");
	Check(StepGripClaim(s, Frame(false, false), kWindow) == GripClaimer::Nobody, "no closing: nobody");
	GripClaimFrame off = Frame(true, true);
	off.usable = false;
	Check(StepGripClaim(s, off, kWindow) == GripClaimer::Nobody && s.waited < 0.0f,
	      "a menu up or the hands off: nobody, nothing kept");
}

void TestGestureTakesIt() {
	std::printf("In the zone, the second grip in time:\n");
	GripClaimState s;
	Check(StepGripClaim(s, Frame(true, true), kWindow) == GripClaimer::Nobody && s.waited == 0.0f,
	      "the weapon hand closes first at the ring: the handover waits");
	Check(StepGripClaim(s, Frame(true, false), kWindow) == GripClaimer::Nobody, "0.125 s on: still waiting");
	GripClaimFrame began = Frame(true, false);
	began.gestureBegan = true;
	Check(StepGripClaim(s, began, kWindow) == GripClaimer::Gesture && s.waited < 0.0f && s.claimed,
	      "the other grip closes, the gesture begins: the gesture's, the handover dropped");
	for (int i = 0; i < 10; ++i) {
		StepGripClaim(s, Frame(false, false), kWindow);
	}
	Check(s.claimed && s.waited < 0.0f, "past the window: no handover comes late");
	Check(StepGripClaim(s, Frame(false, true), kWindow) == GripClaimer::Nobody,
	      "a closing of the same pair (one grip opened and closed again): still nobody's");
	StepGripClaim(s, Frame(false, false, true), kWindow);
	Check(!s.claimed, "both grips open: the pair is over");
	Check(StepGripClaim(s, Frame(false, true), kWindow) == GripClaimer::HandoverNow, "the next closing: the handover again");

	GripClaimState other;
	GripClaimFrame both = Frame(true, true);
	both.gestureBegan = true;
	Check(StepGripClaim(other, both, kWindow) == GripClaimer::Gesture && other.claimed,
	      "the off hand first, the weapon hand closing begins the gesture: the gesture's, no handover");
}

void TestTheWaitRunsOut() {
	std::printf("In the zone, no second grip:\n");
	GripClaimState s;
	StepGripClaim(s, Frame(true, true), kWindow);
	GripClaimer who = GripClaimer::Nobody;
	int frames = 0;
	while (who == GripClaimer::Nobody && frames < 20) {
		who = StepGripClaim(s, Frame(true, false), kWindow);
		++frames;
	}
	Check(who == GripClaimer::HandoverAfterWait && frames == 4, "one grip only: the handover after the window, 0.5 s late");
	Check(s.waited < 0.0f && !s.claimed, "and done");
	Check(StepGripClaim(s, Frame(true, false), kWindow) == GripClaimer::Nobody, "once only");

	GripClaimState left;
	StepGripClaim(left, Frame(true, true), kWindow);
	StepGripClaim(left, Frame(false, false), kWindow);
	who = StepGripClaim(left, Frame(false, true), kWindow);
	Check(who == GripClaimer::Nobody, "the hands moved apart while waiting, a second closing: one wait, nothing more");
	for (int i = 0; i < 2; ++i) {
		who = StepGripClaim(left, Frame(false, false), kWindow);
	}
	Check(who == GripClaimer::HandoverAfterWait, "the window still decides: the handover late");

	GripClaimState refused;
	StepGripClaim(refused, Frame(true, true), kWindow);
	who = StepGripClaim(refused, Frame(true, false), kWindow);
	Check(who == GripClaimer::Nobody, "the second grip came, the gesture refused it (no begin): still waiting");
	for (int i = 0; i < 3; ++i) {
		who = StepGripClaim(refused, Frame(true, false), kWindow);
	}
	Check(who == GripClaimer::HandoverAfterWait, "the handover late, as when no grip came");

	GripClaimState menu;
	StepGripClaim(menu, Frame(true, true), kWindow);
	GripClaimFrame up = Frame(true, false);
	up.usable = false;
	Check(StepGripClaim(menu, up, kWindow) == GripClaimer::Nobody && menu.waited < 0.0f,
	      "a menu up while waiting: the waiting handover dropped, as a closing under a menu is");
}

}  // namespace

int main() {
	TestOutsideTheZone();
	TestGestureTakesIt();
	TestTheWaitRunsOut();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

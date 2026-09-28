// Checks fists by making a fist (vr/Fist.h): the curls that make a fist, the
// hold and the hysteresis, and when the ready click goes.

#include <cstdio>

#include "vr/Fist.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

FistInput Hand(float fingers, EquippedKind equipped = EquippedKind::Nothing,
               WeaponSeen seen = WeaponSeen::Sheathed) {
	FistInput in;
	in.allowed = true;
	in.curlValid = true;
	in.curl[0] = 0.0f;  // the thumb, on the stick
	for (int f = 1; f < 5; ++f) {
		in.curl[f] = fingers;
	}
	in.equipped = equipped;
	in.seen = seen;
	in.dt = 0.1f;
	return in;
}

FistVerdict Hold(FistState& s, const FistInput& in, int frames, const FistSettings& settings) {
	FistVerdict v;
	FistVerdict any;
	for (int i = 0; i < frames; ++i) {
		v = StepFist(s, in, settings);
		any.changed = any.changed || v.changed;
		any.readyClick = any.readyClick || v.readyClick;
		any.unequip = any.unequip || v.unequip;
	}
	v.changed = any.changed;
	v.readyClick = any.readyClick;
	v.unequip = any.unequip;
	return v;
}

void TestFist() {
	std::printf("A fist, and the hand opened\n");
	const FistSettings settings;
	{
		FistSettings off;
		off.enabled = false;
		FistState s;
		const FistVerdict v = Hold(s, Hand(1.0f), 5, off);
		Check(!v.known && !v.closed && !v.readyClick, "switched off: nothing");
	}
	{
		FistState s;
		FistInput none = Hand(1.0f);
		none.curlValid = false;
		const FistVerdict v = Hold(s, none, 5, settings);
		Check(!v.known && !v.readyClick, "no skeleton: nothing known");
		Check(FistAllowsStrike(true, v), "and fists strike as before");
	}
	{
		FistState s;
		FistVerdict v = Hold(s, Hand(0.1f), 5, settings);
		Check(v.known && !v.closed && !v.changed, "an open hand stays open");
		v = StepFist(s, Hand(0.95f), settings);
		Check(!v.closed, "a fist for one frame is not yet one");
		v = Hold(s, Hand(0.95f), 3, settings);
		Check(v.closed && v.changed && v.readyClick, "held a quarter second: fists up");
		v = Hold(s, Hand(0.95f), 3, settings);
		Check(v.closed && !v.changed && !v.readyClick, "once");
		v = Hold(s, Hand(0.5f, EquippedKind::Nothing, WeaponSeen::Drawn), 10, settings);
		Check(v.closed && !v.changed, "loosened between the thresholds: still a fist");
		v = Hold(s, Hand(0.1f, EquippedKind::Nothing, WeaponSeen::Drawn), 4, settings);
		Check(!v.closed && v.changed && v.readyClick, "opened: fists down");
	}
	{
		FistState s;
		FistInput oneOpen = Hand(0.95f);
		oneOpen.curl[4] = 0.2f;
		const FistVerdict v = Hold(s, oneOpen, 5, settings);
		Check(!v.closed, "the little finger out: not a fist");
	}
	{
		FistState s;
		StepFist(s, Hand(0.95f), settings);
		StepFist(s, Hand(0.1f), settings);
		const FistVerdict v = Hold(s, Hand(0.95f), 2, settings);
		Check(!v.closed, "a flicker restarts the hold");
	}
	{
		FistState s;
		const FistVerdict v =
			Hold(s, Hand(0.95f, EquippedKind::OneHand, WeaponSeen::Drawn), 4, settings);
		Check(v.closed && v.changed && !v.readyClick && !v.unequip,
		      "a sword drawn: the fist is the hand round it, nothing happens");
		Check(FistAllowsStrike(false, v), "and a weapon strikes regardless");
	}
	{
		FistState s;
		FistVerdict v = Hold(s, Hand(0.95f, EquippedKind::OneHand, WeaponSeen::Sheathed), 4, settings);
		Check(v.unequip && !v.readyClick, "a sword sheathed: taken off first");
		v = StepFist(s, Hand(0.95f, EquippedKind::OneHand, WeaponSeen::Sheathed), settings);
		Check(!v.unequip && !v.readyClick, "once, and nothing while it is still in the slot");
		v = StepFist(s, Hand(0.95f, EquippedKind::Nothing, WeaponSeen::Sheathed), settings);
		Check(v.readyClick, "the slot empty: the fists raised");
		v = StepFist(s, Hand(0.95f, EquippedKind::Nothing, WeaponSeen::Drawn), settings);
		Check(!v.readyClick, "once");
	}
	{
		FistState s;
		Hold(s, Hand(0.95f, EquippedKind::Bow, WeaponSeen::Sheathed), 4, settings);
		FistInput slow = Hand(0.95f, EquippedKind::Bow, WeaponSeen::Sheathed);
		slow.dt = 2.5f;
		const FistVerdict v = StepFist(s, slow, settings);
		Check(v.gaveUp && !v.readyClick, "the bow never leaves the slot: given up");
	}
	{
		FistState s;
		Hold(s, Hand(0.95f, EquippedKind::OneHand, WeaponSeen::Sheathed), 4, settings);
		const FistVerdict v =
			Hold(s, Hand(0.1f, EquippedKind::OneHand, WeaponSeen::Sheathed), 4, settings);
		Check(!v.readyClick && !v.closed, "opened before the slot emptied: no fists raised");
		const FistVerdict after =
			Hold(s, Hand(0.1f, EquippedKind::Nothing, WeaponSeen::Sheathed), 3, settings);
		Check(!after.readyClick, "and not later either");
	}
	{
		FistState s;
		Hold(s, Hand(0.95f, EquippedKind::OneHand, WeaponSeen::Sheathed), 4, settings);
		FistInput menu = Hand(0.95f, EquippedKind::Nothing, WeaponSeen::Sheathed);
		menu.allowed = false;
		StepFist(s, menu, settings);
		const FistVerdict v =
			StepFist(s, Hand(0.95f, EquippedKind::Nothing, WeaponSeen::Sheathed), settings);
		Check(!v.readyClick, "a menu in between drops the pending raise");
	}
	{
		FistState s;
		const FistVerdict v =
			Hold(s, Hand(0.95f, EquippedKind::Nothing, WeaponSeen::Drawn), 4, settings);
		Check(v.closed && !v.readyClick, "fists already up: no click");
	}
	{
		FistState s;
		FistInput menu = Hand(0.95f);
		menu.allowed = false;
		const FistVerdict v = Hold(s, menu, 4, settings);
		Check(v.closed && !v.readyClick, "in a menu: the fist counts, nothing is sent");
	}
	{
		FistState s;
		FistInput holding = Hand(0.95f);
		holding.busy = true;
		FistVerdict v = Hold(s, holding, 5, settings);
		Check(!v.closed && !v.readyClick, "curled round a held object: not fists");
		v = Hold(s, Hand(0.95f), 3, settings);
		Check(v.closed && v.readyClick, "let go, the fist kept: fists up");
	}
	{
		FistState s;
		FistVerdict v = Hold(s, Hand(0.1f), 3, settings);
		Check(!FistAllowsStrike(true, v), "fists up, the hand open: a swing does not strike");
		v = Hold(s, Hand(0.95f), 3, settings);
		Check(FistAllowsStrike(true, v), "a fist: it does");
	}
}


// The close limit from the settings, and the open limit kept below it.
void TestLimits() {
	std::printf("The fist's limits\n");
	FistSettings s;
	Check(FistOpenLimit(s) == s.openCurl, "the defaults (0.80, 0.35): the open limit as set");
	s.closeCurl = 0.45f;
	Check(FistOpenLimit(s) > 0.299f && FistOpenLimit(s) < 0.301f, "the close limit lowered to 0.45: open below 0.30");
	s.closeCurl = 0.4f;
	s.openCurl = 0.6f;
	Check(FistOpenLimit(s) > 0.249f && FistOpenLimit(s) < 0.251f, "an open limit above the close limit: pulled below it");
	s.closeCurl = 0.1f;
	s.openCurl = 0.05f;
	Check(FistOpenLimit(s) == 0.0f, "a close limit under the gap: open at 0, never negative");
	{
		FistSettings low;
		low.closeCurl = 0.5f;
		FistState st;
		FistVerdict v = Hold(st, Hand(0.55f), 5, low);
		Check(v.closed && v.readyClick, "closed at 0.5: a hand at 0.55 is a fist");
		v = Hold(st, Hand(0.55f), 10, low);
		Check(v.closed && !v.changed, "and stays one, no clicks by turns");
		FistState def;
		v = Hold(def, Hand(0.55f), 5, FistSettings{});
		Check(!v.closed, "at the default 0.80 the same hand is not");
	}
	{
		FistSettings crossed;
		crossed.closeCurl = 0.5f;
		crossed.openCurl = 0.7f;
		FistState st;
		FistVerdict v = Hold(st, Hand(0.6f), 5, crossed);
		Check(v.closed, "an open limit set above the close limit: a hand at 0.6 closes");
		v = Hold(st, Hand(0.6f), 10, crossed);
		Check(v.closed && !v.changed, "and does not open again at once");
		v = Hold(st, Hand(0.3f), 5, crossed);
		Check(!v.closed && v.changed, "opened below 0.35: open");
	}
}

}  // namespace

int main() {
	TestFist();
	TestLimits();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

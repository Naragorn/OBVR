// Charged spells (vr/SpellCharge.h): every flow of the casting hand's trigger - a tap, a hold
// through the stages and its release, a menu while charging, a press while a release is held,
// the switch off - and what goes to the engine each frame.

#include <cstdio>

#include "vr/SpellCharge.h"

namespace {

using namespace obvr::vr;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kFrame = 1.0f / 72.0f;

SpellChargeStep Frame(SpellChargeState& st, bool held, const SpellChargeSettings& s, bool menu = false) {
	SpellChargeInput in;
	in.triggerHeld = held;
	in.menuUp = menu;
	in.dtSeconds = kFrame;
	return StepSpellCharge(st, in, s);
}

// Frames of `held` until `seconds` have gone; the last step returned.
SpellChargeStep For(SpellChargeState& st, bool held, float seconds, const SpellChargeSettings& s,
                    bool& anyCast, bool& anyHold, bool& anyFire) {
	SpellChargeStep last;
	for (float t = 0.0f; t < seconds - 1e-4f; t += kFrame) {
		last = Frame(st, held, s);
		anyCast = anyCast || last.castControl;
		anyHold = anyHold || last.holdRelease;
		anyFire = anyFire || last.fireRelease;
	}
	return last;
}

void TestTap() {
	std::printf("A tap casts as before\n");
	SpellChargeSettings s;
	SpellChargeState st;
	SpellChargeStep r = Frame(st, true, s);
	Check(r.pressed && r.castControl && !r.holdRelease && !r.fireRelease, "the press: the cast control on, nothing held");
	bool cast = false, hold = false, fire = false;
	For(st, true, 0.1f, s, cast, hold, fire);
	r = Frame(st, false, s);
	Check(!hold && !fire && !r.holdRelease && !r.fireRelease, "let go before the tap time: nothing held back, nothing to make");
	Check(r.stage == 0, "and no stage: the engine's own release, at full strength");
	cast = hold = fire = false;
	r = For(st, false, 1.0f, s, cast, hold, fire);
	Check(!cast && !hold && !fire, "after it: no control, no hold");
}

void TestPulse() {
	std::printf("The control is a pulse, never held\n");
	SpellChargeSettings s;
	SpellChargeState st;
	int castFrames = 0;
	for (int i = 0; i < 300; ++i) {
		castFrames += Frame(st, true, s).castControl ? 1 : 0;
	}
	Check(castFrames >= 7 && castFrames <= 8, "held 4 s: the control on for the pulse's 0.1 s only (7-8 frames at 72 Hz)");
	SpellChargeState one;
	Frame(one, true, s);
	const bool second = Frame(one, false, s).castControl;
	Check(second, "a press of one frame still gets its whole pulse");
}

void TestCharge() {
	std::printf("Held past the tap, the spell charges and goes at the let go\n");
	SpellChargeSettings s;
	SpellChargeState st;
	bool cast = false, hold = false, fire = false;
	SpellChargeStep r = For(st, true, 0.24f, s, cast, hold, fire);
	Check(!hold && !r.holdRelease, "0.24 s: not yet a charge");
	r = Frame(st, true, s);
	r = Frame(st, true, s);
	Check(r.holdRelease && r.stage == 1, "past 0.25 s: charging, stage 1, the release held back");
	r = For(st, true, 0.5f, s, cast, hold, fire);
	Check(r.holdRelease && r.stage == 2, "past 0.75 s: stage 2");
	r = For(st, true, 0.8f, s, cast, hold, fire);
	Check(r.holdRelease && r.stage == 3 && !fire, "past 1.5 s: stage 3, still held");
	r = For(st, true, 3.0f, s, cast, hold, fire);
	Check(r.holdRelease && r.stage == 3 && !fire, "held on: stage 3 is the last, it waits");
	r = Frame(st, false, s);
	Check(r.fireRelease && !r.holdRelease && r.stage == 3, "let go: the release made, at stage 3");
	r = Frame(st, false, s);
	Check(!r.fireRelease && !r.holdRelease, "once only");
}

void TestStageAtLetGo() {
	std::printf("The stage is the one reached when the trigger lets go\n");
	SpellChargeSettings s;
	SpellChargeState st;
	bool cast = false, hold = false, fire = false;
	For(st, true, 0.5f, s, cast, hold, fire);
	SpellChargeStep r = Frame(st, false, s);
	Check(r.fireRelease && r.stage == 1, "held 0.5 s: stage 1");
	For(st, true, 1.0f, s, cast, hold, fire);
	r = Frame(st, false, s);
	Check(r.fireRelease && r.stage == 2, "held 1 s: stage 2");
}

void TestMenuDrops() {
	std::printf("A menu while charging drops the spell\n");
	SpellChargeSettings s;
	SpellChargeState st;
	bool cast = false, hold = false, fire = false;
	For(st, true, 1.0f, s, cast, hold, fire);
	SpellChargeStep r = Frame(st, true, s, true);
	Check(r.dropRelease && !r.fireRelease && !r.holdRelease, "the menu up: dropped, not made");
	r = Frame(st, false, s);
	Check(!r.fireRelease && !r.dropRelease, "the let go after it makes nothing");
	SpellChargeState kept;
	For(kept, true, 1.0f, s, cast, hold, fire);
	Frame(kept, true, s, true);
	hold = fire = false;
	r = For(kept, true, 1.0f, s, cast, hold, fire);
	Check(!hold && !fire && !r.dropRelease, "the trigger still held after the drop: no new charge");
	r = Frame(kept, false, s);
	Check(!r.fireRelease, "and its let go makes nothing");
	r = Frame(kept, true, s);
	Check(r.pressed && r.castControl, "the next press casts again");
	SpellChargeState tap;
	r = Frame(tap, true, s, true);
	Check(!r.dropRelease && r.castControl, "a press with a menu up (not charging): passed on as before");
}

void TestPressWhileHeld() {
	std::printf("A press while a release is still held makes it first\n");
	SpellChargeSettings s;
	SpellChargeState st;
	bool cast = false, hold = false, fire = false;
	For(st, true, 1.0f, s, cast, hold, fire);
	st.held = false;  // the let go missed (the controls not stepped for a frame)
	SpellChargeStep r = Frame(st, true, s);
	Check(r.fireRelease && r.pressed && r.castControl && !r.holdRelease, "the held one made, the new press begins");
}

void TestOff() {
	std::printf("Switched off: vanilla\n");
	SpellChargeSettings s;
	s.enabled = false;
	SpellChargeState st;
	bool cast = false, hold = false, fire = false;
	SpellChargeStep r = For(st, true, 2.0f, s, cast, hold, fire);
	Check(r.castControl && !hold && !fire, "held: the control held, nothing held back");
	SpellChargeSettings on;
	SpellChargeState charged;
	For(charged, true, 1.0f, on, cast, hold, fire);
	r = Frame(charged, true, s);
	Check(r.fireRelease && !r.holdRelease, "switched off while charging: the held release is made, not lost");
}

void TestOrb() {
	std::printf("The spell in the hand: shown while charged, growing to its full size at stage 3\n");
	SpellChargeSettings s;
	SpellChargeState st;
	SpellOrbView v = SpellOrbFor(Frame(st, true, s), s);
	Check(!v.shown, "the press: not shown (a tap may follow)");
	bool cast = false, hold = false, fire = false;
	SpellChargeStep r = For(st, true, 0.22f, s, cast, hold, fire);
	Check(!SpellOrbFor(r, s).shown, "0.23 s after the press: not yet");
	r = For(st, true, 0.04f, s, cast, hold, fire);
	v = SpellOrbFor(r, s);
	Check(v.shown && !v.full && v.scale >= kSpellOrbFirstScale && v.scale < kSpellOrbFirstScale + 0.02f,
	      "the charge begun: shown, at its first size");
	float was = v.scale;
	bool grew = true;
	for (int i = 0; i < 60; ++i) {  // to 1.1 s held, short of stage 3
		v = SpellOrbFor(Frame(st, true, s), s);
		grew = grew && v.shown && v.scale >= was;
		was = v.scale;
	}
	Check(grew && !v.full && v.scale < kSpellOrbFullScale, "held on: it only grows, short of full before stage 3");
	r = For(st, true, 1.0f, s, cast, hold, fire);
	v = SpellOrbFor(r, s);
	Check(v.shown && v.full && v.scale == kSpellOrbFullScale, "past stage 3: its full size, no bigger");
	v = SpellOrbFor(Frame(st, false, s), s);
	Check(!v.shown, "let go: gone from the hand (the spell itself flies)");
	SpellChargeState menu;
	For(menu, true, 1.0f, s, cast, hold, fire);
	Check(!SpellOrbFor(Frame(menu, true, s, true), s).shown, "a menu drops it: gone");
	Check(!SpellOrbFor(Frame(menu, true, s), s).shown, "and it does not come back while still held");
	SpellChargeSettings off;
	off.enabled = false;
	SpellChargeState vanilla;
	r = For(vanilla, true, 2.0f, off, cast, hold, fire);
	Check(!SpellOrbFor(r, off).shown, "switched off: never shown");
	SpellChargeSettings flat;
	flat.stage3Seconds = flat.tapSeconds;
	SpellChargeState at;
	r = For(at, true, 0.3f, flat, cast, hold, fire);
	v = SpellOrbFor(r, flat);
	Check(v.shown && v.full && v.scale == kSpellOrbFullScale, "stage 3 at the tap time: full size at once");
}

void TestStrength() {
	std::printf("Strength and cost by the stage\n");
	SpellChargeSettings s;
	Check(SpellChargeFactor(0, s) == 1.0f, "a tap (no stage): the spell as it is");
	Check(SpellChargeFactor(1, s) == 1.0f, "stage 1: as it is");
	Check(SpellChargeFactor(2, s) == 1.25f, "stage 2: 125 %");
	Check(SpellChargeFactor(3, s) == 1.5f, "stage 3: 150 %");
	Check(SpellChargeFactor(7, s) == 1.5f, "past stage 3: no more");
	SpellChargeSettings look;
	look.strength = false;
	Check(SpellChargeFactor(3, look) == 1.0f, "look and sound only: stage 3 as it is");

	Check(AffordableChargeFactor(1.5f, 40.0f, 10.0f) == 1.5f, "enough magicka: the whole stage");
	const float some = AffordableChargeFactor(1.5f, 12.0f, 10.0f);
	Check(some > 1.199f && some < 1.201f, "12 magicka for a 10 spell: 120 %, what it can pay");
	Check(AffordableChargeFactor(1.5f, 8.0f, 10.0f) == 1.0f, "less than the spell itself: as it is (the engine refuses it)");
	Check(AffordableChargeFactor(1.5f, 40.0f, 0.0f) == 1.5f, "a spell that costs nothing: the whole stage");
	Check(AffordableChargeFactor(1.5f, 40.0f, -1.0f) == 1.0f, "the cost unread: as it is");
	Check(AffordableChargeFactor(0.5f, 40.0f, 10.0f) == 1.0f, "never below the spell as it is");

	Check(ExtraChargeCost(10.0f, 1.5f) == 5.0f, "paid 10 at 150 %: 5 more");
	Check(ExtraChargeCost(10.0f, 1.0f) == 0.0f, "at 100 %: nothing more");
	Check(ExtraChargeCost(-3.0f, 1.5f) == 0.0f, "nothing paid (or magicka gained): nothing more");
}

void TestCastAnimPause() {
	std::printf("The cast animation in two halves: paused at its release while the spell is held\n");
	Check(IsCastAnimGroup(0x22) && IsCastAnimGroup(0x23) && IsCastAnimGroup(0x24), "CastSelf, CastTouch, CastTarget");
	Check(IsCastAnimGroup(0x25) && IsCastAnimGroup(0x27), "and their Alt groups");
	Check(!IsCastAnimGroup(0x00) && !IsCastAnimGroup(0x13) && !IsCastAnimGroup(0x21) && !IsCastAnimGroup(0x28),
	      "idle, the bow, the torch, a jump: not a cast");

	CastAnimPause p;
	float offset = -99.0f;
	Check(!StepCastAnimPause(p, false, false, 0x24, 0.2f, 10.0f, offset) && offset == -99.0f && !p.paused,
	      "the first half (nothing held back yet): played as the engine plays it, nothing written");
	Check(StepCastAnimPause(p, true, false, 0x24, 0.403f, 10.0f, offset) && p.paused, "held back: paused");
	Check(offset > 0.403f - 10.0f - 1e-4f && offset < 0.403f - 10.0f + 1e-4f,
	      "at the time the engine came to its release");
	Check(StepCastAnimPause(p, true, false, 0x24, 0.42f, 12.5f, offset) && offset > 0.403f - 12.5f - 1e-4f &&
	          offset < 0.403f - 12.5f + 1e-4f,
	      "seconds later: still at that time, however far the clock went");
	offset = -99.0f;
	Check(!StepCastAnimPause(p, false, false, 0x24, 0.42f, 12.6f, offset) && offset == -99.0f && !p.paused,
	      "let go (the release asked for): nothing written, the second half plays on from there");

	CastAnimPause q;
	StepCastAnimPause(q, true, false, 0x22, 0.375f, 3.0f, offset);
	offset = -99.0f;
	Check(!StepCastAnimPause(q, true, false, 0x00, 0.1f, 4.0f, offset) && offset == -99.0f && !q.paused,
	      "the slot no longer a cast (another animation took it): let go, nothing written");
	Check(StepCastAnimPause(q, true, false, 0x23, 0.278f, 5.0f, offset) && q.paused &&
	          offset > 0.278f - 5.0f - 1e-4f && offset < 0.278f - 5.0f + 1e-4f,
	      "a cast again while held back: paused anew where it is");

	CastAnimPause r;
	offset = -99.0f;
	Check(!StepCastAnimPause(r, true, false, 0x13, 1.0f, 2.0f, offset) && offset == -99.0f && !r.paused,
	      "held back but the slot is the bow's draw: never paused");
}

void TestTouchWaitsAtItsStart() {
	std::printf("A touch spell: no animation while held, the whole of it at the let go\n");
	Check(IsTouchCastGroup(0x23) && IsTouchCastGroup(0x26), "CastTouch and CastTouchAlt");
	Check(!IsTouchCastGroup(0x22) && !IsTouchCastGroup(0x24), "self and target: not touch");

	CastAnimPause p;
	float offset = -99.0f;
	Check(StepCastAnimPause(p, false, true, 0x23, 14.191f, 4.0f, offset) && p.paused,
	      "the trigger held, the engine's release not reached: a touch cast waits at its start");
	Check(offset > -4.0f - 1e-4f && offset < -4.0f + 1e-4f,
	      "at time 0, not at the last play's time the sequence still reads on its first frame (14.2 s measured)");
	Check(StepCastAnimPause(p, false, true, 0x23, 0.02f, 9.0f, offset) && offset > -9.0f - 1e-4f &&
	          offset < -9.0f + 1e-4f,
	      "seconds later: still there");
	offset = -99.0f;
	Check(!StepCastAnimPause(p, false, false, 0x23, 0.01f, 9.1f, offset) && offset == -99.0f && !p.paused,
	      "let go: the whole animation plays, and the engine releases at its own key");

	CastAnimPause t;
	offset = -99.0f;
	Check(!StepCastAnimPause(t, false, true, 0x24, 0.1f, 4.0f, offset) && offset == -99.0f && !t.paused,
	      "a target spell held: its first half plays as before");
	Check(StepCastAnimPause(t, true, true, 0x24, 0.403f, 4.5f, offset) && t.paused, "and it waits at its release key");

	SpellChargeSettings s;
	SpellChargeState st;
	SpellChargeStep r = Frame(st, true, s);
	Check(r.castHeld, "the press: the cast held");
	bool cast = false, hold = false, fire = false;
	r = For(st, true, 1.0f, s, cast, hold, fire);
	Check(r.castHeld, "held through the charge: still held");
	r = Frame(st, false, s);
	Check(!r.castHeld && r.fireRelease, "the let go: not held, the frame the release is asked for");
	SpellChargeState tap;
	Frame(tap, true, s);
	Check(!Frame(tap, false, s).castHeld, "a tap's let go: not held");
	SpellChargeState menu;
	For(menu, true, 1.0f, s, cast, hold, fire);
	Check(!Frame(menu, true, s, true).castHeld, "a menu drops the charge: not held");
	Check(!Frame(menu, true, s).castHeld, "nor while the trigger stays down after it");
	SpellChargeSettings off;
	off.enabled = false;
	SpellChargeState vanilla;
	Check(!Frame(vanilla, true, off).castHeld, "switched off: never held, the cast as vanilla");
}

void TestOrbUntilTheSpellGoes() {
	std::printf("The spell in the hand stays until the spell leaves it (a touch spell's whole animation first)\n");
	SpellChargeSettings s;
	SpellChargeState st;
	bool cast = false, hold = false, fire = false;
	For(st, true, 2.0f, s, cast, hold, fire);
	SpellChargeStep r = Frame(st, false, s);
	Check(r.fireRelease, "let go at stage 3");
	SpellOrbView v = SpellOrbFor(r, s, true);
	Check(v.shown && v.full && v.scale == kSpellOrbFullScale,
	      "the release asked for, not yet made: still in the hand, at the size it reached");
	r = For(st, false, 0.26f, s, cast, hold, fire);
	v = SpellOrbFor(r, s, true);
	Check(v.shown && v.full, "the touch animation playing, the engine's release not come yet: still there");
	Check(!SpellOrbFor(r, s, false).shown, "the engine's release made: gone");
	Check(!SpellOrbFor(r, s).shown, "nothing awaited (a target spell's release made at the let go): gone at once");

	SpellChargeState mid;
	For(mid, true, 0.5f, s, cast, hold, fire);
	r = Frame(mid, false, s);
	v = SpellOrbFor(r, s, true);
	Check(v.shown && !v.full && v.scale > kSpellOrbFirstScale && v.scale < kSpellOrbFullScale,
	      "let go at stage 1: kept at the size it had then");

	SpellChargeState tap;
	Frame(tap, true, s);
	r = Frame(tap, false, s);
	Check(!SpellOrbFor(r, s, true).shown, "a tap: never in the hand, whatever is awaited");

	SpellChargeState menu;
	For(menu, true, 1.0f, s, cast, hold, fire);
	r = Frame(menu, true, s, true);
	Check(!SpellOrbFor(r, s, false).shown, "dropped by a menu (nothing awaited): gone");
}

void TestClockStep() {
	std::printf("The anim clock's step between two frames\n");
	const float step = AnimClockStep(10.0139f, 10.0f);
	Check(step > 0.0138f && step < 0.0140f, "a frame at 72 Hz: its 14 ms");
	Check(AnimClockStep(0.2f, 10.0f) == 0.0f, "the clock went back (a new anim data, a load): no step");
	Check(AnimClockStep(12.0f, 10.0f) == 0.0f, "a jump of seconds (a load, a wait): no step");
	Check(AnimClockStep(10.0f, 10.0f) == 0.0f, "the game paused: no step");
	const float managerTime = 50.0f, offset = -49.36f;  // the sequence at 0.64 s
	const float later = 50.0f + 0.0139f;
	const float still = StillSequenceOffset(offset, 0.0139f);
	Check(later + still > 0.6399f && later + still < 0.6401f, "kept still: a frame later, still at 0.64 s");
	Check(managerTime + offset > 0.6399f && managerTime + offset < 0.6401f, "(at 0.64 s before the step)");
}

}  // namespace

int main() {
	TestTap();
	TestPulse();
	TestCharge();
	TestStageAtLetGo();
	TestMenuDrops();
	TestPressWhileHeld();
	TestOff();
	TestOrb();
	TestStrength();
	TestCastAnimPause();
	TestTouchWaitsAtItsStart();
	TestOrbUntilTheSpellGoes();
	TestClockStep();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

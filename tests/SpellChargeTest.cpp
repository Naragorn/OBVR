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
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

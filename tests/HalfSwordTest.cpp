// Checks half-swording (vr/HalfSwordLogic.h): which weapons, where the blade
// is as a place for the left hand, and that the two-hand grip takes it there.

#include <cstdio>
#include <limits>

#include "vr/HalfSwordLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

using namespace obvr;
using namespace obvr::vr;

// A claymore's handle as StepTwoHands measures it: the pommel 20 below the
// right palm, the game's own left hand 7 below, the tip 90 above.
HandleSpan Handle() { return HandleSpanFor(TwoHandSettings{}, true, -20.0f, -7.0f); }

void TestWeapon() {
	std::printf("Which weapons\n");
	HalfSwordSettings s;
	Check(s.enabled, "on by default");
	Check(HalfSwordWeapon(s, 1), "a two-handed blade: yes");
	Check(!HalfSwordWeapon(s, 0), "a one-handed blade: no");
	Check(!HalfSwordWeapon(s, 2) && !HalfSwordWeapon(s, 3), "blunt weapons: no");
	Check(!HalfSwordWeapon(s, 4) && !HalfSwordWeapon(s, 5) && !HalfSwordWeapon(s, -1), "staff, bow, nothing: no");
	HalfSwordSettings off;
	off.enabled = false;
	Check(!HalfSwordWeapon(off, 1), "HalfSwording=0: no");
}

void TestBlade() {
	std::printf("The blade as a place\n");
	HalfSwordSettings s;
	const HandleSpan h = WithBlade(s, Handle(), 90.0f);
	Check(h.valid && h.above, "measured: the blade added");
	Check(Near(h.aboveLow, 12.0f) && Near(h.aboveHigh, 82.0f), "from past the guard to short of the tip");
	Check(Near(h.low, Handle().low) && Near(h.high, Handle().high), "the handle below unchanged");
	Check(!WithBlade(s, HandleSpan{}, 90.0f).above, "the handle not measured: nothing added");
	Check(!WithBlade(s, Handle(), std::numeric_limits<float>::quiet_NaN()).above, "no tip: nothing added");
	Check(!WithBlade(s, Handle(), 18.0f).above, "too short a blade: nothing added");
	HalfSwordSettings odd;
	odd.fromUnits = -1.0f;
	odd.tipInsetUnits = -1.0f;
	const HandleSpan d = WithBlade(odd, Handle(), 90.0f);
	Check(Near(d.aboveLow, 12.0f) && Near(d.aboveHigh, 82.0f), "settings out of range: the defaults");
}

void TestTakes() {
	std::printf("The left hand takes the blade\n");
	const TwoHandSettings t;
	const HandleSpan plain = Handle();
	const HandleSpan blade = WithBlade(HalfSwordSettings{}, Handle(), 90.0f);
	Check(!TwoHandTakes(t, true, true, 40.0f, 3.0f, plain), "without half-swording the blade is refused");
	Check(TwoHandTakes(t, true, true, 40.0f, 3.0f, blade), "with it the blade is taken");
	Check(TwoHandTakes(t, true, true, -10.0f, 3.0f, blade), "the handle below still taken");
	Check(!TwoHandTakes(t, true, true, 40.0f, 20.0f, blade), "20 units beside the blade: not");
	Check(!TwoHandTakes(t, true, true, 110.0f, 3.0f, blade), "past the tip: not");
	Check(Near(LeftHandAxial(blade, 40.0f, -7.0f), 40.0f), "held where it closed");
	Check(Near(LeftHandAxial(blade, 5.0f, -7.0f), 12.0f), "on the guard: put past it");
	Check(Near(LeftHandAxial(blade, 88.0f, -7.0f), 82.0f), "at the tip: put short of it");
	Check(Near(NearestOnHandle(blade, -7.0f, 40.0f, 6.0f).distance, 6.0f), "near the blade: shaped by its distance");
	Check(Near(SlideAxial(blade, 40.0f, 60.0f, -7.0f), 60.0f), "slides up the blade");
	Check(Near(SlideAxial(blade, 40.0f, -5.0f, -7.0f), 12.0f), "does not slide through the right hand");
}

}  // namespace

int main() {
	TestWeapon();
	TestBlade();
	TestTakes();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

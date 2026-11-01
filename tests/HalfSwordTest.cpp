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

// The right hand's hold through one frame each.
RightHoldInput Frame(bool rightHold, bool closedNow, float rightAxial, float rightLateral = 3.0f) {
	RightHoldInput in;
	in.halfSwordDrawn = true;
	in.leftHolds = true;
	in.leftAxial = 30.0f;
	in.rightHold = rightHold;
	in.rightClosedNow = closedNow;
	in.rightAxial = rightAxial;
	in.rightLateral = rightLateral;
	in.handle = WithBlade(HalfSwordSettings{}, Handle(), 90.0f);
	in.dt = 0.05f;
	return in;
}

// Opens the right hand on the weapon for `seconds`.
RightHoldEvent OpenFor(RightHoldState& st, RightHoldInput in, float seconds) {
	in.rightHold = false;
	in.rightClosedNow = false;
	RightHoldEvent last = RightHoldEvent::None;
	for (float t = 0.0f; t < seconds; t += in.dt) {
		const RightHoldEvent e = StepRightHold(st, HalfSwordSettings{}, TwoHandSettings{}, in);
		if (e != RightHoldEvent::None) {
			last = e;
		}
	}
	return last;
}

void TestRightHold() {
	std::printf("The right hand on the blade\n");
	const HalfSwordSettings hs;
	const TwoHandSettings th;
	{
		RightHoldState st;
		Check(st.hold == RightHold::Handle, "at first on the handle");
		Check(WeaponHeldByOther(st, Frame(false, false, 0.0f)), "the left on the blade: the right letting go drops nothing");
		RightHoldInput onHandle = Frame(false, false, 0.0f);
		onHandle.leftAxial = -7.0f;
		Check(!WeaponHeldByOther(st, onHandle), "the left on the handle: the right letting go drops it, as before");
		RightHoldInput alone = Frame(false, false, 0.0f);
		alone.leftHolds = false;
		Check(!WeaponHeldByOther(st, alone), "the left not holding: dropped");
		Check(OpenFor(st, Frame(false, false, 0.0f), 0.02f) == RightHoldEvent::None && st.hold == RightHold::Handle,
		      "a flicker of the grip: still on the handle");
		Check(OpenFor(st, Frame(false, false, 0.0f), 0.3f) == RightHoldEvent::LetGo && st.hold == RightHold::Free,
		      "opened past the release time: let go, the left hand carries the weapon");
		Check(WeaponHeldByOther(st, Frame(false, false, 0.0f)), "free: the left holds it");
		Check(!EngineStrikes(st), "free: no blade strike of the engine's");
		RightHoldEvent e = StepRightHold(st, hs, th, Frame(true, true, 50.0f, 20.0f));
		Check(e == RightHoldEvent::Missed, "closed 20 units off the blade: missed");
		e = StepRightHold(st, hs, th, Frame(true, true, 50.0f));
		Check(e == RightHoldEvent::TookBlade && st.hold == RightHold::Blade && Near(st.gripAxial, 50.0f) &&
		          Near(st.blend, 0.0f),
		      "closed on the blade above the left hand: holds it there, the weapon on its way to the hand");
		Check(!EngineStrikes(st) && HiltStrikes(st), "both on the blade: the hilt strikes, not the engine's blade");
		StepRightHold(st, hs, th, Frame(true, false, 50.0f));
		Check(st.blend > 0.0f && st.blend < 1.0f, "the weapon comes to the hand over the blend");
		for (int i = 0; i < 10; ++i) {
			StepRightHold(st, hs, th, Frame(true, false, 50.0f));
		}
		Check(Near(st.blend, 1.0f), "and arrives");
		Check(OpenFor(st, Frame(false, false, 50.0f), 0.3f) == RightHoldEvent::LetGo && st.hold == RightHold::Free,
		      "the right opens on the blade: free again, the left carries it");
		e = StepRightHold(st, hs, th, Frame(true, true, 3.0f));
		Check(e == RightHoldEvent::TookHandle && st.hold == RightHold::Handle && EngineStrikes(st),
		      "closed on the handle: back as it was, the engine's strike again");
	}
	{
		RightHoldState st;
		OpenFor(st, Frame(false, false, 0.0f), 0.3f);
		StepRightHold(st, hs, th, Frame(true, true, 88.0f));
		Check(st.hold == RightHold::Blade && Near(st.gripAxial, 82.0f), "closed at the tip: put short of it");
		RightHoldInput lost = Frame(false, false, 50.0f);
		lost.leftHolds = false;
		Check(OpenFor(st, lost, 0.3f) == RightHoldEvent::Lost && st.hold == RightHold::Handle,
		      "both hands open: nobody holds it - lost (the release drops it)");
	}
	{
		RightHoldState st;
		OpenFor(st, Frame(false, false, 0.0f), 0.3f);
		RightHoldInput gone = Frame(false, false, 0.0f);
		gone.leftHolds = false;
		Check(StepRightHold(st, hs, th, gone) == RightHoldEvent::Lost && st.hold == RightHold::Handle,
		      "free, and the left lets go: lost");
	}
	{
		RightHoldState st;
		OpenFor(st, Frame(false, false, 0.0f), 0.3f);
		StepRightHold(st, hs, th, Frame(true, true, 50.0f));
		RightHoldInput one = Frame(true, false, 50.0f);
		one.leftHolds = false;
		StepRightHold(st, hs, th, one);
		Check(st.hold == RightHold::Blade, "on the blade, the left lets go: the right holds it alone there");
		RightHoldInput sheathed = Frame(true, false, 50.0f);
		sheathed.halfSwordDrawn = false;
		Check(StepRightHold(st, hs, th, sheathed) == RightHoldEvent::Reset && st.hold == RightHold::Handle &&
		          Near(st.blend, 1.0f),
		      "the sword sheathed or gone: back on the handle at once");
		Check(StepRightHold(st, hs, th, sheathed) == RightHoldEvent::None, "and nothing more said");
	}
	{
		RightHoldState st;
		RightHoldInput tooShort = Frame(false, false, 0.0f);
		tooShort.leftAxial = 80.0f;
		OpenFor(st, tooShort, 0.3f);
		RightHoldInput close = tooShort;
		close.rightHold = true;
		close.rightClosedNow = true;
		close.rightAxial = 85.0f;
		Check(StepRightHold(st, hs, th, close) == RightHoldEvent::Missed,
		      "no room past the left hand on that side, the handle far: missed");
	}
	{
		// Nadi in the headset, 2026-11-01: the left hand at 50 on the blade, the
		// right closed between it and the guard every time (12 to 33 units up).
		RightHoldState st;
		RightHoldInput in = Frame(false, false, 0.0f);
		in.leftAxial = 50.0f;
		OpenFor(st, in, 0.3f);
		in.rightHold = true;
		in.rightClosedNow = true;
		in.rightAxial = 28.5f;
		in.rightLateral = 2.1f;
		Check(StepRightHold(st, hs, th, in) == RightHoldEvent::TookBlade && Near(st.gripAxial, 28.5f),
		      "closed on the blade between the guard and the left hand: holds it there");
	}
	{
		RightHoldState st;
		RightHoldInput in = Frame(false, false, 0.0f);
		in.leftAxial = 50.0f;
		OpenFor(st, in, 0.3f);
		in.rightHold = true;
		in.rightClosedNow = true;
		in.rightAxial = 46.0f;
		Check(StepRightHold(st, hs, th, in) == RightHoldEvent::TookBlade && Near(st.gripAxial, 43.0f),
		      "closed against the left hand: put a fist's width below it");
	}
	{
		RightHoldState st;
		RightHoldInput in = Frame(false, false, 0.0f);
		in.leftAxial = 50.0f;
		OpenFor(st, in, 0.3f);
		in.rightHold = true;
		in.rightClosedNow = true;
		in.rightAxial = 8.9f;
		in.rightLateral = 0.7f;
		Check(StepRightHold(st, hs, th, in) == RightHoldEvent::TookHandle,
		      "closed 9 units up, below the blade: the handle (as in the headset)");
	}
	{
		HalfSwordSettings off;
		off.rightHand = false;
		RightHoldState st;
		Check(!WeaponHeldByOther(st, Frame(false, false, 0.0f), off), "HalfSwordRightHand=0: the right letting go drops it");
		RightHoldInput in = Frame(false, false, 0.0f);
		for (int i = 0; i < 10; ++i) {
			StepRightHold(st, off, th, in);
		}
		Check(st.hold == RightHold::Handle, "and it never lets the left carry it");
	}
}

void TestNearestPlace() {
	std::printf("Where the free right hand would take hold\n");
	RightHoldInput in = Frame(false, false, 20.0f, 4.0f);
	RightPlace p = NearestRightPlace(in);
	Check(p.valid && p.blade && Near(p.axial, 20.0f) && Near(p.distance, 4.0f), "beside the blade: there, 4 off");
	in.rightAxial = 33.0f;
	p = NearestRightPlace(in);
	Check(p.blade && Near(p.axial, 37.0f), "on the left hand's tip side, too close: a fist's width past it");
	in.rightAxial = -10.0f;
	in.rightLateral = 3.0f;
	p = NearestRightPlace(in);
	Check(!p.blade && Near(p.axial, 0.0f) && Near(p.distance, 3.0f), "on the handle: the game's place, 3 off");
	in.rightAxial = -40.0f;
	p = NearestRightPlace(in);
	Check(!p.blade && p.distance > 20.0f, "past the pommel: the handle, far");
	in.leftHolds = false;
	Check(!NearestRightPlace(in).valid, "the left hand not holding: nowhere");
	in = Frame(false, false, 20.0f);
	in.rightLateral = std::numeric_limits<float>::quiet_NaN();
	Check(!NearestRightPlace(in).valid, "no distance from the line: nowhere");
	in = Frame(false, false, 20.0f);
	in.handle = Handle();
	p = NearestRightPlace(in);
	Check(p.valid && !p.blade, "no blade measured: only the handle");
}

void TestPoses() {
	std::printf("Where the weapon is\n");
	WeaponPose current;
	current.rot = NiMatrix33::Identity();  // the blade along +y
	current.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	bool reversed = true;
	const NiPoint3 fistPoint{10.0f, 0.0f, 0.0f};
	WeaponPose p = BladeHoldPose(current, NiPoint3{0.0f, 1.0f, 0.0f}, fistPoint, 40.0f, &reversed);
	Check(!reversed && Near(p.pos.x, 10.0f) && Near(p.pos.y, -40.0f), "the fist along the blade: slid down to it, not turned");
	const NiPoint3 at = p.pos + p.rot * NiPoint3{0.0f, 40.0f, 0.0f};
	Check(Near(at.x, 10.0f) && Near(at.y, 0.0f), "the grip's point in the fist");
	p = BladeHoldPose(current, NiPoint3{0.0f, -1.0f, 0.0f}, fistPoint, 40.0f, &reversed);
	const NiPoint3 axis = p.rot * NiPoint3{0.0f, 1.0f, 0.0f};
	Check(reversed && Near(axis.y, 1.0f), "the fist the other way along it: held the other way round (reversed), not turned");
	p = BladeHoldPose(current, NiPoint3{1.0f, 0.0f, 0.2f}, fistPoint, 40.0f, &reversed);
	const NiPoint3 tilted = p.rot * NiPoint3{0.0f, 1.0f, 0.0f};
	Check(Near(tilted.x * tilted.x + tilted.z * tilted.z, 1.0f) && Near(tilted.y, 0.0f), "a fist across it: turned onto the fist");
	const NiPoint3 grip = p.pos + p.rot * NiPoint3{0.0f, 40.0f, 0.0f};
	Check(Near(grip.x, 10.0f) && Near(grip.y, 0.0f) && Near(grip.z, 0.0f), "its grip's point in the fist still");
	WeaponPose turned;
	turned.rot = RotationBetween(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, -1.0f, 0.0f});
	turned.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	BladeHoldPose(turned, NiPoint3{0.0f, 1.0f, 0.0f}, fistPoint, 40.0f, &reversed);
	Check(reversed, "the blade pointing back along the fist: reversed (the hilt ahead)");

	// The hand brought to the sword (FistOnBlade).
	WeaponPose hand2;
	hand2.rot = NiMatrix33::Identity();  // the fist's line along +y
	hand2.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	const NiPoint3 fistLine{0.0f, 1.0f, 0.0f};
	const NiPoint3 fistAt{0.0f, 2.0f, 0.0f};
	WeaponPose h = FistOnBlade(hand2, fistLine, fistAt, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{5.0f, 10.0f, 0.0f}, true);
	Check(Near(h.pos.x, 5.0f) && Near(h.pos.y, 8.0f), "along the sword already: only moved onto it");
	h = FistOnBlade(hand2, fistLine, fistAt, NiPoint3{0.0f, -1.0f, 0.0f}, NiPoint3{5.0f, 10.0f, 0.0f}, true);
	Check(Near((h.rot * fistLine).y, 1.0f), "the sword the other way along the fist: held either way, not turned");
	h = FistOnBlade(hand2, fistLine, fistAt, NiPoint3{0.0f, -1.0f, 0.0f}, NiPoint3{5.0f, 10.0f, 0.0f}, false);
	Check(Near((h.rot * fistLine).y, -1.0f), "on the handle only the game's way: turned round");
	h = FistOnBlade(hand2, fistLine, fistAt, NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{5.0f, 10.0f, 0.0f}, true);
	const NiPoint3 fp = h.pos + h.rot * fistAt;
	Check(Near((h.rot * fistLine).x, 1.0f) && Near(fp.x, 5.0f) && Near(fp.y, 10.0f),
	      "across the sword: tilted onto it, the fist's point on the sword");

	WeaponPose hand;
	hand.rot = RotationBetween(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f});
	hand.pos = NiPoint3{5.0f, 6.0f, 7.0f};
	WeaponPose world;
	world.rot = RotationBetween(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f});
	world.pos = NiPoint3{1.0f, 2.0f, 3.0f};
	const WeaponPose back = Compose(hand, RelativeTo(hand, world));
	Check(Near(back.pos.x, 1.0f) && Near(back.pos.y, 2.0f) && Near(back.pos.z, 3.0f) &&
	          Near((back.rot * NiPoint3{0.0f, 1.0f, 0.0f}).z, 1.0f),
	      "carried by a hand: the same pose back");
	WeaponPose b;
	b.rot = NiMatrix33::Identity();
	b.pos = NiPoint3{10.0f, 0.0f, 0.0f};
	const WeaponPose half = BlendPose(current, b, 0.5f);
	Check(Near(half.pos.x, 5.0f), "half way between two poses");
	Check(Near(BlendPose(current, b, 2.0f).pos.x, 10.0f) && Near(BlendPose(current, b, -1.0f).pos.x, 0.0f),
	      "a share out of range: kept to the ends");
}

void TestHilt() {
	std::printf("The hilt strikes\n");
	game::PommelSettings p;
	Check(HiltStrikeFor(p, true, 1.0f) == game::ShoveKind::None, "too slow: nothing");
	Check(HiltStrikeFor(p, true, 2.5f) == game::ShoveKind::Light, "from 2 m/s: they stagger");
	Check(HiltStrikeFor(p, true, 4.5f) == game::ShoveKind::Hard, "from 4 m/s: they fall");
	Check(HiltStrikeFor(p, false, 4.5f) == game::ShoveKind::None, "not both on the blade: nothing");
	game::PommelSettings off;
	off.enabled = false;
	Check(HiltStrikeFor(off, true, 4.5f) == game::ShoveKind::None, "PommelStrikes=0: nothing");
	Check(HiltStrikeFor(p, true, std::numeric_limits<float>::quiet_NaN()) == game::ShoveKind::None, "no speed: nothing");
}

}  // namespace

int main() {
	TestWeapon();
	TestBlade();
	TestTakes();
	TestRightHold();
	TestNearestPlace();
	TestPoses();
	TestHilt();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

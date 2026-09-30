// Checks holding a two-handed weapon with both hands (vr/TwoHandLogic.h).

#include <cstdio>
#include <limits>

#include "vr/TwoHandLogic.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;
const float kNaN = std::numeric_limits<float>::quiet_NaN();

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }
bool NearV(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

void TestPlace() {
	std::printf("Where the left hand is\n");
	float axial = 0.0f, lateral = 0.0f;
	AxialLateral(NiPoint3{3.0f, 20.0f, 0.0f}, NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, axial, lateral);
	Check(Near(axial, 20.0f) && Near(lateral, 3.0f), "20 up the line, 3 beside it");
	AxialLateral(NiPoint3{0.0f, -15.0f, 4.0f}, NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, axial, lateral);
	Check(Near(axial, -15.0f) && Near(lateral, 4.0f), "15 behind, towards the pommel");
}

void TestTake() {
	std::printf("Taking hold\n");
	const TwoHandSettings s;
	Check(TwoHandTakes(s, true, true, -15.0f, 3.0f), "below the right hand, on the pommel side: taken");
	Check(TwoHandTakes(s, true, true, -5.0f, 3.0f), "a hand's width below: taken");
	Check(!TwoHandTakes(s, true, true, 20.0f, 3.0f), "above the right hand, on the blade: not");
	Check(!TwoHandTakes(s, false, true, -15.0f, 3.0f), "a one-hander or sheathed: not");
	Check(!TwoHandTakes(s, true, false, -15.0f, 3.0f), "the grip already closed: not");
	Check(!TwoHandTakes(s, true, true, -15.0f, 20.0f), "20 units beside the line: not");
	Check(!TwoHandTakes(s, true, true, -40.0f, 3.0f), "far behind: not");
	Check(TwoHandTakes(s, true, true, -2.0f, 3.0f), "2 below, right beside the right hand: taken");
	Check(!TwoHandTakes(s, true, true, -1.0f, 3.0f), "on the right hand itself: not");
	Check(!TwoHandTakes(s, true, true, -15.0f, kNaN), "no distance from the line: not");
	TwoHandSettings off;
	off.enabled = false;
	Check(!TwoHandTakes(off, true, true, -15.0f, 3.0f), "switched off: not");
}

void TestHandle() {
	std::printf("The measured handle\n");
	const TwoHandSettings s;
	const HandleSpan h = HandleSpanFor(s, true, -30.0f, -6.0f);
	Check(h.valid && Near(h.low, -27.0f) && Near(h.high, -6.0f),
	      "a pommel 30 below, the game's hand 6 below: the handle from 27 to 6 below");
	Check(TwoHandTakes(s, true, true, -15.0f, 3.0f, h), "on the handle: taken");
	Check(TwoHandTakes(s, true, true, -32.0f, 3.0f, h), "5 past the pommel end: taken");
	Check(!TwoHandTakes(s, true, true, -40.0f, 3.0f, h), "13 past the pommel end, in the air: not");
	Check(!TwoHandTakes(s, true, true, 20.0f, 3.0f, h), "above the right hand, on the blade: not");
	Check(!TwoHandTakes(s, true, true, -15.0f, 20.0f, h), "beside the handle: not");
	Check(Near(LeftHandAxial(h, -35.0f, -6.0f), -27.0f), "past the end: put on the end");
	Check(Near(LeftHandAxial(h, -15.0f, -6.0f), -15.0f), "on the handle: where it closed");
	Check(Near(LeftHandAxial(h, -5.0f, -6.0f), -6.0f), "closer than the game's hand: right under the right hand");
	const HandleSpan shortHandle = HandleSpanFor(s, true, -8.0f, -6.0f);
	Check(shortHandle.valid && Near(shortHandle.low, -5.0f) && Near(shortHandle.high, -5.0f),
	      "a one-hander's short handle: the one place at its pommel end, cupping it");
	Check(Near(LeftHandAxial(shortHandle, -15.0f, -6.0f), -5.0f), "wherever it closes: on the pommel");
	Check(!HandleSpanFor(s, false, -30.0f, -6.0f).valid, "not measured: nothing");
	Check(!HandleSpanFor(s, true, kNaN, -6.0f).valid, "no pommel: nothing");
	Check(!HandleSpanFor(s, true, -30.0f, kNaN).valid, "no game's hand: nothing");
	Check(Near(LeftHandAxial(HandleSpan{}, -20.0f, -6.0f), -6.0f), "no measured handle: where the game holds it");
	Check(!h.above && !HandleSpanFor(s, true, -30.0f, -6.0f, false, 40.0f).above,
	      "not a staff: nothing above the right hand");

	std::printf("A staff's shaft, both sides of the right hand\n");
	// The tester's staff (2026-09-30): 81 below the right palm, 39 above.
	const HandleSpan staff = HandleSpanFor(s, true, -81.0f, -7.0f, true, 39.0f);
	Check(staff.valid && staff.above && Near(staff.low, -78.0f) && Near(staff.high, -7.0f) &&
	          Near(staff.aboveLow, 7.0f) && Near(staff.aboveHigh, 36.0f),
	      "the shaft from 78 below to 7 below, and from 7 above to 36 above");
	Check(TwoHandTakes(s, true, true, 24.0f, 5.0f, staff) && TwoHandTakes(s, true, true, 35.0f, 5.0f, staff),
	      "24 and 35 above the right hand, the tester's grips: taken");
	Check(TwoHandTakes(s, true, true, -60.0f, 5.0f, staff), "far down the shaft: taken");
	Check(TwoHandTakes(s, true, true, 44.0f, 5.0f, staff) && !TwoHandTakes(s, true, true, 47.0f, 5.0f, staff),
	      "past the far end: up to the overhang, not beyond");
	Check(!TwoHandTakes(s, true, true, 1.0f, 5.0f, staff) && !TwoHandTakes(s, true, true, -1.0f, 5.0f, staff),
	      "on the right hand itself: not");
	Check(!TwoHandTakes(s, true, true, 24.0f, 20.0f, staff), "beside the shaft: not");
	Check(Near(LeftHandAxial(staff, 24.0f, -7.0f), 24.0f) && Near(LeftHandAxial(staff, -40.0f, -7.0f), -40.0f),
	      "put where it closed, on either side");
	Check(Near(LeftHandAxial(staff, 3.0f, -7.0f), 7.0f) && Near(LeftHandAxial(staff, -3.0f, -7.0f), -7.0f),
	      "closer than a fist to the right hand: a fist's width off it, on its own side");
	Check(Near(LeftHandAxial(staff, 50.0f, -7.0f), 36.0f), "past the far end: on the end");
	const HandleSpan tipless = HandleSpanFor(s, true, -81.0f, -7.0f, true, kNaN);
	Check(tipless.valid && !tipless.above, "no far end measured: below only");
	Check(!HandleSpanFor(s, true, -81.0f, -7.0f, true, 9.0f).above, "no room above the right hand: below only");

	std::printf("Where the shaft lies across the node's axis\n");
	Check(NearV(ShaftOffset(true, 40, NiPoint3{3.0f, -37.0f, -4.0f}), NiPoint3{3.0f, 0.0f, -4.0f}),
	      "a staff's shaft 5 off the axis: moved across by it, not along");
	Check(NearV(ShaftOffset(false, 40, NiPoint3{3.0f, 0.0f, -4.0f}), NiPoint3{0.0f, 0.0f, 0.0f}),
	      "not measured: the node's own axis");
	Check(NearV(ShaftOffset(true, kShaftMinVertices - 1, NiPoint3{3.0f, 0.0f, -4.0f}), NiPoint3{0.0f, 0.0f, 0.0f}) &&
	          NearV(ShaftOffset(true, kShaftMinVertices, NiPoint3{3.0f, 0.0f, -4.0f}), NiPoint3{3.0f, 0.0f, -4.0f}),
	      "too few vertices there: the axis; enough: the shaft");
	Check(NearV(ShaftOffset(true, 40, NiPoint3{12.0f, 0.0f, 12.0f}), NiPoint3{0.0f, 0.0f, 0.0f}),
	      "17 off, further than a shaft from the hand: the axis");
	Check(NearV(ShaftOffset(true, 40, NiPoint3{kNaN, 0.0f, 1.0f}), NiPoint3{0.0f, 0.0f, 0.0f}), "no number: the axis");

	std::printf("Sliding keeps to its side of the right hand\n");
	Check(Near(SlideAxial(staff, 20.0f, -10.0f, -7.0f), 7.0f), "held above, the controller below: stays above");
	Check(Near(SlideAxial(staff, -20.0f, 10.0f, -7.0f), -7.0f), "held below, the controller above: stays below");
	Check(Near(SlideAxial(staff, 20.0f, 30.0f, -7.0f), 30.0f) && Near(SlideAxial(staff, -20.0f, -50.0f, -7.0f), -50.0f),
	      "along its own side: follows");
	Check(Near(SlideAxial(h, -15.0f, 10.0f, -6.0f), -6.0f), "a sword: never above");
	Check(Near(SlideAxial(HandleSpan{}, -15.0f, -20.0f, -6.0f), -6.0f), "no measured handle: the game's place");
}

void TestVanillaGrip() {
	std::printf("The game's own left hand\n");
	// The weapon turned a quarter round z: its axis (+y) is the world's -x.
	NiMatrix33 w{};
	w.data[0][1] = -1.0f;
	w.data[1][0] = 1.0f;
	w.data[2][2] = 1.0f;
	const NiPoint3 weaponAt{10.0f, 0.0f, 0.0f};
	// The left hand 8 down the weapon and 6 beside it, turned with it; the
	// left palm 7 down, 3 beside; the right palm 1 up.
	const NiPoint3 leftBone = weaponAt + w * NiPoint3{0.0f, -8.0f, 6.0f};
	const NiPoint3 leftPalm = weaponAt + w * NiPoint3{3.0f, -7.0f, 0.0f};
	const NiPoint3 rightPalm = weaponAt + w * NiPoint3{0.0f, 1.0f, 2.0f};
	const VanillaGrip g = VanillaGripFrom(w, weaponAt, w, leftBone, leftPalm, rightPalm, 12.0f);
	Check(g.valid && NearV(g.pos, NiPoint3{0.0f, -8.0f, 6.0f}), "the bone in the weapon's frame");
	Check(NearV(g.rot * NiPoint3{1, 2, 3}, NiPoint3{1, 2, 3}), "turned with the weapon: no turn against it");
	Check(Near(g.leftPalmAxial, -7.0f) && Near(g.rightPalmAxial, 1.0f) && Near(g.Below(), -8.0f),
	      "the palms along the axis: the left 8 below the right");
	Check(Near(g.leftPalmLateral, 3.0f), "3 from the axis");
	const NiPoint3 farPalm = weaponAt + w * NiPoint3{20.0f, -7.0f, 0.0f};
	Check(!VanillaGripFrom(w, weaponAt, w, leftBone, farPalm, rightPalm, 12.0f).valid,
	      "the left palm 20 from the axis: not holding the handle");
	const NiPoint3 abovePalm = weaponAt + w * NiPoint3{3.0f, 5.0f, 0.0f};
	Check(!VanillaGripFrom(w, weaponAt, w, leftBone, abovePalm, rightPalm, 12.0f).valid,
	      "the left palm above the right: not holding the handle");

	std::printf("The left hand put on the handle\n");
	const NiPoint3 moved{0.0f, 0.0f, 50.0f};
	const LeftHandPose same = LeftHandOnHandle(g, g.Below(), w, moved, w * g.rot, g.rightPalmAxial);
	Check(NearV(same.pos, moved + w * g.pos) && NearV(same.rot * NiPoint3{1, 0, 0}, w * NiPoint3{1, 0, 0}),
	      "at the game's place: the game's pose, carried with the weapon");
	const LeftHandPose lower = LeftHandOnHandle(g, -14.0f, w, moved, w * g.rot, g.rightPalmAxial);
	Check(NearV(lower.pos, moved + w * NiPoint3{0.0f, -14.0f, 6.0f}),
	      "6 further down: the same grip, 6 further down the axis");

	std::printf("The player's own turn of the hand kept\n");
	// Turned 90 degrees round the handle (the weapon's y): kept whole.
	NiMatrix33 roundY{};
	roundY.data[0][2] = 1.0f;
	roundY.data[1][1] = 1.0f;
	roundY.data[2][0] = -1.0f;
	const NiMatrix33 twisted = w * roundY * g.rot;
	const LeftHandPose round = LeftHandOnHandle(g, g.Below(), w, moved, twisted, g.rightPalmAxial);
	Check(NearV(round.rot * NiPoint3{1, 0, 0}, twisted * NiPoint3{1, 0, 0}) &&
	          NearV(round.rot * NiPoint3{0, 0, 1}, twisted * NiPoint3{0, 0, 1}),
	      "turned round the handle: the hand's turn kept as it is");
	const NiPoint3 roundLocal = Transposed(w) * (round.pos - moved);
	Check(Near(roundLocal.y, -8.0f) && Near(roundLocal.x * roundLocal.x + roundLocal.z * roundLocal.z, 36.0f),
	      "and the bone still 8 down the handle and 6 from it: the fist round the handle");
	// Tilted 20 degrees off the handle (round the weapon's x): tilted back
	// onto it, no more.
	const float c = 0.9396926f;
	const float sn = 0.3420201f;
	NiMatrix33 tilt{};
	tilt.data[0][0] = 1.0f;
	tilt.data[1][1] = c;
	tilt.data[1][2] = -sn;
	tilt.data[2][1] = sn;
	tilt.data[2][2] = c;
	const NiMatrix33 tilted = w * tilt * g.rot;
	const LeftHandPose back = LeftHandOnHandle(g, g.Below(), w, moved, tilted, g.rightPalmAxial);
	Check(NearV(back.rot * NiPoint3{0, 1, 0}, w * NiPoint3{0, 1, 0}), "tilted off the handle: the handle along the weapon again");
	Check(NearV(back.rot * NiPoint3{1, 0, 0}, tilted * NiPoint3{1, 0, 0}),
	      "the axis of the tilt untouched: only the tilt taken out");
	const NiPoint3 backLocal = Transposed(w) * (back.pos - moved);
	Check(Near(backLocal.y, -8.0f) && Near(backLocal.x * backLocal.x + backLocal.z * backLocal.z, 36.0f),
	      "on the handle where it belongs");

	std::printf("The hand held the other way round\n");
	// Turned half round the weapon's x, then tilted 20 degrees more: its
	// handle points down the weapon. Tilted onto it the other way, 20 degrees,
	// not turned 160.
	NiMatrix33 half{};
	half.data[0][0] = 1.0f;
	half.data[1][1] = -1.0f;
	half.data[2][2] = -1.0f;
	const NiMatrix33 upsideDown = w * tilt * half * g.rot;
	const LeftHandPose flipped = LeftHandOnHandle(g, -20.0f, w, moved, upsideDown, g.rightPalmAxial);
	Check(NearV(flipped.rot * NiPoint3{0, 1, 0}, w * NiPoint3{0, -1, 0}),
	      "its handle down the weapon: along it the other way");
	Check(NearV(flipped.rot * NiPoint3{1, 0, 0}, upsideDown * NiPoint3{1, 0, 0}),
	      "only the 20 degree tilt taken out");
	const NiPoint3 flippedPalm = Transposed(w) * (flipped.pos - moved);
	// The palm 20 below the right palm (at 1): 19 down; the bone 1 past the
	// palm along the hand's own handle, which points down the weapon: 18.
	Check(Near(flippedPalm.y, -18.0f) && Near(flippedPalm.x * flippedPalm.x + flippedPalm.z * flippedPalm.z, 36.0f),
	      "the palm where it was put, the fist round the handle");

	std::printf("The right palm where this weapon has it\n");
	// A staff: the right palm 37 down the Weapon node, not 1 up as in the grip.
	const LeftHandPose onStaff = LeftHandOnHandle(g, -14.0f, w, moved, w * g.rot, -37.0f);
	Check(NearV(onStaff.pos, moved + w * NiPoint3{0.0f, -52.0f, 6.0f}),
	      "14 below that palm: 51 down the node, the bone 1 further");
}

void TestApproach() {
	std::printf("The open hand coming near the handle\n");
	const TwoHandSettings s;  // 18 to 3
	Check(Near(PreshapeWeight(s, 20.0f), 0.0f), "further than 18: nothing");
	Check(Near(PreshapeWeight(s, 10.5f), 0.5f), "halfway: half");
	Check(Near(PreshapeWeight(s, 2.0f), 1.0f), "at the handle: all of it");
	Check(PreshapeWeight(s, 15.0f) > 0.0f && PreshapeWeight(s, 15.0f) < 0.2f, "coming in: gently");
	TwoHandSettings off = s;
	off.preshapeUnits = 0.0f;
	Check(Near(PreshapeWeight(off, 2.0f), 0.0f), "switched off: nothing");
	off = s;
	off.enabled = false;
	Check(Near(PreshapeWeight(off, 2.0f), 0.0f), "two hands off: nothing");
	Check(Near(PreshapeWeight(s, kNaN), 0.0f), "no distance: nothing");

	const HandleSpan h = HandleSpanFor(s, true, -30.0f, -6.0f);  // 27 to 6 below
	HandleReach r = NearestOnHandle(h, -6.0f, -15.0f, 4.0f);
	Check(Near(r.axial, -15.0f) && Near(r.distance, 4.0f), "beside the handle: straight across to it");
	r = NearestOnHandle(h, -6.0f, -30.0f, 4.0f);
	Check(Near(r.axial, -27.0f) && Near(r.distance, 5.0f), "past its end: to the end");
	r = NearestOnHandle(h, -6.0f, 10.0f, 0.0f);
	Check(Near(r.axial, -6.0f) && Near(r.distance, 16.0f), "up at the blade: the handle's top, far off");
	r = NearestOnHandle(HandleSpan{}, -7.0f, -12.0f, 3.0f);
	Check(Near(r.axial, -7.0f) && Near(r.distance, 5.8309518f), "not measured: the game's own place");

	Check(Near(HeldWeight(0.6f, 0.0f), 0.6f) && Near(HeldWeight(0.6f, 1.0f), 1.0f),
	      "closing already shaped: from there the rest of the way");
	Check(Near(HeldWeight(0.0f, 0.5f), 0.5f) && Near(HeldWeight(2.0f, 0.0f), 1.0f) && Near(HeldWeight(-1.0f, 0.0f), 0.0f),
	      "not shaped: the whole way; out of range held to 0 to 1");
}

void TestRotationBetween() {
	std::printf("The smallest turn from one direction to another\n");
	const NiMatrix33 r = RotationBetween(NiPoint3{0, 1, 0}, NiPoint3{0, 0, 1});
	Check(NearV(r * NiPoint3{0, 1, 0}, NiPoint3{0, 0, 1}), "y onto z");
	Check(NearV(r * NiPoint3{1, 0, 0}, NiPoint3{1, 0, 0}), "the axis across both stays");
	const NiMatrix33 back = RotationBetween(NiPoint3{0, 1, 0}, NiPoint3{0, -1, 0});
	Check(NearV(back * NiPoint3{0, 1, 0}, NiPoint3{0, -1, 0}), "opposite: turned half round");
	const NiMatrix33 backX = RotationBetween(NiPoint3{1, 0, 0}, NiPoint3{-1, 0, 0});
	Check(NearV(backX * NiPoint3{1, 0, 0}, NiPoint3{-1, 0, 0}), "opposite along x: turned half round too");
	const NiMatrix33 none = RotationBetween(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0});
	Check(NearV(none * NiPoint3{1, 2, 3}, NiPoint3{1, 2, 3}), "no direction: no turn");
}

void TestHold() {
	std::printf("Holding\n");
	const TwoHandSettings s;
	TwoHandState t;
	StartTwoHand(t, -15.0f, -12.0f);
	Check(t.active && Near(t.distance, 15.0f) && Near(t.handAxial, -12.0f),
	      "taken 15 below: the controllers 15 apart, the palm put 12 below");
	Check(TwoHandHolds(t, s, true, true, 25.0f, 0.011f), "25 from the handle: still held");
	Check(TwoHandHolds(t, s, true, true, 35.0f, 0.05f) && TwoHandHolds(t, s, true, true, 35.0f, 0.05f),
	      "35 from the handle for a tenth of a second (a tracking jump): still held");
	Check(TwoHandHolds(t, s, true, true, 5.0f, 0.011f) && Near(t.farSeconds, 0.0f),
	      "back on the handle: held, the time off it forgotten");
	Check(TwoHandHolds(t, s, true, true, 35.0f, 0.1f) && !TwoHandHolds(t, s, true, true, 35.0f, 0.05f) &&
	          !t.active && Near(t.handAxial, -12.0f),
	      "35 from the handle for 0.15 s: let go, where the hand was put kept for the way back");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(TwoHandHolds(t, s, true, false, 2.0f, 0.05f) && TwoHandHolds(t, s, true, false, 2.0f, 0.05f),
	      "the grip reading open for a tenth of a second: still held");
	Check(TwoHandHolds(t, s, true, true, 2.0f, 0.011f) && Near(t.openSeconds, 0.0f),
	      "closed again: held, the open time forgotten");
	Check(TwoHandHolds(t, s, true, false, 2.0f, 0.1f) && !TwoHandHolds(t, s, true, false, 2.0f, 0.05f) && !t.active,
	      "open for 0.15 s: let go");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(TwoHandHolds(t, s, true, false, 2.0f, 0.0f), "no time passed with the grip open: held");
	TwoHandSettings now = s;
	now.releaseSeconds = 0.0f;
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, now, true, false, 2.0f, 0.011f), "no grace time: the grip opens, let go at once");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, s, false, true, 2.0f, 0.011f), "sheathed: let go");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, s, true, true, kNaN, 0.011f), "no distance: let go");
	Check(!TwoHandHolds(t, s, true, true, 2.0f, 0.011f), "not held: nothing");

	std::printf("Which weapons both hands hold\n");
	Check(TwoHandWeapon(s, true, false), "a two-hander");
	Check(TwoHandWeapon(s, false, true), "a one-hander, with one-handers on");
	TwoHandSettings noOne = s;
	noOne.oneHanders = false;
	Check(!TwoHandWeapon(noOne, false, true) && TwoHandWeapon(noOne, true, false), "one-handers off: only two-handers");
	Check(!TwoHandWeapon(s, false, false), "anything else: no");

	std::printf("Sliding along the handle\n");
	Check(TwoHandSlides(s, true), "the trigger held: the hand slides");
	Check(!TwoHandSlides(s, false), "the trigger let go: it stays where it is");
	TwoHandSettings fixedHand = s;
	fixedHand.slide = false;
	Check(!TwoHandSlides(fixedHand, true), "sliding off: never, trigger or not");
}

void TestGripFromFiles() {
	std::printf("The game's grip from its files\n");
	const VanillaGrip g = GameGripFromFiles();
	Check(g.valid && g.Below() > -7.05f && g.Below() < -6.95f, "valid, the left palm 7 below the right, as the game shows it live");
	Check(g.leftPalmLateral < kGamePalmOnHandleUnits, "its palm on the handle by the live check's own measure");
	const NiPoint3 x = g.rot * NiPoint3{1, 0, 0};
	const NiPoint3 y = g.rot * NiPoint3{0, 1, 0};
	const float dot = x.x * y.x + x.y * y.y + x.z * y.z;
	Check(Near(x.LengthSquared(), 1.0f) && Near(y.LengthSquared(), 1.0f) && dot < 1e-2f && dot > -1e-2f,
	      "a rotation: unit axes at right angles");
}

void TestBlend() {
	std::printf("The hand's way to the handle and back\n");
	const TwoHandSettings s;  // 0.2 s
	Check(Near(StepTwoHandBlend(0.0f, true, 0.05f, s), 0.25f), "held, a twentieth of a second: a quarter of the way");
	Check(Near(StepTwoHandBlend(0.9f, true, 0.05f, s), 1.0f), "the last step: arrived, no further");
	Check(Near(StepTwoHandBlend(1.0f, false, 0.05f, s), 0.75f), "let go: a quarter back");
	Check(Near(StepTwoHandBlend(0.1f, false, 0.05f, s), 0.0f), "the last step back: at the controller");
	Check(Near(StepTwoHandBlend(0.5f, true, -1.0f, s), 0.5f), "no time passed: where it was");
	TwoHandSettings now;
	now.blendSeconds = 0.0f;
	Check(Near(StepTwoHandBlend(0.0f, true, 0.01f, now), 1.0f) && Near(StepTwoHandBlend(1.0f, false, 0.01f, now), 0.0f),
	      "no blend time: at once, both ways");
	Check(Near(TwoHandBlendWeight(0.0f), 0.0f) && Near(TwoHandBlendWeight(1.0f), 1.0f) &&
	          Near(TwoHandBlendWeight(0.5f), 0.5f),
	      "eased: from nothing to all, halfway at half");
	Check(TwoHandBlendWeight(0.1f) < 0.1f && TwoHandBlendWeight(0.9f) > 0.9f, "starting and arriving gently");
	Check(Near(TwoHandBlendWeight(-1.0f), 0.0f) && Near(TwoHandBlendWeight(2.0f), 1.0f), "outside the way: its ends");
}

void TestTranspose() {
	std::printf("The transpose\n");
	NiMatrix33 r{};
	r.data[0][0] = 1.0f;
	r.data[1][2] = -1.0f;
	r.data[2][1] = 1.0f;
	const NiMatrix33 t = Transposed(r);
	Check(NearV(t * (r * NiPoint3{0.3f, 0.5f, 0.2f}), NiPoint3{0.3f, 0.5f, 0.2f}), "the transpose undoes a turn");
}

}  // namespace

int main() {
	TestPlace();
	TestTake();
	TestHandle();
	TestVanillaGrip();
	TestHold();
	TestGripFromFiles();
	TestBlend();
	TestApproach();
	TestRotationBetween();
	TestTranspose();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

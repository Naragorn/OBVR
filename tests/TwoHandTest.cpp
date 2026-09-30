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
	Check(!TwoHandTakes(s, true, true, -2.0f, 3.0f), "on the right hand itself: not");
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
	Check(!HandleSpanFor(s, true, -8.0f, -6.0f).valid, "a handle too short for the hand to move: not measured");
	Check(!HandleSpanFor(s, false, -30.0f, -6.0f).valid, "not measured: nothing");
	Check(!HandleSpanFor(s, true, kNaN, -6.0f).valid, "no pommel: nothing");
	Check(!HandleSpanFor(s, true, -30.0f, kNaN).valid, "no game's hand: nothing");
	Check(Near(LeftHandAxial(HandleSpan{}, -20.0f, -6.0f), -6.0f), "no measured handle: where the game holds it");
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
	const LeftHandPose same = LeftHandOnHandle(g, g.Below(), w, moved, w * g.rot);
	Check(NearV(same.pos, moved + w * g.pos) && NearV(same.rot * NiPoint3{1, 0, 0}, w * NiPoint3{1, 0, 0}),
	      "at the game's place: the game's pose, carried with the weapon");
	const LeftHandPose lower = LeftHandOnHandle(g, -14.0f, w, moved, w * g.rot);
	Check(NearV(lower.pos, moved + w * NiPoint3{0.0f, -14.0f, 6.0f}),
	      "6 further down: the same grip, 6 further down the axis");

	std::printf("The player's own turn of the hand kept\n");
	// Turned 90 degrees round the handle (the weapon's y): kept whole.
	NiMatrix33 roundY{};
	roundY.data[0][2] = 1.0f;
	roundY.data[1][1] = 1.0f;
	roundY.data[2][0] = -1.0f;
	const NiMatrix33 twisted = w * roundY * g.rot;
	const LeftHandPose round = LeftHandOnHandle(g, g.Below(), w, moved, twisted);
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
	const LeftHandPose back = LeftHandOnHandle(g, g.Below(), w, moved, tilted);
	Check(NearV(back.rot * NiPoint3{0, 1, 0}, w * NiPoint3{0, 1, 0}), "tilted off the handle: the handle along the weapon again");
	Check(NearV(back.rot * NiPoint3{1, 0, 0}, tilted * NiPoint3{1, 0, 0}),
	      "the axis of the tilt untouched: only the tilt taken out");
	const NiPoint3 backLocal = Transposed(w) * (back.pos - moved);
	Check(Near(backLocal.y, -8.0f) && Near(backLocal.x * backLocal.x + backLocal.z * backLocal.z, 36.0f),
	      "on the handle where it belongs");
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
	Check(TwoHandHolds(t, s, true, true, 25.0f), "10 further apart: still held");
	Check(!TwoHandHolds(t, s, true, true, 60.0f) && !t.active && Near(t.handAxial, -12.0f),
	      "45 further apart: let go, where the hand was put kept for the way back");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, s, true, false, 20.0f) && !t.active, "the grip opens: let go");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, s, false, true, 20.0f), "sheathed: let go");
	Check(!TwoHandHolds(t, s, true, true, 20.0f), "not held: nothing");
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

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
	const LeftHandPose same = LeftHandOnHandle(g, g.Below(), w, moved);
	Check(NearV(same.pos, moved + w * g.pos) && NearV(same.rot * NiPoint3{1, 0, 0}, w * NiPoint3{1, 0, 0}),
	      "at the game's place: the game's pose, carried with the weapon");
	const LeftHandPose lower = LeftHandOnHandle(g, -14.0f, w, moved);
	Check(NearV(lower.pos, moved + w * NiPoint3{0.0f, -14.0f, 6.0f}),
	      "6 further down: the same grip, 6 further down the axis");
}

void TestHold() {
	std::printf("Holding\n");
	const TwoHandSettings s;
	TwoHandState t;
	StartTwoHand(t, -15.0f, -12.0f);
	Check(t.active && Near(t.distance, 15.0f) && Near(t.handAxial, -12.0f),
	      "taken 15 below: the controllers 15 apart, the palm put 12 below");
	Check(TwoHandHolds(t, s, true, true, 25.0f), "10 further apart: still held");
	Check(!TwoHandHolds(t, s, true, true, 60.0f) && !t.active, "45 further apart: let go");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, s, true, false, 20.0f) && !t.active, "the grip opens: let go");
	StartTwoHand(t, -20.0f, -20.0f);
	Check(!TwoHandHolds(t, s, false, true, 20.0f), "sheathed: let go");
	Check(!TwoHandHolds(t, s, true, true, 20.0f), "not held: nothing");
}

void TestDirection() {
	std::printf("Where the weapon points\n");
	const NiPoint3 right{0, 0, 0};
	Check(NearV(TwoHandDirection(right, NiPoint3{0, 0, -30}), NiPoint3{0, 0, 1}),
	      "the left hand below: pointing away from it");
	Check(NearV(TwoHandDirection(right, NiPoint3{30, 0, 0}), NiPoint3{-1, 0, 0}),
	      "the left hand to the side: pointing away from it");
	Check(NearV(TwoHandDirection(right, right), NiPoint3{0, 0, 0}), "the hands in one place: nowhere");
}

void TestRotation() {
	std::printf("The turn\n");
	const NiMatrix33 r = RotationBetween(NiPoint3{0, 1, 0}, NiPoint3{0, 0, 1});
	Check(NearV(r * NiPoint3{0, 1, 0}, NiPoint3{0, 0, 1}), "y onto z");
	Check(NearV(r * NiPoint3{1, 0, 0}, NiPoint3{1, 0, 0}), "the axis across both stays");
	const NiMatrix33 same = RotationBetween(NiPoint3{0, 1, 0}, NiPoint3{0, 2, 0});
	Check(NearV(same * NiPoint3{1, 0, 0}, NiPoint3{1, 0, 0}), "the same direction: no turn");
	const NiMatrix33 back = RotationBetween(NiPoint3{0, 1, 0}, NiPoint3{0, -1, 0});
	Check(NearV(back * NiPoint3{0, 1, 0}, NiPoint3{0, -1, 0}), "opposite: turned half round");
	const NiMatrix33 none = RotationBetween(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0});
	Check(NearV(none * NiPoint3{1, 2, 3}, NiPoint3{1, 2, 3}), "no direction: no turn");
	const NiMatrix33 t = Transposed(r);
	Check(NearV(t * (r * NiPoint3{0.3f, 0.5f, 0.2f}), NiPoint3{0.3f, 0.5f, 0.2f}), "the transpose undoes it");
}

}  // namespace

int main() {
	TestPlace();
	TestTake();
	TestHandle();
	TestVanillaGrip();
	TestHold();
	TestDirection();
	TestRotation();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

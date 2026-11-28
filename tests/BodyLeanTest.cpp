// Checks the lean (game/BodyLeanLogic.h): how far a trunk reaches for a drop
// - none, part, the whole trunk and more, numbers that are not numbers; the
// anchor - a first frame, a lean within reach held, a step dragging it, an
// upright head drawing it back, a long frame, hips that are not numbers; the
// pivot's raise - within reach, a lean the raise explains, capped by the
// let-down, out of reach, nothing to give; and the rotation between two
// directions - the same, a quarter turn, opposite, no direction.

#include <cstdio>
#include <limits>

#include "game/BodyLeanLogic.h"

namespace {

using namespace obvr::game;
using obvr::NiMatrix33;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();
// A body facing +x: its right is -y (HeadingOf's yaw is the right axis's).
const float kFacingX = -0.5f * 3.14159265f;

void TestAllowed() {
	std::printf("How far the trunk reaches\n");
	Check(Near(LeanAllowedUnits(0.0f, 50.0f, 3.0f), 3.0f), "upright: only the slack");
	Check(Near(LeanAllowedUnits(10.0f, 50.0f, 3.0f), 30.0f + 3.0f), "10 down on a trunk of 50: 30 forward, plus the slack");
	Check(Near(LeanAllowedUnits(80.0f, 50.0f, 0.0f), 50.0f), "down by more than the trunk: the whole trunk");
	Check(Near(LeanAllowedUnits(kNaN, 50.0f, 3.0f), 3.0f), "a drop that is not a number: the slack");
	Check(Near(LeanAllowedUnits(10.0f, kNaN, 3.0f), 3.0f), "a trunk that is not a number: the slack");
	Check(Near(LeanAllowedUnits(10.0f, 50.0f, -1.0f), 30.0f), "a negative slack: none");
}

void TestAnchor() {
	std::printf("Where the hips stay\n");
	LeanAnchor a;
	NiPoint3 lean = StepLeanAnchor(a, 100.0f, 200.0f, 0.0f, 50.0f, 0.011f, kFacingX);
	Check(a.valid && Near(a.x, 100.0f) && Near(a.y, 200.0f) && Near(lean.x, 0.0f), "the first frame: the anchor at the hips, no lean");

	// Leaning: the head 20 forward and 10 down (8 past the upright line: the
	// trunk reaches 27.1 + 3).
	lean = StepLeanAnchor(a, 120.0f, 200.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	Check(Near(a.x, 100.0f) && Near(lean.x, 20.0f) && Near(lean.y, 0.0f), "a lean within reach: the hips stay, the lean is 20");

	// The head 60 forward with the same drop: a step past 30.1.
	const float reach = obvr::math::Sqrt(50.0f * 50.0f - 42.0f * 42.0f) + 3.0f;
	lean = StepLeanAnchor(a, 160.0f, 200.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	Check(Near(lean.x, reach) && Near(a.x, 160.0f - reach), "past the reach: the anchor dragged, the lean at its edge");

	// Upright again: drawn back under the head.
	LeanAnchor b;
	StepLeanAnchor(b, 0.0f, 0.0f, 0.0f, 50.0f, 0.011f, kFacingX);
	lean = StepLeanAnchor(b, 2.0f, 0.0f, 0.0f, 50.0f, 0.1f, kFacingX);
	Check(Near(b.x, 0.4f) && Near(lean.x, 1.6f), "upright, within the slack: drawn back a fifth in a tenth of a second");
	lean = StepLeanAnchor(b, 2.0f, 0.0f, 0.0f, 50.0f, 5.0f, kFacingX);
	Check(Near(lean.x, 0.0f) && Near(b.x, 2.0f), "a long frame upright: under the head, never past");
	lean = StepLeanAnchor(b, 2.0f, 0.0f, kNaN, 50.0f, 0.011f, kFacingX);
	Check(lean.x <= 3.0f, "a drop that is not a number: the slack only, as upright");

	lean = StepLeanAnchor(b, kNaN, 0.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	Check(!b.valid && Near(lean.x, 0.0f), "hips that are not numbers: the anchor forgotten, no lean");
	LeanAnchor c;
	c.valid = true;
	c.x = kNaN;
	lean = StepLeanAnchor(c, 5.0f, 5.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	Check(Near(c.x, 5.0f) && Near(lean.x, 0.0f), "an anchor that is not a number: started again at the hips");
}

// Walking by the stick moves the player, not the head against it: the
// anchor goes with the player (Nadi, 2026-10-28: "er beugt sich richtig, nur
// halt mit dem linken stick auch statt kopf").
void TestCarry() {
	std::printf("The anchor carried with the player\n");
	LeanAnchor a;
	StepLeanAnchor(a, 0.0f, 0.0f, 0.0f, 50.0f, 0.011f, kFacingX);
	// The stick walks the player 4 units a frame for 20 frames, the head still
	// over the hips the whole way: the placement's hips go with it.
	float worst = 0.0f;
	for (int i = 1; i <= 20; ++i) {
		CarryLeanAnchor(a, 4.0f, 0.0f);
		const NiPoint3 lean = StepLeanAnchor(a, 4.0f * static_cast<float>(i), 0.0f, 0.0f, 50.0f, 0.011f, kFacingX);
		const float l = obvr::math::Sqrt(lean.LengthSquared());
		worst = l > worst ? l : worst;
	}
	Check(Near(worst, 0.0f), "walking by the stick with the head still: no lean at any frame");

	// The same walk while leaning 20 forward: the lean stays the head's.
	LeanAnchor b;
	StepLeanAnchor(b, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	NiPoint3 lean{};
	for (int i = 1; i <= 20; ++i) {
		CarryLeanAnchor(b, 0.0f, 4.0f);
		lean = StepLeanAnchor(b, 20.0f, 4.0f * static_cast<float>(i), 10.0f, 50.0f, 0.011f, kFacingX);
	}
	Check(Near(lean.x, 20.0f) && Near(lean.y, 0.0f), "leaning while walking: the lean is the head's 20, not the walk's");

	// Without the carry (the bug): the walk itself read as a lean.
	LeanAnchor c;
	StepLeanAnchor(c, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	lean = StepLeanAnchor(c, 20.0f, 0.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	Check(Near(lean.x, 20.0f), "uncarried, a 20-unit walk reads as a 20-unit lean - what the carry takes out");

	LeanAnchor d;
	CarryLeanAnchor(d, 5.0f, 5.0f);
	Check(!d.valid, "no anchor yet: nothing carried");
	StepLeanAnchor(d, 1.0f, 1.0f, 0.0f, 50.0f, 0.011f, kFacingX);
	CarryLeanAnchor(d, kNaN, 0.0f);
	Check(Near(d.x, 1.0f) && Near(d.y, 1.0f), "a move that is not a number: the anchor stays");
	CarryLeanAnchor(d, 500.0f, 0.0f);
	Check(!d.valid, "a jump no walk makes (a load, a door): the anchor forgotten, started again");
}

void TestDrop() {
	std::printf("How far below standing\n");
	LeanAnchor a;
	Check(Near(StepLeanDrop(a, 8.0f, 0.011f), 0.0f) && Near(a.standingLowered, 8.0f),
	      "the first frame: standing there, a player shorter than the character let down 8 - no drop");
	Check(Near(StepLeanDrop(a, 18.0f, 0.011f), 10.0f - 10.0f * 0.011f / 30.0f), "10 further down: a drop of 10");
	const float was = a.standingLowered;
	Check(Near(StepLeanDrop(a, 6.0f, 0.011f), 0.0f) && Near(a.standingLowered, was + (6.0f - was) * 0.055f),
	      "higher than standing: no drop, the standing moved a twentieth of the way in a frame");
	Check(Near(StepLeanDrop(a, 6.0f, 0.5f), 0.0f) && Near(a.standingLowered, 6.0f),
	      "held higher for half a second: the new standing");
	StepLeanDrop(a, 16.0f, 30.0f);
	Check(Near(a.standingLowered, 16.0f), "held lower for half a minute: followed down, all the way at most");
	Check(Near(StepLeanDrop(a, kNaN, 0.011f), 0.0f) && Near(a.standingLowered, 16.0f), "a let-down that is not a number: no drop, kept");

	// One frame's spike 16 higher (a tracking glitch), then standing again:
	// until 2026-11-28 the spike was the standing, and every frame after read
	// as let down 16.
	StepLeanDrop(a, 0.0f, 0.011f);
	const float drop = StepLeanDrop(a, 16.0f, 0.011f);
	Check(drop < kLeanUprightUnits, "a one-frame spike up: standing again reads upright, not let down by the spike");
	LeanAnchor b;
	StepLeanDrop(b, 10.0f, 0.011f);
	StepLeanDrop(b, 4.0f, kNaN);
	Check(Near(b.standingLowered, 4.0f), "higher with a frame time that is not a number: taken at once");
}

// The reach from the upright line, and aside (2026-11-28: the body leaned
// aside and stayed so).
void TestReach() {
	std::printf("How far forward and aside\n");
	LeanReach r = LeanReachFor(2.0f, 38.0f);
	Check(Near(r.forward, 3.0f) && Near(r.aside, 3.0f), "at the upright line: the slack only (from zero, 12.2 more)");
	r = LeanReachFor(2.5f, 38.0f);
	const float half = obvr::math::Sqrt(38.0f * 38.0f - 37.5f * 37.5f);
	Check(Near(r.forward, half + 3.0f) && Near(r.aside, half + 3.0f), "half a unit past it: 6.1 more, not 16.6");
	r = LeanReachFor(30.0f, 38.0f);
	Check(Near(r.forward, obvr::math::Sqrt(38.0f * 38.0f - 10.0f * 10.0f) + 3.0f) && Near(r.aside, 19.0f + 3.0f),
	      "far down: forward the trunk's reach, aside held to half the trunk (30 degrees)");
	r = LeanReachFor(kNaN, 38.0f);
	Check(Near(r.forward, 3.0f) && Near(r.aside, 3.0f), "a drop that is not a number: the slack");
	r = LeanReachFor(30.0f, kNaN);
	Check(Near(r.forward, 3.0f) && Near(r.aside, 3.0f), "a trunk that is not a number: the slack");

	// 2.5 below standing, the hips 10 forward of the anchor: held to 9.1.
	LeanAnchor a;
	StepLeanAnchor(a, 0.0f, 0.0f, 2.5f, 38.0f, 0.011f, kFacingX);
	NiPoint3 lean = StepLeanAnchor(a, 10.0f, 0.0f, 2.5f, 38.0f, 0.0f, kFacingX);
	Check(Near(lean.x, half + 3.0f), "2.5 down, 10 forward: the lean held to 9.1, the anchor dragged");
	// Far down, the hips 30 aside (the body's right is -y): held to 22.
	LeanAnchor b;
	StepLeanAnchor(b, 0.0f, 0.0f, 30.0f, 38.0f, 0.011f, kFacingX);
	lean = StepLeanAnchor(b, 0.0f, -30.0f, 30.0f, 38.0f, 0.0f, kFacingX);
	Check(Near(lean.y, -22.0f) && Near(lean.x, 0.0f), "far down, 30 aside: held to 22, the anchor dragged");
	LeanAnchor c;
	StepLeanAnchor(c, 0.0f, 0.0f, 30.0f, 38.0f, 0.011f, kFacingX);
	lean = StepLeanAnchor(c, 30.0f, 0.0f, 30.0f, 38.0f, 0.0f, kFacingX);
	Check(Near(lean.x, 30.0f), "far down, 30 forward: held, the trunk reaches 39.7 forward");
	// Diagonal: 20 forward and 20 aside is past the ellipse (forward 39.7,
	// aside 22): dragged back along its own line onto it.
	LeanAnchor d;
	StepLeanAnchor(d, 0.0f, 0.0f, 30.0f, 38.0f, 0.011f, kFacingX);
	lean = StepLeanAnchor(d, 20.0f, -20.0f, 30.0f, 38.0f, 0.0f, kFacingX);
	const float ahead = obvr::math::Sqrt(38.0f * 38.0f - 10.0f * 10.0f) + 3.0f;
	const float keep = 1.0f / obvr::math::Sqrt((20.0f / 22.0f) * (20.0f / 22.0f) + (20.0f / ahead) * (20.0f / ahead));
	Check(Near(lean.x, 20.0f * keep) && Near(lean.y, -20.0f * keep), "diagonal past the edge: onto the ellipse along its line");
	// A yaw that is not a number: aside as far as forward, as before.
	LeanAnchor e;
	StepLeanAnchor(e, 0.0f, 0.0f, 30.0f, 38.0f, 0.011f, kNaN);
	lean = StepLeanAnchor(e, 0.0f, -30.0f, 30.0f, 38.0f, 0.0f, kNaN);
	Check(Near(lean.y, -30.0f), "a yaw that is not a number: aside reaches as far as forward");
}

// Leaning, the part aside goes back under the head; forward stays.
void TestSideReturn() {
	std::printf("Aside drawn back while leaning\n");
	LeanAnchor a;
	StepLeanAnchor(a, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	NiPoint3 lean{};
	for (int i = 0; i < 182; ++i) {  // two seconds at 11 ms
		lean = StepLeanAnchor(a, 10.0f, -10.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	}
	Check(Near(lean.x, 10.0f) && lean.y > -4.0f && lean.y < -3.0f,
	      "10 forward and 10 aside held for 2 s: forward 10 kept, aside down to 3.7");
	LeanAnchor b;
	StepLeanAnchor(b, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f, kFacingX);
	lean = StepLeanAnchor(b, 0.0f, -10.0f, 10.0f, 50.0f, 5.0f, kFacingX);
	Check(Near(lean.y, 0.0f), "a long frame: aside all the way back, never past");
	LeanAnchor c;
	StepLeanAnchor(c, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f, kNaN);
	lean = StepLeanAnchor(c, 0.0f, -10.0f, 10.0f, 50.0f, 1.0f, kNaN);
	Check(Near(lean.y, -10.0f), "a yaw that is not a number: nothing drawn back while leaning, as before");
}

void TestCarryWithPlayer() {
	std::printf("The carry from the player's two poses\n");
	LeanAnchor a;
	a.valid = true;
	a.x = 110.0f;
	a.y = 0.0f;
	const NiMatrix33 still = NiMatrix33::Identity();
	CarryLeanAnchorWithPlayer(a, NiPoint3{100.0f, 0.0f, 0.0f}, still, NiPoint3{105.0f, 2.0f, 0.0f}, still);
	Check(Near(a.x, 115.0f) && Near(a.y, 2.0f), "the player walked 5 and 2: the anchor with it");
	NiMatrix33 quarter{};
	quarter.data[0][1] = -1.0f;  // a quarter turn about up: x onto y
	quarter.data[1][0] = 1.0f;
	quarter.data[2][2] = 1.0f;
	CarryLeanAnchorWithPlayer(a, NiPoint3{105.0f, 2.0f, 0.0f}, still, NiPoint3{105.0f, 2.0f, 0.0f}, quarter);
	Check(Near(a.x, 105.0f) && Near(a.y, 12.0f), "the player turned a quarter: the anchor 10 ahead turned with it");
	LeanAnchor b;
	CarryLeanAnchorWithPlayer(b, NiPoint3{0.0f, 0.0f, 0.0f}, still, NiPoint3{5.0f, 0.0f, 0.0f}, still);
	Check(!b.valid && Near(b.x, 0.0f), "no anchor yet: nothing carried");
}

void TestDegrees() {
	std::printf("The angle between two directions\n");
	Check(Near(DegreesBetween(NiPoint3{0.0f, 0.0f, 2.0f}, NiPoint3{0.0f, 1.0f, 1.0f}), 45.0f), "up and up-forward: 45");
	Check(Near(DegreesBetween(NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{0.0f, 0.0f, -1.0f}), 180.0f), "opposite: 180");
	Check(Near(DegreesBetween(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 1.0f}), 0.0f), "no direction: 0");
	Check(Near(DegreesBetween(NiPoint3{kNaN, 0.0f, 1.0f}, NiPoint3{0.0f, 1.0f, 1.0f}), 0.0f), "not a number: 0");
}

void TestRaise() {
	std::printf("The pivot's raise\n");
	// The trunk 50 straight up; the body let down 10 under a head that leaned 30.
	const NiPoint3 toHead{0.0f, 0.0f, 40.0f};  // the head 10 lower than the trunk's length, over the pivot
	float r = LeanPivotRaise(toHead, NiPoint3{0.0f, 0.0f, 0.0f}, 50.0f, 10.0f);
	Check(Near(r, 0.0f), "no lean, the head within the trunk's length: nothing");
	r = LeanPivotRaise(toHead, NiPoint3{30.0f, 0.0f, 0.0f}, 50.0f, 10.0f);
	Check(Near(r, 0.0f), "held back 30 at 40 up: exactly a trunk's length off, nothing to raise");
	r = LeanPivotRaise(NiPoint3{0.0f, 0.0f, 45.0f}, NiPoint3{30.0f, 0.0f, 0.0f}, 50.0f, 10.0f);
	Check(Near(r, 5.0f), "held back 30 at 45 up: raised 5, back to a trunk's length");
	r = LeanPivotRaise(NiPoint3{0.0f, 0.0f, 49.0f}, NiPoint3{30.0f, 0.0f, 0.0f}, 50.0f, 2.0f);
	Check(Near(r, 2.0f), "never more than the let-down");
	r = LeanPivotRaise(NiPoint3{0.0f, 0.0f, 10.0f}, NiPoint3{80.0f, 0.0f, 0.0f}, 50.0f, 10.0f);
	Check(Near(r, 0.0f), "a head out of the trunk's reach: nothing");
	r = LeanPivotRaise(toHead, NiPoint3{30.0f, 0.0f, 0.0f}, 50.0f, 0.0f);
	Check(Near(r, 0.0f), "no let-down to give back: nothing");
	r = LeanPivotRaise(NiPoint3{kNaN, 0.0f, 0.0f}, NiPoint3{30.0f, 0.0f, 0.0f}, 50.0f, 10.0f);
	Check(Near(r, 0.0f), "numbers that are not numbers: nothing");
}

NiPoint3 Apply(const NiMatrix33& r, const NiPoint3& v) { return r * v; }

void TestRotation() {
	std::printf("The rotation between two directions\n");
	NiMatrix33 r = RotationBetween(NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{0.0f, 0.0f, 3.0f});
	Check(Near(r.data[0][0], 1.0f) && Near(r.data[2][2], 1.0f) && Near(r.data[0][2], 0.0f), "the same direction: the identity");
	r = RotationBetween(NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{0.0f, 1.0f, 0.0f});
	NiPoint3 v = Apply(r, NiPoint3{0.0f, 0.0f, 50.0f});
	Check(Near(v.y, 50.0f) && Near(v.z, 0.0f), "up onto forward: a quarter turn, the length kept");
	v = Apply(r, NiPoint3{1.0f, 0.0f, 0.0f});
	Check(Near(v.x, 1.0f), "the axis itself is left");
	r = RotationBetween(NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{0.0f, 0.3f, 1.0f});
	v = Apply(r, NiPoint3{0.0f, 0.0f, 1.0f});
	const float l = obvr::math::Sqrt(0.09f + 1.0f);
	Check(Near(v.y, 0.3f / l) && Near(v.z, 1.0f / l), "a lean of 17 degrees: onto the new direction");
	r = RotationBetween(NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{0.0f, 0.0f, -1.0f});
	v = Apply(r, NiPoint3{0.0f, 0.0f, 1.0f});
	Check(Near(v.z, -1.0f), "opposite: half a turn");
	r = RotationBetween(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f});
	Check(Near(r.data[1][1], 1.0f) && Near(r.data[1][2], 0.0f), "no direction: the identity");
	r = RotationBetween(NiPoint3{kNaN, 0.0f, 1.0f}, NiPoint3{0.0f, 1.0f, 0.0f});
	Check(Near(r.data[0][0], 1.0f), "not a number: the identity");
}

}  // namespace

int main() {
	TestAllowed();
	TestAnchor();
	TestDrop();
	TestReach();
	TestSideReturn();
	TestCarry();
	TestCarryWithPlayer();
	TestDegrees();
	TestRaise();
	TestRotation();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

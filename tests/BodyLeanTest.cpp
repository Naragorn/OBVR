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
	NiPoint3 lean = StepLeanAnchor(a, 100.0f, 200.0f, 0.0f, 50.0f, 0.011f);
	Check(a.valid && Near(a.x, 100.0f) && Near(a.y, 200.0f) && Near(lean.x, 0.0f), "the first frame: the anchor at the hips, no lean");

	// Leaning: the head 20 forward and 10 down (the trunk reaches 30 + 3).
	lean = StepLeanAnchor(a, 120.0f, 200.0f, 10.0f, 50.0f, 0.011f);
	Check(Near(a.x, 100.0f) && Near(lean.x, 20.0f) && Near(lean.y, 0.0f), "a lean within reach: the hips stay, the lean is 20");

	// The head 60 forward with the same drop: a step past 33.
	lean = StepLeanAnchor(a, 160.0f, 200.0f, 10.0f, 50.0f, 0.011f);
	Check(Near(lean.x, 33.0f) && Near(a.x, 127.0f), "past the reach: the anchor dragged, the lean at its edge");

	// Upright again: drawn back under the head.
	LeanAnchor b;
	StepLeanAnchor(b, 0.0f, 0.0f, 0.0f, 50.0f, 0.011f);
	lean = StepLeanAnchor(b, 2.0f, 0.0f, 0.0f, 50.0f, 0.1f);
	Check(Near(b.x, 0.4f) && Near(lean.x, 1.6f), "upright, within the slack: drawn back a fifth in a tenth of a second");
	lean = StepLeanAnchor(b, 2.0f, 0.0f, 0.0f, 50.0f, 5.0f);
	Check(Near(lean.x, 0.0f) && Near(b.x, 2.0f), "a long frame upright: under the head, never past");
	lean = StepLeanAnchor(b, 2.0f, 0.0f, kNaN, 50.0f, 0.011f);
	Check(lean.x <= 3.0f, "a drop that is not a number: the slack only, as upright");

	lean = StepLeanAnchor(b, kNaN, 0.0f, 10.0f, 50.0f, 0.011f);
	Check(!b.valid && Near(lean.x, 0.0f), "hips that are not numbers: the anchor forgotten, no lean");
	LeanAnchor c;
	c.valid = true;
	c.x = kNaN;
	lean = StepLeanAnchor(c, 5.0f, 5.0f, 10.0f, 50.0f, 0.011f);
	Check(Near(c.x, 5.0f) && Near(lean.x, 0.0f), "an anchor that is not a number: started again at the hips");
}

// Walking by the stick moves the player, not the head against it: the
// anchor goes with the player (Nadi, 2026-10-28: "er beugt sich richtig, nur
// halt mit dem linken stick auch statt kopf").
void TestCarry() {
	std::printf("The anchor carried with the player\n");
	LeanAnchor a;
	StepLeanAnchor(a, 0.0f, 0.0f, 0.0f, 50.0f, 0.011f);
	// The stick walks the player 4 units a frame for 20 frames, the head still
	// over the hips the whole way: the placement's hips go with it.
	float worst = 0.0f;
	for (int i = 1; i <= 20; ++i) {
		CarryLeanAnchor(a, 4.0f, 0.0f);
		const NiPoint3 lean = StepLeanAnchor(a, 4.0f * static_cast<float>(i), 0.0f, 0.0f, 50.0f, 0.011f);
		const float l = obvr::math::Sqrt(lean.LengthSquared());
		worst = l > worst ? l : worst;
	}
	Check(Near(worst, 0.0f), "walking by the stick with the head still: no lean at any frame");

	// The same walk while leaning 20 forward: the lean stays the head's.
	LeanAnchor b;
	StepLeanAnchor(b, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f);
	NiPoint3 lean{};
	for (int i = 1; i <= 20; ++i) {
		CarryLeanAnchor(b, 0.0f, 4.0f);
		lean = StepLeanAnchor(b, 20.0f, 4.0f * static_cast<float>(i), 10.0f, 50.0f, 0.011f);
	}
	Check(Near(lean.x, 20.0f) && Near(lean.y, 0.0f), "leaning while walking: the lean is the head's 20, not the walk's");

	// Without the carry (the bug): the walk itself read as a lean.
	LeanAnchor c;
	StepLeanAnchor(c, 0.0f, 0.0f, 10.0f, 50.0f, 0.011f);
	lean = StepLeanAnchor(c, 20.0f, 0.0f, 10.0f, 50.0f, 0.011f);
	Check(Near(lean.x, 20.0f), "uncarried, a 20-unit walk reads as a 20-unit lean - what the carry takes out");

	LeanAnchor d;
	CarryLeanAnchor(d, 5.0f, 5.0f);
	Check(!d.valid, "no anchor yet: nothing carried");
	StepLeanAnchor(d, 1.0f, 1.0f, 0.0f, 50.0f, 0.011f);
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
	Check(Near(StepLeanDrop(a, 6.0f, 0.011f), 0.0f) && Near(a.standingLowered, 6.0f), "higher than standing: the new standing");
	StepLeanDrop(a, 16.0f, 30.0f);
	Check(Near(a.standingLowered, 16.0f), "held lower for half a minute: followed down, all the way at most");
	Check(Near(StepLeanDrop(a, kNaN, 0.011f), 0.0f) && Near(a.standingLowered, 16.0f), "a let-down that is not a number: no drop, kept");
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
	TestCarry();
	TestRaise();
	TestRotation();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

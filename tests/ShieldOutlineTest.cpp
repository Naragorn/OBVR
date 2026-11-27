// Checks the shield's outline (game/ShieldOutlineLogic.h) and the arrow rule
// that takes it (game/ArrowShieldLogic.h, ArrowAtShieldFor): a round shield's
// outline is its circle within a row's step; a kite's narrow tip and a
// square's corners are judged by the shape, where the circle of the long
// spoke was wrong both ways; rows without a vertex are filled from their
// neighbours; too few rows give no outline and the circle stands in; the
// flight's segment through the outline and past it; a disc whose axes are
// swapped or mirrored judges the same; the arrow's half width as the margin.

#include <cmath>
#include <cstdio>

#include "game/ArrowShieldLogic.h"
#include "game/ShieldGuardLogic.h"
#include "game/ShieldOutlineLogic.h"

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

// An outline from a shape's half width along its first axis, `a` from
// `low` to `high`, the walk's centre at (ca, cb) from the shape's own
// origin: the vertices on both edges and down the middle, every `stepA`.
template <typename HalfWidth>
ShieldOutline Shape(float low, float high, HalfWidth halfWidth, float ca = 3.0f, float cb = -2.0f,
                    float stepA = 1.0f, int every = 1) {
	OutlineBins bins;
	int row = 0;
	float lowB = 0.0f;
	float highB = 0.0f;
	for (float a = low; a <= high + 1e-3f; a += stepA, ++row) {
		if (row % every != 0) {
			continue;
		}
		const float w = halfWidth(a);
		bins.Add(a - ca, -w - cb);
		bins.Add(a - ca, w - cb);
		bins.Add(a - ca, -cb);
		lowB = -w < lowB ? -w : lowB;
		highB = w > highB ? w : highB;
	}
	// The disc's middle as ShieldDiscFrom puts it: the middle of the reach.
	const float middleA = (low + high) * 0.5f;
	const ShieldOutline o = OutlineFromBins(bins, low - ca, high - ca, (lowB + highB) * 0.5f - cb);
	(void)middleA;
	return o;
}

float Round30(float a) { return std::sqrt(a * a < 900.0f ? 900.0f - a * a : 0.0f); }
// A kite 80 long: 25 wide (half) from a = 10 to the top at 40, narrowing to
// a point at the tip, a = -40.
float Kite(float a) { return a >= 10.0f ? 25.0f : 25.0f * (a + 40.0f) / 50.0f; }
float Square(float) { return 38.0f; }

void TestRound() {
	std::printf("A round shield\n");
	const ShieldOutline o = Shape(-30.0f, 30.0f, Round30);
	const float step = 60.0f / static_cast<float>(kOutlineRows);
	Check(o.valid && o.filledRead >= 20, "read: its rows filled");
	Check(OutlineDistance(o, 0.0f, 28.0f) < 0.0f && OutlineDistance(o, 0.0f, 32.0f) > 0.0f, "the rim at 30 across the middle");
	Check(std::fabs(OutlineDistance(o, 0.0f, 30.0f)) <= step, "the circle's rim within a row's step");
	Check(OutlineDistance(o, 20.0f, 20.0f) < 0.0f, "inside on the diagonal (28 from the middle)");
	Check(OutlineDistance(o, 23.0f, 23.0f) > 0.0f, "outside on the diagonal (33 from the middle)");
	Check(OutlineDistance(o, 33.0f, 0.0f) > 2.0f, "past the top: outside");
	std::printf("        widest %.1f, narrowest %.1f\n", static_cast<double>(o.widest), static_cast<double>(o.narrowest));
	Check(o.widest > 57.0f && o.widest < 61.0f && o.narrowest < 30.0f, "widest about 60, the end rows narrower");
}

void TestKiteAndSquare() {
	std::printf("A kite and a square: the shape, not the circle\n");
	const ShieldOutline kite = Shape(-40.0f, 40.0f, Kite);
	Check(kite.valid, "the kite read");
	Check(OutlineDistance(kite, -35.0f, 8.0f) > 2.0f,
	      "beside the tip (8 across, the kite 2.5 wide there; the rows' step leaves 3 of the 5.5): off");
	Check(OutlineDistance(kite, 30.0f, 20.0f) < 0.0f, "at the wide top (20 across): on");
	const ShieldOutline square = Shape(-40.0f, 40.0f, Square);
	Check(OutlineDistance(square, 38.0f, 36.0f) < 0.0f, "a square's corner (52 from the middle): on");
	Check(OutlineDistance(square, 0.0f, 41.0f) > 2.0f, "past its side: off");
}

void TestSparseAndFew() {
	std::printf("Holes and too few rows\n");
	// Vertices only every 6 units along the first axis: rows without one.
	const ShieldOutline sparse = Shape(-40.0f, 40.0f, Square, 0.0f, 0.0f, 1.0f, 6);
	Check(sparse.valid && sparse.filledRead < kOutlineRows, "some rows empty as read");
	bool allFilled = true;
	for (UInt32 r = 1; r + 1 < kOutlineRows; ++r) {
		allFilled = allFilled && sparse.filled[r];
	}
	Check(allFilled, "the holes between filled rows filled from their neighbours");
	Check(OutlineDistance(sparse, 2.0f, 30.0f) < 0.0f, "a point in a filled hole: on");
	// The kite's tip narrowing between two filled rows: interpolated, not widened.
	const ShieldOutline kiteSparse = Shape(-40.0f, 40.0f, Kite, 0.0f, 0.0f, 1.0f, 7);
	Check(OutlineDistance(kiteSparse, -30.0f, 12.0f) > 0.0f, "the kite's tip from sparse rows: still narrow");
	// Vertices in a band of 6 units only: under a quarter of the rows.
	OutlineBins bins;
	for (float a = -3.0f; a <= 3.0f; a += 0.5f) {
		bins.Add(a, -10.0f);
		bins.Add(a, 10.0f);
	}
	bins.Add(-40.0f, 0.0f);
	bins.Add(40.0f, 0.0f);
	Check(!OutlineFromBins(bins, -40.0f, 40.0f, 0.0f).valid, "too few rows filled: no outline");
	Check(!OutlineFromBins(bins, 5.0f, 5.0f, 0.0f).valid, "no extent: no outline");
	Check(OutlineDistance(ShieldOutline{}, 0.0f, 0.0f) > 1e8f, "no outline: nothing is inside");
}

// The world: the disc at C, its face +y, its first axis up (+z), its second +x.
const NiPoint3 kC{0.0f, 10.0f, 100.0f};

ArrowShieldInput Arrow(const ShieldOutline* o, float a, float b, float along) {
	ArrowShieldInput in;
	in.targetIsPlayer = true;
	in.byContact = true;
	in.discValid = true;
	in.disc.centre = kC;
	in.disc.normal = NiPoint3{0.0f, 1.0f, 0.0f};
	in.disc.radius = 40.0f;  // the long spoke, as ShieldDiscInWorld keeps it
	in.disc.halfThick = 4.0f;
	in.axisA = NiPoint3{0.0f, 0.0f, 1.0f};
	in.axisB = NiPoint3{1.0f, 0.0f, 0.0f};
	in.outline = o;
	in.pointValid = true;
	in.point = kC + in.axisA * a + in.axisB * b + in.disc.normal * along;
	in.fromValid = true;
	in.from = in.point + in.disc.normal * 60.0f;  // from ahead, straight in
	return in;
}

void TestArrowRule() {
	std::printf("The arrow rule by the outline\n");
	const ShieldOutline kite = Shape(-40.0f, 40.0f, Kite);
	// Red against e598814's circle: beside the tip is within the long spoke's
	// circle and its 6 margin.
	ArrowShieldVerdict v = ArrowAtShieldFor(Arrow(&kite, -35.0f, 8.0f, 2.0f));
	Check(v.answer == ArrowAtShield::Through && v.byOutline && v.outlineUnits > 2.0f,
	      "a point beside the kite's tip: through (the circle had it on)");
	v = ArrowAtShieldFor(Arrow(&kite, 30.0f, 20.0f, 2.0f));
	Check(v.answer == ArrowAtShield::Blocked && v.byOutline && v.outlineUnits < 0.0f, "at its wide top: blocked");
	const ShieldOutline square = Shape(-40.0f, 40.0f, Square);
	v = ArrowAtShieldFor(Arrow(&square, 39.0f, 37.0f, 1.0f));
	Check(v.answer == ArrowAtShield::Blocked, "a square's corner, outside the circle and its margin: blocked");
	// The margin: the arrow's half width only.
	const float edge = 38.0f;
	v = ArrowAtShieldFor(Arrow(&square, 0.0f, edge + kArrowHalfWidthUnits - 0.2f, 0.0f));
	Check(v.answer == ArrowAtShield::Blocked, "just within the arrow's half width past the side: blocked");
	ArrowShieldInput past = Arrow(&square, 0.0f, edge + kArrowHalfWidthUnits + 0.5f, 0.0f);
	past.from = past.point + NiPoint3{0.0f, 60.0f, 0.0f};
	v = ArrowAtShieldFor(past);
	Check(v.answer == ArrowAtShield::Through, "just past it: through");
	// Too far before or behind the plane: the normal's margin, as before.
	v = ArrowAtShieldFor(Arrow(&kite, 30.0f, 0.0f, 4.0f + kArrowDiscFaceMarginUnits + 1.0f));
	Check(v.answer != ArrowAtShield::Blocked || v.bySegment, "the point before the plane past the margin: not by the point");
}

void TestArrowSegment() {
	std::printf("The flight's segment through the outline\n");
	const ShieldOutline kite = Shape(-40.0f, 40.0f, Kite);
	// The contact on the body 30 behind the plane; the flight crossed the
	// plane at the kite's top: blocked; beside its tip: through.
	ArrowShieldInput in = Arrow(&kite, 30.0f, 20.0f, -30.0f);
	ArrowShieldVerdict v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Blocked && v.bySegment && v.byOutline, "crossed at the wide top: blocked");
	in = Arrow(&kite, -35.0f, 8.0f, -30.0f);
	v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Through && v.bySegment, "crossed beside the tip: through");
	in = Arrow(&kite, -35.0f, 8.0f, -30.0f);
	in.from = in.point - in.disc.normal * 20.0f;  // never reached the plane
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Through, "a flight that never reaches the plane: through");
}

void TestArrowFallbackAndAxes() {
	std::printf("No outline, and a disc turned or mirrored\n");
	// No outline: the circle as built in e598814 (radius + 6).
	ArrowShieldVerdict v = ArrowAtShieldFor(Arrow(nullptr, -35.0f, 8.0f, 2.0f));
	Check(v.answer == ArrowAtShield::Blocked && !v.byOutline, "no outline: the circle's verdict");
	const ShieldOutline invalid;
	v = ArrowAtShieldFor(Arrow(&invalid, -35.0f, 8.0f, 2.0f));
	Check(v.answer == ArrowAtShield::Blocked && !v.byOutline, "an outline not read: the circle's");
	// The second axis mirrored: the outline read with b turned, the world
	// axis turned with it - the same verdicts.
	const ShieldOutline kite = Shape(-40.0f, 40.0f, Kite);
	ArrowShieldInput m = Arrow(&kite, -35.0f, 8.0f, 2.0f);
	m.axisB = m.axisB * -1.0f;
	m.point = kC + m.axisA * -35.0f + m.axisB * -8.0f + m.disc.normal * 2.0f;
	Check(ArrowAtShieldFor(m).answer == ArrowAtShield::Through, "mirrored: beside the tip still through");
	// The disc turned a quarter about its normal: the first axis across.
	ArrowShieldInput t = Arrow(&kite, 30.0f, 20.0f, 2.0f);
	t.axisA = NiPoint3{1.0f, 0.0f, 0.0f};
	t.axisB = NiPoint3{0.0f, 0.0f, -1.0f};
	t.point = kC + t.axisA * 30.0f + t.axisB * 20.0f + t.disc.normal * 2.0f;
	t.from = t.point + t.disc.normal * 60.0f;
	Check(ArrowAtShieldFor(t).answer == ArrowAtShield::Blocked, "turned: the wide top still blocked");
	t.point = kC + t.axisA * -35.0f + t.axisB * 8.0f + t.disc.normal * 2.0f;
	t.from = t.point + t.disc.normal * 60.0f;
	Check(ArrowAtShieldFor(t).answer == ArrowAtShield::Through, "turned: beside the tip still through");
}

void TestIntoGuard() {
	std::printf("The disc into the frame's guard\n");
	ShieldDisc disc;
	disc.valid = true;
	disc.centre = NiPoint3{0.0f, 0.0f, 5.0f};
	disc.axis[0] = NiPoint3{1.0f, 0.0f, 0.0f};
	disc.axis[1] = NiPoint3{0.0f, 1.0f, 0.0f};
	disc.axis[2] = NiPoint3{0.0f, 0.0f, 1.0f};
	disc.half[0] = 40.0f;
	disc.half[1] = 25.0f;
	disc.half[2] = 3.0f;
	disc.outline = Shape(-40.0f, 40.0f, Kite);
	// The hand turned a quarter about z: x to y, y to -x.
	NiMatrix33 rot{};
	rot.data[0][1] = -1.0f;
	rot.data[1][0] = 1.0f;
	rot.data[2][2] = 1.0f;
	BlockGuard g;
	ShieldIntoGuard(disc, rot, NiPoint3{100.0f, 0.0f, 0.0f}, g);
	Check(g.shieldValid && g.shieldOutline == &disc.outline, "the guard points to the disc's outline");
	Check(std::fabs(g.shieldAxisA.y - 1.0f) < 1e-4f && std::fabs(g.shieldAxisB.x + 1.0f) < 1e-4f,
	      "its in-plane axes turned with the hand");
	Check(std::fabs(g.shieldRadius - 40.0f) < 1e-4f, "the melee rule's radius: the longer spoke, as before");
}

}  // namespace

int main() {
	TestRound();
	TestKiteAndSquare();
	TestSparseAndFew();
	TestArrowRule();
	TestArrowSegment();
	TestArrowFallbackAndAxes();
	TestIntoGuard();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

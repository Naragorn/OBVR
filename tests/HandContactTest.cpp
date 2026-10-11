// Checks the free hand's and the shield's contact (game/HandContactLogic.h):
// which hand meets the world as what, the speed that goes through people,
// the shield's disc from a model's vertices (the axes by Jacobi, the reach
// along them, what is refused), the spans and the margin the disc sweeps
// with, the ball that stands in, when the disc is read again - and the
// weapon's step run with a hand's span and with a disc's two spokes against
// a floor: held with the right point and the right margin, the ball swept
// once.

#include <cmath>
#include <cstdio>
#include <string>

#include "game/HandContactLogic.h"

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

float Abs(float a) { return a < 0.0f ? -a : a; }

const float kPi = 3.14159265f;

// A floor at a height: a ray going down through it meets its face, nothing
// else is in this world.
struct FloorWorld {
	float z = -40.0f;
	UInt32 casts = 0;

	BladeRayHit Cast(const NiPoint3& from, const NiPoint3& to) {
		++casts;
		BladeRayHit h;
		if (!(from.z > z) || !(to.z <= z)) {
			return h;
		}
		h.hit = true;
		h.fraction = (from.z - z) / (from.z - to.z);
		h.point = from + (to - from) * h.fraction;
		h.normal = NiPoint3{0.0f, 0.0f, 1.0f};
		h.kind = ContactKind::Fixed;
		h.body = 1;
		return h;
	}
};

FreeHandFrame FreeFrame(bool weaponHand) {
	FreeHandFrame f;
	f.valid = true;
	f.weaponHand = weaponHand;
	return f;
}

void TestWhichHand() {
	std::printf("Which hand meets the world as what\n");
	FreeHandFrame f = FreeFrame(true);
	Check(FreeHandShape(f) == HandShape::Hand, "the weapon hand with nothing drawn: a hand");
	Check(FreeHandShape(FreeFrame(false)) == HandShape::Hand, "the off hand: a hand");
	f.valid = false;
	Check(FreeHandShape(f) == HandShape::None, "not tracked: nothing");
	f = FreeFrame(true);
	f.meleeDrawn = true;
	Check(FreeHandShape(f) == HandShape::None, "the weapon hand with a sword drawn: the blade's, nothing here");
	f = FreeFrame(false);
	f.meleeDrawn = true;
	Check(FreeHandShape(f) == HandShape::Hand, "the off hand with a sword drawn: a hand");
	f.bowDrawn = true;
	Check(FreeHandShape(f) == HandShape::None, "a bow drawn: the bow hand is the bow's");
	f = FreeFrame(true);
	f.bowDrawn = true;
	Check(FreeHandShape(f) == HandShape::None, "and the drawing hand the string's");
	f = FreeFrame(false);
	f.onHandle = true;
	Check(FreeHandShape(f) == HandShape::None, "on a two-hander's handle: the handle's");
	f = FreeFrame(false);
	f.holdsObject = true;
	Check(FreeHandShape(f) == HandShape::None, "round a held object: the object's");
	f = FreeFrame(true);
	f.leads = true;
	Check(FreeHandShape(f) == HandShape::None, "leading someone: theirs");
	f = FreeFrame(false);
	f.grapples = true;
	Check(FreeHandShape(f) == HandShape::None, "holding a fighter: theirs");
	f = FreeFrame(false);
	f.shieldWorn = true;
	Check(FreeHandShape(f) == HandShape::Shield, "the off hand with a shield on its arm: the shield");
	f.meleeDrawn = true;
	Check(FreeHandShape(f) == HandShape::Shield, "sword and shield: the shield still");
	f.holdsObject = true;
	Check(FreeHandShape(f) == HandShape::None, "the shield arm's hand round an object: the object's");
	f = FreeFrame(true);
	f.shieldWorn = true;
	Check(FreeHandShape(f) == HandShape::Hand, "a shield worn is the off arm's: the weapon hand is a hand");
	Check(std::string(HandShapeName(HandShape::Shield)) == "the shield" &&
	          std::string(HandShapeName(HandShape::Hand)) == "the hand" &&
	          std::string(HandShapeName(HandShape::None)) == "none",
	      "the names");
}

void TestSwinging() {
	std::printf("A hand fast enough to go through\n");
	Check(!HandSwinging(0.5f, 1.3f, false), "half a metre a second: resting");
	Check(HandSwinging(1.3f, 1.3f, false), "the shove's speed: through");
	Check(HandSwinging(0.1f, 1.3f, true), "a fist's swing, however slow: through");
	Check(!HandSwinging(5.0f, 0.0f, false), "no shove speed set: never by speed");
	Check(HandSwinging(5.0f, 0.0f, true), "but a fist's swing still");

	std::printf("The speed judged over a window\n");
	HandTravel t;
	Check(Near(t.MeanSpeed(0.1f), 0.0f), "nothing yet: 0");
	t.Add(0.011f, 2.0f);
	Check(Near(t.MeanSpeed(0.1f), 0.0f), "one frame of history: too little to judge, 0");
	for (int i = 0; i < 20; ++i) {
		t.Add(0.011f, 2.0f);
	}
	Check(Near(t.MeanSpeed(0.1f), 2.0f), "a steady 2 m/s: 2");
	HandTravel jump;
	for (int i = 0; i < 20; ++i) {
		jump.Add(0.011f, 0.0f);
	}
	jump.Add(0.011f, 9.0f);  // a 10 cm step in one frame
	Check(Near(jump.MeanSpeed(0.1f), 0.99f, 0.02f), "a hand still, then a 10 cm step in a frame: about 1 m/s, not 9");
	for (int i = 0; i < 20; ++i) {
		jump.Add(0.011f, 0.0f);
	}
	Check(Near(jump.MeanSpeed(0.1f), 0.0f), "and still again: 0");
	HandTravel steps;
	for (int round = 0; round < 6; ++round) {
		steps.Add(0.011f, 9.0f);
		for (int i = 0; i < 3; ++i) {
			steps.Add(0.011f, 0.0f);
		}
	}
	Check(steps.MeanSpeed(0.1f) > 1.8f && steps.MeanSpeed(0.1f) < 2.6f, "10 cm steps every 44 ms: about 2.2 m/s");
	HandTravel slow;
	for (int i = 0; i < 40; ++i) {
		slow.Add(0.025f, 0.0f);
	}
	slow.Add(0.025f, 2.0f);  // a 5 cm step at a slow frame rate
	Check(Near(slow.MeanSpeed(0.1f), 0.5f, 0.02f), "a 5 cm step at 40 fps: half a metre a second");
	HandTravel odd;
	odd.Add(0.0f, 5.0f);
	odd.Add(-1.0f, 5.0f);
	Check(odd.count == 0, "a frame of no time is not kept");
	for (int i = 0; i < 20; ++i) {
		odd.Add(0.011f, -3.0f);
	}
	Check(Near(odd.MeanSpeed(0.1f), 0.0f), "a speed below zero counts as none");
	HandTravel full;
	for (int i = 0; i < 100; ++i) {
		full.Add(0.011f, 1.0f);
	}
	Check(full.count == kHandTravelEntries && Near(full.MeanSpeed(0.1f), 1.0f), "more frames than the room: the latest kept");
	Check(Near(full.MeanSpeed(0.0f), 0.0f), "no window: 0");
	full.Clear();
	Check(full.count == 0 && Near(full.MeanSpeed(0.1f), 0.0f), "cleared: nothing");
}

void TestJacobi() {
	std::printf("Jacobi's eigenvectors\n");
	double a[3][3] = {{4.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 9.0}};
	double v[3][3];
	double d[3];
	JacobiEigen3(a, v, d);
	Check(Near(static_cast<float>(d[0]), 4.0f) && Near(static_cast<float>(d[1]), 1.0f) &&
	          Near(static_cast<float>(d[2]), 9.0f),
	      "a diagonal matrix: its diagonal");
	Check(Near(static_cast<float>(v[0][0]), 1.0f) && Near(static_cast<float>(v[1][1]), 1.0f) &&
	          Near(static_cast<float>(v[2][2]), 1.0f),
	      "and the axes themselves");
	double b[3][3] = {{2.0, 1.0, 0.0}, {1.0, 2.0, 0.0}, {0.0, 0.0, 5.0}};
	double orig[3][3];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			orig[i][j] = b[i][j];
		}
	}
	JacobiEigen3(b, v, d);
	bool eigen = true;
	for (int c = 0; c < 3; ++c) {
		for (int i = 0; i < 3; ++i) {
			double av = 0.0;
			for (int k = 0; k < 3; ++k) {
				av += orig[i][k] * v[k][c];
			}
			eigen = eigen && Near(static_cast<float>(av), static_cast<float>(d[c] * v[i][c]), 1e-4f);
		}
	}
	Check(eigen, "a coupled matrix: A v = d v for every column");
	bool values = false;
	for (int c = 0; c < 3 && !values; ++c) {
		values = Near(static_cast<float>(d[c]), 3.0f, 1e-4f);
	}
	Check(values, "with 3 among the eigenvalues");
	bool unit = true;
	for (int c = 0; c < 3; ++c) {
		unit = unit && Near(static_cast<float>(v[0][c] * v[0][c] + v[1][c] * v[1][c] + v[2][c] * v[2][c]), 1.0f, 1e-4f);
	}
	Check(unit, "the eigenvectors unit length");
}

// A round shield's vertices: a ring of radius r about `centre` in the plane
// of two unit axes, inner rings, and a rim of thickness `thick` either way
// along the normal.
void ShieldPoints(const NiPoint3& centre, const NiPoint3& u, const NiPoint3& w, const NiPoint3& n, float r,
                  float thick, DiscScatter* scatter, DiscExtent* extent, const NiPoint3* extentCentre,
                  const NiPoint3* axes) {
	for (int ring = 1; ring <= 4; ++ring) {
		const float radius = r * static_cast<float>(ring) / 4.0f;
		for (int i = 0; i < 24; ++i) {
			const float angle = 2.0f * kPi * static_cast<float>(i) / 24.0f;
			const NiPoint3 flat = centre + u * (radius * static_cast<float>(std::cos(angle))) +
			                      w * (radius * static_cast<float>(std::sin(angle)));
			const float side = (i % 2 == 0 ? 1.0f : -1.0f) * thick;
			const NiPoint3 p = flat + n * side;
			if (scatter != nullptr) {
				scatter->Add(p);
			}
			if (extent != nullptr) {
				extent->Add(p, *extentCentre, axes);
			}
		}
	}
}

void TestDisc() {
	std::printf("The shield's disc from its vertices\n");
	const NiPoint3 centre{5.0f, -3.0f, 8.0f};
	const NiPoint3 u{1.0f, 0.0f, 0.0f};
	const NiPoint3 w{0.0f, 0.70710678f, 0.70710678f};
	const NiPoint3 n{0.0f, 0.70710678f, -0.70710678f};
	DiscScatter scatter;
	ShieldPoints(centre, u, w, n, 20.0f, 1.0f, &scatter, nullptr, nullptr, nullptr);
	NiPoint3 mean;
	NiPoint3 axes[3];
	float spread[3];
	Check(DiscAxesFrom(scatter, mean, axes, spread), "a shield's scatter: axes found");
	Check(Near(mean.x, 5.0f) && Near(mean.y, -3.0f) && Near(mean.z, 8.0f), "its centre");
	Check(Abs(DotOf3(axes[2], n)) > 0.99f, "the normal is the direction of least spread");
	Check(Abs(DotOf3(axes[0], n)) < 0.05f && Abs(DotOf3(axes[1], n)) < 0.05f, "the spokes lie in the disc");
	Check(spread[0] >= spread[1] && spread[1] > spread[2], "the spreads in order");
	Check(Near(DotOf3(axes[0], axes[1]), 0.0f) && Near(DotOf3(CrossOf3(axes[0], axes[1]), axes[2]), 1.0f),
	      "a right-handed set");
	DiscExtent extent;
	ShieldPoints(centre, u, w, n, 20.0f, 1.0f, nullptr, &extent, &mean, axes);
	ShieldDisc disc;
	Check(ShieldDiscFrom(mean, axes, extent, disc) && disc.valid, "the disc from the reach along them");
	Check(Near(disc.half[0], 20.0f, 0.3f) && Near(disc.half[1], 20.0f, 0.3f), "spokes of 20 units");
	Check(Near(disc.half[2], 1.0f, 0.05f), "half the thickness, 1");
	Check(Near(disc.centre.x, 5.0f, 0.3f) && Near(disc.centre.y, -3.0f, 0.3f) && Near(disc.centre.z, 8.0f, 0.3f),
	      "the centre in the middle of its reach");
	Check(disc.vertices == scatter.count, "every vertex counted");

	BodySpan a;
	BodySpan b;
	float margin = 0.0f;
	ShieldDiscSpans(disc, a, b, margin);
	Check(a.valid && b.valid, "two spokes to sweep");
	Check(Near((a.b - a.a).LengthSquared(), 1600.0f, 20.0f) && Near((b.b - b.a).LengthSquared(), 1600.0f, 20.0f),
	      "each 40 units end to end");
	Check(Near(DotOf3(a.b - a.a, b.b - b.a), 0.0f, 1.0f), "across each other");
	Check(Near(margin, 2.0f, 0.1f), "held half its thickness and a little short of a surface");
	ShieldDisc thick = disc;
	thick.half[2] = 5.0f;
	ShieldDiscSpans(thick, a, b, margin);
	Check(Near(margin, kShieldMarginMaxUnits), "a bossed shield: the margin capped");
	ShieldDisc thin = disc;
	thin.half[2] = 0.1f;
	ShieldDiscSpans(thin, a, b, margin);
	Check(Near(margin, kBladeContactMarginUnits), "a paper-thin one: the blade's margin at least");

	DiscScatter few;
	for (int i = 0; i < 5; ++i) {
		few.Add(NiPoint3{static_cast<float>(i), 0.0f, 0.0f});
	}
	Check(!DiscAxesFrom(few, mean, axes, spread), "five vertices: no disc");
	DiscScatter same;
	for (int i = 0; i < 20; ++i) {
		same.Add(NiPoint3{1.0f, 2.0f, 3.0f});
	}
	Check(!DiscAxesFrom(same, mean, axes, spread), "twenty of the same point: no spread, no disc");
	DiscScatter line;
	for (int i = 0; i < 20; ++i) {
		line.Add(NiPoint3{static_cast<float>(i), 0.0f, 0.0f});
	}
	Check(!DiscAxesFrom(line, mean, axes, spread), "a line of points: spread one way only, no disc");

	DiscScatter small;
	ShieldPoints(centre, u, w, n, 3.0f, 0.5f, &small, nullptr, nullptr, nullptr);
	Check(DiscAxesFrom(small, mean, axes, spread), "a 3-unit disc: axes");
	DiscExtent smallExtent;
	ShieldPoints(centre, u, w, n, 3.0f, 0.5f, nullptr, &smallExtent, &mean, axes);
	Check(!ShieldDiscFrom(mean, axes, smallExtent, disc) && !disc.valid, "but too small for a shield");
	DiscScatter huge;
	ShieldPoints(centre, u, w, n, 80.0f, 1.0f, &huge, nullptr, nullptr, nullptr);
	DiscAxesFrom(huge, mean, axes, spread);
	DiscExtent hugeExtent;
	ShieldPoints(centre, u, w, n, 80.0f, 1.0f, nullptr, &hugeExtent, &mean, axes);
	Check(!ShieldDiscFrom(mean, axes, hugeExtent, disc), "80 units: too large for one");
	DiscScatter block;
	ShieldPoints(centre, u, w, n, 20.0f, 15.0f, &block, nullptr, nullptr, nullptr);
	DiscAxesFrom(block, mean, axes, spread);
	DiscExtent blockExtent;
	ShieldPoints(centre, u, w, n, 20.0f, 15.0f, nullptr, &blockExtent, &mean, axes);
	Check(!ShieldDiscFrom(mean, axes, blockExtent, disc), "a block 30 thick: not flat, no shield");
	DiscExtent none;
	Check(!ShieldDiscFrom(mean, axes, none, disc), "no reach read: no disc");

	BodySpan ball;
	ShieldBallSpan(NiPoint3{1.0f, 2.0f, 3.0f}, 25.0f, ball, margin);
	Check(ball.valid && Near(ball.a.x, 1.0f) && Near(ball.b.z, 3.0f) && Near(margin, 25.0f),
	      "the ball that stands in: one point, the radius as the margin");
	ShieldBallSpan(NiPoint3{1.0f, 2.0f, 3.0f}, 0.0f, ball, margin);
	Check(!ball.valid, "no radius: no ball");

	// The disc in the world, for the stoppers: carried by a hand pose, round
	// at the longer spoke, half its thickness and a little (never under the
	// blade's margin).
	ShieldDisc kite;
	kite.valid = true;
	kite.centre = NiPoint3{0.0f, 10.0f, 0.0f};
	kite.axis[0] = NiPoint3{1.0f, 0.0f, 0.0f};
	kite.axis[1] = NiPoint3{0.0f, 1.0f, 0.0f};
	kite.axis[2] = NiPoint3{0.0f, 0.0f, 1.0f};
	kite.half[0] = 17.0f;
	kite.half[1] = 22.0f;
	kite.half[2] = 6.0f;
	const BladeDisc inWorld = ShieldDiscInWorld(kite, RotationAbout(NiPoint3{1.0f, 0.0f, 0.0f}, kPi / 2.0f),
	                                            NiPoint3{100.0f, 0.0f, 0.0f}, kStopperOwnShield);
	Check(Near(inWorld.centre.x, 100.0f) && Near(inWorld.centre.y, 0.0f) && Near(inWorld.centre.z, 10.0f),
	      "the centre carried out by the hand (a quarter turn about x takes y to z)");
	Check(Near(inWorld.normal.y, -1.0f, 0.01f), "the normal turned with it");
	Check(Near(inWorld.radius, 22.0f) && Near(inWorld.halfThick, 6.5f) && inWorld.id == kStopperOwnShield,
	      "the longer spoke, half the thickness and a little");
	kite.half[2] = 0.2f;
	Check(Near(ShieldDiscInWorld(kite, NiMatrix33::Identity(), NiPoint3{0, 0, 0}, 1).halfThick, kBladeContactMarginUnits),
	      "paper thin: the blade's margin at least");
	// The drawn blade as a stopper for the free hand.
	BladeBodies stoppers;
	BodySpan blade;
	blade.valid = true;
	blade.a = NiPoint3{0.0f, 0.0f, 0.0f};
	blade.b = NiPoint3{0.0f, 60.0f, 0.0f};
	BladePose drawn;
	drawn.pos = NiPoint3{5.0f, 5.0f, 5.0f};
	Check(AddOwnBladeStopper(drawn, blade, stoppers) && stoppers.count == 1 && stoppers.cap[0].actor == kStopperOwnBlade &&
	          Near(stoppers.cap[0].b.y, 65.0f) && Near(stoppers.cap[0].radius, kOwnBladeStopRadiusUnits),
	      "the blade's capsule at its drawn pose");
	BodySpan noBlade;
	Check(!AddOwnBladeStopper(drawn, noBlade, stoppers) && stoppers.count == 1, "no span read: none");

	Check(ShieldDiscStale(false, 1, 1, 20.0f, 20.0f, 0), "none kept: read");
	Check(!ShieldDiscStale(true, 1, 1, 20.0f, 20.2f, 10), "the same node and bound, young: kept");
	Check(ShieldDiscStale(true, 1, 2, 20.0f, 20.0f, 10), "another node: read again");
	Check(ShieldDiscStale(true, 1, 1, 20.0f, 21.0f, 10), "the bound changed: read again");
	Check(ShieldDiscStale(true, 1, 1, 20.0f, 20.0f, kShieldDiscReadFrames), "old: read again");
}

BladeContactFrame HandFrame(const NiPoint3& at, const BodySpan& span, const BodySpan& span2, float margin) {
	BladeContactFrame f;
	f.active = true;
	f.wanted.pos = at;
	f.span = span;
	f.span2 = span2;
	f.marginUnits = margin;
	f.eye = NiPoint3{0.0f, -40.0f, 40.0f};
	f.dtSeconds = 0.011f;
	return f;
}

void TestHandOnFloor() {
	std::printf("A hand lowered onto the floor\n");
	// A hand pointing down: 15 units from the wrist to the fingertips along -z.
	BodySpan hand;
	hand.valid = true;
	hand.a = NiPoint3{0.0f, 0.0f, 0.0f};
	hand.b = NiPoint3{0.0f, 0.0f, -15.0f};
	FloorWorld floor;
	BladeContactSettings set;
	BladeContactState s;
	StepBladeContact(s, set, HandFrame(NiPoint3{0.0f, 0.0f, 0.0f}, hand, BodySpan{}, kHandContactMarginUnits), floor);
	BladeContactVerdict v =
		StepBladeContact(s, set, HandFrame(NiPoint3{0.0f, 0.0f, -40.0f}, hand, BodySpan{}, kHandContactMarginUnits), floor);
	Check(v.held && v.event == BladeContactEvent::Touched, "lowered through the floor: held, a touch");
	Check(Near(BladePointAt(v.pose, hand, 1.0f).z, -40.0f + kHandContactMarginUnits, 0.05f),
	      "the fingertips held a hand's half width above it");
	Check(Near(v.contact.along, 1.0f), "the fingertips met it");
	v = StepBladeContact(s, set, HandFrame(NiPoint3{0.0f, 0.0f, 0.0f}, hand, BodySpan{}, kHandContactMarginUnits), floor);
	Check(!v.held, "raised: free");
}

void TestShieldOnFloor() {
	std::printf("A shield lowered onto the floor\n");
	// A disc of radius 20 in the hand's frame: spokes along x and y, flat.
	ShieldDisc disc;
	disc.valid = true;
	disc.centre = NiPoint3{0.0f, 10.0f, 0.0f};
	disc.half[0] = 20.0f;
	disc.half[1] = 20.0f;
	disc.half[2] = 1.0f;
	BodySpan a;
	BodySpan b;
	float margin = 0.0f;
	ShieldDiscSpans(disc, a, b, margin);
	FloorWorld floor;
	BladeContactSettings set;
	// Held flat (its normal up) and lowered: the whole face meets the floor
	// at once, held the margin above it.
	BladeContactState flat;
	StepBladeContact(flat, set, HandFrame(NiPoint3{0.0f, 0.0f, 0.0f}, a, b, margin), floor);
	BladeContactVerdict v = StepBladeContact(flat, set, HandFrame(NiPoint3{0.0f, 0.0f, -50.0f}, a, b, margin), floor);
	Check(v.held && Near(v.pose.pos.z, -40.0f + margin, 0.05f), "flat onto the floor: held the margin above it");
	// Held upright (turned a quarter about x: the y spoke now along z) and
	// lowered: the rim meets the floor first, the centre a spoke's length
	// above it.
	BladeContactState upright;
	BladeContactFrame f = HandFrame(NiPoint3{0.0f, 0.0f, 0.0f}, a, b, margin);
	f.wanted.rot = RotationAbout(NiPoint3{1.0f, 0.0f, 0.0f}, kPi / 2.0f);
	StepBladeContact(upright, set, f, floor);
	f.wanted.pos = NiPoint3{0.0f, 0.0f, -50.0f};
	v = StepBladeContact(upright, set, f, floor);
	const NiPoint3 centre = v.pose.pos + v.pose.rot * disc.centre;
	Check(v.held && Near(centre.z, -40.0f + 20.0f + margin, 0.1f), "upright onto the floor: the rim holds it, the centre a spoke up");
	// Only the y spoke reaches down there: the second span is what held it.
	Check(v.contact.hit && (Near(v.contact.along, 0.0f) || Near(v.contact.along, 1.0f)), "met at a spoke's end");
	// Turned a quarter about y instead: the x spoke stands up, and the first
	// span holds it the same way.
	BladeContactState other;
	f = HandFrame(NiPoint3{0.0f, 0.0f, 0.0f}, a, b, margin);
	f.wanted.rot = RotationAbout(NiPoint3{0.0f, 1.0f, 0.0f}, kPi / 2.0f);
	StepBladeContact(other, set, f, floor);
	f.wanted.pos = NiPoint3{0.0f, 0.0f, -50.0f};
	v = StepBladeContact(other, set, f, floor);
	const NiPoint3 centre2 = v.pose.pos + v.pose.rot * disc.centre;
	Check(v.held && Near(centre2.z, -40.0f + 20.0f + margin, 0.1f), "the other spoke upright: held the same");
	// The ball that stands in: swept as one point, held its radius above.
	BodySpan ball;
	ShieldBallSpan(NiPoint3{0.0f, 10.0f, 0.0f}, 16.0f, ball, margin);
	BladeContactState ballState;
	StepBladeContact(ballState, set, HandFrame(NiPoint3{0.0f, 0.0f, 0.0f}, ball, BodySpan{}, margin), floor);
	floor.casts = 0;
	v = StepBladeContact(ballState, set, HandFrame(NiPoint3{0.0f, 0.0f, -50.0f}, ball, BodySpan{}, margin), floor);
	Check(v.held && Near(v.pose.pos.z, -40.0f + 16.0f, 0.05f), "the ball: held its radius above the floor");
	Check(floor.casts <= 6, "and swept as one point, not five");
}

}  // namespace

int main() {
	TestWhichHand();
	TestSwinging();
	TestJacobi();
	TestDisc();
	TestHandOnFloor();
	TestShieldOnFloor();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

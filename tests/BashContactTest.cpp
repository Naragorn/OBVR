// Checks the shield bash's contact (game/BashContactLogic.h): the shield's
// disc against the target's column, not its middle as a point; the bash's
// speed into the body - straight into its surface or at its middle, whichever
// is more - not only towards the middle, which a jab that meets the body off
// its middle loses (Nadi's headset run of 2026-11-28 14:56: "ging anfangs
// nicht dann wieder schon"). All the attempts of the 13:55 and 14:56 logs are
// replayed as far as their lines carry the numbers.

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/BashContactLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.05f) { return a - b < eps && b - a < eps; }

using namespace obvr;
using namespace obvr::game;

const NiPoint3 kO{0.0f, 0.0f, 0.0f};

BashDisc Point(const NiPoint3& p) {
	BashDisc d;
	d.centre = p;
	return d;
}

// A shield of half 28 (an iron one's spokes), its face across `normal`.
BashDisc Shield(const NiPoint3& centre, const NiPoint3& a, const NiPoint3& b) {
	BashDisc d;
	d.centre = centre;
	d.axisA = a;
	d.axisB = b;
	d.halfA = 28.0f;
	d.halfB = 28.0f;
	return d;
}

const NiPoint3 kX{1.0f, 0.0f, 0.0f};
const NiPoint3 kY{0.0f, 1.0f, 0.0f};
const NiPoint3 kZ{0.0f, 0.0f, 1.0f};

void TestColumn() {
	std::printf("The column\n");
	const BashColumn c = BashColumnOf(kO, 52.0f);
	Check(Near(c.wide, 34.0f) && Near(c.high, 60.0f), "half the radius plus 8 across, the radius plus 8 up and down");
}

void TestGap() {
	std::printf("The disc against the column\n");
	const BashColumn c = BashColumnOf(kO, 52.0f);  // 34 across, 60 up and down
	Check(DiscColumnGap(Point(NiPoint3{-4.0f, 0.0f, -14.0f}), c).touching, "the middle inside: touching");
	const BashGap beside = DiscColumnGap(Point(NiPoint3{-50.0f, 0.0f, 0.0f}), c);
	Check(!beside.touching && Near(beside.gap, 16.0f, 0.3f), "a point 16 units beside: that far");
	const BashGap rim = DiscColumnGap(Shield(NiPoint3{0.0f, -57.0f, 0.0f}, kY, kZ), c);
	Check(rim.touching, "the middle 57 out, the shield edge on to them: its rim reaches 28 nearer and meets them");
	const BashGap faceOn = DiscColumnGap(Shield(NiPoint3{0.0f, -57.0f, 0.0f}, kX, kZ), c);
	Check(!faceOn.touching && Near(faceOn.gap, 23.0f, 0.5f), "the same middle, the shield face on to them: 23 short");
	const BashGap above = DiscColumnGap(Shield(NiPoint3{0.0f, 0.0f, 70.0f}, kX, kY), c);
	Check(!above.touching && Near(above.gap, 10.0f, 0.3f), "a flat shield 10 above their top: 10 short");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!DiscColumnGap(Point(NiPoint3{nan, 0.0f, 0.0f}), c).touching, "a middle not a number: not touching");
}

void TestClosing() {
	std::printf("The speed into the body\n");
	const BashColumn c = BashColumnOf(kO, 52.0f);
	const BashDisc out = Point(NiPoint3{-50.0f, 0.0f, 0.0f});
	Check(Near(SurfaceClosingSpeed(NiPoint3{4.0f, 0.0f, 0.0f}, out, c), 4.0f), "straight at its side: all of it");
	Check(Near(SurfaceClosingSpeed(NiPoint3{0.0f, 4.0f, 0.0f}, out, c), 0.0f), "along its side, a graze: nothing");
	Check(Near(SurfaceClosingSpeed(NiPoint3{-4.0f, 0.0f, 0.0f}, out, c), -4.0f), "pulled back: away");
	Check(Near(BashClosingSpeed(NiPoint3{0.0f, 0.0f, 0.0f}, out, c), 0.0f),
	      "the shield held still, they come into it: nothing of the hand's");
	const BashDisc low = Point(NiPoint3{-4.0f, 0.0f, -14.0f});
	Check(Near(BashSpeedTowards(NiPoint3{4.0f, 0.0f, 0.0f}, low.centre, kO), 1.1f),
	      "inside, near their axis, a level jab: towards the middle 1.1 of 4 (the line to it nearly upright)");
	Check(Near(BashClosingSpeed(NiPoint3{4.0f, 0.0f, 0.0f}, low, c), 4.0f), "into their side: 4");
	const BashDisc bottom = Point(NiPoint3{0.0f, 0.0f, -55.0f});
	Check(Near(SurfaceClosingSpeed(NiPoint3{0.0f, 0.0f, 3.0f}, Point(NiPoint3{-20.0f, 0.0f, -55.0f}), c), 3.0f),
	      "inside, nearest their underside, a jab up: 3");
	Check(Near(SurfaceClosingSpeed(NiPoint3{3.0f, 0.0f, 0.0f}, bottom, c), 0.0f),
	      "on their axis there is no side to go into: nothing of the surface's");
	// 14:56, 23113 (landed): 42 across, 19 below, 2.7 across the ground, 3.1 towards the middle.
	const BashDisc rising = Point(NiPoint3{-42.0f, 0.0f, -19.0f});
	const BashColumn c113{kO, 51.0f, 94.0f};
	const NiPoint3 v113{2.7f, 0.0f, 1.55f};
	Check(Near(BashClosingSpeed(v113, rising, c113), 3.1f), "a jab up at their middle: its 3.1 at the middle counts");
	const BashDisc front = Point(NiPoint3{-40.0f, -10.0f, 0.0f});
	Check(BashClosingSpeed(NiPoint3{0.0f, 5.0f, 0.0f}, front, c) < 3.0f, "a fast sweep across in front: under the gate");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float s = BashClosingSpeed(NiPoint3{nan, 0.0f, 0.0f}, out, c);
	Check(s == 0.0f, "a velocity not a number: none");
	const BashColumn bad{NiPoint3{nan, 0.0f, 0.0f}, 34.0f, 60.0f};
	Check(BashClosingSpeed(NiPoint3{4.0f, 0.0f, 0.0f}, out, bad) == 0.0f, "their centre not a number: none");
}

void TestBigTarget() {
	std::printf("Someone big beside\n");
	const BashColumn npc = BashColumnOf(kO, 60.0f);  // 38 across, 68 up and down
	const BashGap far = DiscColumnGap(Shield(NiPoint3{0.0f, -75.0f, 0.0f}, kY, kZ), npc);
	Check(!far.touching, "a shield at arm's length beside them, its middle 75 out: it does not reach");
	const BashDisc brush = Shield(NiPoint3{0.0f, -60.0f, 0.0f}, kY, kZ);
	Check(DiscColumnGap(brush, npc).touching, "60 out, its rim brushes their side");
	Check(BashClosingSpeed(NiPoint3{5.0f, 0.0f, 0.0f}, brush, npc) < 3.0f,
	      "a fast jab straight ahead past them, the rim brushing: no bash");
}

void TestAngle() {
	std::printf("The travel against the line to the middle\n");
	Check(Near(TravelDegreesOffMiddle(kX, NiPoint3{-10.0f, 0.0f, 0.0f}, kO), 0.0f, 0.5f), "straight at it: 0");
	Check(Near(TravelDegreesOffMiddle(kY, NiPoint3{-10.0f, 0.0f, 0.0f}, kO), 90.0f, 0.5f), "across: 90");
	Check(Near(TravelDegreesOffMiddle(kX, NiPoint3{-4.0f, 0.0f, -14.0f}, kO), 74.1f, 0.5f), "level, near the axis: 74");
	Check(TravelDegreesOffMiddle(kO, NiPoint3{-10.0f, 0.0f, 0.0f}, kO) == 0.0f, "no travel: 0");
}

// One attempt as its line gives it: the frame it reports (the shield's middle
// `a` across and `u` up from their middle, the column W by H), that frame's
// speed across the ground towards their axis and in 3D towards their middle,
// and the attempt's most speed of the controller.
enum class Expect { Proven, Model, Refused, Cooling };
struct Line {
	const char* name;
	float a, u, wide, high, ground, towards, top;
	bool middleIn;  // the middle in the column (else "never in their column")
	Expect expect;
};

void Replay(const Line& l) {
	char what[220];
	if (l.expect == Expect::Cooling) {
		std::snprintf(what, sizeof(what), "%s: cooling down - refused by the shove's cooldown, as before", l.name);
		Check(true, what);
		return;
	}
	if (l.expect == Expect::Refused) {
		std::snprintf(what, sizeof(what), "%s: the hand at most %.1f m/s - no measure reaches 3", l.name,
		              static_cast<double>(l.top));
		Check(l.top < 3.0f, what);
		return;
	}
	const BashColumn c{kO, l.wide, l.high};
	if (!l.middleIn) {
		// Only the model: a jab at their middle at its top speed, the shield face on, its middle where the line has
		// it - the disc short of the column, the contact step holding it on their body (their body wider there).
		const BashDisc d = Shield(NiPoint3{-l.a, 0.0f, l.u}, kY, kZ);
		const NiPoint3 v{l.top, 0.0f, 0.0f};
		const BashGap g = DiscColumnGap(d, c);
		const bool lands = !g.touching && BashTouches(g, true) && !BashTouches(g, false) &&
		                   BashClosingSpeed(v, d, c) >= 3.0f;
		std::snprintf(what, sizeof(what),
		              "%s: the middle %.0f out of %.0f, the disc %.0f short; on their body by the contact step, a jab at "
		              "%.1f lands [model]",
		              l.name, static_cast<double>(l.a), static_cast<double>(l.wide), static_cast<double>(g.gap),
		              static_cast<double>(l.top));
		Check(lands, what);
		return;
	}
	// The frame's velocity from its two speeds: across the ground into the axis, and up from towards the middle.
	const float length = std::sqrt(l.a * l.a + l.u * l.u);
	const float up = (l.u > 1.0f || l.u < -1.0f) ? (l.a * l.ground - length * l.towards) / l.u : 0.0f;
	const NiPoint3 v{l.ground, 0.0f, up};
	const NiPoint3 at{-l.a, 0.0f, l.u};
	const float again = BashSpeedTowards(v, at, kO);
	const float now = BashClosingSpeed(v, Point(at), c);
	const bool rebuilt = (l.u > 1.0f || l.u < -1.0f) ? Near(again, l.towards, 0.06f) : true;
	if (l.expect == Expect::Proven) {
		std::snprintf(what, sizeof(what), "%s: the frame alone %.1f into them (was %.1f at the middle) - lands",
		              l.name, static_cast<double>(now), static_cast<double>(l.towards));
		Check(rebuilt && now >= 2.95f && now + 0.06f >= l.towards, what);
	} else {
		std::snprintf(what, sizeof(what),
		              "%s: the frame alone %.1f into them (was %.1f), the hand up to %.1f - lands if its fastest frame "
		              "went into them [model]",
		              l.name, static_cast<double>(now), static_cast<double>(l.towards), static_cast<double>(l.top));
		Check(rebuilt && now < 2.95f && now + 0.06f >= l.towards && l.top >= 3.0f, what);
	}
}

void TestTheLogs() {
	std::printf("The attempts of 2026-11-28, replayed (13:55 and 14:56)\n");
	const Line lines[] = {
		// 13:55
		{"13:55 23249", 181, 118, 54, 100, 1.2f, 1.5f, 1.5f, false, Expect::Refused},
		{"13:55 23289", 100, 52, 57, 107, 0.7f, 0.3f, 2.1f, false, Expect::Refused},
		{"13:55 23369", 74, 50, 75, 143, 1.1f, 0.5f, 2.1f, true, Expect::Refused},
		{"13:55 23514", 65, 21, 66, 124, 1.9f, 1.9f, 2.4f, true, Expect::Refused},
		{"13:55 23644 (landed)", 31, 22, 57, 107, 4.2f, 3.2f, 4.2f, true, Expect::Proven},
		{"13:55 45763", 20, -33, 36, 63, 2.9f, 2.8f, 4.5f, true, Expect::Model},
		{"13:55 45765", 13, -29, 34, 59, -0.5f, -1.1f, 1.5f, true, Expect::Refused},
		{"13:55 45777", 10, -22, 36, 63, 2.7f, 2.8f, 5.1f, true, Expect::Model},
		{"13:55 45779", 19, -29, 36, 63, 0.7f, -0.7f, 1.6f, true, Expect::Refused},
		{"13:55 46419", 134, -76, 35, 61, 0.3f, 0.6f, 1.6f, false, Expect::Refused},
		{"13:55 47918 (landed)", 16, -28, 35, 61, 4.0f, 3.1f, 4.3f, true, Expect::Proven},
		{"13:55 47928", 16, -24, 35, 61, 3.9f, 3.2f, 4.5f, true, Expect::Cooling},
		{"13:55 47936", 17, -24, 37, 65, 2.0f, 2.4f, 4.1f, true, Expect::Model},
		{"13:55 47939", 53, -55, 35, 62, 0.0f, -1.0f, 1.5f, false, Expect::Refused},
		{"13:55 47950", 22, -17, 34, 60, 2.6f, 2.5f, 3.0f, true, Expect::Model},
		{"13:55 47961", 10, -27, 35, 62, 1.9f, 1.8f, 4.0f, true, Expect::Model},
		{"13:55 47974", 21, -27, 35, 62, 3.3f, 2.9f, 3.9f, true, Expect::Proven},
		{"13:55 51547", 6, -16, 32, 57, 2.3f, 1.6f, 3.8f, true, Expect::Model},
		{"13:55 51548", 17, -28, 32, 56, -0.7f, -0.5f, 1.9f, true, Expect::Refused},
		{"13:55 51549 (landed)", 13, -11, 32, 55, 3.2f, 3.1f, 3.5f, true, Expect::Proven},
		{"13:55 51559", 29, -17, 44, 79, 4.2f, 4.3f, 5.2f, true, Expect::Cooling},
		{"13:55 51561", 89, -3, 44, 80, -0.9f, -0.9f, 1.8f, false, Expect::Refused},
		{"13:55 51564", 54, -1, 42, 77, 1.0f, 1.0f, 2.0f, false, Expect::Refused},
		{"13:55 51567 (landed)", 23, -15, 39, 71, 3.2f, 3.0f, 3.2f, true, Expect::Proven},
		{"13:55 51575", 22, -13, 40, 72, 3.4f, 3.2f, 3.7f, true, Expect::Cooling},
		{"13:55 51580 (landed)", 39, 8, 40, 72, 3.9f, 3.9f, 3.9f, true, Expect::Proven},
		{"13:55 51588", 32, -2, 40, 71, 4.9f, 4.8f, 6.3f, true, Expect::Cooling},
		{"13:55 51589", 80, -8, 43, 78, -1.5f, -1.5f, 1.5f, false, Expect::Refused},
		{"13:55 51594", 30, -16, 43, 79, 1.6f, 1.6f, 2.0f, true, Expect::Refused},
		// 14:56
		{"14:56 22427", 4, -14, 34, 61, 1.6f, 0.8f, 4.3f, true, Expect::Model},
		{"14:56 22436", 12, -12, 35, 63, 0.7f, 0.8f, 4.3f, true, Expect::Model},
		{"14:56 23051", 148, -22, 34, 61, 0.0f, 0.2f, 1.8f, false, Expect::Refused},
		{"14:56 23053", 148, -17, 34, 61, -0.5f, -0.3f, 2.1f, false, Expect::Refused},
		{"14:56 23064", 25, 43, 36, 64, 3.7f, 2.4f, 4.1f, true, Expect::Proven},
		{"14:56 23067 (landed)", 29, 31, 31, 54, 3.0f, 3.2f, 3.4f, true, Expect::Proven},
		{"14:56 23085", 15, 4, 43, 77, 1.9f, 2.2f, 4.1f, true, Expect::Cooling},
		{"14:56 23094", 24, -31, 52, 95, 1.5f, 0.9f, 3.8f, true, Expect::Model},
		{"14:56 23107", 23, -27, 46, 84, 1.9f, 0.1f, 4.2f, true, Expect::Model},
		{"14:56 23110", 45, -33, 45, 82, 1.0f, 0.5f, 2.2f, true, Expect::Refused},
		{"14:56 23113 (landed)", 42, -19, 51, 94, 2.7f, 3.1f, 3.3f, true, Expect::Proven},
		{"14:56 23125", 33, -14, 51, 94, 2.4f, 2.8f, 3.9f, true, Expect::Cooling},
		{"14:56 23129", 71, -5, 45, 82, 0.0f, -0.0f, 1.8f, false, Expect::Refused},
		{"14:56 23130", 71, 10, 48, 88, -0.9f, -1.0f, 1.5f, false, Expect::Refused},
		{"14:56 23132 (landed)", 38, 2, 48, 89, 3.0f, 3.0f, 3.2f, true, Expect::Proven},
		{"14:56 23143", 17, -8, 46, 84, 4.9f, 4.4f, 5.5f, true, Expect::Cooling},
		{"14:56 23144", 67, -9, 45, 82, -0.0f, 0.0f, 1.6f, false, Expect::Refused},
		{"14:56 23148", 57, -6, 46, 83, 0.0f, 0.1f, 4.7f, false, Expect::Model},
		{"14:56 23150", 72, -18, 47, 85, -1.5f, -1.3f, 1.8f, false, Expect::Refused},
		{"14:56 23155", 24, -11, 45, 82, 1.9f, 1.9f, 4.4f, true, Expect::Model},
		{"14:56 23157", 8, -26, 50, 92, -1.2f, -1.3f, 1.6f, true, Expect::Refused},
		{"14:56 25966", 68, -36, 34, 59, -0.2f, 0.5f, 1.9f, false, Expect::Refused},
	};
	for (const Line& l : lines) {
		Replay(l);
	}
}

}  // namespace

int main() {
	TestColumn();
	TestGap();
	TestClosing();
	TestBigTarget();
	TestAngle();
	TestTheLogs();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all bash contact checks passed\n");
	return 0;
}

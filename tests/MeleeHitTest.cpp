// Checks the arithmetic of a blade meeting a body: the blade in the world
// from the hand's pose, the distance from a point to a segment, the strike
// against a bound sphere, the once-per-swing ledger, and which weapon types
// are swung.

#include <cstdio>

#include "core/Rotation.h"
#include "game/MeleeHit.h"

namespace {

using namespace obvr;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

bool Near(const NiPoint3& a, const NiPoint3& b, float eps = 0.01f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

void TestBladeInWorld() {
	std::printf("The blade in the world\n");
	const NiMatrix33 identity = NiMatrix33::Identity();
	const NiPoint3 eyes{100.0f, 200.0f, 300.0f};

	Blade blade = BladeInWorld(identity, eyes, identity, NiPoint3{20.0f, 30.0f, -10.0f}, 90.0f);
	Check(Near(blade.base, NiPoint3{120.0f, 230.0f, 290.0f}),
	      "an unturned camera puts the hilt at the eyes plus the hand's offset");
	Check(Near(blade.tip, NiPoint3{120.0f, 320.0f, 290.0f}),
	      "and an unturned hand points the blade forward (y) for the reach");

	// The hand turned ninety degrees to the right about the up axis: its
	// forward is now the world's +x.
	const NiMatrix33 handRight = EulerToMatrix(0.0f, 0.0f, -90.0f);
	blade = BladeInWorld(identity, eyes, handRight, NiPoint3{0.0f, 0.0f, 0.0f}, 50.0f);
	const NiPoint3 along = blade.tip - blade.base;
	Check(Near(along.x, 50.0f, 0.05f) || Near(along.x, -50.0f, 0.05f),
	      "a hand turned about the up axis points the blade sideways");
	Check(Near(along.y, 0.0f, 0.05f) && Near(along.z, 0.0f, 0.05f), "and nowhere else");

	// The camera turned instead: the hand's offset and the blade turn with it.
	const NiMatrix33 cameraRight = EulerToMatrix(0.0f, 0.0f, -90.0f);
	blade = BladeInWorld(cameraRight, eyes, identity, NiPoint3{0.0f, 30.0f, 0.0f}, 50.0f);
	const NiPoint3 hilt = blade.base - eyes;
	const NiPoint3 dir = blade.tip - blade.base;
	Check(Near(hilt.y, 0.0f, 0.05f) && (Near(hilt.x, 30.0f, 0.05f) || Near(hilt.x, -30.0f, 0.05f)),
	      "a turned camera carries the hand's offset round");
	Check(Near(dir.y, 0.0f, 0.05f) && Near(dir.z, 0.0f, 0.05f) &&
	          ((hilt.x > 0.0f) == (dir.x > 0.0f)),
	      "and the blade round with it, the same way");

	blade = BladeInWorld(identity, eyes, identity, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f);
	Check(Near(blade.base, blade.tip), "no reach is a blade of no length");
}

void TestSegmentDistance() {
	std::printf("Point to segment\n");
	const NiPoint3 a{0.0f, 0.0f, 0.0f};
	const NiPoint3 b{10.0f, 0.0f, 0.0f};
	Check(Near(SegmentPointDistance(a, b, NiPoint3{5.0f, 3.0f, 0.0f}), 3.0f),
	      "beside the middle: the perpendicular");
	Check(Near(SegmentPointDistance(a, b, NiPoint3{-4.0f, 3.0f, 0.0f}), 5.0f),
	      "before the start: the distance to the start");
	Check(Near(SegmentPointDistance(a, b, NiPoint3{13.0f, 4.0f, 0.0f}), 5.0f),
	      "past the end: the distance to the end");
	Check(Near(SegmentPointDistance(a, b, NiPoint3{7.0f, 0.0f, 0.0f}), 0.0f), "on it: nothing");
	Check(Near(SegmentPointDistance(a, a, NiPoint3{0.0f, 0.0f, 2.0f}), 2.0f),
	      "a segment of no length is its one point");
}

void TestStrike() {
	std::printf("The strike\n");
	Blade blade;
	blade.base = NiPoint3{0.0f, 0.0f, 100.0f};
	blade.tip = NiPoint3{0.0f, 90.0f, 100.0f};
	// A body sixty units across, its centre forty units ahead of the tip's
	// path, sideways.
	const NiPoint3 centre{40.0f, 50.0f, 100.0f};
	Check(!BladeStrikes(blade, centre, 60.0f, 0.5f, 8.0f),
	      "forty units away with thirty plus eight allowed: a miss");
	Check(BladeStrikes(blade, centre, 60.0f, 0.7f, 0.0f),
	      "forty-two allowed: a strike");
	Check(BladeStrikes(blade, NiPoint3{0.0f, 120.0f, 100.0f}, 60.0f, 0.5f, 0.0f),
	      "thirty past the tip, thirty allowed: a strike at the very end");
	Check(!BladeStrikes(blade, NiPoint3{0.0f, 121.0f, 100.0f}, 60.0f, 0.5f, 0.0f),
	      "one more unit past: a miss");
	Check(BladeStrikes(blade, NiPoint3{0.0f, 45.0f, 100.0f}, 0.0f, 0.7f, 0.0f),
	      "a bound of no radius still takes a blade through its centre");
	Check(!BladeStrikes(blade, NiPoint3{1.0f, 45.0f, 100.0f}, 0.0f, 0.7f, 0.0f),
	      "but nothing beside it");
	Check(BladeStrikes(blade, NiPoint3{5.0f, 45.0f, 100.0f}, 0.0f, 0.7f, 8.0f),
	      "unless the pad allows it");
	Check(!BladeStrikes(blade, NiPoint3{5.0f, 45.0f, 100.0f}, -60.0f, -1.0f, -8.0f),
	      "negative numbers count as zero, not as a strike on everything");
}

void TestLedger() {
	std::printf("Once per swing\n");
	SwingLedger ledger;
	int a = 0, b = 0;
	Check(LedgerAdmits(ledger, 1, &a), "the first body of a swing is admitted");
	Check(!LedgerAdmits(ledger, 1, &a), "the same body again in the same swing is not");
	Check(LedgerAdmits(ledger, 1, &b), "another body in the same swing is");
	Check(LedgerAdmits(ledger, 2, &a), "the next swing starts afresh");
	Check(!LedgerAdmits(ledger, 2, &a), "and remembers its own");

	SwingLedger full;
	int bodies[SwingLedger::kCapacity + 1] = {};
	for (UInt32 at = 0; at < SwingLedger::kCapacity; ++at) {
		LedgerAdmits(full, 7, &bodies[at]);
	}
	Check(!LedgerAdmits(full, 7, &bodies[SwingLedger::kCapacity]),
	      "a swing does not strike a ninth body");
	Check(LedgerAdmits(full, 8, &bodies[SwingLedger::kCapacity]), "the next swing does");
}

void TestWeaponTypes() {
	std::printf("Which weapons are swung\n");
	Check(WeaponIsSwung(static_cast<SInt32>(WeaponTypeCode::None)), "fists are");
	Check(WeaponIsSwung(0) && WeaponIsSwung(1) && WeaponIsSwung(2) && WeaponIsSwung(3),
	      "blades and blunt weapons, one and two handed, are");
	Check(!WeaponIsSwung(static_cast<SInt32>(WeaponTypeCode::Staff)), "a staff is not");
	Check(!WeaponIsSwung(static_cast<SInt32>(WeaponTypeCode::Bow)), "a bow is not");
	Check(!WeaponIsSwung(6) && !WeaponIsSwung(-2), "nor anything unknown");
}

}  // namespace

int main() {
	TestBladeInWorld();
	TestSegmentDistance();
	TestStrike();
	TestLedger();
	TestWeaponTypes();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

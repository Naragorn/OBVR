// Checks holding a two-handed weapon with both hands (vr/TwoHandLogic.h).

#include <cstdio>

#include "vr/TwoHandLogic.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

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
	Check(TwoHandTakes(s, true, true, 20.0f, 3.0f), "a two-hander, the grip closing on the handle: taken");
	Check(TwoHandTakes(s, true, true, -15.0f, 3.0f), "below the right hand, on the pommel side: taken");
	Check(!TwoHandTakes(s, false, true, 20.0f, 3.0f), "a one-hander or sheathed: not");
	Check(!TwoHandTakes(s, true, false, 20.0f, 3.0f), "the grip already closed: not");
	Check(!TwoHandTakes(s, true, true, 20.0f, 20.0f), "20 units beside the line: not");
	Check(!TwoHandTakes(s, true, true, 60.0f, 3.0f), "up at the blade: not");
	Check(!TwoHandTakes(s, true, true, -40.0f, 3.0f), "far behind: not");
	Check(!TwoHandTakes(s, true, true, 2.0f, 3.0f), "on the right hand itself: not");
	TwoHandSettings off;
	off.enabled = false;
	Check(!TwoHandTakes(off, true, true, 20.0f, 3.0f), "switched off: not");
}

void TestHold() {
	std::printf("Holding\n");
	const TwoHandSettings s;
	TwoHandState t;
	StartTwoHand(t, -15.0f);
	Check(t.active && t.sign == -1.0f && Near(t.distance, 15.0f), "taken below: the pommel side, 15 apart");
	Check(TwoHandHolds(t, s, true, true, 25.0f), "10 further apart: still held");
	Check(!TwoHandHolds(t, s, true, true, 60.0f) && !t.active, "45 further apart: let go");
	StartTwoHand(t, 20.0f);
	Check(!TwoHandHolds(t, s, true, false, 20.0f) && !t.active, "the grip opens: let go");
	StartTwoHand(t, 20.0f);
	Check(!TwoHandHolds(t, s, false, true, 20.0f), "sheathed: let go");
	Check(!TwoHandHolds(t, s, true, true, 20.0f), "not held: nothing");
}

void TestDirection() {
	std::printf("Where the weapon points\n");
	TwoHandState up;
	StartTwoHand(up, 20.0f);
	const NiPoint3 right{0, 0, 0};
	Check(NearV(TwoHandDirection(up, right, NiPoint3{0, 0, 30}), NiPoint3{0, 0, 1}),
	      "the left hand above, up the handle: pointing at it");
	TwoHandState down;
	StartTwoHand(down, -20.0f);
	Check(NearV(TwoHandDirection(down, right, NiPoint3{0, 0, -30}), NiPoint3{0, 0, 1}),
	      "the left hand below: pointing away from it");
	Check(NearV(TwoHandDirection(up, right, right), NiPoint3{0, 0, 0}), "the hands in one place: nowhere");
	Check(NearV(TwoHandLeftAt(down, right, NiPoint3{0, 0, 1}), NiPoint3{0, 0, -20}),
	      "the left hand held where it took hold, 20 below");
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

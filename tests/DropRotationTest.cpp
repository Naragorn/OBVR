// Checks the dropped item's turn (game/DropRotationLogic.h): the engine's
// own build of a reference's matrix from its three angles (0x004D7AF0,
// read), the angles found back from a matrix for every turn including the
// two straight up and down, and where the item hangs in the hand carried from
// the drawn pose to the hand's pose now.

#include <cmath>
#include <cstdio>

#include "game/DropRotationLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 1.0e-4f) { return std::fabs(a - b) < eps; }

bool SameMatrix(const NiMatrix33& a, const NiMatrix33& b, float eps = 1.0e-4f) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.data[r][c], b.data[r][c], eps)) {
				return false;
			}
		}
	}
	return true;
}

void TestEngineBuild() {
	std::printf("The engine's build of a reference's matrix (0x004D7AF0)\n");
	const float a = 0.3f;
	const NiMatrix33 x = RefMatrixFromAngles(NiPoint3{a, 0.0f, 0.0f});
	// 0x0070FD30: [[1,0,0],[0,c,s],[0,-s,c]]
	Check(Near(x.data[1][1], std::cos(a)) && Near(x.data[1][2], std::sin(a)) && Near(x.data[2][1], -std::sin(a)),
	      "about x: [0 c s; 0 -s c] as read");
	const NiMatrix33 y = RefMatrixFromAngles(NiPoint3{0.0f, a, 0.0f});
	// 0x0070FD80: [[c,0,-s],[0,1,0],[s,0,c]]
	Check(Near(y.data[0][2], -std::sin(a)) && Near(y.data[2][0], std::sin(a)), "about y: [c 0 -s; s 0 c] as read");
	const NiMatrix33 z = RefMatrixFromAngles(NiPoint3{0.0f, 0.0f, a});
	// 0x0070FDD0: [[c,s,0],[-s,c,0],[0,0,1]]
	Check(Near(z.data[0][1], std::sin(a)) && Near(z.data[1][0], -std::sin(a)), "about z: [c s 0; -s c 0] as read");
	const NiMatrix33 all = RefMatrixFromAngles(NiPoint3{0.2f, -0.4f, 1.1f});
	Check(SameMatrix(all, RefMatrixFromAngles(NiPoint3{0.2f, 0.0f, 0.0f}) *
	                          RefMatrixFromAngles(NiPoint3{0.0f, -0.4f, 0.0f}) *
	                          RefMatrixFromAngles(NiPoint3{0.0f, 0.0f, 1.1f})),
	      "all three: x times y times z, as the engine multiplies them in turn");
}

void TestRoundTrip() {
	std::printf("The angles found back from a matrix\n");
	int bad = 0;
	int tried = 0;
	for (float x = -3.0f; x <= 3.0f; x += 0.7f) {
		for (float y = -1.5f; y <= 1.5f; y += 0.35f) {
			for (float z = -3.0f; z <= 3.0f; z += 0.9f) {
				const NiMatrix33 m = RefMatrixFromAngles(NiPoint3{x, y, z});
				const NiMatrix33 back = RefMatrixFromAngles(RefAnglesFromMatrix(m));
				++tried;
				bad += SameMatrix(m, back, 2.0e-4f) ? 0 : 1;
			}
		}
	}
	char what[96];
	std::snprintf(what, sizeof(what), "every turn of a grid (%d) built again the same from its angles", tried);
	Check(bad == 0, what);
	const NiMatrix33 up = RefMatrixFromAngles(NiPoint3{0.5f, 1.5707963f, 0.8f});
	Check(SameMatrix(up, RefMatrixFromAngles(RefAnglesFromMatrix(up)), 2.0e-4f),
	      "the blade straight up (y a quarter turn): the same matrix again");
	const NiMatrix33 down = RefMatrixFromAngles(NiPoint3{-0.7f, -1.5707963f, 0.2f});
	Check(SameMatrix(down, RefMatrixFromAngles(RefAnglesFromMatrix(down)), 2.0e-4f),
	      "the blade straight down: the same matrix again");
	Check(SameMatrix(RefMatrixFromAngles(RefAnglesFromMatrix(NiMatrix33::Identity())), NiMatrix33::Identity()),
	      "no turn: no turn");
}

void TestAngleBetween() {
	std::printf("How far two turns lie apart\n");
	const NiMatrix33 a = RefMatrixFromAngles(NiPoint3{0.0f, 0.0f, 0.0f});
	const NiMatrix33 b = RefMatrixFromAngles(NiPoint3{0.0f, 0.0f, 0.5f});
	Check(Near(TurnBetweenDegrees(a, b), 0.5f * 57.29578f, 0.01f), "half a radian about z: 28.6 degrees");
	Check(Near(TurnBetweenDegrees(b, b), 0.0f, 0.01f), "the same: 0");
}

void TestCarry() {
	std::printf("The item carried from the drawn hand to the hand now\n");
	// The hand drawn at a turn and a place; the item hanging off it.
	const NiMatrix33 drawnHand = RefMatrixFromAngles(NiPoint3{0.1f, 0.2f, 0.3f});
	const NiPoint3 drawnHandPos{10.0f, 20.0f, 30.0f};
	const NiMatrix33 inHand = RefMatrixFromAngles(NiPoint3{-0.5f, 0.0f, 1.0f});
	const NiPoint3 inHandPos{0.0f, 5.0f, -2.0f};
	const NiMatrix33 itemRot = drawnHand * inHand;
	const NiPoint3 itemPos = drawnHandPos + drawnHand * inHandPos;
	// The same hand now: the item where it was.
	ItemPose same = CarryItemPose(drawnHand, drawnHandPos, itemRot, itemPos, drawnHand, drawnHandPos);
	Check(SameMatrix(same.rot, itemRot) && Near(same.pos.x, itemPos.x) && Near(same.pos.z, itemPos.z),
	      "the hand not moved: the item as drawn");
	// The hand turned and moved since: the item with it.
	const NiMatrix33 nowHand = RefMatrixFromAngles(NiPoint3{0.4f, -0.3f, 0.9f});
	const NiPoint3 nowHandPos{-5.0f, 2.0f, 40.0f};
	const ItemPose moved = CarryItemPose(drawnHand, drawnHandPos, itemRot, itemPos, nowHand, nowHandPos);
	const NiPoint3 wantPos = nowHandPos + nowHand * inHandPos;
	Check(SameMatrix(moved.rot, nowHand * inHand), "the hand turned: the item turned with it");
	Check(Near(moved.pos.x, wantPos.x, 1.0e-3f) && Near(moved.pos.y, wantPos.y, 1.0e-3f) &&
	          Near(moved.pos.z, wantPos.z, 1.0e-3f),
	      "the hand moved: the item where the hand holds it now");
}

}  // namespace

int main() {
	TestEngineBuild();
	TestRoundTrip();
	TestAngleBetween();
	TestCarry();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

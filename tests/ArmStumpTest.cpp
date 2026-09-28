// Checks the forearm stump's arithmetic (game/ArmStump.h): which skin bones
// collapse where, the collapse, the shared spine map, and when it is drawn.

#include <cmath>
#include <cstdio>

#include "game/ArmStump.h"

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

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
bool NearP(const NiPoint3& a, const NiPoint3& b, float eps = 1e-3f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

void TestRoles() {
	std::printf("Which bones collapse\n");
	Check(StumpRoleOf("Bip01 L UpperArm") == StumpRole::LeftUpper &&
	          StumpRoleOf("Bip01 L UpperArmTwist") == StumpRole::LeftUpper &&
	          StumpRoleOf("Bip01 L Clavicle") == StumpRole::LeftUpper,
	      "the left side above the elbow: into the left elbow");
	Check(StumpRoleOf("Bip01 R UpperArm") == StumpRole::RightUpper &&
	          StumpRoleOf("bip01 r clavicle") == StumpRole::RightUpper,
	      "the right side, case aside: into the right elbow");
	Check(StumpRoleOf("Bip01 R Hand") == StumpRole::Keep && StumpRoleOf("Bip01 L Finger1") == StumpRole::Keep,
	      "the hand and the fingers stay");
	Check(StumpRoleOf("Bip01 Spine2") == StumpRole::Centre && StumpRoleOf("Bip01 Neck1") == StumpRole::Centre &&
	          StumpRoleOf("Bip01 Pelvis") == StumpRole::Centre,
	      "a bone of neither side: the shared one");
	Check(StumpRoleOf(nullptr) == StumpRole::Keep, "no name: kept");
	Check(NameIs("Arms", "arms") && !NameIs("Arms2", "Arms") && !NameIs("Arm", "Arms") && !NameIs(nullptr, "Arms"),
	      "the shape by its exact name");
	Check(NameHas("Bip01 L UpperArm", "upperarm") && !NameHas("Bip01 L Hand", "UpperArm") && !NameHas(nullptr, "x"),
	      "a part of a name");
}

void TestCollapse() {
	std::printf("The collapse\n");
	const NiPoint3 elbow{10.0f, 20.0f, 30.0f};
	const NiTransform t = CollapseAt(elbow);
	const NiPoint3 distant = ThroughTransform(t, NiPoint3{500.0f, -300.0f, 80.0f});
	Check(NearP(distant, elbow, 1.0f), "a vertex half a body away lands within a unit of the elbow");
	Check(NearP(ThroughTransform(t, NiPoint3{0.0f, 0.0f, 0.0f}), elbow), "the bone's own origin: the elbow");
}

void TestCentre() {
	std::printf("The shared spine\n");
	// The spine turned a quarter round z, at (5, 0, 100), scale 1: a vertex
	// at bone-space (0, -13, 0) is in the world at (5 + 13, 0, 100) - to the
	// right, the body's right being +x.
	NiTransform spine;
	spine.rot = NiMatrix33::Identity();
	spine.rot.data[0][0] = 0.0f;
	spine.rot.data[0][1] = -1.0f;
	spine.rot.data[1][0] = 1.0f;
	spine.rot.data[1][1] = 0.0f;
	spine.pos = NiPoint3{5.0f, 0.0f, 100.0f};
	spine.scale = 1.0f;
	const NiPoint3 left{-20.0f, 30.0f, 80.0f};
	const NiPoint3 right{25.0f, 35.0f, 82.0f};
	const NiPoint3 across{1.0f, 0.0f, 0.0f};
	const NiPoint3 centre{5.0f, 0.0f, 100.0f};
	const NiTransform t = CentreStump(spine, left, right, across, centre, 13.0f);
	const NiPoint3 rightShoulder{0.0f, -13.0f, 0.0f};
	const NiPoint3 leftShoulder{0.0f, 13.0f, 0.0f};
	Check(NearP(ThroughTransform(spine, rightShoulder), NiPoint3{18.0f, 0.0f, 100.0f}), "the test's spine as drawn");
	Check(NearP(ThroughTransform(t, rightShoulder), right), "13 units to the right: the right elbow");
	Check(NearP(ThroughTransform(t, leftShoulder), left), "13 to the left: the left elbow");
	Check(NearP(ThroughTransform(t, NiPoint3{0.0f, 0.0f, 0.0f}), (left + right) * 0.5f), "the middle: between them");
	Check(NearP(ThroughTransform(t, NiPoint3{7.0f, -13.0f, -4.0f}), right),
	      "up, down, forward and back make no difference: flat along them");
	spine.scale = 2.0f;
	const NiTransform scaled = CentreStump(spine, left, right, across, centre, 13.0f);
	Check(NearP(ThroughTransform(scaled, NiPoint3{0.0f, -6.5f, 0.0f}), right), "the spine's scale is counted");
	const NiTransform none = CentreStump(spine, left, right, across, centre, 0.0f);
	Check(NearP(ThroughTransform(none, rightShoulder), (left + right) * 0.5f), "no shoulder width: all in the middle");
}

void TestHelpers() {
	std::printf("The rest\n");
	Check(NearP(UnitFromTo(NiPoint3{1.0f, 1.0f, 1.0f}, NiPoint3{1.0f, 4.0f, 1.0f}), NiPoint3{0.0f, 1.0f, 0.0f}),
	      "a unit from one point to another");
	Check(NearP(UnitFromTo(NiPoint3{1.0f, 1.0f, 1.0f}, NiPoint3{1.0f, 1.0f, 1.0f}), NiPoint3{1.0f, 0.0f, 0.0f}),
	      "two that coincide: +x");
	Check(StumpWanted(true, true, true), "wanted, bare, pinned: drawn");
	Check(!StumpWanted(false, true, true) && !StumpWanted(true, false, true) && !StumpWanted(true, true, false),
	      "without any one of them: not");
}


void TestSquash() {
	std::printf("The short forearm\n");
	Check(StumpRoleOf("Bip01 L Forearm") == StumpRole::LeftForearm &&
	          StumpRoleOf("Bip01 R ForearmTwist") == StumpRole::RightForearm,
	      "the forearm and its twist: squashed, each side its own");
	// A forearm bone at the elbow (0, -20, 0), the wrist at the origin, the
	// arm running along -y.
	NiTransform forearm;
	forearm.rot = NiMatrix33::Identity();
	forearm.pos = NiPoint3{0.0f, -20.0f, 0.0f};
	forearm.scale = 1.0f;
	const NiPoint3 wrist{0.0f, 0.0f, 0.0f};
	const NiPoint3 axis{0.0f, -1.0f, 0.0f};
	const NiTransform t = SquashAlong(forearm, wrist, axis, 0.25f);
	Check(NearP(ThroughTransform(t, NiPoint3{0.0f, 0.0f, 0.0f}), NiPoint3{0.0f, -5.0f, 0.0f}),
	      "the elbow a quarter of the way from the wrist");
	Check(NearP(ThroughTransform(t, NiPoint3{3.0f, 20.0f, -2.0f}), NiPoint3{3.0f, 0.0f, -2.0f}),
	      "a skin point at the wrist stays put, across the arm untouched");
	Check(NearP(ThroughTransform(t, NiPoint3{3.0f, 10.0f, 0.0f}), NiPoint3{3.0f, -2.5f, 0.0f}),
	      "half way down the forearm: a quarter of that");
	Check(NearP(StumpEnd(wrist, NiPoint3{0.0f, -20.0f, 0.0f}, 0.25f), NiPoint3{0.0f, -5.0f, 0.0f}),
	      "the stump ends where the squashed elbow is");
	Check(NearP(ThroughTransform(SquashAlong(forearm, wrist, axis, 1.0f), NiPoint3{1.0f, 2.0f, 3.0f}),
	            ThroughTransform(forearm, NiPoint3{1.0f, 2.0f, 3.0f})),
	      "a share of 1: the real forearm");
}

}  // namespace

int main() {
	TestRoles();
	TestCollapse();
	TestCentre();
	TestSquash();
	TestHelpers();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

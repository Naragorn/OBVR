// Checks the trunk stood upright (game/BodyUprightLogic.h): a chain already
// upright left; a chain swayed aside, facing two ways, turned until no
// segment leans aside, its forward bow, its lengths and its pelvis kept; a
// segment too short across to say; a pose leaning past a spine's bend; the
// hips across from the root, near and too far; numbers that are not numbers
// and chains of no use.

#include <cstdio>
#include <limits>

#include "game/BodyIkLogic.h"
#include "game/BodyUprightLogic.h"

namespace {

using namespace obvr::game;
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
const float kDeg = 0.01745329252f;

float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float Length(const NiPoint3& a) { return obvr::math::Sqrt(Dot(a, a)); }

// The turns applied as the bones take them: joint k turned about the body's
// forward through itself, every joint above it carried.
void Apply(NiPoint3* joints, UInt32 count, const TrunkUpright& up, float bodyYaw) {
	const NiPoint3 forward = UprightForward(bodyYaw);
	for (UInt32 k = 0; k < up.turns; ++k) {
		const obvr::NiMatrix33 r = ik::TurnAbout(forward, up.turn[k]);
		for (UInt32 j = k + 1; j < count; ++j) {
			joints[j] = joints[k] + r * (joints[j] - joints[k]);
		}
	}
}

// A chain in the body's own frame (x right, y forward, z up) put into the
// world for a body facing `bodyYaw`, standing at `at`.
void Place(const NiPoint3* local, NiPoint3* world, UInt32 count, float bodyYaw, const NiPoint3& at) {
	const NiPoint3 right = UprightRight(bodyYaw);
	const NiPoint3 forward = UprightForward(bodyYaw);
	for (UInt32 i = 0; i < count; ++i) {
		world[i] = at + right * local[i].x + forward * local[i].y + NiPoint3{0.0f, 0.0f, local[i].z};
	}
}

// The idle's sway, made up: the pelvis 4 to the right of the root, the spine
// bending left over the chest and back over the neck, bowed a little forward.
const NiPoint3 kSwayed[7] = {
	{4.0f, 0.0f, 67.0f},   // pelvis
	{4.5f, 0.5f, 74.0f},   // spine
	{3.5f, 1.0f, 82.0f},   // spine1
	{2.0f, 1.5f, 90.0f},   // spine2
	{1.0f, 2.0f, 100.0f},  // neck
	{1.2f, 2.5f, 104.0f},  // neck1
	{1.8f, 3.0f, 112.0f},  // head
};

void TestSwayed(float bodyYaw, const char* label) {
	std::printf("A swayed idle stood upright, facing %s\n", label);
	const NiPoint3 root{100.0f, -50.0f, 10.0f};
	NiPoint3 joints[7];
	Place(kSwayed, joints, 7, bodyYaw, root);
	NiPoint3 before[7];
	for (UInt32 i = 0; i < 7; ++i) {
		before[i] = joints[i];
	}
	const TrunkUpright up = UprightTrunk(joints, 7, root, bodyYaw);
	Check(up.valid && up.turns == 6, "valid, six joints turned");
	Check(Near(up.hipsAsideUnits, 4.0f) && up.moveHips, "the hips 4 to the right of the root, to be moved");
	const float lean = obvr::math::Atan2(1.8f - 4.0f, 112.0f - 67.0f);
	Check(Near(up.leanRadians, lean, 0.001f), "the head's lean off the pelvis read: 2.8 degrees to the left");
	Apply(joints, 7, up, bodyYaw);
	const NiPoint3 right = UprightRight(bodyYaw);
	const NiPoint3 forward = UprightForward(bodyYaw);
	float worstAside = 0.0f;
	float worstAhead = 0.0f;
	float worstLength = 0.0f;
	for (UInt32 k = 0; k + 1 < 7; ++k) {
		const NiPoint3 now = joints[k + 1] - joints[k];
		const NiPoint3 was = before[k + 1] - before[k];
		const float aside = Dot(now, right) < 0.0f ? -Dot(now, right) : Dot(now, right);
		const float ahead = Dot(now, forward) - Dot(was, forward);
		const float length = Length(now) - Length(was);
		worstAside = aside > worstAside ? aside : worstAside;
		worstAhead = (ahead < 0.0f ? -ahead : ahead) > worstAhead ? (ahead < 0.0f ? -ahead : ahead) : worstAhead;
		worstLength = (length < 0.0f ? -length : length) > worstLength ? (length < 0.0f ? -length : length) : worstLength;
	}
	Check(worstAside < 0.001f, "no segment leans aside any more");
	Check(worstAhead < 0.001f, "every segment's bow forward kept");
	Check(worstLength < 0.001f, "every segment's length kept");
	Check(Near(joints[0].x, before[0].x) && Near(joints[0].y, before[0].y) && Near(joints[0].z, before[0].z),
	      "the pelvis where it was (the hips' move is the caller's)");
	const float headAside = Dot(joints[6] - joints[0], right);
	Check(Near(headAside, 0.0f, 0.001f), "the head straight over the pelvis");
}

void TestStraight() {
	std::printf("A chain already upright\n");
	const NiPoint3 chain[3] = {{0.0f, 0.0f, 67.0f}, {0.0f, 2.0f, 80.0f}, {0.0f, 3.0f, 112.0f}};
	const TrunkUpright up = UprightTrunk(chain, 3, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f);
	Check(up.valid && up.turns == 2 && Near(up.turn[0], 0.0f) && Near(up.turn[1], 0.0f), "left as it is");
	Check(Near(up.leanRadians, 0.0f) && Near(up.hipsAsideUnits, 0.0f), "no lean, the hips over the root");
}

void TestShortAcross() {
	std::printf("A segment too short across\n");
	// The middle segment runs straight forward: nothing across, nothing up.
	NiPoint3 chain[4] = {{0.0f, 0.0f, 67.0f}, {2.0f, 0.0f, 77.0f}, {2.1f, 6.0f, 77.2f}, {4.0f, 6.0f, 107.0f}};
	const TrunkUpright up = UprightTrunk(chain, 4, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f);
	Check(up.valid && Near(up.turn[1], 0.0f), "valid; the short one given its neighbour's angle, so not turned");
	Apply(chain, 4, up, 0.0f);
	Check(Near(chain[1].x - chain[0].x, 0.0f, 0.001f) && Near(chain[3].x - chain[2].x, 0.0f, 0.001f),
	      "the segments that say something upright");
}

void TestRefused() {
	std::printf("Poses of no use\n");
	const NiPoint3 root{0.0f, 0.0f, 0.0f};
	const NiPoint3 far[2] = {{0.0f, 0.0f, 67.0f}, {12.0f, 0.0f, 80.0f}};  // 42.7 degrees aside
	Check(!UprightTrunk(far, 2, root, 0.0f).valid, "a segment leaning 43 degrees aside: no standing pose, left");
	const NiPoint3 within[2] = {{0.0f, 0.0f, 67.0f}, {7.0f, 0.0f, 80.0f}};  // 28.3 degrees
	Check(UprightTrunk(within, 2, root, 0.0f).valid, "28 degrees: still a pose to stand upright");
	const NiPoint3 nan[2] = {{0.0f, 0.0f, 67.0f}, {kNaN, 0.0f, 80.0f}};
	Check(!UprightTrunk(nan, 2, root, 0.0f).valid, "a joint that is not a number");
	Check(!UprightTrunk(within, 2, NiPoint3{kNaN, 0.0f, 0.0f}, 0.0f).valid, "a root that is not a number");
	Check(!UprightTrunk(within, 2, root, kNaN).valid, "a yaw that is not a number");
	Check(!UprightTrunk(within, 1, root, 0.0f).valid, "one joint: no segment");
	Check(!UprightTrunk(nullptr, 2, root, 0.0f).valid, "no joints");
	NiPoint3 many[kUprightMostJoints + 1] = {};
	Check(!UprightTrunk(many, kUprightMostJoints + 1, root, 0.0f).valid, "more joints than kept");
}

void TestHips() {
	std::printf("The hips across from the root\n");
	const NiPoint3 root{0.0f, 0.0f, 0.0f};
	const NiPoint3 near[2] = {{-5.0f, 3.0f, 67.0f}, {-5.0f, 3.0f, 112.0f}};
	TrunkUpright up = UprightTrunk(near, 2, root, 0.0f);
	Check(Near(up.hipsAsideUnits, -5.0f) && up.moveHips, "5 to the left: moved; the 3 forward not counted");
	const NiPoint3 far[2] = {{15.0f, 0.0f, 67.0f}, {15.0f, 0.0f, 112.0f}};
	up = UprightTrunk(far, 2, root, 0.0f);
	Check(up.valid && Near(up.hipsAsideUnits, 15.0f) && !up.moveHips, "15 across: no standing pose's, not moved");
	up = UprightTrunk(near, 2, root, 90.0f * kDeg);  // facing -x: the right is +y
	Check(Near(up.hipsAsideUnits, 3.0f), "facing another way: across is the body's right, not the world's x");
}

}  // namespace

int main() {
	TestStraight();
	TestSwayed(0.0f, "the world's y");
	TestSwayed(2.3f, "another way");
	TestShortAcross();
	TestRefused();
	TestHips();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

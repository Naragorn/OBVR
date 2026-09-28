// Checks the thumb joint by joint (vr/ThumbPose.h): the rotation between two
// bone rotations, the rotation vector, each joint's share from the open hand
// to the fist, and the three shares from a skeleton; and how the game's
// thumb links take them (game::LinkShare).

#include <cmath>
#include <cstdio>

#include "game/HandGrip.h"
#include "vr/ThumbPose.h"

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

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

// A turn by `degrees` about the unit axis (x, y, z).
BoneRotation Turn(float degrees, float x, float y, float z) {
	const float half = degrees * 3.14159265f / 360.0f;
	const float s = std::sin(half);
	return BoneRotation{std::cos(half), x * s, y * s, z * s};
}

// q times r.
BoneRotation Times(const BoneRotation& q, const BoneRotation& r) {
	return BoneRotation{q.w * r.w - q.x * r.x - q.y * r.y - q.z * r.z, q.w * r.x + q.x * r.w + q.y * r.z - q.z * r.y,
	                    q.w * r.y - q.x * r.z + q.y * r.w + q.z * r.x, q.w * r.z + q.x * r.y - q.y * r.x + q.z * r.w};
}

void TestRotations() {
	std::printf("Rotations\n");
	const BoneRotation a = Turn(30.0f, 0.0f, 0.0f, 1.0f);
	const BoneRotation b = Turn(70.0f, 0.0f, 0.0f, 1.0f);
	const BoneRotation between = RotationBetween(a, b);
	Check(Near(between.w, std::cos(20.0f * 3.14159265f / 180.0f)) && Near(between.z, std::sin(20.0f * 3.14159265f / 180.0f)),
	      "from 30 to 70 degrees about z: 40 about z");
	const BoneRotation start = Turn(25.0f, 1.0f, 0.0f, 0.0f);
	const BoneRotation step = Turn(50.0f, 0.0f, 1.0f, 0.0f);
	const BoneRotation found = RotationBetween(start, Times(start, step));
	Check(Near(found.w, step.w) && Near(found.y, step.y) && Near(found.x, 0.0f),
	      "q then a turn in q's own frame: that turn");
	float v[3];
	RotationVector(Turn(90.0f, 0.0f, 1.0f, 0.0f), v);
	Check(Near(v[1], 3.14159265f / 2.0f) && Near(v[0], 0.0f), "a quarter turn about y: (0, pi/2, 0)");
	const BoneRotation negated{-step.w, -step.x, -step.y, -step.z};
	float w[3];
	RotationVector(negated, w);
	RotationVector(step, v);
	Check(Near(w[1], v[1]), "the same rotation written negated: the same vector");
	RotationVector(BoneRotation{}, v);
	Check(v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f, "none: the zero vector");
}

void TestJoint() {
	std::printf("One joint\n");
	const BoneRotation open = Turn(10.0f, 1.0f, 0.0f, 0.0f);
	const BoneRotation fist = Times(open, Turn(80.0f, 0.0f, 0.0f, 1.0f));
	Check(Near(JointShare(open, fist, open), 0.0f), "the open hand: 0");
	Check(Near(JointShare(open, fist, fist), 1.0f), "the fist: 1");
	Check(Near(JointShare(open, fist, Times(open, Turn(20.0f, 0.0f, 0.0f, 1.0f))), 0.25f),
	      "a quarter of the fist's turn: 0.25");
	Check(Near(JointShare(open, fist, Times(open, Turn(-30.0f, 0.0f, 0.0f, 1.0f))), 0.0f),
	      "bent back past the open hand: held at 0");
	Check(Near(JointShare(open, fist, Times(open, Turn(120.0f, 0.0f, 0.0f, 1.0f))), 1.0f),
	      "further than the fist: held at 1");
	Check(Near(JointShare(open, fist, Times(open, Turn(40.0f, 1.0f, 0.0f, 0.0f))), 0.0f),
	      "a turn across the fist's axis: nothing of it counts");
	const BoneRotation mixed = Times(open, Turn(40.0f, 0.0f, 0.6f, 0.8f));
	Check(Near(JointShare(open, fist, mixed), 0.4f, 2e-3f), "a turn partly along it: that part (0.8 of 40 over 80)");
	Check(JointShare(open, open, fist) < 0.0f, "a joint the fist does not turn: cannot be measured");
}

void TestSkeleton() {
	std::printf("The skeleton\n");
	BoneRotation open[kSkeletonBoneCount];
	BoneRotation fist[kSkeletonBoneCount];
	BoneRotation now[kSkeletonBoneCount];
	for (int j = 0; j < kThumbJoints; ++j) {
		const int bone = kSkeletonThumbFirst + j;
		open[bone] = Turn(5.0f * static_cast<float>(j), 0.0f, 1.0f, 0.0f);
		fist[bone] = Times(open[bone], Turn(60.0f, 0.0f, 0.0f, 1.0f));
		now[bone] = Times(open[bone], Turn(60.0f * 0.5f * static_cast<float>(j), 0.0f, 0.0f, 1.0f));
	}
	float out[kThumbJoints] = {9.0f, 9.0f, 9.0f};
	Check(ThumbShares(open, fist, now, out), "three joints measured");
	Check(Near(out[0], 0.0f) && Near(out[1], 0.5f) && Near(out[2], 1.0f), "base open, middle half, tip curled");
	fist[kSkeletonThumbFirst + 1] = open[kSkeletonThumbFirst + 1];
	float kept[kThumbJoints] = {9.0f, 9.0f, 9.0f};
	Check(!ThumbShares(open, fist, now, kept) && kept[0] == 9.0f && kept[2] == 9.0f,
	      "one joint the fist leaves alone: none measured, nothing written");
}

void TestLinks() {
	std::printf("The game's thumb links\n");
	game::FingerCurls curls;
	curls.curl[0] = 0.9f;
	curls.curl[2] = 0.4f;
	curls.thumb[0] = 0.1f;
	curls.thumb[1] = 0.2f;
	curls.thumb[2] = 0.3f;
	Check(Near(game::LinkShare(curls, 0, 1), 0.9f), "without the joints the thumb follows its curl");
	curls.thumbJoints = true;
	Check(Near(game::LinkShare(curls, 0, 0), 0.1f) && Near(game::LinkShare(curls, 0, 1), 0.2f) &&
	          Near(game::LinkShare(curls, 0, 2), 0.3f),
	      "with them each thumb link follows its own joint");
	Check(Near(game::LinkShare(curls, 2, 7), 0.4f), "another finger still follows its curl");
	Check(Near(game::LinkShare(curls, 0, 5), 0.9f), "a thumb link out of range: the curl");
	Check(game::LinkShare(curls, 5, 15) == 0.0f && game::LinkShare(curls, -1, 0) == 0.0f, "no such finger: 0");
}

}  // namespace

int main() {
	TestRotations();
	TestJoint();
	TestSkeleton();
	TestLinks();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

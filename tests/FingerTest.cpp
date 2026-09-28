// Checks the tracked fingers' arithmetic (game/HandGrip.h): which pose a hand
// takes, which link a bone is, the game's two poses and the blend between
// them, and the left hand's mirror image.

#include <cmath>
#include <cstdio>

#include "game/HandGrip.h"

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

bool NearM(const NiMatrix33& a, const NiMatrix33& b, float eps = 1e-3f) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.data[r][c], b.data[r][c], eps)) {
				return false;
			}
		}
	}
	return true;
}

bool NearQ(const FingerQuat& a, const FingerQuat& b, float eps = 1e-3f) {
	return Near(a.w, b.w, eps) && Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

float Length(const FingerQuat& q) { return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z); }

// Rows and columns each of unit length and at right angles, determinant +1.
bool IsRotation(const NiMatrix33& m) {
	const NiMatrix33 t = [&] {
		NiMatrix33 x;
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				x.data[r][c] = m.data[c][r];
			}
		}
		return x;
	}();
	const float det = m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) -
	                  m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) +
	                  m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
	return NearM(m * t, NiMatrix33::Identity(), 1e-3f) && Near(det, 1.0f, 1e-3f);
}

void TestPose() {
	std::printf("Which pose\n");
	for (int bits = 0; bits < 16; ++bits) {
		const bool tracking = (bits & 1) != 0;
		const bool curlValid = (bits & 2) != 0;
		const bool grip = (bits & 4) != 0;
		const bool holds = (bits & 8) != 0;
		const FingerPose want = grip                                    ? FingerPose::Grip
		                        : (tracking && curlValid && !holds) ? FingerPose::Tracked
		                                                                : FingerPose::Animation;
		if (FingerPoseFor(tracking, curlValid, grip, holds) != want) {
			Check(false, "a combination decided wrongly");
			return;
		}
	}
	Check(true, "all sixteen combinations");
	Check(FingerPoseFor(true, true, true, true) == FingerPose::Grip,
	      "a held object closes the hand, tracking or not, a weapon or not");
	Check(FingerPoseFor(true, true, false, false) == FingerPose::Tracked, "an empty hand with curls: tracked");
	Check(FingerPoseFor(true, true, false, true) == FingerPose::Animation,
	      "a weapon or a torch in the hand: the animation's grip");
	Check(FingerPoseFor(true, false, false, false) == FingerPose::Animation,
	      "no curls from the device (a wand): the animation");
	Check(FingerPoseFor(false, true, false, false) == FingerPose::Animation, "tracking off: the animation");
}

void TestLinks() {
	std::printf("Which link\n");
	int finger = -1;
	int link = -1;
	Check(FingerLinkOf("Bip01 R Finger0", finger, link) && finger == 0 && link == 0, "the thumb's first link");
	Check(FingerLinkOf("Bip01 R Finger02", finger, link) && finger == 0 && link == 2, "the thumb's tip");
	Check(FingerLinkOf("Bip01 L Finger1", finger, link) && finger == 1 && link == 3, "the index finger");
	Check(FingerLinkOf("Bip01 L Finger21", finger, link) && finger == 2 && link == 7, "the middle finger's second link");
	Check(FingerLinkOf("bip01 r finger42", finger, link) && finger == 4 && link == 14,
	      "the little finger's tip, case aside");
	Check(!FingerLinkOf("Bip01 R Finger5", finger, link), "no sixth finger");
	Check(!FingerLinkOf("Bip01 R Finger13", finger, link), "no fourth link");
	Check(!FingerLinkOf("Bip01 R Finger1x", finger, link) && !FingerLinkOf("Bip01 R Finger111", finger, link),
	      "anything after the link: not one");
	Check(!FingerLinkOf("Bip01 R Hand", finger, link) && !FingerLinkOf("Weapon", finger, link) &&
	          !FingerLinkOf(nullptr, finger, link) && !FingerLinkOf("Bip01 R Finger", finger, link),
	      "not a finger at all");
}

void TestQuaternions() {
	std::printf("The quaternions\n");
	Check(NearM(RotationOfQuat(FingerQuat{1.0f, 0.0f, 0.0f, 0.0f}), NiMatrix33::Identity()), "none: the identity");
	const float h = std::sqrt(0.5f);
	const NiMatrix33 quarter = RotationOfQuat(FingerQuat{h, 0.0f, 0.0f, h});
	Check(NearM(quarter, CurledAboutZ(NiMatrix33::Identity(), 90.0f)),
	      "a quarter turn about +z: the grip curl's own turn by +90");
	const NiMatrix33 aboutX = RotationOfQuat(FingerQuat{h, h, 0.0f, 0.0f});
	Check(Near(aboutX.data[2][1], 1.0f) && Near(aboutX.data[1][2], -1.0f), "a quarter turn about +x: y to z");
	const NiMatrix33 aboutY = RotationOfQuat(FingerQuat{h, 0.0f, h, 0.0f});
	Check(Near(aboutY.data[0][2], 1.0f) && Near(aboutY.data[2][0], -1.0f), "a quarter turn about +y: z to x");

	const FingerQuat a{1.0f, 0.0f, 0.0f, 0.0f};
	const FingerQuat b{h, 0.0f, 0.0f, h};
	Check(NearQ(BlendQuat(a, b, 0.0f), a) && NearQ(BlendQuat(a, b, 1.0f), b), "the ends: the two poses");
	Check(NearQ(BlendQuat(a, b, -1.0f), a) && NearQ(BlendQuat(a, b, 2.0f), b), "beyond the ends: held at them");
	const FingerQuat mid = BlendQuat(a, b, 0.5f);
	Check(Near(Length(mid), 1.0f) && Near(mid.z / mid.w, std::tan(22.5f * 3.14159265f / 180.0f)),
	      "half way: unit length, the half turn");
	const FingerQuat negB{-b.w, -b.x, -b.y, -b.z};
	Check(NearM(RotationOfQuat(BlendQuat(a, negB, 0.5f)), RotationOfQuat(mid)),
	      "the same rotation written negated: the shorter way still");
	Check(NearQ(BlendQuat(FingerQuat{0.0f, 0.0f, 0.0f, 0.0f}, FingerQuat{0.0f, 0.0f, 0.0f, 0.0f}, 0.5f),
	            FingerQuat{0.0f, 0.0f, 0.0f, 0.0f}),
	      "nothing to normalise: the first returned");
	Check(NearQ(MirroredForLeft(FingerQuat{0.7f, 0.1f, -0.2f, 0.3f}), FingerQuat{0.7f, -0.1f, 0.2f, 0.3f}),
	      "the left hand's mirror: x and y negated");
}

void TestTables() {
	std::printf("The game's two poses\n");
	bool unit = true;
	bool rotations = true;
	bool closes = true;
	for (int i = 0; i < kFingerLinkCount; ++i) {
		unit = unit && Near(Length(kOpenRight[i]), 1.0f, 3e-3f) && Near(Length(kFistRight[i]), 1.0f, 3e-3f);
		for (float t = 0.0f; t <= 1.0f; t += 0.25f) {
			rotations = rotations && IsRotation(TrackedLinkRotation(true, i, t)) &&
			            IsRotation(TrackedLinkRotation(false, i, t));
		}
		// Every link but the thumb's first (its turn is across the palm) is
		// bent further about +z in the fist.
		if (i != 0) {
			closes = closes && kFistRight[i].z > kOpenRight[i].z + 0.3f;
		}
	}
	Check(unit, "every key of both poses is a unit quaternion, to the files' three decimals");
	Check(rotations, "every blend on either hand is a rotation");
	Check(closes, "the fist bends each link further about +z than the open hand");
	Check(NearM(TrackedLinkRotation(true, 4, 0.0f), RotationOfQuat(kOpenRight[4])) &&
	          NearM(TrackedLinkRotation(true, 4, 1.0f), RotationOfQuat(kFistRight[4])),
	      "the right hand: open at 0, the fist at 1");
	Check(NearM(TrackedLinkRotation(false, 3, 1.0f), RotationOfQuat(MirroredForLeft(kFistRight[3]))),
	      "the left hand: the mirror image");
	// The left fist's index link as handtohandidle.kf has it.
	Check(NearM(TrackedLinkRotation(false, 3, 1.0f), RotationOfQuat(FingerQuat{0.722f, 0.075f, 0.012f, 0.688f}),
	            3e-3f),
	      "the mirrored right is the file's left");
	Check(NearM(TrackedLinkRotation(true, -1, 0.5f), NiMatrix33::Identity()) &&
	          NearM(TrackedLinkRotation(true, kFingerLinkCount, 0.5f), NiMatrix33::Identity()),
	      "no such link: the identity");
	const NiMatrix33 half = TrackedLinkRotation(true, 4, 0.5f);
	Check(!NearM(half, TrackedLinkRotation(true, 4, 0.0f), 0.05f) &&
	          !NearM(half, TrackedLinkRotation(true, 4, 1.0f), 0.05f),
	      "half curled: between the two");
}

}  // namespace

int main() {
	TestPose();
	TestLinks();
	TestQuaternions();
	TestTables();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

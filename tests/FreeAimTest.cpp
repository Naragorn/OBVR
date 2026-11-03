// The headset turning the view while the mouse or the stick aims
// (camera/FreeAimLogic.h; a Nexus user's request of 2026-11-02: the mouse
// and the stick turn the camera and the aim, the headset additionally turns
// and tilts the camera but not the aim, the crosshair the reference point).

#include <cmath>
#include <cstdio>

#include "camera/FreeAimLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::camera;

constexpr float kDeg = 3.14159265f / 180.0f;

bool Near(float a, float b, float within = 1e-4f) { return std::fabs(a - b) <= within; }

bool NearMatrix(const NiMatrix33& a, const NiMatrix33& b, float within = 1e-4f) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.data[r][c], b.data[r][c], within)) {
				return false;
			}
		}
	}
	return true;
}

NiPoint3 Column(const NiMatrix33& m, int c) { return NiPoint3{m.data[0][c], m.data[1][c], m.data[2][c]}; }

float PitchOf(const NiMatrix33& m) { return std::asin(Column(m, 1).z) / kDeg; }
float YawOf(const NiMatrix33& m) {
	const NiPoint3 f = Column(m, 1);
	return std::atan2(-f.x, f.y) / kDeg;
}

void TestActive() {
	std::printf("when the headset turns the view only\n");
	Check(FreeAimActive(true, true, false, true), "the switch on, headset look on, seated, a headset: on");
	Check(!FreeAimActive(false, true, false, true), "the switch off: off");
	Check(!FreeAimActive(true, false, false, true), "headset look off (the cinema view): off");
	Check(!FreeAimActive(true, true, true, true), "Full VR (the hands aim): off");
	Check(!FreeAimActive(true, true, false, false), "no headset: off");
}

void TestView() {
	std::printf("the view: the mouse's turn and tilt, the head's on top, the horizon level\n");
	const NiMatrix33 level = YawPitch(20.0f * kDeg, 0.0f);
	const NiMatrix33 head = YawPitch(30.0f * kDeg, 10.0f * kDeg);
	Check(NearMatrix(FreeAimView(level, head), level * head), "the mouse level: the view as the head on the base");
	const NiMatrix33 tilted = YawPitch(20.0f * kDeg, -25.0f * kDeg);
	const NiMatrix33 v = FreeAimView(tilted, head);
	Check(Near(PitchOf(v), -15.0f, 1e-2f) && Near(YawOf(v), 50.0f, 1e-2f),
	      "the mouse 25 down, the head 30 left and 10 up: 50 left, 15 down");
	Check(Near(Column(v, 0).z, 0.0f), "the head unrolled: the view's right level - no tipped horizon");
	const NiMatrix33 naive = tilted * head;
	Check(!Near(Column(naive, 0).z, 0.0f, 1e-2f), "(the plain product tips it: what this replaces)");
	const NiMatrix33 rolled = head * RollAbout(12.0f * kDeg);
	const NiMatrix33 vr = FreeAimView(tilted, rolled);
	// Rolled 12 about the view's own forward, tilted 15 down: its right
	// rises by sin 12 times cos 15 - the roll in the view's frame, no more.
	Check(Near(Column(vr, 0).z, -std::sin(12.0f * kDeg) * std::cos(15.0f * kDeg), 1e-4f) &&
	          Near(PitchOf(vr), -15.0f, 1e-2f),
	      "the head rolled 12: the view rolled 12 about its own forward, its tilt kept");
	const NiMatrix33 v2 = FreeAimView(YawPitch(0.0f, 70.0f * kDeg), YawPitch(0.0f, 40.0f * kDeg));
	Check(Near(PitchOf(v2), kFreeAimPitchMostDegrees, 1e-2f), "70 and 40 up: held at the most");
	const NiMatrix33 v3 = FreeAimView(YawPitch(0.0f, -70.0f * kDeg), YawPitch(0.0f, -40.0f * kDeg));
	Check(Near(PitchOf(v3), -kFreeAimPitchMostDegrees, 1e-2f), "and as far down");
	Check(NearMatrix(FreeAimView(tilted, NiMatrix33::Identity()), tilted), "the head still: the mouse's own view");
}

void TestAimInView() {
	std::printf("the aim in the view: where the crosshair hangs from the head\n");
	const NiMatrix33 base = YawPitch(20.0f * kDeg, -25.0f * kDeg);
	const NiPoint3 aim = Column(base, 1);
	const NiPoint3 ahead = AimInHeadset(FreeAimView(base, NiMatrix33::Identity()), aim);
	Check(Near(ahead.x, 0.0f) && Near(ahead.y, 0.0f) && Near(ahead.z, -1.0f), "the head still: straight ahead (-z)");
	const NiPoint3 left = AimInHeadset(FreeAimView(base, YawPitch(30.0f * kDeg, 0.0f)), aim);
	Check(left.x > 0.4f && left.z < 0.0f, "the head 30 left: the crosshair off to the right");
	Check(Near(std::sqrt(left.x * left.x + left.y * left.y + left.z * left.z), 1.0f), "a unit direction");
	const NiPoint3 up = AimInHeadset(FreeAimView(base, YawPitch(0.0f, 20.0f * kDeg)), aim);
	Check(up.y < -0.3f && Near(up.x, 0.0f), "the head 20 up: the crosshair below");
	const vr::openvr::HmdMatrix34 m = AimQuadMatrix(2.0f, left);
	Check(Near(m.m[0][3], 2.0f * left.x) && Near(m.m[1][3], 2.0f * left.y) && Near(m.m[2][3], 2.0f * left.z),
	      "the quad 2 m along the aim");
	Check(Near(m.m[0][2], -left.x) && Near(m.m[2][2], -left.z), "facing back along it, to the eyes");
	const float upDotFront = m.m[0][1] * m.m[0][2] + m.m[1][1] * m.m[1][2] + m.m[2][1] * m.m[2][2];
	Check(Near(upDotFront, 0.0f) && m.m[1][1] > 0.5f, "its up across the front, upright");
}

}  // namespace

int main() {
	TestActive();
	TestView();
	TestAimInView();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

// Checks where the laser's pieces hang: the beam, the dot at its end and the
// crosshair on it all lie on the one ray the hit tests use, facing the way
// the overlays need, and the same ray carried into the world for the pick.

#include <cstdio>

#include "core/Rotation.h"
#include "vr/LaserGeometry.h"

namespace {

using obvr::EulerToMatrix;
using obvr::NiMatrix33;
using obvr::NiPoint3;
using namespace obvr::vr;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

NiPoint3 Column(const openvr::HmdMatrix34& m, int c) {
	return NiPoint3{m.m[0][c], m.m[1][c], m.m[2][c]};
}

float Dot3(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

NiPoint3 Cross3(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float eps = 0.001f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

bool RightHanded(const openvr::HmdMatrix34& m) {
	const NiPoint3 x = Column(m, 0);
	const NiPoint3 y = Column(m, 1);
	const NiPoint3 z = Column(m, 2);
	return Near(Dot3(x, x), 1.0f) && Near(Dot3(y, y), 1.0f) && Near(Dot3(x, y), 0.0f) &&
	       NearPoint(Cross3(x, y), z);
}

void TestBeam() {
	std::printf("The beam\n");
	openvr::HmdMatrix34 m = LaserBeamMatrix(1.0f, 0.0f, 0.0f, 0.0f);
	Check(NearPoint(Column(m, 1), NiPoint3{0, 0, -1}) && NearPoint(Column(m, 2), NiPoint3{0, 1, 0}),
	      "no angles: up along -z, front the controller's up");
	Check(Near(m.m[2][3], -0.5f), "its centre half a metre out");

	m = LaserBeamMatrix(2.0f, 40.0f, 5.0f, -0.04f);
	const NiPoint3 direction = LaserDirectionLocal(40.0f, 5.0f);
	Check(NearPoint(Column(m, 1), direction), "tilted: up is the hit test's direction");
	Check(RightHanded(m), "and the frame stays right-handed");
	const NiPoint3 centre{m.m[0][3], m.m[1][3], m.m[2][3]};
	Check(NearPoint(centre, direction * (-0.04f + 1.0f)), "centred half its length past the start");
}

void TestPoint() {
	std::printf("The dot and the crosshair on the ray\n");
	openvr::HmdMatrix34 m = LaserPointMatrix(2.0f, 0.0f, 0.0f, 0.0f);
	Check(Near(m.m[0][0], 1.0f) && Near(m.m[1][1], 1.0f) && Near(m.m[2][2], 1.0f) &&
	          Near(m.m[2][3], -2.0f),
	      "no angles: the plain placement two metres ahead, as the crosshair always had");

	m = LaserPointMatrix(1.5f, 40.0f, -5.0f, -0.04f);
	const NiPoint3 direction = LaserDirectionLocal(40.0f, -5.0f);
	Check(NearPoint(Column(m, 2), direction * -1.0f), "tilted: it faces back along the beam");
	Check(RightHanded(m), "and the frame stays right-handed");
	const NiPoint3 at{m.m[0][3], m.m[1][3], m.m[2][3]};
	Check(NearPoint(at, direction * (1.5f - 0.04f)), "and stands where the beam of that length ends");

	Check(Near(LaserDotWidth(2.0f), 0.032f) && Near(LaserDotWidth(0.1f), 0.008f),
	      "the dot keeps its apparent size, with a floor near the hand");
}

void TestWorldRay() {
	std::printf("The laser in the world\n");
	const NiMatrix33 identity = NiMatrix33::Identity();
	LaserWorldRay ray = HandLaserWorldRay(identity, NiPoint3{100, 200, 300}, identity,
	                                      NiPoint3{10, 20, -5}, 0.0f, 0.0f, 0.0f, 70.0f);
	Check(NearPoint(ray.direction, NiPoint3{0, 1, 0}), "an untilted laser points the game's forward");
	Check(NearPoint(ray.origin, NiPoint3{110, 220, 295}), "from the hand, carried by the head");

	ray = HandLaserWorldRay(identity, NiPoint3{0, 0, 0}, identity, NiPoint3{0, 0, 0}, 90.0f, 0.0f,
	                        0.0f, 70.0f);
	Check(NearPoint(ray.direction, NiPoint3{0, 0, -1}), "tilted down 90: straight down, -z in the game");

	ray = HandLaserWorldRay(identity, NiPoint3{0, 0, 0}, identity, NiPoint3{0, 0, 0}, 0.0f, 0.0f,
	                        -0.04f, 70.0f);
	Check(NearPoint(ray.origin, NiPoint3{0, -2.8f, 0}), "the start 4 cm back is 2.8 units back");

	const NiMatrix33 turned = EulerToMatrix(0.0f, 0.0f, 90.0f);
	ray = HandLaserWorldRay(turned, NiPoint3{0, 0, 0}, identity, NiPoint3{0, 0, 0}, 0.0f, 0.0f,
	                        0.0f, 70.0f);
	Check(NearPoint(ray.direction, turned * NiPoint3{0, 1, 0}), "a turned head turns the ray with it");
}

}  // namespace

int main() {
	TestBeam();
	TestPoint();
	TestWorldRay();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

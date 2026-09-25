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

void TestReach() {
	std::printf("Grab by reach: the pick through the hand, and the reach\n");
	const NiMatrix33 identity = NiMatrix33::Identity();
	const NiPoint3 head{10.0f, 20.0f, 100.0f};
	// A hand 40 units ahead and 30 down: the ray runs from the head through
	// it, starting 20 units short of the hand.
	LaserWorldRay ray = HandReachWorldRay(identity, head, NiPoint3{0.0f, 40.0f, -30.0f}, 20.0f);
	Check(NearPoint(ray.direction, NiPoint3{0.0f, 0.8f, -0.6f}), "from the head through the hand");
	Check(NearPoint(ray.origin, NiPoint3{10.0f, 20.0f + 0.8f * 30.0f, 100.0f - 0.6f * 30.0f}),
	      "starting the back distance short of the hand");
	// Back longer than the reach to the hand: starts at the head, never behind it.
	ray = HandReachWorldRay(identity, head, NiPoint3{0.0f, 10.0f, 0.0f}, 50.0f);
	Check(NearPoint(ray.origin, head) && NearPoint(ray.direction, NiPoint3{0.0f, 1.0f, 0.0f}),
	      "a hand closer than the back distance starts the ray at the head");
	// The head's turn carries the hand's offset.
	const NiMatrix33 turned = EulerToMatrix(0.0f, 0.0f, 90.0f);
	ray = HandReachWorldRay(turned, head, NiPoint3{0.0f, 10.0f, 0.0f}, 0.0f);
	Check(NearPoint(ray.direction, turned * NiPoint3{0.0f, 1.0f, 0.0f}) &&
	          NearPoint(ray.origin, head + turned * NiPoint3{0.0f, 10.0f, 0.0f}),
	      "a turned head turns the ray, and zero back starts at the hand");
	// A hand at the eyes: the head's forward, from the head.
	ray = HandReachWorldRay(turned, head, NiPoint3{0.0f, 0.0f, 0.001f}, 20.0f);
	Check(NearPoint(ray.origin, head) && NearPoint(ray.direction, turned * NiPoint3{0.0f, 1.0f, 0.0f}),
	      "a hand at the eyes falls back to the head's forward");

	Check(WithinReach(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{3.0f, 4.0f, 0.0f}, 5.0f),
	      "exactly at the reach is within it");
	Check(!WithinReach(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{3.0f, 4.0f, 0.1f}, 5.0f),
	      "a hair beyond is not");
	Check(!WithinReach(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f),
	      "no reach takes nothing, not even what the hand is in");
}

void TestParts() {
	std::printf("Beam and dot on their own switches\n");
	for (unsigned mask = 0; mask < 32; ++mask) {
		const bool pointing = (mask & 1) != 0;
		const bool beam = (mask & 2) != 0;
		const bool dot = (mask & 4) != 0;
		const bool device = (mask & 8) != 0;
		const float length = (mask & 16) != 0 ? 1.5f : 0.0f;
		const LaserParts parts = LaserPartsShown(pointing, beam, dot, device, length);
		const bool can = pointing && device && length > 0.01f;
		Check(parts.beam == (can && beam) && parts.dot == (can && dot),
		      "each piece follows its own switch, both need a pointing hand, a device and a length");
	}
	const LaserParts dotOnly = LaserPartsShown(true, false, true, true, 1.0f);
	Check(!dotOnly.beam && dotOnly.dot, "the beam off leaves the dot");
	const LaserParts beamOnly = LaserPartsShown(true, true, false, true, 1.0f);
	Check(beamOnly.beam && !beamOnly.dot, "the dot off leaves the beam");
}

void TestWorldInTracking() {
	std::printf("A world point in the tracking space, for the reach marker\n");
	// The head at (1, 1.5, 2) in the room, unturned; the camera at the world
	// origin, unturned; 70 units a metre.
	obvr::vr::openvr::HmdMatrix34 head{};
	head.m[0][0] = head.m[1][1] = head.m[2][2] = 1.0f;
	head.m[0][3] = 1.0f;
	head.m[1][3] = 1.5f;
	head.m[2][3] = 2.0f;
	const NiMatrix33 identity = NiMatrix33::Identity();
	const NiPoint3 camera{0.0f, 0.0f, 0.0f};
	// 70 units ahead is a metre ahead: OpenVR's -z.
	Check(NearPoint(WorldPointInTracking(head, identity, camera, NiPoint3{0.0f, 70.0f, 0.0f}, 70.0f),
	                NiPoint3{1.0f, 1.5f, 1.0f}),
	      "ahead in the game is ahead of the head in the room");
	Check(NearPoint(WorldPointInTracking(head, identity, camera, NiPoint3{35.0f, 0.0f, 70.0f}, 70.0f),
	                NiPoint3{1.5f, 2.5f, 2.0f}),
	      "right and up carry over");
	// The camera turned a quarter left in the game: a point to its right in
	// the world is ahead of it.
	const NiMatrix33 turned = EulerToMatrix(0.0f, 0.0f, 90.0f);
	const NiPoint3 ahead = turned * NiPoint3{0.0f, 70.0f, 0.0f};
	Check(NearPoint(WorldPointInTracking(head, turned, camera, ahead, 70.0f), NiPoint3{1.0f, 1.5f, 1.0f}),
	      "the camera's own turn is taken out");
	const obvr::vr::openvr::HmdMatrix34 facing = FacingHeadAt(head, NiPoint3{3.0f, 4.0f, 5.0f});
	Check(facing.m[0][0] == 1.0f && facing.m[0][3] == 3.0f && facing.m[1][3] == 4.0f &&
	          facing.m[2][3] == 5.0f,
	      "the marker is turned like the head, placed at the point");
}

int main() {
	TestWorldInTracking();
	TestParts();
	TestReach();
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

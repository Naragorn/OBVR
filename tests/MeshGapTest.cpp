// Checks the gap between a point resting on a person's capsule and the mesh
// they wear there (game/MeshGapLogic.h, [Debug] MeshGapTrace): the capsule
// the point lies on, the point in the air over the mesh and sunk into it,
// the narrow lane empty and the wide one found, both empty, the point on
// the axis, no vertices, a limb past the reach and the far side left out,
// and the nearest vertex.

#include <cmath>
#include <cstdio>
#include <vector>

#include "game/MeshGapLogic.h"

using obvr::NiPoint3;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

constexpr float kPi = 3.14159265358979f;

// A cylinder of vertices round the x axis from x0 to x1, `radius` round,
// `step` units between rings and `perRing` to a ring.
std::vector<NiPoint3> Cylinder(float radius, float x0, float x1, float step = 1.0f, int perRing = 72) {
	std::vector<NiPoint3> pts;
	for (float x = x0; x <= x1 + 1e-4f; x += step) {
		for (int k = 0; k < perRing; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(perRing);
			pts.push_back(NiPoint3{x, radius * std::cos(a), radius * std::sin(a)});
		}
	}
	return pts;
}

const NiPoint3 kA{0.0f, 0.0f, 0.0f};
const NiPoint3 kB{30.0f, 0.0f, 0.0f};

void TestCapsuleUnderPoint() {
	std::printf("The capsule the point lies on\n");
	BladeBodies bodies;
	bodies.Add(kA, kB, 7.0f, 1, BodyPart::Torso);
	bodies.Add(NiPoint3{0.0f, 20.0f, 0.0f}, NiPoint3{30.0f, 20.0f, 0.0f}, 4.0f, 1, BodyPart::LeftArm);
	bodies.Add(NiPoint3{0.0f, 8.0f, 0.0f}, NiPoint3{30.0f, 8.0f, 0.0f}, 1.0f, 2, BodyPart::Torso);
	Check(CapsuleUnderPoint(bodies, 1, NiPoint3{15.0f, 7.0f, 0.0f}) == 0, "on the torso's surface: the torso");
	Check(CapsuleUnderPoint(bodies, 1, NiPoint3{15.0f, 16.0f, 0.0f}) == 1, "on the arm's surface: the arm");
	Check(CapsuleUnderPoint(bodies, 1, NiPoint3{15.0f, 8.0f, 0.0f}) == 0,
	      "someone else's capsule nearer is not theirs");
	Check(CapsuleUnderPoint(bodies, 3, NiPoint3{15.0f, 7.0f, 0.0f}) == -1, "an actor with no capsule: none");
}

void TestGap() {
	std::printf("The gap\n");
	const std::vector<NiPoint3> plate = Cylinder(5.0f, 0.0f, 30.0f);
	const MeshGap air = MeasureMeshGap(NiPoint3{15.0f, 7.0f, 0.0f}, kA, kB, plate.data(),
	                                   static_cast<UInt32>(plate.size()));
	Check(air.measured, "over a mesh 5 round: measured");
	Check(Near(air.gapUnits, 2.0f) && Near(air.pointRadius, 7.0f) && Near(air.meshRadius, 5.0f),
	      "the point 7 out: 2 units of air");
	Check(Near(air.lane, kMeshGapLaneUnits) && air.laneVertices > 0, "found in the narrow lane");
	Check(Near(air.nearestUnits, 2.0f), "the nearest vertex 2 units off");

	const std::vector<NiPoint3> pauldron = Cylinder(9.0f, 0.0f, 30.0f);
	const MeshGap sunk = MeasureMeshGap(NiPoint3{15.0f, 0.0f, 7.0f}, kA, kB, pauldron.data(),
	                                    static_cast<UInt32>(pauldron.size()));
	Check(sunk.measured && Near(sunk.gapUnits, -2.0f) && Near(sunk.meshRadius, 9.0f),
	      "under a mesh 9 round: 2 units into it");
}

void TestLanes() {
	std::printf("The lanes\n");
	// rings 8 units apart, the point between two: the narrow lane (2.5)
	// misses both, the wide one (5) takes them
	const std::vector<NiPoint3> sparse = Cylinder(5.0f, 0.0f, 32.0f, 8.0f);
	const MeshGap wide = MeasureMeshGap(NiPoint3{12.0f, 7.0f, 0.0f}, kA, kB, sparse.data(),
	                                    static_cast<UInt32>(sparse.size()));
	Check(wide.measured && Near(wide.lane, kMeshGapWideLaneUnits), "a sparse mesh: found in the wide lane");
	Check(Near(wide.gapUnits, 2.0f, 0.05f), "its gap still 2");

	// one ring at x 0 only, the point at x 15: no lane reaches it
	const std::vector<NiPoint3> far = Cylinder(5.0f, 0.0f, 0.0f);
	const MeshGap none = MeasureMeshGap(NiPoint3{15.0f, 7.0f, 0.0f}, kA, kB, far.data(), static_cast<UInt32>(far.size()));
	Check(!none.measured, "no vertex in either lane: not measured");
	Check(Near(none.nearestUnits, std::sqrt(15.0f * 15.0f + 2.0f * 2.0f), 0.05f), "the nearest vertex still told");
}

void TestNotMeasured() {
	std::printf("Nothing to measure\n");
	const std::vector<NiPoint3> plate = Cylinder(5.0f, 0.0f, 30.0f);
	Check(!MeasureMeshGap(NiPoint3{15.0f, 0.0f, 0.0f}, kA, kB, plate.data(), static_cast<UInt32>(plate.size())).measured,
	      "the point on the axis: no way out");
	Check(!MeasureMeshGap(NiPoint3{15.0f, 7.0f, 0.0f}, kA, kB, nullptr, 0).measured, "no vertices");
}

void TestLeftOut() {
	std::printf("Vertices left out\n");
	std::vector<NiPoint3> mesh = Cylinder(5.0f, 0.0f, 30.0f);
	// an arm beside the torso, in the lane 20 units out: past the reach
	mesh.push_back(NiPoint3{15.0f, 25.0f, 0.0f});
	const MeshGap g = MeasureMeshGap(NiPoint3{15.0f, 7.0f, 0.0f}, kA, kB, mesh.data(), static_cast<UInt32>(mesh.size()));
	Check(g.measured && Near(g.gapUnits, 2.0f), "a limb past the reach is not the surface");

	// the far side of a lopsided body: only behind the axis, the near side
	// 5 round
	std::vector<NiPoint3> lopsided = Cylinder(5.0f, 0.0f, 30.0f);
	lopsided.push_back(NiPoint3{15.0f, -11.0f, 0.0f});
	const MeshGap h =
		MeasureMeshGap(NiPoint3{15.0f, 7.0f, 0.0f}, kA, kB, lopsided.data(), static_cast<UInt32>(lopsided.size()));
	Check(h.measured && Near(h.gapUnits, 2.0f), "the far side behind the axis is left out");
}

}  // namespace

int main() {
	TestCapsuleUnderPoint();
	TestGap();
	TestLanes();
	TestNotMeasured();
	TestLeftOut();
	std::printf(g_failures == 0 ? "All mesh gap checks passed\n" : "%d mesh gap checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

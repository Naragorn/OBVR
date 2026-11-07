// A fitted capsule's radial field (game/RadialField.h): the lanes and the
// bins are each other's inverse over the side, both ends and round the
// axis; a field built on shapes whose surface is known - an elliptic
// cylinder, a capsule rounder than the one fitted, nothing at all - reads
// that surface back between its bins; the gap's sign, a point on the axis,
// a placed field's scale.

#include <cstdio>
#include <vector>

#include "game/RadialFieldBuild.h"

using namespace obvr::game;
using obvr::NiPoint3;
namespace math = obvr::math;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

float Abs(float v) { return v < 0.0f ? -v : v; }

const NiPoint3 kA{0.0f, 0.0f, 0.0f};
const NiPoint3 kB{0.0f, 0.0f, 40.0f};

void TestLanes() {
	std::printf("the lanes and the bins\n");
	const NiPoint3 u = FieldSectorZero(kA, kB, NiPoint3{0.0f, 0.0f, 1.0f});
	const FieldPlace frame{kA, kB, u, 1.0f, NiPoint3{0.0f, 0.0f, 1.0f}};
	Check(Abs(DotOf3(u, kB - kA)) < 1e-4f && Abs(u.LengthSquared() - 1.0f) < 1e-4f, "sector 0 is a unit square to the axis");
	float worst = 0.0f;
	for (UInt32 row = 0; row < kFieldRows; ++row) {
		for (UInt32 sector = 0; sector < kFieldSectors; ++sector) {
			NiPoint3 foot;
			NiPoint3 out;
			FieldBinLane(row, sector, frame, foot, out);
			const FieldLane lane = FieldLaneOf(foot + out * 5.0f, frame);
			const float dr = Abs(lane.row - static_cast<float>(row));
			const float ds = Abs(lane.sector - static_cast<float>(sector));
			worst = dr > worst ? dr : worst;
			worst = ds > worst ? ds : worst;
		}
	}
	Check(worst < 1e-3f, "every bin's centre reads back as that bin: both ends, the side, round the axis");
	const FieldLane below = FieldLaneOf(NiPoint3{6.0f, 0.0f, -0.001f}, frame);
	const FieldLane above = FieldLaneOf(NiPoint3{6.0f, 0.0f, 0.001f}, frame);
	Check(Abs(below.row - above.row) < 0.01f && Abs(above.row - (kFieldCapRings - 0.5f)) < 0.01f,
	      "over the rim the row goes on without a step");
	const FieldLane pole = FieldLaneOf(NiPoint3{0.0f, 0.0f, 50.0f}, frame);
	Check(pole.row > static_cast<float>(kFieldRows - 1) && Abs(pole.pointRadius - 10.0f) < 1e-3f,
	      "past an end the lane runs from that end, its pole past the last row");
	const FieldLane onAxis = FieldLaneOf(NiPoint3{0.0f, 0.0f, 20.0f}, frame);
	Check(onAxis.pointRadius < 1e-4f && Abs(DotOf3(onAxis.out, u) - 1.0f) < 1e-4f,
	      "a point on the axis: no distance, sector 0's direction out");
}

void TestNothingMeasured() {
	std::printf("a field with no mesh\n");
	const RadialField f = BuildRadialField(kA, kB, 7.0f, nullptr, 0);
	bool all = f.built && f.measured == 0 && Abs(f.outer - 7.0f) < 1e-5f;
	for (UInt32 i = 0; i < kFieldBins; ++i) {
		all = all && Abs(f.radius[i] - 7.0f) < 1e-5f;
	}
	Check(all, "every bin keeps the capsule's radius");
	FieldPlace place = HomePlace(f, kA, kB);
	Check(Abs(FieldGapAt(f, place, NiPoint3{10.0f, 0.0f, 20.0f}) - 3.0f) < 1e-4f, "outside it the gap is above 0");
	Check(Abs(FieldGapAt(f, place, NiPoint3{0.0f, 2.0f, 20.0f}) + 5.0f) < 1e-4f, "inside it the gap is below 0");
	Check(Abs(FieldGapAt(f, place, NiPoint3{0.0f, 0.0f, 20.0f}) + 7.0f) < 1e-4f, "on the axis it is the whole radius deep");
	Check(Abs(FieldGapAt(f, place, NiPoint3{0.0f, 0.0f, 50.0f}) - 3.0f) < 1e-4f, "past an end it is measured from the end");
	place.scale = 2.0f;
	Check(Abs(FieldGapAt(f, place, NiPoint3{20.0f, 0.0f, 20.0f}) - 6.0f) < 1e-4f, "a placed field's radii scale with it");
}

// An elliptic cylinder 10 by 5 round the z axis, 0..40 along it, its
// vertices half a unit apart (worn meshes: 0.4-0.5).
std::vector<NiPoint3> Ellipse() {
	std::vector<NiPoint3> v;
	for (float z = 0.0f; z <= 40.0f; z += 0.5f) {
		for (int k = 0; k < 120; ++k) {
			const float t = 2.0f * math::kPi * static_cast<float>(k) / 120.0f;
			v.push_back(NiPoint3{10.0f * math::Cos(t), 5.0f * math::Sin(t), z});
		}
	}
	return v;
}

float EllipseRadius(float angle) {
	const float c = math::Cos(angle) / 10.0f;
	const float s = math::Sin(angle) / 5.0f;
	return 1.0f / math::Sqrt(c * c + s * s);
}

void TestEllipse() {
	std::printf("an elliptic cylinder in a round capsule\n");
	const std::vector<NiPoint3> v = Ellipse();
	const RadialField f = BuildRadialField(kA, kB, 7.5f, v.data(), static_cast<UInt32>(v.size()));
	const FieldPlace place = HomePlace(f, kA, kB);
	Check(f.measured >= kFieldSlices * kFieldSectors, "every bin of the side measured");
	float worstField = 0.0f;
	float worstCapsule = 0.0f;
	for (int s = 1; s < 16; ++s) {
		for (int k = 0; k < 48; ++k) {
			// between the bins: off their centres along and round
			const float angle = 2.0f * math::kPi * (static_cast<float>(k) + 0.25f) / 48.0f;
			const float z = 40.0f * static_cast<float>(s) / 16.0f;
			const float truth = EllipseRadius(angle);
			const NiPoint3 onMesh{truth * math::Cos(angle), truth * math::Sin(angle), z};
			const float gap = Abs(FieldGapAt(f, place, onMesh));
			worstField = gap > worstField ? gap : worstField;
			const float capsuleGap = Abs(truth - 7.5f);
			worstCapsule = capsuleGap > worstCapsule ? capsuleGap : worstCapsule;
		}
	}
	char line[160];
	std::snprintf(line, sizeof(line), "the mesh read back between the bins within 0.6 units (worst %.2f; the capsule %.2f)",
	              static_cast<double>(worstField), static_cast<double>(worstCapsule));
	Check(worstField < 0.6f, line);
	Check(f.outer > 9.5f && f.outer < 10.5f, "the field's outer radius is the ellipse's long half axis");
}

// A capsule of radius 8 (its side and both round ends), half a unit apart.
std::vector<NiPoint3> RoundCapsule() {
	std::vector<NiPoint3> v;
	for (float z = 0.0f; z <= 40.0f; z += 0.5f) {
		for (int k = 0; k < 100; ++k) {
			const float t = 2.0f * math::kPi * static_cast<float>(k) / 100.0f;
			v.push_back(NiPoint3{8.0f * math::Cos(t), 8.0f * math::Sin(t), z});
		}
	}
	for (int e = 0; e < 2; ++e) {
		for (int i = 1; i <= 25; ++i) {
			const float tilt = math::kHalfPi * static_cast<float>(i) / 25.0f;
			const int round = 1 + static_cast<int>(100.0f * math::Cos(tilt));
			for (int k = 0; k < round; ++k) {
				const float t = 2.0f * math::kPi * static_cast<float>(k) / static_cast<float>(round);
				const float r = 8.0f * math::Cos(tilt);
				const float h = 8.0f * math::Sin(tilt);
				v.push_back(NiPoint3{r * math::Cos(t), r * math::Sin(t), e == 0 ? -h : 40.0f + h});
			}
		}
	}
	return v;
}

void TestEnds() {
	std::printf("round ends wider than the capsule fitted\n");
	const std::vector<NiPoint3> v = RoundCapsule();
	const RadialField f = BuildRadialField(kA, kB, 6.0f, v.data(), static_cast<UInt32>(v.size()));
	const FieldPlace place = HomePlace(f, kA, kB);
	float worst = 0.0f;
	for (int e = 0; e < 2; ++e) {
		for (int i = 1; i < 6; ++i) {
			for (int k = 0; k < 24; ++k) {
				const float tilt = math::kHalfPi * static_cast<float>(i) / 6.0f;
				const float angle = 2.0f * math::kPi * (static_cast<float>(k) + 0.3f) / 24.0f;
				const NiPoint3 out{math::Cos(tilt) * math::Cos(angle), math::Cos(tilt) * math::Sin(angle),
				                   (e == 0 ? -1.0f : 1.0f) * math::Sin(tilt)};
				const NiPoint3 onMesh = (e == 0 ? kA : kB) + out * 8.0f;
				const float gap = Abs(FieldGapAt(f, place, onMesh));
				worst = gap > worst ? gap : worst;
			}
		}
	}
	char line[128];
	std::snprintf(line, sizeof(line), "both ends' mesh read back within 0.5 units (worst %.2f; the capsule 2.00)",
	              static_cast<double>(worst));
	Check(worst < 0.5f, line);
	const NiPoint3 surface = FieldSurfaceAt(f, place, NiPoint3{0.0f, 0.0f, 60.0f});
	Check(Abs(surface.z - 48.0f) < 0.5f, "the surface past an end's pole is the mesh's, not the capsule's");
}

void TestPlaced() {
	std::printf("a field read where its capsule is placed\n");
	const std::vector<NiPoint3> v = Ellipse();
	const RadialField f = BuildRadialField(kA, kB, 7.5f, v.data(), static_cast<UInt32>(v.size()));
	// turned a quarter round the x axis and moved: z becomes -y
	const auto turn = [](const NiPoint3& p) { return NiPoint3{p.x + 100.0f, -p.z, p.y + 50.0f}; };
	const FieldPlace place{turn(kA), turn(kB), NiPoint3{f.u.x, -f.u.z, f.u.y}, 1.0f, NiPoint3{f.w.x, -f.w.z, f.w.y}};
	const FieldPlace home = HomePlace(f, kA, kB);
	float worst = 0.0f;
	for (int k = 0; k < 36; ++k) {
		const float angle = 2.0f * math::kPi * static_cast<float>(k) / 36.0f;
		const NiPoint3 p{12.0f * math::Cos(angle), 12.0f * math::Sin(angle), 13.0f};
		const NiPoint3 q = turn(p);
		const float d = Abs(FieldGapAt(f, home, p) - FieldGapAt(f, place, q));
		worst = d > worst ? d : worst;
	}
	Check(worst < 1e-3f, "turned and moved with its capsule, the field reads the same");
	// a ball (no length, so no axis of its own): a flattened sphere, 8 round
	// and 4 high, turned as above
	std::vector<NiPoint3> squat;
	for (int i = -20; i <= 20; ++i) {
		for (int k = 0; k < 80; ++k) {
			const float tilt = math::kHalfPi * static_cast<float>(i) / 20.0f;
			const float t = 2.0f * math::kPi * static_cast<float>(k) / 80.0f;
			squat.push_back(NiPoint3{8.0f * math::Cos(tilt) * math::Cos(t), 8.0f * math::Cos(tilt) * math::Sin(t),
			                         4.0f * math::Sin(tilt)});
		}
	}
	const RadialField ball = BuildRadialField(kA, kA, 6.0f, squat.data(), static_cast<UInt32>(squat.size()));
	const FieldPlace ballHome = HomePlace(ball, kA, kA);
	const FieldPlace ballTurned{turn(kA), turn(kA), NiPoint3{ball.u.x, -ball.u.z, ball.u.y}, 1.0f,
	                            NiPoint3{ball.w.x, -ball.w.z, ball.w.y}};
	worst = 0.0f;
	for (int k = 0; k < 36; ++k) {
		const float angle = 2.0f * math::kPi * static_cast<float>(k) / 36.0f;
		const NiPoint3 p{9.0f * math::Cos(angle), 3.0f, 9.0f * math::Sin(angle)};
		const float d = Abs(FieldGapAt(ball, ballHome, p) - FieldGapAt(ball, ballTurned, turn(p)));
		worst = d > worst ? d : worst;
	}
	Check(worst < 1e-3f, "a ball's field turns with its frame's axis, not the space's up");
	Check(Abs(FieldGapAt(ball, ballHome, NiPoint3{0.0f, 0.0f, 6.0f}) - 2.0f) < 0.3f &&
	          Abs(FieldGapAt(ball, ballHome, NiPoint3{10.0f, 0.0f, 0.0f}) - 2.0f) < 0.3f,
	      "a ball's field: its flat top and its round middle");
}

bool Near(const NiPoint3& p, const NiPoint3& q, float within) { return (p - q).LengthSquared() <= within * within; }

// Without a field every shape query is the round capsule's own geometry.
void TestShapesWithoutField() {
	std::printf("the shape queries without a field\n");
	const FieldRef none;
	bool same = true;
	const NiPoint3 points[] = {{10.0f, 0.0f, 20.0f}, {0.0f, 3.0f, 5.0f}, {0.0f, 0.0f, 20.0f}, {2.0f, 2.0f, -9.0f}};
	for (const NiPoint3& p : points) {
		NiPoint3 n1;
		NiPoint3 n2;
		const float depth = CapsuleDepthAt(p, kA, kB, 7.0f, n1);
		same = same && Abs(ShapeGapAt(kA, kB, 7.0f, none, p, &n2) + depth) < 1e-5f && Near(n1, n2, 1e-5f);
	}
	Check(same, "a point's gap and normal: the capsule's depth, turned round");
	const NiPoint3 from{20.0f, -5.0f, 10.0f};
	const NiPoint3 to{-20.0f, 5.0f, 30.0f};
	NiPoint3 at;
	NiPoint3 normal;
	const float gap = ShapeSegmentGap(kA, kB, 7.0f, none, from, to, &at, &normal);
	Check(Abs(gap - (SegmentSegmentDistance(from, to, kA, kB) - 7.0f)) < 1e-4f, "a segment's gap: the segments' distance less the radius");
	const NiPoint3 along{0.0f, 0.0f, 20.0f};
	Check(ShapeSegmentGap(kA, kB, 7.0f, none, along - NiPoint3{0.0f, 0.0f, 5.0f}, along, nullptr, &normal) < -6.9f &&
	          normal.LengthSquared() == 0.0f,
	      "a segment on the axis: the whole radius in, no direction out");
	float f1 = 0.0f;
	float f2 = 0.0f;
	NiPoint3 m1;
	NiPoint3 m2;
	bool in1 = false;
	bool in2 = false;
	const NiPoint3 sweepFrom{30.0f, 0.0f, 20.0f};
	const NiPoint3 sweepTo{-30.0f, 0.0f, 20.0f};
	const bool e1 = SegmentEntersCapsule(sweepFrom, sweepTo, kA, kB, 8.5f, f1, m1, in1);
	const bool e2 = ShapeSegmentEnters(kA, kB, 7.0f, none, sweepFrom, sweepTo, 1.5f, f2, m2, in2);
	Check(e1 && e2 && Abs(f1 - f2) < 1e-6f && Near(m1, m2, 1e-6f) && in1 == in2,
	      "a sweep enters where the capsule grown by its reach is entered");
	Check(Abs(ShapeOuter(7.0f, none) - 7.0f) < 1e-6f, "the bound is the capsule");
}

// With the elliptic cylinder's field (10 by 5 in a capsule of 7.5).
void TestShapesWithField() {
	std::printf("the shape queries with a field\n");
	const std::vector<NiPoint3> v = Ellipse();
	const RadialField f = BuildRadialField(kA, kB, 7.5f, v.data(), static_cast<UInt32>(v.size()));
	const FieldRef ref{&f, f.u, 1.0f, f.w};
	Check(Abs(ShapeOuter(7.5f, ref) - f.outer) < 1e-6f, "the bound is the field's outer radius");
	NiPoint3 normal;
	const float face = ShapeGapAt(kA, kB, 7.5f, ref, NiPoint3{0.0f, 6.0f, 20.0f}, &normal);
	Check(Abs(face - 1.0f) < 0.6f && normal.y > 0.99f, "a point a unit off the flat face: a unit out, the face's normal (the capsule: 1.5 in)");
	// off the ellipse where its face is aslant to the axis's lanes (45
	// degrees round: the surface's normal 37 degrees from the radial way)
	const NiPoint3 onSlope{10.0f * math::Cos(math::kPi / 4.0f), 5.0f * math::Sin(math::kPi / 4.0f), 20.0f};
	const NiPoint3 slopeNormal{0.4472f, 0.8944f, 0.0f};
	ShapeGapAt(kA, kB, 7.5f, ref, onSlope + slopeNormal * 1.0f, &normal);
	const float agree = DotOf3(normal, slopeNormal);
	char slopeLine[160];
	std::snprintf(slopeLine, sizeof(slopeLine), "the normal on an aslant face is the surface's, not the lane's (cos %.3f; the lane's 0.80)",
	              static_cast<double>(agree));
	Check(agree > 0.985f, slopeLine);
	// a blade laid flat along the face, a unit off it
	NiPoint3 at;
	const float lying = ShapeSegmentGap(kA, kB, 7.5f, ref, NiPoint3{-3.0f, 6.0f, 5.0f}, NiPoint3{3.0f, 6.0f, 35.0f}, &at, &normal);
	Check(Abs(lying - 1.0f) < 0.6f && Abs(at.y - 6.0f) < 1e-3f && normal.y > 0.95f,
	      "a segment along the flat face: its gap and the direction off the face");
	const float across = ShapeSegmentGap(kA, kB, 7.5f, ref, NiPoint3{11.0f, -20.0f, 20.0f}, NiPoint3{11.0f, 20.0f, 20.0f}, &at);
	Check(Abs(across - 1.0f) < 0.6f && Abs(at.y) < 1.0f, "a segment past the long side: its nearest point at the side, a unit off");
	const float far = ShapeSegmentGap(kA, kB, 7.5f, ref, NiPoint3{40.0f, -5.0f, 20.0f}, NiPoint3{40.0f, 5.0f, 20.0f});
	const float truth = ShapeGapAt(kA, kB, 7.5f, ref, NiPoint3{40.0f, 0.0f, 20.0f});
	Check(far <= truth + 1e-4f && far > truth - 1.0f, "far off: the bound's gap, never more than the field's");
	float fraction = 0.0f;
	bool inside = false;
	const bool hit = ShapeSegmentEnters(kA, kB, 7.5f, ref, NiPoint3{0.0f, 25.0f, 20.0f}, NiPoint3{0.0f, -25.0f, 20.0f},
	                                    1.5f, fraction, normal, inside);
	// the face at y 5 grown by 1.5: entered at y 6.5, 18.5 of the 50 units
	Check(hit && !inside && Abs(fraction * 50.0f - 18.5f) < 0.6f && normal.y > 0.99f,
	      "a sweep at the flat face stops its reach short of the face, not of the round capsule");
	const bool past = ShapeSegmentEnters(kA, kB, 7.5f, ref, NiPoint3{0.0f, 25.0f, 20.0f}, NiPoint3{0.0f, 7.0f, 20.0f}, 1.5f,
	                                     fraction, normal, inside);
	Check(!past, "a sweep ending in the round capsule's reach but short of the face's does not enter");
	const bool in = ShapeSegmentEnters(kA, kB, 7.5f, ref, NiPoint3{0.0f, 5.5f, 20.0f}, NiPoint3{0.0f, 25.0f, 20.0f}, 1.5f,
	                                   fraction, normal, inside);
	Check(in && inside && fraction == 0.0f, "a sweep starting within the reach is inside at once");
	const bool miss = ShapeSegmentEnters(kA, kB, 7.5f, ref, NiPoint3{30.0f, 25.0f, 20.0f}, NiPoint3{30.0f, -25.0f, 20.0f},
	                                     1.5f, fraction, normal, inside);
	Check(!miss, "a sweep passing the bound by misses");
}

}  // namespace

int main() {
	TestLanes();
	TestNothingMeasured();
	TestEllipse();
	TestEnds();
	TestPlaced();
	TestShapesWithoutField();
	TestShapesWithField();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("All radial field checks passed\n");
	return 0;
}

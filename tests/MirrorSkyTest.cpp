// Checks the sky in the mirror's render (game/MirrorSkyLogic.h,
// docs/mirror-spec.md 16): where the camera node's local translation is set so
// that the engine's own copy (RenderScene 0x0040C830 copies it into the Sky and
// Weather roots and updates them) puts the sky on the mirror's camera, with
// every refusal. The engine's placement is played over plain values: with the
// sky left on the eye, the two mirror cameras of the owner's run (OBVR.log
// 2026-11-28 20:17, "Mirror trace" lines 4393 and 4415) see the dome's points
// in different directions - the stereo of a thing at the dome's radius; on
// their own cameras, in the same direction - the sky at infinity.

#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>

#include "game/MirrorSkyLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool condition, const char* what) {
	++g_checks;
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

NiTransform Identity() {
	NiTransform t{};
	t.rot = NiMatrix33::Identity();
	t.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	t.scale = 1.0f;
	return t;
}

// The engine's world from a parent and a local translation (Gamebryo's update:
// parent.pos + parent.rot * (local * parent.scale)).
NiPoint3 WorldOf(const NiTransform& parent, const NiPoint3& local) {
	return parent.pos + parent.rot * (local * parent.scale);
}

MirrorSkyInputs Ready(const NiPoint3& camera) {
	MirrorSkyInputs in;
	in.cameraNode = true;
	in.skyRoot = true;
	in.skyParent = true;
	in.skyParentWorld = Identity();
	in.camera = camera;
	return in;
}

// The owner's run of 2026-11-28 20:17, the first traced frame (scene call
// 3951): the two mirror cameras [M]. The eye's place is not logged; it is
// derived as the left camera mirrored through the glass's centre (the glass at
// 47376.8 83190.1 269.4, the eye 47.6 units in front): near enough for the
// geometry here, which only needs a centre about 95 units off both cameras.
const NiPoint3 kLeft{47380.9f, 83237.3f, 255.3f};
const NiPoint3 kRight{47385.2f, 83236.8f, 254.7f};
const NiPoint3 kGlass{47376.8f, 83190.1f, 269.4f};
const NiPoint3 kEye{2.0f * kGlass.x - kLeft.x, 2.0f * kGlass.y - kLeft.y, 2.0f * kGlass.z - kLeft.z};

NiPoint3 Unit(const NiPoint3& v) {
	const float length = std::sqrt(v.LengthSquared());
	return v * (1.0f / length);
}

// The angle in degrees between the directions two cameras see one point of a
// dome of radius `radius` centred on `centreLeft` (for the left camera's
// render) and `centreRight` (the right's) in direction `dir`.
float DisparityDegrees(const NiPoint3& centreLeft, const NiPoint3& centreRight, const NiPoint3& dir, float radius) {
	// The angle from the cross and dot products (an arccosine near 1 loses
	// hundredths of a degree in floats).
	const NiPoint3 a = Unit((centreLeft + dir * radius) - kLeft);
	const NiPoint3 b = Unit((centreRight + dir * radius) - kRight);
	const double cx = static_cast<double>(a.y) * b.z - static_cast<double>(a.z) * b.y;
	const double cy = static_cast<double>(a.z) * b.x - static_cast<double>(a.x) * b.z;
	const double cz = static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x;
	const double dot = static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y + static_cast<double>(a.z) * b.z;
	return static_cast<float>(std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), dot) * 57.29577951308232);
}

void Placed() {
	std::printf("the sky's root on the mirror's camera\n");
	{
		const MirrorSkyPlan p = PlanMirrorSky(Ready(kLeft));
		Check(p.refusal == MirrorSkyRefusal::None, "a world, a sky root under the identity: placed");
		Check(NearPoint(p.nodeLocal, kLeft, 0.01f), "under the identity the node's local is the camera's place");
	}
	{
		MirrorSkyInputs in = Ready(kRight);
		in.skyParent = false;
		const MirrorSkyPlan p = PlanMirrorSky(in);
		Check(p.refusal == MirrorSkyRefusal::None && NearPoint(p.nodeLocal, kRight, 0.01f),
		      "no parent: the local is the world, the camera's place");
	}
	{
		// A parent turned 30 degrees about z, moved and scaled: the engine's
		// copy of the node's local into the root, through the parent, lands on
		// the camera.
		MirrorSkyInputs in = Ready(kLeft);
		const float c = std::cos(0.5235988f);
		const float s = std::sin(0.5235988f);
		in.skyParentWorld.rot.data[0][0] = c;
		in.skyParentWorld.rot.data[0][1] = -s;
		in.skyParentWorld.rot.data[1][0] = s;
		in.skyParentWorld.rot.data[1][1] = c;
		in.skyParentWorld.pos = NiPoint3{1200.0f, -340.0f, 55.0f};
		in.skyParentWorld.scale = 1.5f;
		const MirrorSkyPlan p = PlanMirrorSky(in);
		Check(p.refusal == MirrorSkyRefusal::None, "a turned, moved, scaled parent: placed");
		Check(NearPoint(WorldOf(in.skyParentWorld, p.nodeLocal), kLeft, 0.05f),
		      "the root's world through that parent is the camera's place");
	}
}

void Refused() {
	std::printf("refusals, in order\n");
	MirrorSkyInputs none = Ready(kLeft);
	none.cameraNode = false;
	none.skyRoot = false;
	none.camera.x = std::numeric_limits<float>::quiet_NaN();
	Check(PlanMirrorSky(none).refusal == MirrorSkyRefusal::NoCameraNode, "no camera node first");
	MirrorSkyInputs noRoot = Ready(kLeft);
	noRoot.skyRoot = false;
	noRoot.camera.x = std::numeric_limits<float>::quiet_NaN();
	Check(PlanMirrorSky(noRoot).refusal == MirrorSkyRefusal::NoSkyRoot, "then no sky root");
	const float bad[] = {0.0f, 1.0e-7f, std::numeric_limits<float>::quiet_NaN(),
	                     std::numeric_limits<float>::infinity()};
	for (float scale : bad) {
		MirrorSkyInputs in = Ready(kLeft);
		in.skyParentWorld.scale = scale;
		Check(PlanMirrorSky(in).refusal == MirrorSkyRefusal::ParentNotUsable,
		      "a parent scale of 0, near 0, no number or endless: refused");
	}
	{
		MirrorSkyInputs in = Ready(kLeft);
		in.skyParentWorld.rot.data[1][2] = std::numeric_limits<float>::quiet_NaN();
		Check(PlanMirrorSky(in).refusal == MirrorSkyRefusal::ParentNotUsable, "a parent rotation of no number: refused");
	}
	{
		MirrorSkyInputs in = Ready(kLeft);
		in.skyParentWorld.pos.y = std::numeric_limits<float>::infinity();
		Check(PlanMirrorSky(in).refusal == MirrorSkyRefusal::ParentNotUsable, "a parent place endless: refused");
	}
	{
		MirrorSkyInputs in = Ready(kLeft);
		in.skyParentWorld.rot = NiMatrix33{};
		Check(PlanMirrorSky(in).refusal == MirrorSkyRefusal::ParentNotUsable, "a parent rotation collapsed to 0: refused");
	}
	{
		MirrorSkyInputs in = Ready(kLeft);
		in.skyParent = false;
		in.skyParentWorld.scale = 0.0f;  // not read without a parent
		in.camera.z = std::numeric_limits<float>::quiet_NaN();
		Check(PlanMirrorSky(in).refusal == MirrorSkyRefusal::CameraNotANumber, "a camera of no number: refused");
	}
	Check(std::string_view(MirrorSkyRefusalText(MirrorSkyRefusal::NoSkyRoot)).size() > 0, "each refusal has its words");
}

// The engine's placement over plain values: the root takes the node's local
// (identity parents; both roots hang under one node under the World root,
// 0x00406950 - their transforms are not read, so the test takes the identity).
NiPoint3 SkyCentreFor(const NiPoint3& camera) {
	const MirrorSkyPlan p = PlanMirrorSky(Ready(camera));
	return p.refusal == MirrorSkyRefusal::None ? p.nodeLocal : kEye;  // refused: the node stays on the eye
}

void Stereo() {
	std::printf("the sky's stereo in the two mirror cameras\n");
	const NiPoint3 dirs[] = {Unit(NiPoint3{0.3f, -1.0f, 0.4f}), Unit(NiPoint3{-0.8f, 0.2f, 0.6f}),
	                         Unit(NiPoint3{0.0f, 0.0f, 1.0f}), Unit(NiPoint3{1.0f, 0.1f, 0.05f})};
	const float radii[] = {500.0f, 4000.0f, 30000.0f};
	float worstBefore = 0.0f;
	float worstAfter = 0.0f;
	float leastBefore = 1000.0f;
	for (float radius : radii) {
		for (const NiPoint3& dir : dirs) {
			const float before = DisparityDegrees(kEye, kEye, dir, radius);
			const float after = DisparityDegrees(SkyCentreFor(kLeft), SkyCentreFor(kRight), dir, radius);
			worstBefore = before > worstBefore ? before : worstBefore;
			leastBefore = before < leastBefore ? before : leastBefore;
			worstAfter = after > worstAfter ? after : worstAfter;
		}
	}
	std::printf("    the sky on the eye: the two cameras see a dome point %.4f to %.4f degrees apart\n",
	            static_cast<double>(leastBefore), static_cast<double>(worstBefore));
	std::printf("    the sky on each camera: at most %.6f degrees apart\n", static_cast<double>(worstAfter));
	Check(worstBefore > 0.05f, "the sky left on the eye: dome points across the baseline have a disparity (a thing at the "
	                           "dome's radius)");
	Check(worstAfter < 0.0005f, "the sky on each mirror camera: no disparity at any radius (a thing at infinity)");
	// Straight up, square to the cameras' baseline: the disparity is the
	// baseline over the radius (4.3 units over 4000: about 0.06 degrees).
	const NiPoint3 b = kRight - kLeft;
	const NiPoint3 up = dirs[2];
	const NiPoint3 across{b.y * up.z - b.z * up.y, b.z * up.x - b.x * up.z, b.x * up.y - b.y * up.x};
	const float expected = std::sqrt(across.LengthSquared()) / 4000.0f * 57.2957795f;
	const float at4000 = DisparityDegrees(kEye, kEye, up, 4000.0f);
	std::printf("    straight up at 4000 units: %.4f degrees, the baseline over the radius %.4f\n",
	            static_cast<double>(at4000), static_cast<double>(expected));
	Check(Near(at4000, expected, 0.1f * expected), "before: the disparity is the baseline over the dome's radius");
}

}  // namespace

int main() {
	Placed();
	Refused();
	Stereo();
	std::printf("%d of %d checks failed\n", g_failures, g_checks);
	return g_failures == 0 ? 0 : 1;
}

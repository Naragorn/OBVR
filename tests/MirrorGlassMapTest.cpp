// Checks the picture laid onto the glass as it is (vr/MirrorGlassMap.h,
// docs/mirror-spec.md 13): the view a picture was rendered with, the glass's
// corners projected through it - the same glass on the picture's corners,
// flipped as a mirror shows; a glass grown by the margin; a glass turned since
// the render, where only the projective coordinates are exact; corners past
// the picture, behind the camera, of no number; no view.

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/MirrorGlassMap.h"
#include "vr/MirrorSteadyLogic.h"

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

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

NiPoint3 P(float x, float y, float z) { return NiPoint3{x, y, z}; }

// A glass of w by h units upright in the plane y = distance, its face towards
// the origin (as mirror_glass_test's).
void UprightGlass(float w, float h, float distance, NiPoint3 (&c)[4]) {
	c[0] = P(-0.5f * w, distance, 0.5f * h);
	c[1] = P(0.5f * w, distance, 0.5f * h);
	c[2] = P(-0.5f * w, distance, -0.5f * h);
	c[3] = P(0.5f * w, distance, -0.5f * h);
}

const float kWhole[4] = {0.0f, 0.0f, 1.0f, 1.0f};

MirrorPictureView ViewThrough(const NiPoint3 (&corners)[4], const NiPoint3& eye, const float (&rect)[4]) {
	const GlassPlane g = GlassPlaneOf(corners);
	return PictureViewOf(MirrorCameraFor(g, corners, eye), rect);
}

// Where a point of the glass reads the picture, projected directly.
void Direct(const MirrorPictureView& v, const NiPoint3& p, float& u, float& w) {
	const NiPoint3 d = p - v.position;
	const float z = MirrorDot(d, v.forward);
	u = v.rect[0] + (MirrorDot(d, v.right) / z - v.left) / (v.rightEdge - v.left) * (v.rect[2] - v.rect[0]);
	w = v.rect[1] + (v.top - MirrorDot(d, v.up) / z) / (v.top - v.bottom) * (v.rect[3] - v.rect[1]);
}

void TestView() {
	std::printf("The view a picture was rendered with\n");
	NiPoint3 c[4];
	UprightGlass(20.0f, 10.0f, 40.0f, c);
	const MirrorPictureView v = ViewThrough(c, P(0.0f, 0.0f, 0.0f), kWhole);
	Check(v.valid, "a camera that rendered: a view");
	Check(Near(v.position.y, 80.0f) && Near(v.forward.y, -1.0f) && Near(v.up.z, 1.0f) && Near(v.right.x, -1.0f),
	      "behind the glass, facing out of it, the glass's up, right the glass's left");
	MirrorCamera none;
	Check(!PictureViewOf(none, kWhole).valid, "no camera: no view");
	MirrorCamera flat = MirrorCameraFor(GlassPlaneOf(c), c, P(0.0f, 0.0f, 0.0f));
	flat.right = flat.left;
	Check(!PictureViewOf(flat, kWhole).valid, "a frustum of no width: no view");
}

void TestSameGlass() {
	std::printf("The glass the picture was rendered through\n");
	NiPoint3 c[4];
	UprightGlass(20.0f, 10.0f, 40.0f, c);
	const float rect[4] = {0.0f, 0.0f, 0.75f, 0.5f};
	const MirrorPictureView v = ViewThrough(c, P(3.0f, 0.0f, -2.0f), rect);
	float s[4], t[4], q[4];
	Check(MapGlassCorners(v, c, s, t, q) == GlassMap::Inside, "inside the picture");
	Check(Near(q[0], 1.0f) && Near(q[3], 1.0f), "parallel to the picture: every q is 1");
	Check(Near(s[0], 0.75f) && Near(t[0], 0.0f), "the top left corner reads the picture's top right: a mirror's flip");
	Check(Near(s[1], 0.0f) && Near(s[2], 0.75f) && Near(t[2], 0.5f) && Near(s[3], 0.0f) && Near(t[3], 0.5f),
	      "the other corners on the picture's corners, in the texture's part");
}

void TestMargin() {
	std::printf("A picture rendered with the margin\n");
	NiPoint3 c[4];
	UprightGlass(20.0f, 10.0f, 40.0f, c);
	GlassFrame f = GlassFrameOf(GlassPlaneOf(c));
	NiPoint3 grown[4];
	SteadyGlassCorners(c, f, f, 0.1f, grown);
	const MirrorPictureView v = ViewThrough(grown, P(0.0f, 0.0f, 0.0f), kWhole);
	float s[4], t[4], q[4];
	Check(MapGlassCorners(v, c, s, t, q) == GlassMap::Inside, "the glass inside the grown picture");
	Check(Near(s[0], 1.0f - 0.1f / 1.2f) && Near(t[0], 0.1f / 1.2f) && Near(s[3], 0.1f / 1.2f) && Near(t[3], 1.0f - 0.1f / 1.2f),
	      "a margin of a twelfth all round");
}

void TestTurnedGlass() {
	std::printf("The glass turned 3 degrees since the render\n");
	NiPoint3 c[4];
	UprightGlass(20.0f, 10.0f, 40.0f, c);
	GlassFrame f = GlassFrameOf(GlassPlaneOf(c));
	NiPoint3 grown[4];
	SteadyGlassCorners(c, f, f, 0.1f, grown);
	const MirrorPictureView v = ViewThrough(grown, P(0.0f, 0.0f, 0.0f), kWhole);
	GlassFrame turned = f;
	turned.rot = MirrorAxisTurn(P(0.0f, 0.0f, 1.0f), 3.0f * math::kDegreesToRadians) * f.rot;
	NiPoint3 now[4];
	SteadyGlassCorners(c, f, turned, 0.0f, now);
	float s[4], t[4], q[4];
	Check(MapGlassCorners(v, now, s, t, q) == GlassMap::Inside, "still inside the margin");
	Check(!Near(q[0], q[1], 0.01f), "one edge nearer the camera than the other: q differs");
	// The middle of the diagonal top left - bottom right: the device interpolates
	// s, t, q affinely over the glass, so there they are the corners' means.
	const NiPoint3 middle = (now[0] + now[3]) * 0.5f;
	float u = 0.0f, w = 0.0f;
	Direct(v, middle, u, w);
	const float projective = (s[0] + s[3]) / (q[0] + q[3]);
	const float plain = 0.5f * (s[0] / q[0] + s[3] / q[3]);
	std::printf("        the diagonal's middle reads u %.5f directly, %.5f projective, %.5f plain\n", static_cast<double>(u),
	            static_cast<double>(projective), static_cast<double>(plain));
	Check(Near(projective, u, 1.0e-4f) && Near((t[0] + t[3]) / (q[0] + q[3]), w, 1.0e-4f), "the projective coordinates are exact");
	Check(!Near(plain, u, 1.0e-4f), "plain coordinates would be off there");
}

void TestOutside() {
	std::printf("Corners past the picture, behind the camera, of no number\n");
	NiPoint3 c[4];
	UprightGlass(20.0f, 10.0f, 40.0f, c);
	const MirrorPictureView v = ViewThrough(c, P(0.0f, 0.0f, 0.0f), kWhole);
	NiPoint3 bigger[4];
	UprightGlass(24.0f, 10.0f, 40.0f, bigger);
	float s[4], t[4], q[4];
	Check(MapGlassCorners(v, bigger, s, t, q) == GlassMap::Outside, "a glass wider than the picture: outside, drawn dark there");
	Check(s[0] > 1.0f && s[1] < 0.0f, "its coordinates past the picture's edges (the texture's black beyond)");
	NiPoint3 behind[4];
	UprightGlass(20.0f, 10.0f, 40.0f, behind);
	behind[2] = P(-10.0f, 90.0f, -5.0f);  // past the camera at y = 80
	Check(MapGlassCorners(v, behind, s, t, q) == GlassMap::Behind, "a corner behind the camera: not mapped");
	behind[2] = P(-10.0f, 79.8f, -5.0f);  // 0.2 units in front of it
	Check(MapGlassCorners(v, behind, s, t, q) == GlassMap::Behind, "a corner at the camera: not mapped");
	NiPoint3 broken[4];
	UprightGlass(20.0f, 10.0f, 40.0f, broken);
	broken[1].x = std::numeric_limits<float>::quiet_NaN();
	Check(MapGlassCorners(v, broken, s, t, q) == GlassMap::Behind, "a corner of no number: not mapped");
	Check(MapGlassCorners(MirrorPictureView{}, c, s, t, q) == GlassMap::NoView, "no view: not mapped");
}

}  // namespace

int main() {
	TestView();
	TestSameGlass();
	TestMargin();
	TestTurnedGlass();
	TestOutside();
	if (g_failures > 0) {
		std::printf("%d failing check(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

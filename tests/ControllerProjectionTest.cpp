// Checks the arithmetic of drawing the real controllers into the eyes: rigid
// transforms composed and inverted, a point projected onto an eye texture's
// pixels by the frustum's tangents, and the model's shading.

#include <cstdio>

#include "core/MathFns.h"
#include "render/ControllerProjection.h"

namespace {

using namespace obvr::render;
using obvr::NiPoint3;
using obvr::vr::openvr::HmdMatrix34;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float eps = 0.001f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

HmdMatrix34 Pose(float yawRadiansAboutY, float x, float y, float z) {
	const float c = obvr::math::Cos(yawRadiansAboutY);
	const float s = obvr::math::Sin(yawRadiansAboutY);
	HmdMatrix34 m{};
	m.m[0][0] = c;
	m.m[0][2] = s;
	m.m[1][1] = 1.0f;
	m.m[2][0] = -s;
	m.m[2][2] = c;
	m.m[0][3] = x;
	m.m[1][3] = y;
	m.m[2][3] = z;
	return m;
}

void TestRigid() {
	std::printf("Rigid transforms\n");
	const HmdMatrix34 a = Pose(0.7f, 1.0f, 2.0f, 3.0f);
	const HmdMatrix34 b = Pose(-0.3f, -0.5f, 0.25f, 4.0f);
	const NiPoint3 p{0.3f, -0.2f, 0.9f};
	Check(NearPoint(TransformPoint(ComposeRigid(a, b), p), TransformPoint(a, TransformPoint(b, p))),
	      "composing is applying one after the other");
	Check(NearPoint(TransformPoint(InvertRigid(a), TransformPoint(a, p)), p),
	      "the inverse takes a point back");
	Check(NearPoint(TransformPoint(ComposeRigid(a, InvertRigid(a)), p), p),
	      "a pose times its inverse is the identity");
}

void TestProjection() {
	std::printf("Projection onto the eye texture\n");
	// A symmetric frustum, 90 degrees across and down, "top" negative the
	// way OpenVR names it.
	EyeProjection eye;
	eye.left = -1.0f;
	eye.right = 1.0f;
	eye.top = -1.0f;
	eye.bottom = 1.0f;
	EyePixel px;
	Check(ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -1.0f}, eye, 1000.0f, 800.0f, 0.03f, 4.0f, px) &&
	          Near(px.x, 500.0f) && Near(px.y, 400.0f) && Near(px.rhw, 1.0f),
	      "straight ahead lands in the middle");
	Check(ProjectToEyePixel(NiPoint3{1.0f, 0.0f, -1.0f}, eye, 1000.0f, 800.0f, 0.03f, 4.0f, px) &&
	          Near(px.x, 1000.0f),
	      "45 degrees right is the right edge");
	Check(ProjectToEyePixel(NiPoint3{0.0f, 1.0f, -1.0f}, eye, 1000.0f, 800.0f, 0.03f, 4.0f, px) &&
	          Near(px.y, 0.0f),
	      "45 degrees up is the top row - the tangent called bottom");
	Check(ProjectToEyePixel(NiPoint3{0.0f, -0.5f, -1.0f}, eye, 1000.0f, 800.0f, 0.03f, 4.0f, px) &&
	          Near(px.y, 600.0f),
	      "down is further down the texture");
	// An asymmetric one: the optical axis off-centre, as a real eye has it.
	EyeProjection skew;
	skew.left = -1.2f;
	skew.right = 0.8f;
	skew.top = -1.1f;
	skew.bottom = 0.9f;
	Check(ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -2.0f}, skew, 1000.0f, 1000.0f, 0.03f, 4.0f, px) &&
	          Near(px.x, 600.0f) && Near(px.y, 450.0f),
	      "the axis falls where the tangents put it, not in the middle");
	Check(ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -2.03f}, eye, 100.0f, 100.0f, 0.03f, 4.03f, px) &&
	          Near(px.depth, 0.5f),
	      "depth runs from the near plane to the far");
	Check(ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -9.0f}, eye, 100.0f, 100.0f, 0.03f, 4.0f, px) &&
	          Near(px.depth, 1.0f),
	      "beyond the far plane it stays at the far plane");
	Check(!ProjectToEyePixel(NiPoint3{0.0f, 0.0f, 1.0f}, eye, 100.0f, 100.0f, 0.03f, 4.0f, px),
	      "behind the eye: refused");
	Check(!ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -0.01f}, eye, 100.0f, 100.0f, 0.03f, 4.0f, px),
	      "nearer than the near plane: refused");
	EyeProjection flat = eye;
	flat.right = flat.left;
	Check(!ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -1.0f}, flat, 100.0f, 100.0f, 0.03f, 4.0f, px),
	      "a frustum with no width: refused");
	flat = eye;
	flat.bottom = flat.top;
	Check(!ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -1.0f}, flat, 100.0f, 100.0f, 0.03f, 4.0f, px),
	      "no height: refused");
	Check(!ProjectToEyePixel(NiPoint3{0.0f, 0.0f, -1.0f}, eye, 100.0f, 100.0f, 1.0f, 0.5f, px),
	      "a far plane before the near one: refused");
}

void TestWorldProjection() {
	std::printf("Through the game's projection\n");
	// A standard left-handed Direct3D perspective: tangent 1 across and down,
	// near 10, far 1000 (row vectors).
	const float n = 10.0f;
	const float f = 1000.0f;
	float p[4][4] = {};
	p[0][0] = 1.0f;
	p[1][1] = 1.0f;
	p[2][2] = f / (f - n);
	p[2][3] = 1.0f;
	p[3][2] = -n * f / (f - n);
	EyePixel px;
	// Camera frame: x right, y forward, z up.
	Check(ProjectThroughD3D(NiPoint3{0.0f, 100.0f, 0.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f,
	                        px) &&
	          Near(px.x, 400.0f) && Near(px.y, 300.0f) && Near(px.rhw, 0.01f),
	      "straight ahead lands in the middle of the viewport");
	Check(ProjectThroughD3D(NiPoint3{100.0f, 100.0f, 0.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f,
	                        1.0f, px) &&
	          Near(px.x, 800.0f),
	      "45 degrees right is the right edge");
	Check(ProjectThroughD3D(NiPoint3{0.0f, 100.0f, 100.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f,
	                        1.0f, px) &&
	          Near(px.y, 0.0f),
	      "up is the top row");
	Check(ProjectThroughD3D(NiPoint3{0.0f, 10.0f, 0.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f,
	                        px) &&
	          Near(px.depth, 0.0f) &&
	          ProjectThroughD3D(NiPoint3{0.0f, 1000.0f, 0.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f,
	                            1.0f, px) &&
	          Near(px.depth, 1.0f),
	      "the depth is the game's: 0 at the near plane, 1 at the far");
	Check(ProjectThroughD3D(NiPoint3{0.0f, 100.0f, 0.0f}, p, 50.0f, 20.0f, 800.0f, 600.0f, 0.0f,
	                        1.0f, px) &&
	          Near(px.x, 450.0f) && Near(px.y, 320.0f),
	      "the viewport's corner moves the picture with it");
	Check(!ProjectThroughD3D(NiPoint3{0.0f, 5.0f, 0.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f,
	                         px),
	      "nearer than the near plane: refused");
	Check(!ProjectThroughD3D(NiPoint3{0.0f, -50.0f, 0.0f}, p, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f,
	                         1.0f, px),
	      "behind the camera: refused");

	Check(PerspectiveMatchesCamera(p, 1.0f) && PerspectiveMatchesCamera(p, 1.05f),
	      "the world camera's projection is recognised, within a tenth");
	Check(!PerspectiveMatchesCamera(p, 1.5f), "a different width is not the world camera's");
	Check(!PerspectiveMatchesCamera(p, 0.0f), "no camera tangent to compare with: not used");
	float identity[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
	Check(!PerspectiveMatchesCamera(identity, 1.0f), "an image-space pass's identity is refused");
	float flipped[4][4] = {};
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			flipped[r][c] = p[r][c];
		}
	}
	flipped[2][2] = -flipped[2][2];
	Check(!PerspectiveMatchesCamera(flipped, 1.0f), "a depth running the other way is refused");

	obvr::vr::openvr::HmdVector3 v{{0.1f, 0.2f, 0.3f}};
	Check(NearPoint(ControllerVertexToGame(v), NiPoint3{0.1f, -0.3f, 0.2f}),
	      "a model's up becomes the game's up, its back the game's backward");
}

void TestShade() {
	std::printf("Shading\n");
	const UInt32 lit = ShadedModelColour(NiPoint3{0.0f, 0.8f, 0.6f});
	const UInt32 dark = ShadedModelColour(NiPoint3{0.0f, -1.0f, 0.0f});
	Check((lit >> 24) == 0xFF && (dark >> 24) == 0xFF, "opaque");
	Check(((lit >> 16) & 0xFF) > ((dark >> 16) & 0xFF), "a face towards the light is brighter");
	Check(((dark >> 16) & 0xFF) >= 60, "a face away still reads");
}

}  // namespace

int main() {
	TestRigid();
	TestProjection();
	TestWorldProjection();
	TestShade();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

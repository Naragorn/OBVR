// Checks the mirror's reflection (vr/MirrorGlassLogic.h, docs/mirror-spec.md
// stage 2): a point, a direction and a camera's rotation reflected in a plane,
// the glass's plane from its four corners, the mirror camera and its off-axis
// frustum through the corners, the eye on or behind the glass and the glass
// seen edge-on, the glass in view, the picture's size, the render-this-frame
// decision with every refusal, the failure budget, the cost line's window and
// the stage trace.

#include <cmath>
#include <cstdio>

#include "vr/MirrorGlassLogic.h"

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

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

NiPoint3 P(float x, float y, float z) { return NiPoint3{x, y, z}; }

NiPoint3 Column(const NiMatrix33& m, int c) { return NiPoint3{m.data[0][c], m.data[1][c], m.data[2][c]}; }

NiMatrix33 FromColumns(const NiPoint3& a, const NiPoint3& b, const NiPoint3& c) {
	NiMatrix33 m{};
	const NiPoint3 cols[3] = {a, b, c};
	for (int col = 0; col < 3; ++col) {
		m.data[0][col] = cols[col].x;
		m.data[1][col] = cols[col].y;
		m.data[2][col] = cols[col].z;
	}
	return m;
}

bool Orthonormal(const NiMatrix33& m) {
	const NiPoint3 a = Column(m, 0);
	const NiPoint3 b = Column(m, 1);
	const NiPoint3 c = Column(m, 2);
	return Near(MirrorDot(a, a), 1.0f) && Near(MirrorDot(b, b), 1.0f) && Near(MirrorDot(c, c), 1.0f) && Near(MirrorDot(a, b), 0.0f) &&
	       Near(MirrorDot(a, c), 0.0f) && Near(MirrorDot(b, c), 0.0f);
}

// A glass of w by h units standing upright in the plane y = distance, its face
// towards the origin (a viewer there looking along +y sees TL on its left).
void UprightGlass(float w, float h, float distance, NiPoint3 (&c)[4], float centreX = 0.0f, float centreZ = 0.0f) {
	c[0] = P(centreX - 0.5f * w, distance, centreZ + 0.5f * h);  // top left
	c[1] = P(centreX + 0.5f * w, distance, centreZ + 0.5f * h);  // top right
	c[2] = P(centreX - 0.5f * w, distance, centreZ - 0.5f * h);  // bottom left
	c[3] = P(centreX + 0.5f * w, distance, centreZ - 0.5f * h);  // bottom right
}

void TestReflectPoint() {
	std::printf("A point reflected in a plane\n");
	const NiPoint3 n = P(0.0f, -1.0f, 0.0f);  // the plane y = 10, facing -y
	const NiPoint3 q = P(0.0f, 10.0f, 0.0f);
	Check(NearPoint(ReflectPoint(P(1.0f, 0.0f, 2.0f), q, n), P(1.0f, 20.0f, 2.0f)),
	      "a point 10 in front lands 10 behind, its other axes kept");
	Check(NearPoint(ReflectPoint(P(3.0f, 10.0f, -4.0f), q, n), P(3.0f, 10.0f, -4.0f)), "a point on the plane stays");
	Check(NearPoint(ReflectPoint(ReflectPoint(P(2.0f, 3.0f, 4.0f), q, n), q, n), P(2.0f, 3.0f, 4.0f)),
	      "reflected twice, the point comes back");
	// An oblique plane through (1, 1, 1) with the normal (1, 1, 0)/sqrt(2).
	const float s = 1.0f / std::sqrt(2.0f);
	const NiPoint3 oblique = P(s, s, 0.0f);
	const NiPoint3 r = ReflectPoint(P(3.0f, 1.0f, 5.0f), P(1.0f, 1.0f, 1.0f), oblique);
	Check(NearPoint(r, P(1.0f, -1.0f, 5.0f)), "an oblique plane: (3, 1, 5) lands at (1, -1, 5)");
}

void TestReflectDirection() {
	std::printf("A direction reflected in a plane\n");
	const NiPoint3 n = P(0.0f, -1.0f, 0.0f);
	Check(NearPoint(ReflectDirection(P(0.0f, 1.0f, 0.0f), n), P(0.0f, -1.0f, 0.0f)), "along the normal: turned round");
	Check(NearPoint(ReflectDirection(P(1.0f, 0.0f, 0.0f), n), P(1.0f, 0.0f, 0.0f)), "along the plane: kept");
	const NiPoint3 d = ReflectDirection(P(0.6f, 0.8f, 0.0f), n);
	Check(NearPoint(d, P(0.6f, -0.8f, 0.0f)) && Near(MirrorDot(d, d), 1.0f), "slanted: the normal's part turned, the length kept");
}

void TestReflectCameraRotation() {
	std::printf("A camera's rotation reflected in a plane, kept a rotation (the water camera's way)\n");
	// The NiCamera's columns: forward, up, right. Looking along +y, up +z,
	// right +x (forward x up = right).
	const NiMatrix33 looking = FromColumns(P(0.0f, 1.0f, 0.0f), P(0.0f, 0.0f, 1.0f), P(1.0f, 0.0f, 0.0f));
	Check(Near(Determinant(looking), 1.0f), "the camera before: a rotation");
	const NiPoint3 n = P(0.0f, -1.0f, 0.0f);
	const NiMatrix33 r = ReflectCameraRotation(looking, n);
	Check(NearPoint(Column(r, 0), P(0.0f, -1.0f, 0.0f)), "its forward is the reflected forward");
	Check(NearPoint(Column(r, 1), P(0.0f, 0.0f, 1.0f)), "its up is the reflected up");
	Check(NearPoint(Column(r, 2), P(-1.0f, 0.0f, 0.0f)),
	      "its right is not the reflected right but the other way (the picture is flipped where it is shown)");
	Check(Near(Determinant(r), 1.0f) && Orthonormal(r), "it stays a rotation: no mirrored axes, the triangles keep their winding");
	// A plain reflection of all three columns would be turned over.
	NiMatrix33 turnedOver = looking;
	for (int c = 0; c < 3; ++c) {
		const NiPoint3 col = ReflectDirection(Column(looking, c), n);
		turnedOver.data[0][c] = col.x;
		turnedOver.data[1][c] = col.y;
		turnedOver.data[2][c] = col.z;
	}
	Check(Near(Determinant(turnedOver), -1.0f), "reflecting all three columns would give a mirror image (determinant -1)");
	// A tilted camera and a tilted plane: still a rotation.
	const float s = 1.0f / std::sqrt(2.0f);
	const NiMatrix33 tilted = FromColumns(P(0.0f, s, -s), P(0.0f, s, s), P(1.0f, 0.0f, 0.0f));
	const NiMatrix33 rt = ReflectCameraRotation(tilted, P(0.0f, -s, -s));
	Check(Near(Determinant(rt), 1.0f) && Orthonormal(rt), "tilted camera, tilted plane: a rotation");
}

void TestGlassCornerInWorld() {
	std::printf("A glass corner from the holding controller's frame into the world\n");
	const NiMatrix33 identity = NiMatrix33::Identity();
	// OpenVR's axes: x right, y up, z back. The game's: x right, y forward, z up.
	const NiPoint3 w = GlassCornerInWorld(P(100.0f, 200.0f, 300.0f), identity, 70.0f, P(0.1f, 0.2f, -0.3f));
	Check(NearPoint(w, P(107.0f, 221.0f, 314.0f)), "x right stays, z back becomes forward, y up becomes up, metres to units");
	// The hand turned a quarter about up: its right is the world's forward.
	const NiMatrix33 quarter = FromColumns(P(0.0f, 1.0f, 0.0f), P(-1.0f, 0.0f, 0.0f), P(0.0f, 0.0f, 1.0f));
	const NiPoint3 t = GlassCornerInWorld(P(0.0f, 0.0f, 0.0f), quarter, 70.0f, P(1.0f, 0.0f, 0.0f));
	Check(NearPoint(t, P(0.0f, 70.0f, 0.0f)), "the hand's rotation carries the corner");
}

void TestGlassPlane() {
	std::printf("The glass's plane from its four corners\n");
	NiPoint3 c[4];
	UprightGlass(24.0f, 18.0f, 10.0f, c);
	const GlassPlane g = GlassPlaneOf(c);
	Check(g.valid, "an upright rectangle is a glass");
	Check(NearPoint(g.centre, P(0.0f, 10.0f, 0.0f)), "its centre");
	Check(NearPoint(g.right, P(1.0f, 0.0f, 0.0f)) && NearPoint(g.up, P(0.0f, 0.0f, 1.0f)), "its right (TL to TR) and up (BL to TL)");
	Check(NearPoint(g.normal, P(0.0f, -1.0f, 0.0f)), "its normal points to the side it is seen from (right x up)");
	Check(Near(g.width, 24.0f) && Near(g.height, 18.0f), "its width and height");
	NiPoint3 flat[4] = {c[0], c[0], c[2], c[3]};
	Check(!GlassPlaneOf(flat).valid, "two corners in one place: no glass");
	NiPoint3 thin[4];
	UprightGlass(24.0f, 0.1f, 10.0f, thin);
	Check(!GlassPlaneOf(thin).valid, "a glass thinner than half a unit: no glass");
	NiPoint3 skewed[4] = {c[0], c[1], c[2], P(20.0f, 10.0f, -9.0f)};
	Check(!GlassPlaneOf(skewed).valid, "a fourth corner far off the rectangle: no glass");
	NiPoint3 sheared[4] = {P(-12.0f, 10.0f, 9.0f), P(12.0f, 10.0f, 9.0f), P(-2.0f, 10.0f, -9.0f), P(22.0f, 10.0f, -9.0f)};
	Check(!GlassPlaneOf(sheared).valid, "sides not at right angles: no glass");
	NiPoint3 nan[4] = {P(NAN, 10.0f, 9.0f), c[1], c[2], c[3]};
	Check(!GlassPlaneOf(nan).valid, "a corner that is not a number: no glass");
}

void TestMirrorCameraCentred() {
	std::printf("The mirror camera for an eye straight in front of the glass\n");
	NiPoint3 c[4];
	UprightGlass(24.0f, 18.0f, 10.0f, c);
	const GlassPlane g = GlassPlaneOf(c);
	const MirrorCamera m = MirrorCameraFor(g, c, P(0.0f, 0.0f, 0.0f));
	Check(m.view == MirrorView::Ok, "seen from the front: a camera");
	Check(NearPoint(m.position, P(0.0f, 20.0f, 0.0f)), "the eye reflected: as far behind the glass as it is in front");
	Check(NearPoint(Column(m.rotation, 0), P(0.0f, -1.0f, 0.0f)), "it looks out of the glass, back at the eye's side");
	Check(NearPoint(Column(m.rotation, 1), P(0.0f, 0.0f, 1.0f)), "its up is the glass's up");
	Check(NearPoint(Column(m.rotation, 2), P(-1.0f, 0.0f, 0.0f)), "its right is the glass's left (the picture is flipped on the glass)");
	Check(Near(Determinant(m.rotation), 1.0f) && Orthonormal(m.rotation), "a rotation");
	Check(Near(m.left, -1.2f) && Near(m.right, 1.2f) && Near(m.top, 0.9f) && Near(m.bottom, -0.9f),
	      "the frustum through the corners: tangents half the glass over its distance");
	Check(Near(m.nearUnits, 10.0f), "the near plane is the glass");
	Check(Near(m.facing, 1.0f) && Near(m.eyeDistance, 10.0f), "facing straight on, 10 units away");
	// The same camera as the eye's own, looking straight at the glass, reflected.
	const NiMatrix33 eye = FromColumns(P(0.0f, 1.0f, 0.0f), P(0.0f, 0.0f, 1.0f), P(1.0f, 0.0f, 0.0f));
	const NiMatrix33 reflected = ReflectCameraRotation(eye, g.normal);
	bool same = true;
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			same = same && Near(reflected.data[row][col], m.rotation.data[row][col]);
		}
	}
	Check(same, "it is the reflection of an eye looking straight at the glass");
}

void TestMirrorCameraOffAxis() {
	std::printf("The mirror camera for an eye off to one side: an off-axis frustum\n");
	NiPoint3 c[4];
	UprightGlass(24.0f, 18.0f, 10.0f, c);
	const GlassPlane g = GlassPlaneOf(c);
	const MirrorCamera m = MirrorCameraFor(g, c, P(3.0f, 0.0f, 1.0f));
	Check(m.view == MirrorView::Ok, "a camera");
	Check(NearPoint(m.position, P(3.0f, 20.0f, 1.0f)), "the eye reflected straight through the glass's plane");
	// Right is -x: TL (x -12) is 15 to the camera's right, TR (x 12) 9 to its left.
	Check(Near(m.left, -0.9f) && Near(m.right, 1.5f), "across: from 9 to the left to 15 to the right, over 10");
	Check(Near(m.top, 0.8f) && Near(m.bottom, -1.0f), "up and down: from 8 above to 10 below, over 10");
	Check(m.left < m.right && m.bottom < m.top, "left below right, bottom below top");
	// Every corner on the frustum's edge: TL at the right and top, BR at the left and bottom.
	const NiPoint3 fwd = Column(m.rotation, 0);
	const NiPoint3 up = Column(m.rotation, 1);
	const NiPoint3 right = Column(m.rotation, 2);
	const NiPoint3 dTl = c[0] - m.position;
	const NiPoint3 dBr = c[3] - m.position;
	Check(Near(MirrorDot(dTl, right) / MirrorDot(dTl, fwd), m.right) && Near(MirrorDot(dTl, up) / MirrorDot(dTl, fwd), m.top),
	      "the top left corner is the picture's top right (the glass shows it flipped)");
	Check(Near(MirrorDot(dBr, right) / MirrorDot(dBr, fwd), m.left) && Near(MirrorDot(dBr, up) / MirrorDot(dBr, fwd), m.bottom),
	      "the bottom right corner is the picture's bottom left");
}

void TestMirrorCameraTilted() {
	std::printf("A glass held tilted and turned\n");
	// The upright glass turned 30 degrees about up and tilted back 20: the
	// corners rotated about the glass's centre.
	NiPoint3 c[4];
	UprightGlass(24.0f, 18.0f, 0.0f, c);
	const float a = 30.0f * 3.14159265f / 180.0f;
	const float b = 20.0f * 3.14159265f / 180.0f;
	for (NiPoint3& p : c) {
		const float x = p.x * std::cos(a) - p.y * std::sin(a);
		const float y = p.x * std::sin(a) + p.y * std::cos(a);
		const float y2 = y * std::cos(b) - p.z * std::sin(b);
		const float z2 = y * std::sin(b) + p.z * std::cos(b);
		p = P(x, y2 + 30.0f, z2 + 5.0f);
	}
	const GlassPlane g = GlassPlaneOf(c);
	Check(g.valid, "still a glass");
	const NiPoint3 eye = P(4.0f, 0.0f, 8.0f);
	const MirrorCamera m = MirrorCameraFor(g, c, eye);
	Check(m.view == MirrorView::Ok, "a camera");
	Check(Near(Determinant(m.rotation), 1.0f) && Orthonormal(m.rotation), "a rotation");
	Check(NearPoint(ReflectPoint(eye, g.centre, g.normal), m.position), "at the eye reflected in the glass's plane");
	bool onEdges = true;
	for (const NiPoint3& corner : c) {
		const NiPoint3 d = corner - m.position;
		const float z = MirrorDot(d, Column(m.rotation, 0));
		const float x = MirrorDot(d, Column(m.rotation, 2)) / z;
		const float y = MirrorDot(d, Column(m.rotation, 1)) / z;
		onEdges = onEdges && (Near(x, m.left) || Near(x, m.right)) && (Near(y, m.top) || Near(y, m.bottom));
		onEdges = onEdges && Near(z, m.nearUnits, 0.01f);
	}
	Check(onEdges, "every corner on the frustum's edges, every one at the near plane");
}

void TestMirrorCameraRefusals() {
	std::printf("When there is no mirror camera\n");
	NiPoint3 c[4];
	UprightGlass(24.0f, 18.0f, 10.0f, c);
	const GlassPlane g = GlassPlaneOf(c);
	Check(MirrorCameraFor(g, c, P(0.0f, 10.0f, 0.0f)).view == MirrorView::EyeBehind, "the eye in the glass's plane");
	Check(MirrorCameraFor(g, c, P(0.0f, 9.5f, 0.0f)).view == MirrorView::EyeBehind,
	      "the eye nearer the plane than a unit");
	Check(MirrorCameraFor(g, c, P(0.0f, 30.0f, 0.0f)).view == MirrorView::EyeBehind, "the eye behind the glass");
	Check(MirrorCameraFor(g, c, P(200.0f, 5.0f, 0.0f)).view == MirrorView::EdgeOn,
	      "the eye far to the side, the glass nearly edge-on");
	Check(MirrorCameraFor(g, c, P(40.0f, 0.0f, 0.0f)).view == MirrorView::Ok, "at a slant but not edge-on: a camera");
	NiPoint3 flat[4] = {c[0], c[0], c[2], c[3]};
	Check(MirrorCameraFor(GlassPlaneOf(flat), flat, P(0.0f, 0.0f, 0.0f)).view == MirrorView::NoGlass, "no glass");
	Check(MirrorCameraFor(g, c, P(NAN, 0.0f, 0.0f)).view != MirrorView::Ok, "an eye that is not a number");
}

GlassOnScreen Seen(float minX, float minY, float maxX, float maxY) {
	GlassOnScreen s;
	s.drawn = true;
	s.minX = minX;
	s.minY = minY;
	s.maxX = maxX;
	s.maxY = maxY;
	s.viewportX = 0.0f;
	s.viewportY = 0.0f;
	s.viewportWidth = 4036.0f;
	s.viewportHeight = 3376.0f;
	return s;
}

void TestInView() {
	std::printf("The glass in an eye's view\n");
	GlassOnScreen none;
	Check(!GlassInView(none), "not drawn into the eye: not in view");
	Check(GlassInView(Seen(1000.0f, 1000.0f, 1800.0f, 1600.0f)), "inside the eye's view");
	Check(GlassInView(Seen(-300.0f, 1000.0f, 200.0f, 1600.0f)), "half over the left edge: in view");
	Check(!GlassInView(Seen(-900.0f, 1000.0f, -10.0f, 1600.0f)), "wholly left of the view");
	Check(!GlassInView(Seen(4100.0f, 1000.0f, 4600.0f, 1600.0f)), "wholly right of the view");
	Check(!GlassInView(Seen(1000.0f, 3400.0f, 1800.0f, 3900.0f)), "wholly below the view");
	GlassOnScreen rightHalf = Seen(5000.0f, 1000.0f, 5600.0f, 1600.0f);
	rightHalf.viewportX = 4036.0f;
	Check(GlassInView(rightHalf), "the right eye's half of a side-by-side frame");
	GlassOnScreen eyes[2] = {Seen(1000.0f, 1000.0f, 1800.0f, 1600.0f), Seen(1100.0f, 900.0f, 2000.0f, 1500.0f)};
	Check(Near(GlassLongSidePixels(eyes), 900.0f), "the longest side the glass takes in either eye");
	eyes[1].drawn = false;
	Check(Near(GlassLongSidePixels(eyes), 800.0f), "one eye: its own");
	eyes[0].drawn = false;
	Check(Near(GlassLongSidePixels(eyes), 0.0f), "no eye: nothing");
	GlassOnScreen two[2] = {Seen(0, 0, 1, 1), Seen(0, 0, 1, 1)};
	two[0].eye = P(0.0f, 0.0f, 0.0f);
	two[1].eye = P(4.4f, 0.0f, 0.0f);
	NiPoint3 mid{};
	Check(CyclopeanEye(two, mid) && NearPoint(mid, P(2.2f, 0.0f, 0.0f)), "between the eyes: the middle of the two");
	two[1].drawn = false;
	Check(CyclopeanEye(two, mid) && NearPoint(mid, P(0.0f, 0.0f, 0.0f)), "one eye drawn: that eye");
	two[0].drawn = false;
	Check(!CyclopeanEye(two, mid), "no eye drawn: none");
}

void TestPictureSize() {
	std::printf("The picture's size\n");
	Check(ClampMirrorPixels(1024.0f) == 1024, "MirrorPixels 1024 as it is");
	Check(ClampMirrorPixels(100.0f) == 256 && ClampMirrorPixels(5000.0f) == 2048, "clamped to 256..2048");
	Check(ClampMirrorPixels(NAN) == kMirrorPixelsDefault, "not a number: the default");
	MirrorPictureSize s = MirrorPictureSizeFor(700.0f, 4.0f / 3.0f, 1024, 8072, 3376);
	Check(s.width == 704 && s.height == 528, "a glass 700 pixels across at 4:3: 704 by 528 (rounded up to 8)");
	s = MirrorPictureSizeFor(1900.0f, 4.0f / 3.0f, 1024, 8072, 3376);
	Check(s.width == 1024 && s.height == 768, "a near glass: capped by MirrorPixels");
	s = MirrorPictureSizeFor(20.0f, 4.0f / 3.0f, 1024, 8072, 3376);
	Check(s.width == 64 && s.height == 48, "a far glass: no less than 64 across");
	s = MirrorPictureSizeFor(600.0f, 0.75f, 1024, 8072, 3376);
	Check(s.width == 450 && s.height == 600, "a glass standing tall: its height the long side");
	s = MirrorPictureSizeFor(600.0f, NAN, 1024, 8072, 3376);
	Check(s.width == 600 && s.height == 600, "no shape known: square");
	s = MirrorPictureSizeFor(2000.0f, 2.0f, 2048, 1600, 900);
	Check(s.width == 1600 && s.height == 800, "a frame smaller than the picture: scaled down to fit, the shape kept");
	s = MirrorPictureSizeFor(600.0f, 1.0f, 1024, 0, 0);
	Check(s.width == 0 && s.height == 0, "no frame: no picture");
	const MirrorPort port = MirrorPortFor(MirrorPictureSize{1024, 768}, 8072, 3376);
	Check(Near(port.left, 0.0f) && Near(port.right, 1024.0f / 8072.0f) && Near(port.top, 1.0f) &&
	          Near(port.bottom, 1.0f - 768.0f / 3376.0f),
	      "the world drawn into the frame's top left corner, the picture's size");
	float uv[4] = {};
	MirrorGlassUvFor(MirrorPictureSize{704, 528}, 1024, uv);
	Check(Near(uv[0], 0.0f) && Near(uv[1], 0.0f) && Near(uv[2], 704.0f / 1024.0f) && Near(uv[3], 528.0f / 1024.0f),
	      "the glass shows the part of the texture the picture fills");
}

MirrorPassInputs AllGood() {
	MirrorPassInputs in;
	in.settingOn = true;
	in.loading = false;
	in.eyesCaptured = true;
	in.stopped = false;
	in.view = MirrorView::Ok;
	in.inView = true;
	return in;
}

void TestDecision() {
	std::printf("Whether the mirror renders this frame\n");
	Check(DecideMirrorPass(AllGood()) == MirrorSkip::None, "everything right: it renders");
	MirrorPassInputs in = AllGood();
	in.settingOn = false;
	Check(DecideMirrorPass(in) == MirrorSkip::Off, "MirrorWorld=0: the dark glass");
	in = AllGood();
	in.stopped = true;
	Check(DecideMirrorPass(in) == MirrorSkip::Stopped, "switched off for the session");
	in = AllGood();
	in.loading = true;
	Check(DecideMirrorPass(in) == MirrorSkip::Loading, "while loading");
	in = AllGood();
	in.eyesCaptured = false;
	Check(DecideMirrorPass(in) == MirrorSkip::NotCaptured, "both eyes not captured this frame");
	in = AllGood();
	in.view = MirrorView::NoGlass;
	Check(DecideMirrorPass(in) == MirrorSkip::NoGlass, "no glass");
	in.view = MirrorView::EyeBehind;
	Check(DecideMirrorPass(in) == MirrorSkip::FacingAway, "the eye behind the glass: facing away");
	in.view = MirrorView::EdgeOn;
	Check(DecideMirrorPass(in) == MirrorSkip::EdgeOn, "edge-on");
	in = AllGood();
	in.inView = false;
	Check(DecideMirrorPass(in) == MirrorSkip::OutOfView, "out of both eyes' view");
	in.settingOn = false;
	in.stopped = true;
	in.loading = true;
	in.eyesCaptured = false;
	in.view = MirrorView::EdgeOn;
	Check(DecideMirrorPass(in) == MirrorSkip::Off, "the setting first");
	in.settingOn = true;
	Check(DecideMirrorPass(in) == MirrorSkip::Stopped, "then the session's switch");
	in.stopped = false;
	Check(DecideMirrorPass(in) == MirrorSkip::Loading, "then the load");
	in.loading = false;
	Check(DecideMirrorPass(in) == MirrorSkip::NotCaptured, "then the captures");
	in.eyesCaptured = true;
	Check(DecideMirrorPass(in) == MirrorSkip::EdgeOn, "then the glass, before the view");
	Check(MirrorSkipName(MirrorSkip::OutOfView) != nullptr && MirrorSkipName(MirrorSkip::None) != nullptr,
	      "every answer has a name for the log");
	Check(MirrorWantsBoneCapture(true, true, false), "a single-pass frame keeps the eye's bones while the mirror is up");
	Check(!MirrorWantsBoneCapture(false, true, false) && !MirrorWantsBoneCapture(true, false, false) &&
	          !MirrorWantsBoneCapture(true, true, true),
	      "but not with the page down, the setting off or the pass switched off");
}

void TestFailureBudget() {
	std::printf("The failure budget\n");
	MirrorFailures f;
	Check(!NoteMirrorOutcome(f, false) && !NoteMirrorOutcome(f, false) && !f.stopped, "two failures in a row: still on");
	Check(!NoteMirrorOutcome(f, true) && f.inARow == 0, "a success: the run of failures forgotten");
	NoteMirrorOutcome(f, false);
	NoteMirrorOutcome(f, false);
	Check(NoteMirrorOutcome(f, false) && f.stopped, "three in a row: switched off, said once");
	Check(!NoteMirrorOutcome(f, false) && f.stopped, "a fourth: no second line");
	Check(!NoteMirrorOutcome(f, true) && f.stopped, "a success after: still off for the session");
	MirrorFailures g;
	Check(StopMirror(g) && g.stopped, "a lost device or a failed create: off at once, said once");
	Check(!StopMirror(g), "stopped twice: said once");
}

void TestCostWindow() {
	std::printf("The cost line's window\n");
	MirrorCostWindow w;
	bool full = false;
	for (int i = 0; i < 89; ++i) {
		const bool rendered = i % 10 != 0;
		full = NoteMirrorFrame(w, rendered ? MirrorSkip::None : MirrorSkip::OutOfView, rendered ? 1.0 + (i % 3) : 0.0,
		                       rendered ? 400u : 0u, MirrorPictureSize{704, 528}) ||
		       full;
	}
	Check(!full && w.frames == 89, "89 mirror frames: not yet");
	Check(NoteMirrorFrame(w, MirrorSkip::EdgeOn, 0.0, 0, MirrorPictureSize{}), "the 90th: the line is due");
	Check(w.passes == 80 && w.skipped[static_cast<int>(MirrorSkip::OutOfView)] == 9 &&
	          w.skipped[static_cast<int>(MirrorSkip::EdgeOn)] == 1 && SkippedTotal(w) == 10,
	      "80 passes, 10 skipped by reason");
	Check(Near(static_cast<float>(MeanMs(w)), 2.0f, 0.05f) && Near(static_cast<float>(w.worstMs), 3.0f),
	      "the mean and the worst pass");
	Check(w.passes > 0 && w.draws / w.passes == 400 && w.width == 704 && w.height == 528,
	      "the draws a pass and the last size");
	ResetMirrorCost(w);
	Check(w.frames == 0 && w.passes == 0 && SkippedTotal(w) == 0 && w.worstMs == 0.0, "reset for the next window");
	Check(MeanMs(w) == 0.0, "no passes: a mean of 0");
}

void TestCostWindowHead() {
	std::printf("The cost line's head\n");
	MirrorCostWindow w;
	NoteMirrorHead(w, true, 0.02);
	NoteMirrorHead(w, true, 0.04);
	NoteMirrorHead(w, false, 0.01);
	Check(w.headShown == 2 && w.headAsked == 3, "shown in 2 of 3 passes");
	Check(Near(static_cast<float>(MeanHeadMs(w)), 0.0233f, 0.0005f), "the switch's mean time over the passes asked");
	ResetMirrorCost(w);
	Check(w.headShown == 0 && w.headAsked == 0 && MeanHeadMs(w) == 0.0, "reset, and no pass: a mean of 0");
}

void TestTrace() {
	std::printf("The stage trace\n");
	MirrorTrace t;
	Check(t.On(), "on for the first mirror frame");
	t.FrameDone();
	t.FrameDone();
	Check(t.On(), "still on for the third");
	t.FrameDone();
	Check(!t.On(), "off after three");
	t.FrameDone();
	Check(!t.On() && t.framesLeft == 0, "and stays off");
}

}  // namespace

int main() {
	TestReflectPoint();
	TestReflectDirection();
	TestReflectCameraRotation();
	TestGlassCornerInWorld();
	TestGlassPlane();
	TestMirrorCameraCentred();
	TestMirrorCameraOffAxis();
	TestMirrorCameraTilted();
	TestMirrorCameraRefusals();
	TestInView();
	TestPictureSize();
	TestDecision();
	TestFailureBudget();
	TestCostWindow();
	TestCostWindowHead();
	TestTrace();
	if (g_failures > 0) {
		std::printf("%d failing check(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

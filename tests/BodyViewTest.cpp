// Checks what the eyes see of the player's own body (game/BodyViewLogic.h):
// the skinning the draw does, a shape's winding told from its normals, a
// triangle's bone, and the rays - the body from outside, its inside through
// an opening, through the near plane's cut and with the eye inside it, the
// margin below the view, two-sided surfaces, limbs against the trunk, a
// front and a back at one place - and the verdict over both eyes.

#include <cstdio>
#include <vector>

#include "game/BodyViewLogic.h"

using obvr::NiMatrix33;
using obvr::NiPoint3;
using obvr::NiTransform;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float eps = 0.001f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

NiTransform Identity() {
	NiTransform t{};
	t.rot = NiMatrix33::Identity();
	t.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	t.scale = 1.0f;
	return t;
}

// A quarter turn about z: x onto y.
NiMatrix33 QuarterTurnAboutZ() {
	NiMatrix33 m{};
	m.data[0][1] = -1.0f;
	m.data[1][0] = 1.0f;
	m.data[2][2] = 1.0f;
	return m;
}

NiPoint3 Cross(const NiPoint3& u, const NiPoint3& v) {
	return NiPoint3{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
}

float Dot(const NiPoint3& u, const NiPoint3& v) { return u.x * v.x + u.y * v.y + u.z * v.z; }

// A quad's two triangles, wound so that (b - a) x (c - a) points along
// `out` whichever way round its corners were given.
void AddQuad(std::vector<ViewTriangle>& tris, NiPoint3 p0, NiPoint3 p1, NiPoint3 p2, NiPoint3 p3, NiPoint3 out,
             bool trunk, bool twoSided, UInt8 shape) {
	if (Dot(Cross(p1 - p0, p2 - p0), out) < 0.0f) {
		const NiPoint3 swap = p1;
		p1 = p3;
		p3 = swap;
	}
	ViewTriangle t;
	t.trunk = trunk;
	t.twoSided = twoSided;
	t.shape = shape;
	t.a = p0;
	t.b = p1;
	t.c = p2;
	tris.push_back(t);
	t.a = p0;
	t.b = p2;
	t.c = p3;
	tris.push_back(t);
}

struct BoxSpec {
	NiPoint3 low;
	NiPoint3 high;
	bool openTop = false;  // no face at the top (+z): an opening, as the collar is
	bool trunk = true;
	bool twoSided = false;
	UInt8 shape = 0;
};

// A box wound outward, as the body's shells are.
std::vector<ViewTriangle> Box(const BoxSpec& s) {
	std::vector<ViewTriangle> tris;
	const float x0 = s.low.x, y0 = s.low.y, z0 = s.low.z;
	const float x1 = s.high.x, y1 = s.high.y, z1 = s.high.z;
	if (!s.openTop) {
		AddQuad(tris, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1}, s.trunk, s.twoSided, s.shape);
	}
	AddQuad(tris, {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {0, 0, -1}, s.trunk, s.twoSided, s.shape);
	AddQuad(tris, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {1, 0, 0}, s.trunk, s.twoSided, s.shape);
	AddQuad(tris, {x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}, {-1, 0, 0}, s.trunk, s.twoSided, s.shape);
	AddQuad(tris, {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}, {0, 1, 0}, s.trunk, s.twoSided, s.shape);
	AddQuad(tris, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0}, s.trunk, s.twoSided, s.shape);
	return tris;
}

// A small grid, so counts can be worked out by hand: 20 columns of 0.1 and
// 16 rows of 0.125 across a square view of tangent one each way (45 degrees),
// four margin rows of five degrees below it.
ViewGrid SmallGrid() {
	ViewGrid g;
	g.columns = 20;
	g.rows = 16;
	g.marginRows = 4;
	g.marginDegrees = 20.0f;
	return g;
}

// The eye at the origin looking along +y, up +z.
ViewEye Ahead() { return ViewEye{}; }

// The eye at the origin looking straight down, its up along +y.
ViewEye Down() {
	ViewEye e;
	e.forward = NiPoint3{0.0f, 0.0f, -1.0f};
	e.up = NiPoint3{0.0f, 1.0f, 0.0f};
	e.right = NiPoint3{1.0f, 0.0f, 0.0f};
	return e;
}

ViewTally Cast(const ViewEye& eye, const std::vector<ViewTriangle>& tris, UInt8* cells = nullptr) {
	return CastBodyView(eye, SmallGrid(), tris.data(), static_cast<UInt32>(tris.size()), cells);
}

void TestTransformPoint() {
	std::printf("A point carried by a transform\n");
	NiTransform t = Identity();
	t.scale = 2.0f;
	t.rot = QuarterTurnAboutZ();
	t.pos = NiPoint3{1.0f, 2.0f, 3.0f};
	Check(NearPoint(TransformPoint(t, NiPoint3{1.0f, 0.0f, 0.0f}), NiPoint3{1.0f, 4.0f, 3.0f}),
	      "scaled, then turned, then moved");
}

void TestSkinning() {
	std::printf("The vertices skinned as the draw places them\n");
	const NiPoint3 bind[3] = {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}};
	NiTransform world[2] = {Identity(), Identity()};
	NiTransform toBone[2] = {Identity(), Identity()};
	// Bone 1 stands 100 up in the bind pose; its skin transform takes that off.
	world[1].pos = NiPoint3{0.0f, 0.0f, 100.0f};
	toBone[1].pos = NiPoint3{0.0f, 0.0f, -100.0f};
	const SkinWeight weights[] = {{0, 0, 1.0f}, {1, 0, 0.5f}, {1, 1, 0.5f}, {2, 1, 1.0f}};
	NiPoint3 out[3];
	float sum[3];
	SkinVertices(bind, 3, world, toBone, 2, weights, 4, out, sum);
	Check(NearPoint(out[0], bind[0]) && NearPoint(out[1], bind[1]) && NearPoint(out[2], bind[2]),
	      "in the bind pose every vertex stays where it was bound");
	Check(Near(sum[0], 1.0f) && Near(sum[1], 1.0f) && Near(sum[2], 1.0f) && SkinWeightMisses(sum, 3) == 0,
	      "every vertex weighs one");

	world[1].pos = NiPoint3{0.0f, 0.0f, 120.0f};
	SkinVertices(bind, 3, world, toBone, 2, weights, 4, out, sum);
	Check(NearPoint(out[0], NiPoint3{0.0f, 0.0f, 0.0f}), "bone 1 raised 20: a vertex on bone 0 alone stays");
	Check(NearPoint(out[1], NiPoint3{10.0f, 0.0f, 10.0f}), "a vertex shared half and half goes half the way");
	Check(NearPoint(out[2], NiPoint3{0.0f, 0.0f, 30.0f}), "a vertex on bone 1 alone goes all the way");

	world[1].scale = 0.0f;
	SkinVertices(bind, 3, world, toBone, 2, weights, 4, out, sum);
	Check(NearPoint(out[2], NiPoint3{0.0f, 0.0f, 120.0f}),
	      "bone 1 collapsed to nothing (the head): its vertex lands on the bone's origin");
	Check(NearPoint(out[1], NiPoint3{5.0f, 0.0f, 60.0f}), "a shared vertex goes its share of the way there");

	const SkinWeight misread[] = {{0, 0, 1.0f}, {7, 0, 1.0f}, {1, 5, 1.0f}, {1, 0, 0.4f}, {2, 1, 1.0f}};
	SkinVertices(bind, 3, world, toBone, 2, misread, 5, out, sum);
	Check(Near(sum[1], 0.4f) && SkinWeightMisses(sum, 3) == 1,
	      "weights naming no vertex or no bone are left out; a vertex short of one is a miss");
	const float edge[3] = {1.0f + kSkinWeightSlack * 0.5f, 1.0f - kSkinWeightSlack * 2.0f, 0.0f};
	Check(SkinWeightMisses(edge, 3) == 2, "within the slack is one; past it, and no weight at all, are misses");
}

void TestBones() {
	std::printf("The bone carrying a vertex and a triangle\n");
	const SkinWeight weights[] = {{0, 2, 0.3f}, {0, 4, 0.7f}, {1, 2, 0.6f}, {1, 3, 0.4f}};
	UInt16 bone[3];
	float weight[3];
	DominantBones(weights, 4, 3, bone, weight);
	Check(bone[0] == 4 && Near(weight[0], 0.7f) && bone[1] == 2 && Near(weight[1], 0.6f),
	      "each vertex's heaviest bone");
	Check(bone[2] == kNoBone && Near(weight[2], 0.0f), "a vertex no weight names has none");
	Check(TriangleBone(5, 5, 9, 0.9f, 0.9f, 1.0f) == 5 && TriangleBone(9, 5, 9, 0.5f, 0.9f, 0.5f) == 9 &&
	          TriangleBone(9, 5, 5, 1.0f, 0.5f, 0.5f) == 5,
	      "two corners sharing a bone carry the triangle, wherever they sit");
	Check(TriangleBone(1, 2, 3, 0.9f, 0.5f, 0.5f) == 1 && TriangleBone(1, 2, 3, 0.5f, 0.9f, 0.5f) == 2 &&
	          TriangleBone(1, 2, 3, 0.5f, 0.5f, 0.9f) == 3,
	      "three different bones: the corner that carries the most");
}

void TestWinding() {
	std::printf("A shape's winding told from its normals\n");
	BoxSpec spec;
	spec.low = NiPoint3{-1.0f, -1.0f, -1.0f};
	spec.high = NiPoint3{1.0f, 1.0f, 1.0f};
	const std::vector<ViewTriangle> box = Box(spec);
	// Each triangle as its own three vertices, its normals the face's outward one.
	std::vector<NiPoint3> vertices;
	std::vector<NiPoint3> normals;
	std::vector<UInt16> indices;
	for (const ViewTriangle& t : box) {
		NiPoint3 n = Cross(t.b - t.a, t.c - t.a);
		const UInt16 base = static_cast<UInt16>(vertices.size());
		vertices.push_back(t.a);
		vertices.push_back(t.b);
		vertices.push_back(t.c);
		for (int k = 0; k < 3; ++k) {
			normals.push_back(n);
		}
		indices.push_back(base);
		indices.push_back(static_cast<UInt16>(base + 1));
		indices.push_back(static_cast<UInt16>(base + 2));
	}
	const UInt32 vertexCount = static_cast<UInt32>(vertices.size());
	const UInt32 triangleCount = static_cast<UInt32>(box.size());
	UInt32 agree = 0;
	UInt32 against = 0;
	Check(OutwardWinding(vertices.data(), normals.data(), vertexCount, indices.data(), triangleCount, agree, against) ==
	              1 &&
	          agree == 12 && against == 0,
	      "wound along its normals: +1, all twelve agree");
	std::vector<UInt16> flipped = indices;
	for (UInt32 t = 0; t < triangleCount; ++t) {
		const UInt16 swap = flipped[t * 3 + 1];
		flipped[t * 3 + 1] = flipped[t * 3 + 2];
		flipped[t * 3 + 2] = swap;
	}
	Check(OutwardWinding(vertices.data(), normals.data(), vertexCount, flipped.data(), triangleCount, agree, against) ==
	              -1 &&
	          against == 12,
	      "wound against them: -1");
	std::vector<UInt16> half = indices;
	for (UInt32 t = 0; t < 6; ++t) {
		half[t * 3 + 1] = flipped[t * 3 + 1];
		half[t * 3 + 2] = flipped[t * 3 + 2];
	}
	Check(OutwardWinding(vertices.data(), normals.data(), vertexCount, half.data(), triangleCount, agree, against) == 0 &&
	          agree == 6 && against == 6,
	      "as many each way: cannot be told");
	Check(OutwardWinding(vertices.data(), nullptr, vertexCount, indices.data(), triangleCount, agree, against) == 0 &&
	          agree == 0 && against == 0,
	      "no normals: cannot be told");
	std::vector<UInt16> outOfRange = indices;
	outOfRange[0] = 500;
	OutwardWinding(vertices.data(), normals.data(), vertexCount, outOfRange.data(), triangleCount, agree, against);
	Check(agree == 11, "a triangle naming a vertex past the count is left out");
}

void TestRayHit() {
	std::printf("One ray against one triangle\n");
	const NiPoint3 a{-5.0f, 10.0f, -5.0f};
	const NiPoint3 b{5.0f, 10.0f, -5.0f};
	const NiPoint3 c{0.0f, 10.0f, 5.0f};
	Check(Near(ViewRayHit(a, b, c, 0.0f, 0.0f), 10.0f), "straight ahead through it: its distance along the view");
	Check(Near(ViewRayHit(a, b, c, 0.2f, -0.2f), 10.0f), "aslant through it: still its distance along the view");
	Check(ViewRayHit(a, b, c, 0.9f, 0.0f) < 0.0f && ViewRayHit(a, b, c, 0.0f, 0.9f) < 0.0f,
	      "past its edges: no hit");
	const NiPoint3 a2{-5.0f, 10.0f, 0.0f};
	const NiPoint3 b2{5.0f, 10.0f, 0.0f};
	const NiPoint3 c2{0.0f, 20.0f, 0.0f};
	Check(ViewRayHit(a2, b2, c2, 0.0f, 0.0f) < 0.0f, "along its own plane: no hit");
	const NiPoint3 behind[3] = {{-5.0f, -10.0f, -5.0f}, {5.0f, -10.0f, -5.0f}, {0.0f, -10.0f, 5.0f}};
	Check(ViewRayHit(behind[0], behind[1], behind[2], 0.0f, 0.0f) < 0.0f, "behind the eye: no hit ahead");
}

void TestRows() {
	std::printf("The grid's rays\n");
	const ViewEye eye = Ahead();
	const ViewGrid g = SmallGrid();
	Check(Near(ViewRowTangent(eye, g, 0), 0.9375f) && Near(ViewRowTangent(eye, g, 15), -0.9375f),
	      "the view's rows run from the top edge to the bottom, at their middles");
	Check(Near(ViewRowTangent(eye, g, 16), -obvr::math::Tan(47.5f * obvr::math::kDegreesToRadians)) &&
	          Near(ViewRowTangent(eye, g, 19), -obvr::math::Tan(62.5f * obvr::math::kDegreesToRadians)),
	      "the margin's rows: five degrees each below the bottom edge's 45");
	ViewGrid steep = g;
	steep.marginDegrees = 80.0f;
	Check(Near(ViewRowTangent(eye, steep, 19), -obvr::math::Tan(89.0f * obvr::math::kDegreesToRadians)),
	      "a margin reaching past straight down stops at 89 degrees");
	ViewGrid none = g;
	none.marginRows = 0;
	Check(Near(ViewRowTangent(eye, none, 16), -obvr::math::Tan(55.0f * obvr::math::kDegreesToRadians)),
	      "no margin rows: a row past the view is read as one margin row as wide as the whole margin");
	Check(Near(ViewColumnTangent(eye, g, 0), -0.95f) && Near(ViewColumnTangent(eye, g, 19), 0.95f),
	      "the columns from the left edge to the right, at their middles");
	ViewGrid empty = g;
	empty.columns = 0;
	const ViewTally nothing = CastBodyView(eye, empty, nullptr, 0, nullptr);
	Check(nothing.rays == 0, "a grid with no columns casts nothing");
}

void TestCast() {
	std::printf("The rays: the body from outside and from inside\n");
	const ViewEye ahead = Ahead();
	const std::vector<ViewTriangle> none;
	ViewTally t = Cast(ahead, none);
	Check(t.rays == 20 * 16 && t.seen[kViewSeenNothing] == t.rays && t.trunkInsideBelowDegrees < 0.0f,
	      "nothing in the way: every ray of the view meets nothing, the margin's are not counted");

	BoxSpec closed;
	closed.low = NiPoint3{-5.0f, 30.0f, -5.0f};
	closed.high = NiPoint3{5.0f, 40.0f, 5.0f};
	std::vector<UInt8> cells(20 * 20, 0xEE);
	t = Cast(ahead, Box(closed), cells.data());
	Check(t.seen[kViewSeenTrunkOutside] == 8 && t.seen[kViewSeenTrunkInside] == 0 &&
	          t.seen[kViewSeenLimbOutside] == 0,
	      "a closed box ahead: its front face from outside - four columns by two rows of rays");
	Check(cells[7 * 20 + 9] == kViewSeenTrunkOutside && cells[0] == kViewSeenNothing &&
	          cells[19 * 20 + 19] == kViewSeenNothing,
	      "the cells, from the top row and the left: the middle meets the box, the corners nothing");

	closed.trunk = false;
	t = Cast(ahead, Box(closed));
	Check(t.seen[kViewSeenLimbOutside] == 8 && t.seen[kViewSeenTrunkOutside] == 0, "the same box as a limb");

	BoxSpec around;
	around.low = NiPoint3{-50.0f, -50.0f, -50.0f};
	around.high = NiPoint3{50.0f, 50.0f, 50.0f};
	around.shape = 3;
	t = Cast(ahead, Box(around));
	Check(t.seen[kViewSeenTrunkInside] == t.rays && t.trunkInsideOf[3] == t.rays,
	      "the eye inside the trunk: every ray meets its inside, all of it the shape's");
	around.trunk = false;
	t = Cast(ahead, Box(around));
	Check(t.seen[kViewSeenLimbInside] == t.rays && t.seen[kViewSeenTrunkInside] == 0, "inside a limb: the limb's");

	// The collar: a box open at the top, the eye 30 above its opening, looking
	// down - the rays through the opening meet its inside.
	BoxSpec collar;
	collar.low = NiPoint3{-20.0f, -20.0f, -60.0f};
	collar.high = NiPoint3{20.0f, 20.0f, -30.0f};
	collar.openTop = true;
	const ViewEye down = Down();
	t = Cast(down, Box(collar));
	Check(t.seen[kViewSeenTrunkInside] > 0 && t.seen[kViewSeenTrunkOutside] == 0,
	      "looking down into an opening: its inside, and nothing of its outside");
	Check(BodyViewVerdict(t, t) == ViewVerdict::IntoTheBody, "that is a view into the body");

	collar.twoSided = true;
	t = Cast(down, Box(collar));
	Check(t.seen[kViewSeenTrunkInside] == 0 && t.seen[kViewSeenTrunkOutside] > 0,
	      "drawn from both sides (a cloth): its back is a surface, not an inside");
	collar.twoSided = false;

	// The same opening below the view: the eye looking ahead, level.
	t = Cast(ahead, Box(collar));
	Check(t.seen[kViewSeenTrunkInside] == 0, "the opening below the view's bottom edge: no inside in the view");
	Check(Near(t.trunkInsideBelowDegrees, 12.5f),
	      "the margin finds it: the first row through the opening (56.3 degrees down, past 45) is 12.5 below the edge");
}

void TestNearPlane() {
	std::printf("The near plane\n");
	const ViewEye ahead = Ahead();
	BoxSpec cut;
	cut.low = NiPoint3{-5.0f, 5.0f, -5.0f};
	cut.high = NiPoint3{5.0f, 40.0f, 5.0f};
	ViewTally t = Cast(ahead, Box(cut));
	Check(t.seen[kViewSeenTrunkInside] > 0 && t.seen[kViewSeenTrunkOutside] == 0,
	      "a box reaching nearer than the near plane: its front cut away, its inside shows");
	BoxSpec near;
	near.low = NiPoint3{-5.0f, 2.0f, -5.0f};
	near.high = NiPoint3{5.0f, 8.0f, 5.0f};
	t = Cast(ahead, Box(near));
	Check(t.seen[kViewSeenNothing] == t.rays, "a box wholly nearer than the near plane: not seen at all");
	BoxSpec behind;
	behind.low = NiPoint3{-5.0f, -40.0f, -5.0f};
	behind.high = NiPoint3{5.0f, -30.0f, 5.0f};
	t = Cast(ahead, Box(behind));
	Check(t.seen[kViewSeenNothing] == t.rays, "a box behind the eye: not seen");
}

void TestOddTriangles() {
	std::printf("Triangles across the eye's plane, collapsed ones, one place met twice\n");
	const ViewEye ahead = Ahead();
	// A floor 20 below the eye reaching behind it, its outside up.
	std::vector<ViewTriangle> floor(1);
	floor[0].a = NiPoint3{-100.0f, -50.0f, -20.0f};
	floor[0].b = NiPoint3{100.0f, -50.0f, -20.0f};
	floor[0].c = NiPoint3{0.0f, 200.0f, -20.0f};
	floor[0].trunk = true;
	if (Dot(Cross(floor[0].b - floor[0].a, floor[0].c - floor[0].a), NiPoint3{0.0f, 0.0f, 1.0f}) < 0.0f) {
		const NiPoint3 swap = floor[0].b;
		floor[0].b = floor[0].c;
		floor[0].c = swap;
	}
	std::vector<UInt8> cells(20 * 20, 0xEE);
	ViewTally t = Cast(ahead, floor, cells.data());
	// Row 12 looks 0.5625 down: the floor at 35.6 ahead, inside the triangle.
	Check(cells[12 * 20 + 10] == kViewSeenTrunkOutside && t.seen[kViewSeenTrunkInside] == 0,
	      "a triangle reaching behind the eye is still met where it lies ahead - from above, its outside");
	Check(cells[2 * 20 + 10] == kViewSeenNothing, "and not above the horizon");

	std::vector<ViewTriangle> collapsed(1);
	collapsed[0].a = NiPoint3{0.0f, 30.0f, 0.0f};
	collapsed[0].b = NiPoint3{0.0f, 30.0f, 0.0f};
	collapsed[0].c = NiPoint3{1.0f, 30.0f, 1.0f};
	collapsed[0].trunk = true;
	t = Cast(ahead, collapsed);
	Check(t.seen[kViewSeenNothing] == t.rays, "a triangle collapsed to a line (the head's): not met");

	// A front and a back at one place, 0.02 apart - the back a little smaller,
	// so every ray that meets it meets the front too: the front shows, in
	// either order.
	std::vector<ViewTriangle> pair(2);
	pair[0].a = NiPoint3{-20.0f, 30.0f, -20.0f};
	pair[0].b = NiPoint3{20.0f, 30.0f, -20.0f};
	pair[0].c = NiPoint3{0.0f, 30.0f, 20.0f};
	pair[0].trunk = true;
	if (Dot(Cross(pair[0].b - pair[0].a, pair[0].c - pair[0].a), NiPoint3{0.0f, -1.0f, 0.0f}) < 0.0f) {
		const NiPoint3 swap = pair[0].b;
		pair[0].b = pair[0].c;
		pair[0].c = swap;
	}
	const NiPoint3 middle = (pair[0].a + pair[0].b + pair[0].c) * (1.0f / 3.0f);
	pair[1] = pair[0];
	pair[1].a = middle + (pair[0].a - middle) * 0.9f;
	pair[1].b = middle + (pair[0].c - middle) * 0.9f;  // the other way round: its outside away from the eye
	pair[1].c = middle + (pair[0].b - middle) * 0.9f;
	pair[1].a.y = pair[1].b.y = pair[1].c.y = 29.98f;
	t = Cast(ahead, pair);
	const UInt32 frontFirst = t.seen[kViewSeenTrunkOutside];
	Check(frontFirst > 0 && t.seen[kViewSeenTrunkInside] == 0, "the back a hair nearer than the front: the front shows");
	const ViewTriangle first = pair[0];
	pair[0] = pair[1];
	pair[1] = first;
	t = Cast(ahead, pair);
	Check(t.seen[kViewSeenTrunkOutside] == frontFirst && t.seen[kViewSeenTrunkInside] == 0,
	      "the same met the other way round");
	pair[1].a.y = pair[1].b.y = pair[1].c.y = 31.0f;
	t = Cast(ahead, pair);
	Check(t.seen[kViewSeenTrunkInside] > 0, "the back a whole unit nearer: where it is, the back is what is met");
}

void TestVerdict() {
	std::printf("The verdict over both eyes\n");
	ViewTally seen;
	seen.rays = 1000;
	seen.seen[kViewSeenTrunkOutside] = 5;
	ViewTally little;
	little.rays = 1000;
	little.seen[kViewSeenTrunkOutside] = 4;
	ViewTally into = seen;
	into.seen[kViewSeenTrunkInside] = 1;
	ViewTally limbInside = little;
	limbInside.seen[kViewSeenLimbInside] = 300;
	ViewTally empty;
	Check(BodyViewVerdict(seen, seen) == ViewVerdict::DownTheBody, "the trunk on half a percent, from outside: down the body");
	Check(BodyViewVerdict(little, seen) == ViewVerdict::DownTheBody && BodyViewVerdict(seen, little) == ViewVerdict::DownTheBody,
	      "one eye seeing it is enough");
	Check(BodyViewVerdict(into, seen) == ViewVerdict::IntoTheBody && BodyViewVerdict(seen, into) == ViewVerdict::IntoTheBody,
	      "one ray of either eye on the trunk's inside: into the body");
	Check(BodyViewVerdict(little, little) == ViewVerdict::TrunkOutOfView, "less than half a percent in both: out of view");
	Check(BodyViewVerdict(limbInside, limbInside) == ViewVerdict::TrunkOutOfView,
	      "a limb's inside is not the trunk's: no verdict of into the body");
	Check(Near(ViewShare(empty, kViewSeenTrunkOutside), 0.0f) && BodyViewVerdict(empty, empty) == ViewVerdict::TrunkOutOfView,
	      "no rays at all: nothing seen");
}

}  // namespace

int main() {
	TestTransformPoint();
	TestSkinning();
	TestBones();
	TestWinding();
	TestRayHit();
	TestRows();
	TestCast();
	TestNearPlane();
	TestOddTriangles();
	TestVerdict();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("All checks passed\n");
	return 0;
}

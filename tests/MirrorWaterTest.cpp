// Checks the water's reflection in the mirror's render (render/WaterReprojection.h
// EngineReflectionFrustumFor, docs/mirror-spec.md 16) against a model of the
// engine's own planar reflection, played over plain values:
//
// - the reflection camera as 0x0049BEF0 builds it (read in the decompile): the
//   camera's view direction (column 0) and right (column 2) reflected about the
//   water's height, its up (column 1) made again as right x view - a proper
//   rotation, so its up is the true mirror's turned over; its place reflected;
//   its frustum the one it is handed (0x0070C190 copies the passed camera's
//   +0xEC into it; OBVR.log 2026-11-28 20:17: "the water camera read at ...,
//   frustum -0.416 0.102 0.049 -0.506", the scene camera's);
// - the map's texel of a water point: its place through that camera and frustum,
//   u = 0.5 + 0.5 x, v = 0.5 - 0.5 y of its clip coordinates (a render target's
//   rows run down);
// - the water shader's lookup of the same point: the scene camera's own clip
//   coordinates (the stock vertex shader hands the rows of c0-c3, the scene's
//   view-projection, to the pixel shader in oT2-oT5: WaterReprojection.h
//   PatchWaterVertexShaderNative's patterns), u = 0.5 + 0.5 x, v = 0.5 + 0.5 y -
//   the turned-over up absorbed by reading v upward [derived: the one form under
//   which the flat game's reflection, whose frustum is symmetric, is right].
//
// The flat game's and the eyes' frustums are symmetric (the eyes' measured:
// -1.402 1.402 1.091 -1.091): lookup and map agree. The mirror's are off-axis;
// handed as they are, the lookup misses by (t + b) / (t - b) of the map's
// height and most of the water the mirror sees is not in the map at all.
// Handed with top and bottom mirrored (t' = -b, b' = -t) they agree again.

#include <cmath>
#include <cstdio>
#include <limits>

#include "render/WaterReprojection.h"

using namespace obvr;
using namespace obvr::render;

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

struct V3 {
	double x, y, z;
};
V3 Add(V3 a, V3 b) { return V3{a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 Sub(V3 a, V3 b) { return V3{a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 Mul(V3 a, double s) { return V3{a.x * s, a.y * s, a.z * s}; }
double Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 Cross(V3 a, V3 b) { return V3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 Unit(V3 a) { return Mul(a, 1.0 / std::sqrt(Dot(a, a))); }

// A camera as the engine holds it: view direction, up, right (columns 0, 1, 2)
// with right = view x up (a proper rotation, as OBVR's MirrorCameraFor builds).
struct Cam {
	V3 pos, view, up, right;
};

Cam Looking(V3 pos, V3 view, V3 worldUp) {
	Cam c;
	c.pos = pos;
	c.view = Unit(view);
	c.right = Unit(Cross(c.view, worldUp));
	c.up = Cross(c.right, c.view);
	return c;
}

// 0x0049BEF0's camera mirrored about z = height.
Cam EngineReflection(const Cam& c, double height) {
	Cam r;
	r.pos = V3{c.pos.x, c.pos.y, 2.0 * height - c.pos.z};
	r.view = V3{c.view.x, c.view.y, -c.view.z};
	r.right = V3{c.right.x, c.right.y, -c.right.z};
	r.up = Cross(r.right, r.view);  // made again; the true mirror's up turned over
	return r;
}

struct Clip {
	double x, y;
	bool front;
};

Clip Project(const Cam& c, const game::NiFrustum& f, V3 p) {
	const V3 d = Sub(p, c.pos);
	const double z = Dot(d, c.view);
	const double tx = Dot(d, c.right) / z;
	const double ty = Dot(d, c.up) / z;
	return Clip{(2.0 * tx - (f.r + f.l)) / (f.r - f.l), (2.0 * ty - (f.t + f.b)) / (f.t - f.b), z > 0.0};
}

game::NiFrustum Frustum(float l, float r, float t, float b) {
	game::NiFrustum f{};
	f.l = l;
	f.r = r;
	f.t = t;
	f.b = b;
	f.n = 4.0f;
	f.f = 283840.0f;
	f.o = false;
	return f;
}

// The water points the scene camera sees: its picture sampled on a grid, each
// ray met with the water's plane.
struct Result {
	int points = 0;
	double worstMiss = 0.0;  // the largest distance between lookup and map texel, in map widths
	double meanMissV = 0.0;  // lookup v less map v, the mean
	int outside = 0;         // points whose texel lies outside the map
};

Result Play(const Cam& scene, const game::NiFrustum& seen, const game::NiFrustum& handed, double height) {
	Result out;
	const Cam water = EngineReflection(scene, height);
	for (int i = 0; i <= 10; ++i) {
		for (int j = 0; j <= 10; ++j) {
			const double tx = seen.l + (seen.r - seen.l) * (0.02 + 0.96 * i / 10.0);
			const double ty = seen.b + (seen.t - seen.b) * (0.02 + 0.96 * j / 10.0);
			const V3 ray = Add(Add(scene.view, Mul(scene.right, tx)), Mul(scene.up, ty));
			if (!(ray.z < -1.0e-6)) {
				continue;  // the ray does not reach the water
			}
			const double s = (height - scene.pos.z) / ray.z;
			const V3 p = Add(scene.pos, Mul(ray, s));
			const Clip a = Project(scene, seen, p);
			const Clip m = Project(water, handed, p);
			const double lookupU = 0.5 + 0.5 * a.x;
			const double lookupV = 0.5 + 0.5 * a.y;
			const double mapU = 0.5 + 0.5 * m.x;
			const double mapV = 0.5 - 0.5 * m.y;
			const double miss = std::sqrt((lookupU - mapU) * (lookupU - mapU) + (lookupV - mapV) * (lookupV - mapV));
			out.worstMiss = miss > out.worstMiss ? miss : out.worstMiss;
			out.meanMissV += lookupV - mapV;
			if (!m.front || mapU < 0.0 || mapU > 1.0 || mapV < 0.0 || mapV > 1.0) {
				++out.outside;
			}
			++out.points;
		}
	}
	if (out.points > 0) {
		out.meanMissV /= out.points;
	}
	return out;
}

game::NiFrustum Handed(const game::NiFrustum& f) {
	game::NiFrustum out{};
	EngineReflectionFrustumFor(f, true, out);
	return out;
}

void Report(const char* what, const Result& r) {
	std::printf("    %s: %d water points, the lookup %.4f map heights off the texel on average (v), %.4f at worst, %d "
	            "outside the map\n",
	            what, r.points, r.meanMissV, r.worstMiss, r.outside);
}

void TheModel() {
	std::printf("the model: the flat game's and the eyes' symmetric frustums\n");
	// A camera 120 units over the water looking 25 degrees down, turned.
	const Cam scene = Looking(V3{100.0, -40.0, 120.0}, V3{0.8, 0.5, -0.45}, V3{0.0, 0.0, 1.0});
	const game::NiFrustum eyes = Frustum(-1.402f, 1.402f, 1.091f, -1.091f);  // OBVR.log 20:17, line 1725 [M]
	const Result r = Play(scene, eyes, eyes, 0.0);
	Report("the eyes' frustum handed as it is", r);
	Check(r.points > 30 && r.worstMiss < 1.0e-6, "symmetric: the engine's lookup meets the map's texel exactly");
	Check(r.outside == 0, "symmetric: every water point the camera sees is in the map");
	game::NiFrustum handed{};
	Check(EngineReflectionFrustumFor(eyes, true, handed) && handed.t == eyes.t && handed.b == eyes.b,
	      "the mirror's rule on a symmetric frustum changes nothing");
}

// The mirror's camera of the owner's run: the glass held low, looking back and
// down at the water (the reflected eye, 255 units over the water at height 0).
void TheMirror(const char* which, float l, float r, float t, float b) {
	std::printf("the mirror's %s frustum %.3f %.3f %.3f %.3f\n", which, static_cast<double>(l), static_cast<double>(r),
	            static_cast<double>(t), static_cast<double>(b));
	const Cam scene = Looking(V3{47380.9, 83237.3, 255.3}, V3{-0.1, -0.95, -0.3}, V3{0.0, 0.0, 1.0});
	const game::NiFrustum seen = Frustum(l, r, t, b);
	const Result asIs = Play(scene, seen, seen, 0.0);
	Report("handed as it is (before)", asIs);
	const double expected = -(static_cast<double>(t) + b) / (static_cast<double>(t) - b);
	Check(asIs.points > 30, "the camera sees water");
	Check(std::fabs(asIs.meanMissV - expected) < 1.0e-4,
	      "as it is: the lookup misses by (t + b) / (t - b) of the map's height (0.82 for the left eye)");
	Check(asIs.outside > asIs.points / 2, "as it is: most of the water the mirror sees is not in the map");
	const Result fixed = Play(scene, seen, Handed(seen), 0.0);
	Report("handed with top and bottom mirrored (after)", fixed);
	Check(fixed.worstMiss < 1.0e-6, "mirrored: the lookup meets the map's texel exactly");
	Check(fixed.outside == 0, "mirrored: every water point the mirror sees is in the map");
}

void TheRule() {
	std::printf("the rule\n");
	const game::NiFrustum f = Frustum(-0.416f, 0.102f, 0.049f, -0.506f);
	game::NiFrustum out{};
	Check(EngineReflectionFrustumFor(f, true, out), "the mirror's render: handed mirrored");
	Check(out.l == f.l && out.r == f.r && out.t == -f.b && out.b == -f.t && out.n == f.n && out.f == f.f && !out.o,
	      "top and bottom mirrored, left, right, near and far kept");
	Check(!EngineReflectionFrustumFor(f, false, out) && out.t == f.t && out.b == f.b,
	      "outside the mirror's render: the eyes' frustum handed as it is");
	game::NiFrustum bad = f;
	bad.o = true;
	Check(!EngineReflectionFrustumFor(bad, true, out) && out.t == bad.t, "an orthographic frustum: as it is");
	bad = f;
	bad.t = bad.b;
	Check(!EngineReflectionFrustumFor(bad, true, out), "no height (top at bottom): as it is");
	bad = f;
	bad.t = -0.6f;
	Check(!EngineReflectionFrustumFor(bad, true, out), "top under bottom: as it is");
	bad = f;
	bad.b = std::numeric_limits<float>::quiet_NaN();
	Check(!EngineReflectionFrustumFor(bad, true, out) && std::isnan(out.b), "a bottom of no number: as it is");
	bad = f;
	bad.t = std::numeric_limits<float>::infinity();
	Check(!EngineReflectionFrustumFor(bad, true, out), "an endless top: as it is");
}

}  // namespace

int main() {
	TheModel();
	TheMirror("left eye's (OBVR.log 20:17, line 4393)", -0.416f, 0.102f, 0.049f, -0.506f);
	TheMirror("right eye's (line 4415)", -0.320f, 0.191f, 0.046f, -0.500f);
	TheMirror("left eye's of the 14:56 run, wholly under the axis (spec 15.3)", -0.570f, 0.200f, -0.273f, -1.097f);
	TheRule();
	std::printf("%d of %d checks failed\n", g_failures, g_checks);
	return g_failures == 0 ? 0 : 1;
}

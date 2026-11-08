// Cut marks (game/CutMarkLogic.h, docs/blade-penetration-spec.md step 3):
// the engine's decal frame for a projection direction as the EXE builds it,
// the turn about that direction that lays the texture's long side along the
// cut, the texture by material, and where along a cut the marks go - only
// once the blade went in.

#include <cstdio>

#include "game/CutMarkLogic.h"

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
bool Near(const NiPoint3& a, const NiPoint3& b, float within = 1e-3f) {
	return Abs(a.x - b.x) <= within && Abs(a.y - b.y) <= within && Abs(a.z - b.z) <= within;
}
float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

void TestFrame() {
	std::printf("the engine's decal frame\n");
	const NiPoint3 dirs[4] = {{0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.6f, -0.48f, 0.64f}};
	bool orthonormal = true;
	for (const NiPoint3& d : dirs) {
		NiPoint3 r[3];
		EngineDecalFrame(d, r);
		orthonormal = orthonormal && Near(r[2], d) && Abs(Dot(r[0], r[1])) < 1e-4f && Abs(Dot(r[0], r[2])) < 1e-4f &&
		              Abs(Dot(r[1], r[2])) < 1e-4f && Abs(Dot(r[0], r[0]) - 1.0f) < 1e-4f &&
		              Abs(Dot(r[1], r[1]) - 1.0f) < 1e-4f;
	}
	Check(orthonormal, "its rows square to each other, unit long, the last the direction");
	NiPoint3 r[3];
	EngineDecalFrame(NiPoint3{0.0f, -1.0f, 0.0f}, r);
	Check(Near(r[0], NiPoint3{0.0f, 0.0f, 1.0f}) && Near(r[1], NiPoint3{-1.0f, 0.0f, 0.0f}),
	      "straight along -y: x the smallest part, the helper (0,-z,y), rows (0,0,1) and (-1,0,0)");
	EngineDecalFrame(NiPoint3{0.0f, 0.0f, -1.0f}, r);
	Check(Near(r[1], NiPoint3{0.0f, 1.0f, 0.0f}) || Near(r[1], NiPoint3{0.0f, -1.0f, 0.0f}) ||
	          Near(r[1], NiPoint3{1.0f, 0.0f, 0.0f}) || Near(r[1], NiPoint3{-1.0f, 0.0f, 0.0f}),
	      "straight down: a horizontal frame");
	NiPoint3 bad[3];
	EngineDecalFrame(NiPoint3{0.0f, 0.0f, 0.0f}, bad);
	Check(Near(bad[2], NiPoint3{0.0f, 0.0f, 1.0f}), "no direction: up, whatever comes");
}

void TestAngle() {
	std::printf("the turn that lays the mark along the cut\n");
	const NiPoint3 d{0.0f, -1.0f, 0.0f};
	bool along = true;
	for (float a = -3.0f; a <= 3.0f; a += 0.37f) {
		const NiPoint3 cut{math::Cos(a), 0.0f, math::Sin(a)};
		const float t = CutMarkAngle(cut, d);
		const NiPoint3 u = DecalLongAxis(d, t);
		along = along && Abs(Abs(Dot(u, cut)) - 1.0f) < 1e-3f;
	}
	Check(along, "every cut across the face: the mark's long side along it");
	const NiPoint3 slant{0.0f, 0.0f, 0.0f};
	Check(CutMarkAngle(slant, d) == 0.0f, "no cut direction: no turn");
	const float t = CutMarkAngle(NiPoint3{0.0f, -1.0f, 1.0f}, d);
	const NiPoint3 u = DecalLongAxis(d, t);
	Check(Abs(Abs(u.z) - 1.0f) < 1e-3f, "a cut partly along the projection: its part across the face counts");
}

void TestTexture() {
	std::printf("the texture by material\n");
	Check(CutMarkTexture(BodyMaterial::Skin)[0] != '\0' && CutMarkTexture(BodyMaterial::Cloth)[0] != '\0' &&
	          CutMarkTexture(BodyMaterial::Light)[0] != '\0',
	      "skin, cloth and light armour each one");
	Check(CutMarkTexture(BodyMaterial::Heavy) == nullptr && CutMarkTexture(BodyMaterial::Unknown) == nullptr,
	      "heavy armour and the unknown: none");
}

CutMarkFrame Frame(bool entered, BladeEntryKind kind, const NiPoint3& at) {
	CutMarkFrame f;
	f.entered = entered;
	f.kind = kind;
	f.material = BodyMaterial::Skin;
	f.point = at;
	f.normal = NiPoint3{0.0f, -1.0f, 0.0f};
	f.velocity = NiPoint3{100.0f, 0.0f, 0.0f};
	f.edge = NiPoint3{1.0f, 0.0f, 0.0f};
	return f;
}

void TestPlanner() {
	std::printf("where the marks go\n");
	CutMarkState s;
	CutMarkSettings set;
	Check(!StepCutMarks(s, set, Frame(false, BladeEntryKind::None, NiPoint3{0, 0, 0})).place, "not in: no mark");
	const CutMark first = StepCutMarks(s, set, Frame(true, BladeEntryKind::Slash, NiPoint3{0, 0, 0}));
	Check(first.place && Near(first.direction, NiPoint3{0.0f, 1.0f, 0.0f}) && first.texture != nullptr,
	      "in: a mark at once, projected into the surface");
	Check(Abs(Dot(DecalLongAxis(first.direction, first.angle), NiPoint3{1.0f, 0.0f, 0.0f})) > 0.999f,
	      "a slash's mark along its motion");
	Check(Near(first.from, NiPoint3{0.0f, -set.liftUnits, 0.0f}), "placed from just outside the surface");
	Check(!StepCutMarks(s, set, Frame(true, BladeEntryKind::Slash, NiPoint3{4, 0, 0})).place, "4 units on: none yet");
	Check(StepCutMarks(s, set, Frame(true, BladeEntryKind::Slash, NiPoint3{9, 0, 0})).place, "9 units on: the next");
	UInt32 placed = 2;
	for (int i = 2; i < 40; ++i) {
		placed += StepCutMarks(s, set, Frame(true, BladeEntryKind::Slash, NiPoint3{9.0f * i, 0, 0})).place ? 1 : 0;
	}
	Check(placed == set.mostPerCut, "at most so many a cut");
	StepCutMarks(s, set, Frame(false, BladeEntryKind::None, NiPoint3{0, 0, 0}));
	Check(StepCutMarks(s, set, Frame(true, BladeEntryKind::Slash, NiPoint3{500, 0, 0})).place, "a new cut: marks again");
	CutMarkState st;
	const CutMark stab = StepCutMarks(st, set, Frame(true, BladeEntryKind::Stab, NiPoint3{0, 0, 0}));
	Check(stab.place && Abs(Dot(DecalLongAxis(stab.direction, stab.angle), NiPoint3{1.0f, 0.0f, 0.0f})) > 0.999f,
	      "a stab: one mark along the blade's edge");
	Check(!StepCutMarks(st, set, Frame(true, BladeEntryKind::Stab, NiPoint3{20, 0, 0})).place, "a stab marks once");
	CutMarkState sh;
	CutMarkFrame heavy = Frame(true, BladeEntryKind::Slash, NiPoint3{0, 0, 0});
	heavy.material = BodyMaterial::Heavy;
	Check(!StepCutMarks(sh, set, heavy).place, "heavy armour: no mark even when in");
	CutMarkSettings off;
	off.enabled = false;
	CutMarkState so;
	Check(!StepCutMarks(so, off, Frame(true, BladeEntryKind::Slash, NiPoint3{0, 0, 0})).place, "switched off: none");
}


// Which of the actor's shapes a decal is laid on, one call each: the
// engine's walk from the root counts every shape it visits against
// iMaxDecalsPerFrame (10) - the eight FaceGen shapes first - so a body
// shape late in the tree, or the clothes, were never reached.
void TestShapes() {
	std::printf("The shapes a decal goes to\n");
	DecalShape s[6];
	s[0] = DecalShape{true, false, NiPoint3{0, 0, 60}, 10.0f};   // FaceGen: the engine skips it
	s[1] = DecalShape{false, false, NiPoint3{0, 0, 0}, 30.0f};   // the body, 2 inside its bound's edge
	s[2] = DecalShape{false, false, NiPoint3{0, 5, 0}, 30.0f};   // the shirt over it, deeper inside
	s[3] = DecalShape{false, true, NiPoint3{0, 0, 0}, 30.0f};    // hidden
	s[4] = DecalShape{false, false, NiPoint3{0, 0, -80}, 20.0f}; // the feet, far off
	s[5] = DecalShape{false, false, NiPoint3{40, 0, 0}, 13.0f};  // just past its bound, within the slack
	UInt32 out[4] = {};
	const NiPoint3 p{0, 28, 0};
	UInt32 n = PickDecalShapes(s, 6, p, 4.0f, out, 4);
	Check(n == 2 && out[0] == 2 && out[1] == 1, "the shapes round the point, the deepest in its bound first; not a skipped name, hidden or far one");
	n = PickDecalShapes(s, 6, NiPoint3{24, 0, 0}, 4.0f, out, 4);
	Check(n == 3 && out[2] == 5, "a bound the point is within the slack of counts");
	n = PickDecalShapes(s, 6, p, 4.0f, out, 1);
	Check(n == 1 && out[0] == 2, "no more than asked: the deepest");
	Check(PickDecalShapes(s, 6, NiPoint3{0, 200, 0}, 4.0f, out, 4) == 0, "nothing round the point: none");
	Check(PickDecalShapes(s, 0, p, 4.0f, out, 4) == 0 && PickDecalShapes(s, 6, p, 4.0f, out, 0) == 0,
	      "no shapes or no room: none");
}

}  // namespace

int main() {
	TestFrame();
	TestAngle();
	TestTexture();
	TestPlanner();
	TestShapes();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

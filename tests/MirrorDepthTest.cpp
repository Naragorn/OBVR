// Checks the mirror's depth, stage 4 (vr/MirrorDepthLogic.h, docs/mirror-spec.md
// 14): each eye's reflected camera and frustum through the glass - the two
// apart by the eyes' offset reflected, and MirrorDepth=0 exactly stage 3's
// one camera; the frame's mode (one picture for both, both eyes, the eyes
// taking turns) from the setting and the budget, with the budget's window,
// hysteresis and hold and every refusal; which eyes render a frame and, taking
// turns, which one; the pictures each eye's glass shows, a stale eye's picture
// laid through the camera it was rendered with; the cost line's new parts.

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/MirrorDepthLogic.h"
#include "vr/MirrorGlassMap.h"

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

bool SameMatrix(const NiMatrix33& a, const NiMatrix33& b) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.data[r][c], b.data[r][c])) {
				return false;
			}
		}
	}
	return true;
}

bool SameCamera(const MirrorCamera& a, const MirrorCamera& b) {
	return a.view == b.view && NearPoint(a.position, b.position) && SameMatrix(a.rotation, b.rotation) &&
	       Near(a.left, b.left) && Near(a.right, b.right) && Near(a.top, b.top) && Near(a.bottom, b.bottom) &&
	       Near(a.nearUnits, b.nearUnits);
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

// The two eyes 4.4 units apart (63 mm at 70 units a metre) around x = 0, the
// glass drawn into both.
void TwoEyes(GlassOnScreen (&eyes)[2], float apart = 4.4f) {
	for (int i = 0; i < 2; ++i) {
		eyes[i] = GlassOnScreen{};
		eyes[i].drawn = true;
		eyes[i].maxX = 100.0f;
		eyes[i].maxY = 100.0f;
		eyes[i].viewportWidth = 1000.0f;
		eyes[i].viewportHeight = 1000.0f;
		eyes[i].eye = P(i == 0 ? -0.5f * apart : 0.5f * apart, 0.0f, 0.0f);
	}
}

void TestEyeCameras() {
	std::printf("Each eye's reflected camera through the glass\n");
	NiPoint3 c[4];
	UprightGlass(21.0f, 23.0f, 40.0f, c);
	const GlassPlane glass = GlassPlaneOf(c);
	GlassOnScreen eyes[2];
	TwoEyes(eyes);

	const MirrorEyeCameras off = MirrorEyeCamerasFor(glass, c, eyes, false);
	NiPoint3 middle{0.0f, 0.0f, 0.0f};
	Check(CyclopeanEye(eyes, middle), "the cyclopean eye is there");
	const MirrorCamera stage3 = MirrorCameraFor(glass, c, middle);
	Check(!off.depth, "MirrorDepth=0: no depth");
	Check(SameCamera(off.eye[0], stage3) && SameCamera(off.eye[1], stage3),
	      "MirrorDepth=0: both eyes take exactly stage 3's camera (the cyclopean eye reflected)");

	const MirrorEyeCameras on = MirrorEyeCamerasFor(glass, c, eyes, true);
	Check(on.depth, "MirrorDepth=1: depth");
	Check(on.eye[0].view == MirrorView::Ok && on.eye[1].view == MirrorView::Ok, "both eyes' cameras are there");
	Check(SameCamera(on.eye[0], MirrorCameraFor(glass, c, eyes[0].eye)), "the left eye's camera is its own eye reflected");
	Check(SameCamera(on.eye[1], MirrorCameraFor(glass, c, eyes[1].eye)), "the right eye's camera is its own eye reflected");
	const NiPoint3 apart = on.eye[1].position - on.eye[0].position;
	const NiPoint3 eyeOffset = eyes[1].eye - eyes[0].eye;
	Check(NearPoint(apart, ReflectDirection(eyeOffset, glass.normal)),
	      "the two cameras stand apart by the eyes' offset reflected in the glass");
	Check(Near(std::sqrt(apart.LengthSquared()), 4.4f), "as far apart as the eyes, 4.4 units");
	Check(SameMatrix(on.eye[0].rotation, on.eye[1].rotation), "both look out of the glass along its normal, the same turn");
	Check(!Near(on.eye[0].left, on.eye[1].left) && !Near(on.eye[0].right, on.eye[1].right),
	      "their frustums differ: each runs through the glass's corners from its own place");
	Check(Near(on.eye[0].left, -on.eye[1].right) && Near(on.eye[0].right, -on.eye[1].left),
	      "eyes placed evenly about the glass's middle: frustums mirror each other");

	// An eye in a slanted place: the offset still reflects.
	GlassOnScreen slanted[2];
	TwoEyes(slanted);
	slanted[1].eye = P(2.0f, 3.0f, 1.5f);
	const MirrorEyeCameras s = MirrorEyeCamerasFor(glass, c, slanted, true);
	Check(NearPoint(s.eye[1].position - s.eye[0].position, ReflectDirection(slanted[1].eye - slanted[0].eye, glass.normal)),
	      "an offset with depth: reflected too (the depth part turns round)");

	GlassOnScreen oneEye[2];
	TwoEyes(oneEye);
	oneEye[1].drawn = false;
	const MirrorEyeCameras one = MirrorEyeCamerasFor(glass, c, oneEye, true);
	Check(one.eye[0].view == MirrorView::Ok && one.eye[1].view == MirrorView::NoGlass,
	      "the glass not drawn into the right eye: no camera for it");
	const MirrorEyeCameras oneOff = MirrorEyeCamerasFor(glass, c, oneEye, false);
	Check(SameCamera(oneOff.eye[0], MirrorCameraFor(glass, c, oneEye[0].eye)),
	      "MirrorDepth=0 with one eye: stage 3's camera from that eye");

	GlassOnScreen none[2];
	TwoEyes(none);
	none[0].drawn = none[1].drawn = false;
	const MirrorEyeCameras nothing = MirrorEyeCamerasFor(glass, c, none, true);
	Check(nothing.eye[0].view == MirrorView::NoGlass && nothing.eye[1].view == MirrorView::NoGlass, "no eye: no camera");

	GlassOnScreen behind[2];
	TwoEyes(behind);
	behind[1].eye = P(2.2f, 45.0f, 0.0f);  // past the glass
	const MirrorEyeCameras back = MirrorEyeCamerasFor(glass, c, behind, true);
	Check(back.eye[0].view == MirrorView::Ok && back.eye[1].view == MirrorView::EyeBehind,
	      "one eye behind the glass: its camera refused, the other's kept");

	const MirrorEyeCameras noGlass = MirrorEyeCamerasFor(GlassPlane{}, c, eyes, true);
	Check(noGlass.eye[0].view == MirrorView::NoGlass && noGlass.eye[1].view == MirrorView::NoGlass, "no glass: no camera");
}

void TestBudgetClamp() {
	std::printf("The budget's setting\n");
	Check(Near(ClampMirrorBudgetMs(2.5f), 2.5f), "2.5 as it is");
	Check(Near(ClampMirrorBudgetMs(std::numeric_limits<float>::quiet_NaN()), kMirrorBudgetMsDefault), "no number: the default");
	Check(Near(ClampMirrorBudgetMs(-1.0f), 0.0f), "below 0: 0, never taking turns");
	Check(Near(ClampMirrorBudgetMs(100.0f), kMirrorBudgetMsMax), "above the most: the most");
	Check(Near(kMirrorBudgetMsDefault, 2.5f), "the default is the spec's 2.5 ms a pass");
}

void TestBudget() {
	std::printf("The budget: both eyes, or taking turns\n");
	const float dt = 1.0f / 90.0f;
	MirrorBudget b;
	Check(!b.turns, "starts with both eyes");
	// Under a second of 3 ms passes: no decision yet.
	MirrorBudgetChange change = MirrorBudgetChange::None;
	for (int i = 0; i < 80; ++i) {
		change = StepMirrorBudget(b, 2.5f, dt, 6.0, 2);
	}
	Check(change == MirrorBudgetChange::None && !b.turns, "under a second: no change, however dear");
	for (int i = 0; i < 12 && change == MirrorBudgetChange::None; ++i) {
		change = StepMirrorBudget(b, 2.5f, dt, 6.0, 2);
	}
	Check(change == MirrorBudgetChange::ToTurns && b.turns, "a second at 3 ms a pass over 2.5: the eyes take turns");
	Check(Near(static_cast<float>(b.lastMeanMs), 3.0f), "the second's mean is kept for the cost line");

	// Cheap at once, but within the hold: no flip back.
	bool flipped = false;
	for (int i = 0; i < 100; ++i) {
		flipped = StepMirrorBudget(b, 2.5f, dt, 1.0, 1) != MirrorBudgetChange::None || flipped;
	}
	Check(!flipped && b.turns, "within the hold after a change: still taking turns, however cheap");
	change = MirrorBudgetChange::None;
	MirrorBudget held = b;
	for (int i = 0; i < 300 && change == MirrorBudgetChange::None; ++i) {
		change = StepMirrorBudget(held, 2.5f, dt, 1.0, 1);
	}
	Check(change == MirrorBudgetChange::ToBoth && !held.turns, "past the hold, 1 ms a pass under 0.8 x 2.5: both eyes again");

	// Between 0.8 and 1.0 of the budget: stays where it is (hysteresis), both ways.
	MirrorBudget band = b;
	change = MirrorBudgetChange::None;
	for (int i = 0; i < 600; ++i) {
		change = StepMirrorBudget(band, 2.5f, dt, 2.2, 1);
		if (change != MirrorBudgetChange::None) {
			break;
		}
	}
	Check(change == MirrorBudgetChange::None && band.turns, "2.2 ms a pass, between 2.0 and 2.5: taking turns goes on");
	MirrorBudget bandBoth;
	for (int i = 0; i < 600; ++i) {
		change = StepMirrorBudget(bandBoth, 2.5f, dt, 4.4, 2);
		if (change != MirrorBudgetChange::None) {
			break;
		}
	}
	Check(change == MirrorBudgetChange::None && !bandBoth.turns, "2.2 ms a pass with both eyes: both go on");

	// Refusals.
	MirrorBudget empty;
	for (int i = 0; i < 200; ++i) {
		change = StepMirrorBudget(empty, 2.5f, dt, 0.0, 0);
	}
	Check(change == MirrorBudgetChange::None && !empty.turns, "a second without a pass: nothing to judge");
	MirrorBudget noTime;
	flipped = false;
	for (int i = 0; i < 200; ++i) {
		flipped = StepMirrorBudget(noTime, 2.5f, std::numeric_limits<float>::quiet_NaN(), 6.0, 2) !=
		              MirrorBudgetChange::None ||
		          flipped;
	}
	Check(!flipped && Near(noTime.windowSeconds, 0.0f) && noTime.windowPasses == 0,
	      "a frame time of no number: not counted");
	StepMirrorBudget(noTime, 2.5f, 0.0f, 6.0, 2);
	StepMirrorBudget(noTime, 2.5f, -0.01f, 6.0, 2);
	StepMirrorBudget(noTime, 2.5f, 0.5f, 6.0, 2);
	Check(noTime.windowPasses == 0, "zero, backwards or half a second: not counted (a pause, a load)");

	MirrorBudget off = b;  // taking turns
	Check(StepMirrorBudget(off, 0.0f, dt, 6.0, 1) == MirrorBudgetChange::ToBoth && !off.turns,
	      "a budget of 0: never taking turns, back to both at once");
	Check(StepMirrorBudget(off, 0.0f, dt, 60.0, 2) == MirrorBudgetChange::None && !off.turns,
	      "a budget of 0 and both eyes: stays, however dear");
	MirrorBudget nan = b;
	Check(StepMirrorBudget(nan, std::numeric_limits<float>::quiet_NaN(), dt, 6.0, 1) == MirrorBudgetChange::ToBoth,
	      "a budget of no number: as 0");

	// A tiny budget (the scenario's): turns after the first second.
	MirrorBudget tiny;
	change = MirrorBudgetChange::None;
	for (int i = 0; i < 100 && change == MirrorBudgetChange::None; ++i) {
		change = StepMirrorBudget(tiny, 0.1f, dt, 1.0, 2);
	}
	Check(change == MirrorBudgetChange::ToTurns, "a budget of 0.1 ms: taking turns after the first second");
}

void TestMode() {
	std::printf("The frame's mode\n");
	MirrorBudget both;
	MirrorBudget turns;
	turns.turns = true;
	Check(MirrorDepthModeFor(false, both) == MirrorDepthMode::OneForBoth, "MirrorDepth=0: one picture for both eyes");
	Check(MirrorDepthModeFor(false, turns) == MirrorDepthMode::OneForBoth, "MirrorDepth=0 whatever the budget says");
	Check(MirrorDepthModeFor(true, both) == MirrorDepthMode::Both, "depth within the budget: both eyes");
	Check(MirrorDepthModeFor(true, turns) == MirrorDepthMode::Turns, "depth over the budget: taking turns");
	Check(MirrorDepthModeName(MirrorDepthMode::Turns)[0] != 0 && MirrorDepthModeName(MirrorDepthMode::Count)[0] != 0,
	      "every mode has a name");
}

void TestPlan() {
	std::printf("Which eyes render a frame\n");
	const bool can[2] = {true, true};
	const bool leftOnly[2] = {true, false};
	const bool rightOnly[2] = {false, true};
	const bool neither[2] = {false, false};
	const std::uint32_t fresh[2] = {kMirrorNoPicture, kMirrorNoPicture};
	const std::uint32_t leftOlder[2] = {3, 1};
	const std::uint32_t rightOlder[2] = {0, 1};
	const std::uint32_t even[2] = {1, 1};

	MirrorEyePlan p = PlanMirrorEyes(MirrorDepthMode::OneForBoth, neither, fresh);
	Check(p.render[0] && !p.render[1] && p.passes == 1, "one for both: one pass, the cyclopean one (stage 3), whatever the eyes");
	p = PlanMirrorEyes(MirrorDepthMode::Both, can, even);
	Check(p.render[0] && p.render[1] && p.passes == 2, "both: two passes");
	p = PlanMirrorEyes(MirrorDepthMode::Both, leftOnly, even);
	Check(p.render[0] && !p.render[1] && p.passes == 1, "both, the right eye refused: the left alone");
	p = PlanMirrorEyes(MirrorDepthMode::Both, neither, even);
	Check(!p.render[0] && !p.render[1] && p.passes == 0, "both, neither eye: none");
	p = PlanMirrorEyes(MirrorDepthMode::Turns, can, fresh);
	Check(p.render[0] && !p.render[1] && p.passes == 1, "taking turns, no picture yet: the left first");
	p = PlanMirrorEyes(MirrorDepthMode::Turns, can, leftOlder);
	Check(p.render[0] && !p.render[1], "taking turns: the eye with the older picture (left)");
	p = PlanMirrorEyes(MirrorDepthMode::Turns, can, rightOlder);
	Check(!p.render[0] && p.render[1], "taking turns: the eye with the older picture (right)");
	p = PlanMirrorEyes(MirrorDepthMode::Turns, can, even);
	Check(p.render[0] && !p.render[1], "taking turns, as old: the left");
	p = PlanMirrorEyes(MirrorDepthMode::Turns, rightOnly, leftOlder);
	Check(!p.render[0] && p.render[1] && p.passes == 1, "taking turns, the older eye refused: the other");
	p = PlanMirrorEyes(MirrorDepthMode::Turns, neither, leftOlder);
	Check(p.passes == 0, "taking turns, neither eye: none");
	const std::uint32_t oneNever[2] = {5, kMirrorNoPicture};
	p = PlanMirrorEyes(MirrorDepthMode::Turns, can, oneNever);
	Check(!p.render[0] && p.render[1], "taking turns: an eye without a picture comes before any old one");
}

void TestTurnsAlternate() {
	std::printf("Taking turns: each eye every second frame\n");
	MirrorEyePictures pictures;
	const bool can[2] = {true, true};
	int left = 0;
	int right = 0;
	bool alternated = true;
	int last = -1;
	for (int frame = 0; frame < 20; ++frame) {
		AgeMirrorEyes(pictures);
		const MirrorEyePlan p = PlanMirrorEyes(MirrorDepthMode::Turns, can, pictures.age);
		const int eye = p.render[0] ? 0 : 1;
		alternated = alternated && eye != last;
		last = eye;
		(eye == 0 ? left : right)++;
		NoteMirrorEyeRendered(pictures, eye, MirrorPictureView{}, nullptr);
	}
	Check(alternated && left == 10 && right == 10, "20 frames: 10 each, never the same eye twice in a row");
	Check(pictures.age[0] <= 1 && pictures.age[1] <= 1, "no picture older than one frame");
}

MirrorPictureView ViewAt(float x) {
	MirrorCamera c;
	c.view = MirrorView::Ok;
	c.position = P(x, 80.0f, 0.0f);
	c.rotation = MirrorColumns(P(0.0f, -1.0f, 0.0f), P(0.0f, 0.0f, 1.0f), P(1.0f, 0.0f, 0.0f));
	c.left = -0.3f;
	c.right = 0.3f;
	c.bottom = -0.3f;
	c.top = 0.3f;
	c.nearUnits = 40.0f;
	const float rect[4] = {0.0f, 0.0f, 0.9f, 0.8f};
	return PictureViewOf(c, rect);
}

void TestPictures() {
	std::printf("Each eye's picture and the glass it is laid on\n");
	MirrorEyePictures p;
	Check(!p.have[0] && !p.have[1] && p.age[0] == kMirrorNoPicture, "no picture at first");
	const float rectA[4] = {0.0f, 0.0f, 0.5f, 0.6f};
	const float rectB[4] = {0.0f, 0.0f, 0.7f, 0.8f};
	NoteMirrorEyeRendered(p, 0, ViewAt(-2.2f), rectA);
	NoteMirrorEyeRendered(p, 1, ViewAt(2.2f), rectB);
	Check(p.have[0] && p.have[1] && p.age[0] == 0 && p.age[1] == 0, "both rendered: both fresh");
	Check(Near(p.rect[1][2], 0.7f) && Near(p.rect[0][3], 0.6f), "each keeps the part of its texture it fills");
	AgeMirrorEyes(p);
	NoteMirrorEyeRendered(p, 0, ViewAt(-1.0f), rectA);
	Check(p.age[0] == 0 && p.age[1] == 1, "the next frame the left alone: the right one frame old");
	Check(Near(p.view[0].position.x, -1.0f) && Near(p.view[1].position.x, 2.2f),
	      "the stale right picture keeps the camera it was rendered with");

	// Laid on the glass through its own camera: the same glass maps differently
	// through the two views, and the stale eye's through the old one.
	NiPoint3 glass[4];
	UprightGlass(20.0f, 20.0f, 40.0f, glass);
	float s0[4], t0[4], q0[4], s1[4], t1[4], q1[4];
	MapGlassCorners(p.view[1], glass, s1, t1, q1);
	MapGlassCorners(ViewAt(2.2f), glass, s0, t0, q0);
	Check(Near(s0[0], s1[0]) && Near(t0[3], t1[3]), "the stale eye's glass is laid through its own picture's camera");
	MapGlassCorners(p.view[0], glass, s0, t0, q0);
	Check(!Near(s0[0], s1[0]), "the fresh eye's through its new one");

	int src[2];
	const bool both[2] = {true, true};
	const bool leftOnly[2] = {true, false};
	const bool rightOnly[2] = {false, true};
	const bool none[2] = {false, false};
	MirrorGlassSources(true, both, src);
	Check(src[0] == 0 && src[1] == 1, "depth, both pictures: each eye its own");
	MirrorGlassSources(true, leftOnly, src);
	Check(src[0] == 0 && src[1] == 0, "depth, the right's not made yet: the left's for both (no dark flash)");
	MirrorGlassSources(true, rightOnly, src);
	Check(src[0] == 1 && src[1] == 1, "depth, the left's not there: the right's for both");
	MirrorGlassSources(true, none, src);
	Check(src[0] < 0 && src[1] < 0, "depth, none: the dark glass");
	MirrorGlassSources(false, both, src);
	Check(src[0] == 0 && src[1] == 0, "MirrorDepth=0: the one picture for both (stage 3)");
	MirrorGlassSources(false, rightOnly, src);
	Check(src[0] < 0 && src[1] < 0, "MirrorDepth=0 without the one picture: the dark glass");

	Check(GlassPictureFor(true, true) == 0 && GlassPictureFor(false, true) == 1,
	      "the tablet: each eye draws its own picture when given two");
	Check(GlassPictureFor(true, false) == 0 && GlassPictureFor(false, false) == 0,
	      "the tablet: one picture alone is drawn into both eyes");

	ForgetMirrorEyes(p);
	Check(!p.have[0] && !p.have[1] && p.age[1] == kMirrorNoPicture, "forgotten (the glass dark, the page down)");
	NoteMirrorEyeRendered(p, 3, ViewAt(0.0f), rectA);
	Check(!p.have[0] && !p.have[1], "an eye that is no eye: nothing");
}

void TestCost() {
	std::printf("The cost line's depth parts\n");
	MirrorCostWindow w;
	const MirrorPictureSize size{512, 512};
	NoteMirrorFrame(w, MirrorSkip::None, 1.0, 400, size);
	NoteMirrorExtraPass(w, 1.5, 600);
	NoteMirrorDepthFrame(w, MirrorDepthMode::Both, 2.5, true, true, true);
	NoteMirrorFrame(w, MirrorSkip::None, 2.0, 500, size);
	NoteMirrorDepthFrame(w, MirrorDepthMode::Turns, 2.0, false, true, false);
	NoteMirrorFrame(w, MirrorSkip::None, 1.0, 300, size);
	NoteMirrorDepthFrame(w, MirrorDepthMode::OneForBoth, 1.0, true, false, false);
	Check(w.passes == 4 && w.frames == 3, "four passes in three frames");
	Check(Near(static_cast<float>(MeanMs(w)), 5.5f / 4.0f), "the pass's mean over every pass");
	Check(Near(static_cast<float>(w.worstMs), 2.0f), "the worst single pass");
	Check(w.draws == 1800, "the draws of every pass");
	Check(w.modeFrames[static_cast<int>(MirrorDepthMode::Both)] == 1 &&
	          w.modeFrames[static_cast<int>(MirrorDepthMode::Turns)] == 1 &&
	          w.modeFrames[static_cast<int>(MirrorDepthMode::OneForBoth)] == 1,
	      "a frame in each mode");
	Check(w.eyePasses[0] == 2 && w.eyePasses[1] == 2, "the passes per eye (one for both counts as the left)");
	Check(Near(static_cast<float>(MeanFrameMs(w)), 5.5f / 3.0f) && Near(static_cast<float>(w.frameWorstMs), 2.5f),
	      "the passes together a frame: mean and worst");
	Check(w.stampAsked == 1 && w.stampChanged == 1, "the skin stamp between two passes: asked once, changed once");
	NoteMirrorShadowsKept(w, 3);
	NoteMirrorShadowsKept(w, 1);
	Check(w.shadowsKept == 4, "the shadow maps kept for the mirror's passes");
	NoteMirrorStampCounted(w, true);
	NoteMirrorStampCounted(w, false);
	NoteMirrorStampCounted(w, true);
	Check(w.stampCounted == 2, "the passes OBVR counted the skin stamp up for (game/MirrorStampLogic.h)");
	ResetMirrorCost(w);
	Check(w.modeFrames[1] == 0 && w.eyePasses[1] == 0 && w.stampAsked == 0 && w.shadowsKept == 0 &&
	          w.stampCounted == 0,
	      "reset");
	Check(Near(static_cast<float>(MeanFrameMs(w)), 0.0f), "no frame: a mean of 0");
}

}  // namespace

int main() {
	TestEyeCameras();
	TestBudgetClamp();
	TestBudget();
	TestMode();
	TestPlan();
	TestTurnsAlternate();
	TestPictures();
	TestCost();
	if (g_failures > 0) {
		std::printf("%d failing check(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

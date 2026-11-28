// Checks the holding hand's four fingers kept behind the tablet's page
// (vr/HeldFingers.h; docs/inventory-tablet-spec.md 32.2): the page as placed
// carried into the world, how deep a finger stands before its face, the
// finger's joints at a curl, the curl limited per finger by the page - kept
// as the controller says when behind, the nearest curl behind otherwise, the
// least deep when none is - the gate, only the holder's four fingers touched,
// the mirror, and the reach kept from shrinking while the fingers are held.

#include <chrono>
#include <cmath>
#include <cstdio>

#include "vr/HeldFingers.h"
#include "vr/LaserGeometry.h"

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

bool Near(float a, float b, float tolerance = 0.0005f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

// A hand at the origin, metres, its fingers along +x as a biped's bones
// point: the knuckle 9 cm out, the links 4.5 and 2.5 cm. A curl bends each
// link about its z, from +x towards +y.
HeldHandBones StraightHand(const NiMatrix33& rot = NiMatrix33::Identity()) {
	HeldHandBones h;
	h.valid = true;
	h.hand.rot = rot;
	h.hand.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	h.hand.scale = 1.0f;
	for (int f = 0; f < 4; ++f) {
		FingerChain& c = h.fingers[f];
		c.valid = true;
		c.local[0] = NiPoint3{0.09f, 0.0f, 0.02f * static_cast<float>(f) - 0.03f};
		c.local[1] = NiPoint3{0.045f, 0.0f, 0.0f};
		c.local[2] = NiPoint3{0.025f, 0.0f, 0.0f};
		c.scale[0] = c.scale[1] = c.scale[2] = 1.0f;
	}
	return h;
}

// A page facing +y at `out` along y, its middle `acrossAt` along x, 40 x 40 cm.
SheetInWorld SheetAt(float out, float acrossAt = 0.1f) {
	SheetInWorld s;
	s.valid = true;
	s.centre = NiPoint3{acrossAt, out, 0.0f};
	s.right = NiPoint3{1.0f, 0.0f, 0.0f};
	s.up = NiPoint3{0.0f, 0.0f, 1.0f};
	s.front = NiPoint3{0.0f, 1.0f, 0.0f};
	s.halfWidth = 0.2f;
	s.halfHeight = 0.2f;
	return s;
}

float DepthOf(const HeldHandBones& h, const SheetInWorld& s, int finger, float curl, bool rightBones = true) {
	NiPoint3 points[4];
	FingerPointsAt(h.hand, h.fingers[finger], rightBones, finger + 1, curl, points);
	return DepthBeforeSheet(s, points, 4, kFingerSkinMetres);
}

void TestPointsAndDepth() {
	std::printf("The finger's joints at a curl, and how deep before the page:\n");
	const HeldHandBones h = StraightHand();
	NiPoint3 open[4];
	NiPoint3 fist[4];
	FingerPointsAt(h.hand, h.fingers[0], true, 1, 0.0f, open);
	FingerPointsAt(h.hand, h.fingers[0], true, 1, 1.0f, fist);
	Check(NearPoint(open[0], NiPoint3{0.09f, 0.0f, -0.03f}), "the knuckle where the hand puts it, whatever the curl");
	Check(NearPoint(fist[0], open[0]), "the knuckle the same closed");
	Check(open[3].x > 0.17f && open[3].y > 0.0f && open[3].y < 0.05f, "open: the tip far out along the hand, a little up");
	Check(fist[1].y > open[1].y + 0.02f, "closed: the first link bent towards +y");
	Check(fist[3].x < open[3].x - 0.05f, "closed: the tip back towards the palm");
	const float tipPast = (open[3].x - open[2].x) / (open[2].x - open[1].x);
	Check(Near(tipPast, kIndexTipPastLastJoint, 0.02f), "the tip past the last joint as the index finger's tap tip (FingerTip.h)");
	const SheetInWorld far = SheetAt(0.30f);
	Check(DepthOf(h, far, 0, 1.0f) <= 0.0f, "a page 30 cm off: nothing before it");
	const SheetInWorld near = SheetAt(0.0f);
	Check(DepthOf(h, near, 0, 1.0f) > 0.0f, "a page through the knuckles: before it");
	const SheetInWorld aside = SheetAt(0.0f, 1.0f);
	Check(DepthOf(h, aside, 0, 1.0f) <= 0.0f, "a page beside the hand, the fingers not over its face: nothing before it");
	SheetInWorld none = near;
	none.valid = false;
	Check(DepthOf(h, none, 0, 1.0f) <= 0.0f, "no page: nothing before it");
	// The skin: a point 0.5 cm behind the face is still through it.
	const NiPoint3 p{0.1f, -0.005f, 0.0f};
	Check(DepthBeforeSheet(near, &p, 1, kFingerSkinMetres) > 0.0f, "a joint half a centimetre behind: its skin through");
	const NiPoint3 q{0.1f, -0.02f, 0.0f};
	Check(DepthBeforeSheet(near, &q, 1, kFingerSkinMetres) <= 0.0f, "two centimetres behind: clear");
}

void TestCurlSearch() {
	std::printf("The curl limited by the page:\n");
	// Before the page from a curl of 0.62 on.
	const auto rising = [](float c) { return c - 0.62f; };
	CurlLimit r = CurlBehindSheet(1.0f, CurlSide::None, rising);
	Check(r.limited && r.behind && r.curl <= 0.62f && r.curl > 0.60f, "closing through it: held at the page, 0.62");
	Check(r.side == CurlSide::Less, "on the open side");
	r = CurlBehindSheet(0.4f, CurlSide::None, rising);
	Check(!r.limited && r.behind && Near(r.curl, 0.4f), "already behind: the controller's curl as it is");
	// Before the page between 0.31 and 0.69 only.
	const auto middle = [](float c) { return c > 0.31f && c < 0.69f ? 0.01f : -0.01f; };
	r = CurlBehindSheet(0.5f, CurlSide::None, middle);
	Check(r.behind && r.curl <= 0.31f && r.curl > 0.29f, "both ways open: the open side first");
	r = CurlBehindSheet(0.5f, CurlSide::More, middle);
	Check(r.behind && r.curl >= 0.69f && r.curl < 0.71f, "the side it took last frame kept: no flip at the middle");
	r = CurlBehindSheet(0.6f, CurlSide::Less, middle);
	Check(r.behind && r.curl <= 0.31f && r.curl > 0.29f,
	      "the open side kept while the page is in the way, though the closed side is nearer now");
	r = CurlBehindSheet(0.6f, CurlSide::None, middle);
	Check(r.behind && r.curl >= 0.69f && r.curl < 0.71f, "with no side taken: the nearer");
	const auto closedOnly = [](float c) { return c < 0.69f ? 0.01f : -0.01f; };
	r = CurlBehindSheet(0.5f, CurlSide::Less, closedOnly);
	Check(r.behind && r.curl >= 0.69f && r.curl < 0.71f, "the side kept has no curl behind it: the other side");
	// Before it whatever the curl: the least deep.
	const auto always = [](float c) { return 0.05f + (c - 0.8f) * (c - 0.8f); };
	r = CurlBehindSheet(0.2f, CurlSide::None, always);
	Check(r.limited && !r.behind && Near(r.curl, 0.8f, 0.001f), "no curl behind it: the least deep, 0.8");
	r = CurlBehindSheet(1.7f, CurlSide::None, rising);
	Check(r.curl <= 0.62f, "a curl past 1 taken as 1");
	r = CurlBehindSheet(-0.5f, CurlSide::None, rising);
	Check(!r.limited && Near(r.curl, 0.0f), "below 0 taken as 0");
	// Continuous: a step of the controller's curl moves the limit by no more.
	const float a = CurlBehindSheet(0.90f, CurlSide::Less, rising).curl;
	const float b = CurlBehindSheet(0.95f, CurlSide::Less, rising).curl;
	Check(Near(a, b, 0.01f), "the controller's curl moving past the page: the finger stays at it");
}

void TestOnTheHand() {
	std::printf("On the drawn hand:\n");
	const HeldHandBones h = StraightHand();
	const SheetInWorld s = SheetAt(0.045f);
	const float wantedDepth = DepthOf(h, s, 0, 1.0f);
	Check(wantedDepth > 0.0f, "the fist's index finger through a page 4.5 cm up");
	const CurlLimit r = CurlBehindSheet(1.0f, CurlSide::None,
	                                    [&](float c) { return DepthOf(h, s, 0, c); });
	Check(r.limited && r.behind && r.curl < 1.0f, "limited, behind");
	Check(DepthOf(h, s, 0, r.curl) <= 0.0f, "its joints and tip behind the face at that curl");
	Check(DepthOf(h, s, 0, r.curl + 0.02f) > 0.0f, "and no further: the nearest curl behind");
	// The mirror: the hand turned over (the left-handed mirror's bones) and the page with it.
	const HeldHandBones m = StraightHand(MirrorX());
	SheetInWorld ms = s;
	ms.centre.x = -ms.centre.x;
	ms.right.x = -ms.right.x;
	const CurlLimit mr = CurlBehindSheet(1.0f, CurlSide::None,
	                                     [&](float c) { return DepthOf(m, ms, 0, c); });
	Check(mr.behind && Near(mr.curl, r.curl, 0.005f), "the mirrored hand held at the same curl");
	// The left hand's bones: their links are the right's mirror image (game::MirroredForLeft).
	const CurlLimit lr = CurlBehindSheet(1.0f, CurlSide::None,
	                                     [&](float c) { return DepthOf(h, s, 0, c, false); });
	Check(lr.behind && Near(lr.curl, r.curl, 0.005f), "the left bones' finger the same about its z");
}

HeldFingersInput HeldInput(bool holderRight) {
	HeldFingersInput in;
	in.holding = true;
	in.holderRight = holderRight;
	in.sheet = SheetAt(0.05f);
	in.bones = StraightHand();
	for (int f = 0; f < 5; ++f) {
		in.rightCurl[f] = 0.6f;
		in.leftCurl[f] = 0.6f;
	}
	return in;
}

void TestBothHands() {
	std::printf("Only the holding hand's four fingers:\n");
	HeldFingersInput in = HeldInput(false);
	HeldFingersOutput out = HoldFingersBehind(in);
	Check(out.any, "the left hand holds: its fingers limited");
	Check(Near(out.leftCurl[0], 0.6f), "its thumb as the controller says");
	bool fourLess = true;
	for (int f = 1; f < 5; ++f) {
		fourLess = fourLess && !Near(out.leftCurl[f], 0.6f) && out.limit[f - 1].limited;
	}
	Check(fourLess, "its four fingers each moved off the half-closed curl that reaches furthest");
	bool rightSame = true;
	for (int f = 0; f < 5; ++f) {
		rightSame = rightSame && Near(out.rightCurl[f], 0.6f);
	}
	Check(rightSame, "the tapping right hand untouched");
	in = HeldInput(true);
	out = HoldFingersBehind(in);
	bool leftSame = true;
	for (int f = 0; f < 5; ++f) {
		leftSame = leftSame && Near(out.leftCurl[f], 0.6f);
	}
	Check(out.any && !Near(out.rightCurl[1], 0.6f) && leftSame, "held by the right: the right limited, the tapping left untouched");
	in.holding = false;
	out = HoldFingersBehind(in);
	Check(!out.any && Near(out.rightCurl[1], 0.6f), "not holding: nothing limited");
	in = HeldInput(true);
	in.bones.valid = false;
	out = HoldFingersBehind(in);
	Check(!out.any && Near(out.rightCurl[1], 0.6f), "the bones not read: nothing limited");
	in = HeldInput(true);
	in.bones.fingers[1].valid = false;
	out = HoldFingersBehind(in);
	Check(out.any && Near(out.rightCurl[2], 0.6f) && !Near(out.rightCurl[1], 0.6f), "a finger's links not read: that finger as it is");
	in = HeldInput(true);
	in.sheet = SheetAt(0.30f);
	out = HoldFingersBehind(in);
	Check(!out.any && Near(out.rightCurl[4], 0.6f), "the fingers behind the page anyway: as the controller says");
}

void TestGate() {
	std::printf("When the fingers are held behind the page:\n");
	Check(FingersHeldBehindPage(true, -1.0f, false, true, true), "held in the hand, the fingers tracked, the reach read");
	Check(!FingersHeldBehindPage(false, -1.0f, false, true, true), "no tablet");
	Check(!FingersHeldBehindPage(true, 0.5f, false, true, true), "unrolling between the hands");
	Check(!FingersHeldBehindPage(true, -1.0f, true, true, true), "rolled up");
	Check(!FingersHeldBehindPage(true, -1.0f, false, false, true), "the holder's fingers not following the controller");
	Check(!FingersHeldBehindPage(true, -1.0f, false, true, false),
	      "the hand's reach not read yet: its first reading taken with the fingers free");
}

void TestKeptReach() {
	std::printf("The reach kept while the fingers are held:\n");
	HandReach kept;
	kept.valid = true;
	kept.beforeLeft = 0.063f;
	kept.beforeRight = 0.060f;
	HandReach read;
	read.valid = true;
	read.beforeLeft = 0.041f;
	read.beforeRight = 0.065f;
	const HandReach r = KeptReach(kept, read);
	Check(r.valid && Near(r.beforeLeft, 0.063f) && Near(r.beforeRight, 0.065f),
	      "each side the farthest: the page does not follow the fingers it holds back");
	HandReach none;
	Check(!KeptReach(kept, none).valid, "a misread stays a misread");
	const HandReach first = KeptReach(none, read);
	Check(first.valid && Near(first.beforeLeft, 0.041f), "nothing kept: the reading");
}

void TestSheetOfPlace() {
	std::printf("The page as placed, in the world:\n");
	openvr::HmdMatrix34 m{};
	// Its face along the controller's +z (towards the player), across +x, up +y; its middle 0.1 m up.
	m.m[0][0] = 1.0f;
	m.m[1][1] = 1.0f;
	m.m[2][2] = 1.0f;
	m.m[1][3] = 0.1f;
	const NiMatrix33 head = NiMatrix33::Identity();
	const NiMatrix33 hand = NiMatrix33::Identity();
	const NiPoint3 camera{10.0f, 20.0f, 30.0f};
	const NiPoint3 offset{1.0f, 2.0f, 3.0f};
	const SheetInWorld s = SheetOfPlace(m, 0.34f, 0.25f, head, hand, camera, offset, 70.0f);
	Check(s.valid, "placed");
	Check(NearPoint(s.centre, NiPoint3{11.0f, 22.0f, 33.0f + 7.0f}, 0.01f),
	      "its middle: the controller's place and 0.1 m up its y - the game's z");
	Check(NearPoint(s.front, NiPoint3{0.0f, -1.0f, 0.0f}), "its face: the controller's +z is the game's -y");
	Check(Near(s.halfWidth, 0.17f * 70.0f, 0.01f) && Near(s.halfHeight, 0.125f * 70.0f, 0.01f), "its half sides in units");
	const NiPoint3 back = ControllerLocalOf(head, hand, s.front);
	Check(NearPoint(back, NiPoint3{0.0f, 0.0f, 1.0f}), "carried back by the tap's own mapping: the same face");
	SheetInWorld unplaced = SheetOfPlace(m, 0.0f, 0.25f, head, hand, camera, offset, 70.0f);
	Check(!unplaced.valid, "no width: not placed");
}

// What the limit costs a frame: all four fingers through the page, measured
// (not a check; printed for the spec).
void TestCost() {
	std::printf("The cost:\n");
	const HeldFingersInput in = HeldInput(false);
	const int runs = 20000;
	float sink = 0.0f;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < runs; ++i) {
		sink += HoldFingersBehind(in).leftCurl[1];
	}
	const auto end = std::chrono::steady_clock::now();
	const double us = std::chrono::duration<double, std::micro>(end - start).count() / runs;
	std::printf("  four fingers through the page: %.2f us a frame (%d runs, %.1f)\n", us, runs,
	            static_cast<double>(sink));
	Check(us < 200.0, "well under a fiftieth of a frame's 11 ms");
}

}  // namespace

int main() {
	TestPointsAndDepth();
	TestCurlSearch();
	TestOnTheHand();
	TestBothHands();
	TestGate();
	TestKeptReach();
	TestSheetOfPlace();
	TestCost();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

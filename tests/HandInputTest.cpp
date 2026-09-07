// Checks the pure half of the hand-tracked mode: button bits in the legacy
// mask, and the shortest turn between two headings across the seam.

#include <cstdio>

#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"

namespace {

using obvr::vr::ButtonDown;
using obvr::vr::ShortestTurn;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 0.0001f && b - a < 0.0001f; }

void TestButtons() {
	std::printf("Buttons\n");

	const UInt64 trigger = 1ull << obvr::vr::openvr::kButtonTrigger;
	const UInt64 grip = 1ull << obvr::vr::openvr::kButtonGrip;
	Check(ButtonDown(trigger, obvr::vr::openvr::kButtonTrigger), "the trigger bit reads");
	Check(!ButtonDown(trigger, obvr::vr::openvr::kButtonGrip), "and is not the grip");
	Check(ButtonDown(trigger | grip, obvr::vr::openvr::kButtonGrip), "two down read both");
	Check(!ButtonDown(0, obvr::vr::openvr::kButtonApplicationMenu), "nothing down reads nothing");
	Check(!ButtonDown(1ull << 63, obvr::vr::openvr::kButtonTrigger),
	      "a high bit is not the trigger");
}

void TestShortestTurn() {
	std::printf("Shortest turn\n");

	const float pi = obvr::math::kPi;
	Check(Near(ShortestTurn(0.0f, 0.5f), 0.5f), "a small turn is itself");
	Check(Near(ShortestTurn(0.5f, 0.0f), -0.5f), "and back is its negative");
	Check(Near(ShortestTurn(pi - 0.1f, -pi + 0.1f), 0.2f),
	      "across the seam the turn is the short way round");
	Check(Near(ShortestTurn(-pi + 0.1f, pi - 0.1f), -0.2f), "in both directions");
	Check(Near(ShortestTurn(1.0f, 1.0f), 0.0f), "no difference is no turn");
	Check(Near(ShortestTurn(0.0f, 2.0f * pi), 0.0f), "a full circle is no turn");
}

void TestPoses() {
	std::printf("Poses and quads\n");
	using obvr::vr::ComposePose;
	using obvr::vr::MenuQuad;
	using obvr::vr::QuadFromPose;
	using obvr::vr::openvr::HmdMatrix34;

	// A head one metre up, turned ninety degrees to the left about the up
	// axis: its right is the world's -z... (right-handed, y up: a left turn
	// takes +x to -z and -z to -x). An overlay two metres ahead of that head
	// then sits at the head's position plus two metres of its forward, and
	// keeps its axes.
	HmdMatrix34 head{};
	head.m[0][2] = 1.0f;   // the head's back (+z) is the world's +x
	head.m[1][1] = 1.0f;   // up stays up
	head.m[2][0] = -1.0f;  // the head's right (+x) is the world's -z
	head.m[1][3] = 1.0f;
	HmdMatrix34 ahead{};
	ahead.m[0][0] = 1.0f;
	ahead.m[1][1] = 1.0f;
	ahead.m[2][2] = 1.0f;
	ahead.m[2][3] = -2.0f;
	const HmdMatrix34 pose = ComposePose(head, ahead);
	Check(Near(pose.m[0][3], -2.0f) && Near(pose.m[1][3], 1.0f) && Near(pose.m[2][3], 0.0f),
	      "two metres ahead of a head turned left is two metres along -x");
	Check(Near(pose.m[2][0], -1.0f) && Near(pose.m[1][1], 1.0f) && Near(pose.m[0][2], 1.0f),
	      "and the overlay carries the head's axes");

	HmdMatrix34 identity{};
	identity.m[0][0] = 1.0f;
	identity.m[1][1] = 1.0f;
	identity.m[2][2] = 1.0f;
	const HmdMatrix34 same = ComposePose(identity, ahead);
	Check(Near(same.m[2][3], -2.0f) && Near(same.m[0][3], 0.0f), "composed with nothing, unchanged");

	const MenuQuad quad = QuadFromPose(pose, 1.6f, 1920.0f, 1080.0f);
	Check(quad.valid, "a width and a picture make a quad");
	Check(Near(quad.centre.x, -2.0f) && Near(quad.centre.y, 1.0f), "at the pose's position");
	Check(Near(quad.right.z, -1.0f) && Near(quad.up.y, 1.0f), "with its right and up axes");
	Check(Near(quad.width, 1.6f) && Near(quad.height, 0.9f), "sixteen to nine at 1.6 m is 0.9 m");
	Check(!QuadFromPose(pose, 0.0f, 1920.0f, 1080.0f).valid, "no width, no quad");
	Check(!QuadFromPose(pose, 1.6f, 0.0f, 1080.0f).valid, "no picture, no quad");
}

void TestRepeat() {
	std::printf("Repeat\n");
	using obvr::vr::RepeatState;
	using obvr::vr::StepRepeat;
	RepeatState s;
	Check(!StepRepeat(s, false, 0.016f, 0.3f, 0.1f), "not held: nothing");
	Check(StepRepeat(s, true, 0.016f, 0.3f, 0.1f), "held: once at once");
	Check(!StepRepeat(s, true, 0.1f, 0.3f, 0.1f), "then quiet through the first delay");
	Check(!StepRepeat(s, true, 0.1f, 0.3f, 0.1f), "still quiet");
	Check(StepRepeat(s, true, 0.11f, 0.3f, 0.1f), "after the delay it repeats");
	Check(!StepRepeat(s, true, 0.05f, 0.3f, 0.1f), "not yet again");
	Check(StepRepeat(s, true, 0.06f, 0.3f, 0.1f), "every interval");
	Check(!StepRepeat(s, false, 0.5f, 0.3f, 0.1f), "released: nothing");
	Check(StepRepeat(s, true, 0.016f, 0.3f, 0.1f), "and held again starts over at once");
}

}  // namespace

// The cinema screen at infinity: a 1600x900 window, half-extents tan 0.5
// across and 0.28 down, anchored at the origin looking down -z.
obvr::vr::FlatPicture Cinema() {
	obvr::vr::FlatPicture flat;
	flat.valid = true;
	flat.tanHalfWidth = 0.5f;
	flat.tanHalfHeight = 0.28f;
	flat.pixelLeft = 0.0f;
	flat.pixelTop = 100.0f;
	flat.pixelWidth = 1600.0f;
	flat.pixelHeight = 900.0f;
	return flat;
}

void TestFlatLaser() {
	std::printf("The laser on the flat picture\n");
	using obvr::NiPoint3;
	using obvr::vr::FlatLaserHit;
	using obvr::vr::LaserOnFlatPicture;
	const obvr::vr::FlatPicture flat = Cinema();
	const NiPoint3 head{0.0f, 0.0f, 0.0f};
	const NiPoint3 ahead{0.0f, 0.0f, -1.0f};

	// A hand below the eyes pointing straight ahead: the beam ends on the
	// stand-in plane two metres out, and the head sees that point a little
	// below the picture's middle.
	FlatLaserHit hit = LaserOnFlatPicture(NiPoint3{0.0f, -0.2f, 0.0f}, ahead, head, flat, 2.0f);
	Check(hit.hit, "straight ahead hits");
	Check(Near(hit.lengthMetres, 2.0f), "the beam is two metres long");
	Check(Near(hit.pixelX, 800.0f), "in the middle across");
	// ty = -0.2/2 = -0.1; v = (1 + 0.1/0.28)/2; y = 100 + v*900
	Check(hit.pixelY > 100.0f + 450.0f && hit.pixelY < 100.0f + 900.0f,
	      "and below the middle, as the head sees it");

	// The same hand at the head's height: dead centre.
	hit = LaserOnFlatPicture(head, ahead, head, flat, 2.0f);
	Check(hit.hit && Near(hit.pixelX, 800.0f) && Near(hit.pixelY, 550.0f), "from the eyes: dead centre");

	// A hand held out to the right, pointing ahead: the point is 0.3 right
	// at 2 m, tan 0.15 of the 0.5 half-width, so 65% across.
	hit = LaserOnFlatPicture(NiPoint3{0.3f, 0.0f, 0.0f}, ahead, head, flat, 2.0f);
	Check(hit.hit && Near(hit.pixelX, 800.0f + 0.3f * 800.0f), "a hand to the right lands right of centre");

	// Turned past the edge: no hit. tan 0.5 at 2 m is 1 m; 1.2 m is past.
	const NiPoint3 wide{1.2f, 0.0f, -2.0f};
	const float wideLength = 2.3323808f;
	hit = LaserOnFlatPicture(head, NiPoint3{wide.x / wideLength, 0.0f, wide.z / wideLength}, head,
	                         flat, 2.0f);
	Check(!hit.hit, "past the picture's edge: no hit");

	// Pointing away, or the plane behind the hand: nothing.
	hit = LaserOnFlatPicture(head, NiPoint3{0.0f, 0.0f, 1.0f}, head, flat, 2.0f);
	Check(!hit.hit, "pointing away: no hit");
	hit = LaserOnFlatPicture(NiPoint3{0.0f, 0.0f, -3.0f}, ahead, head, flat, 2.0f);
	Check(!hit.hit, "beyond the plane: no hit");

	// Refusals: an invalid picture, no extent, no distance.
	obvr::vr::FlatPicture bad = flat;
	bad.valid = false;
	Check(!LaserOnFlatPicture(head, ahead, head, bad, 2.0f).hit, "an invalid picture: no hit");
	bad = flat;
	bad.tanHalfWidth = 0.0f;
	Check(!LaserOnFlatPicture(head, ahead, head, bad, 2.0f).hit, "no width: no hit");
	Check(!LaserOnFlatPicture(head, ahead, head, flat, 0.0f).hit, "no plane distance: no hit");

	// The anchor turned: the picture turns with it, and a beam along its
	// new forward is still its centre.
	obvr::vr::FlatPicture turned = flat;
	turned.forward = NiPoint3{1.0f, 0.0f, 0.0f};
	turned.right = NiPoint3{0.0f, 0.0f, 1.0f};
	hit = LaserOnFlatPicture(head, NiPoint3{1.0f, 0.0f, 0.0f}, head, turned, 2.0f);
	Check(hit.hit && Near(hit.pixelX, 800.0f) && Near(hit.pixelY, 550.0f),
	      "a turned anchor: its forward is still the centre");
	Check(!LaserOnFlatPicture(head, ahead, head, turned, 2.0f).hit,
	      "and the old forward now points along the picture: no hit");
}

int main() {
	TestButtons();
	TestShortestTurn();
	TestPoses();
	TestRepeat();
	TestFlatLaser();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

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

int main() {
	TestButtons();
	TestShortestTurn();
	TestPoses();
	TestRepeat();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

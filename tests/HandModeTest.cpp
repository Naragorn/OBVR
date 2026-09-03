// Checks the hand-tracked mode's pure decisions: the gestures, the swing
// detector, the held heavy attack, the trigger and button edges, the stick
// directions, the control planner in and out of menus, the laser's hit on
// a wrist quad, the cursor step, and the wrist transform.

#include <cstdio>

#include "vr/HandInput.h"
#include "vr/HandMode.h"
#include "vr/OpenVRTypes.h"

namespace {

using namespace obvr::vr;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

void TestGestures() {
	std::printf("Gestures\n");
	GestureThresholds t;
	Check(IsBlockGesture(NiPoint3{-0.1f, 0.3f, -0.2f}, t), "a hand up and out in front blocks");
	Check(!IsBlockGesture(NiPoint3{-0.1f, 0.3f, -0.6f}, t), "a hand hanging low does not");
	Check(!IsBlockGesture(NiPoint3{-0.1f, 0.05f, -0.2f}, t), "a hand at the chest does not");
	Check(IsReachBackGesture(NiPoint3{0.2f, -0.15f, -0.1f}, t), "a hand behind the head reaches back");
	Check(!IsReachBackGesture(NiPoint3{0.2f, 0.3f, -0.1f}, t), "a hand in front does not");
	Check(!IsReachBackGesture(NiPoint3{0.2f, -0.15f, -0.6f}, t), "a hand behind but low does not");
}

void TestSpeedAndSwing() {
	std::printf("Speed and swing\n");
	GestureThresholds t;
	Check(Near(HandSpeed(NiPoint3{0, 0, 0}, NiPoint3{0.03f, 0, 0}, 0.01f), 3.0f),
	      "three centimetres in ten milliseconds is three metres a second");
	Check(HandSpeed(NiPoint3{0, 0, 0}, NiPoint3{1, 0, 0}, 0.0f) == 0.0f, "no time is no speed");

	SwingDetector d;
	Check(StepSwing(d, 0.5f, t) == SwingVerdict::None, "a slow hand is idle");
	Check(StepSwing(d, 2.0f, t) == SwingVerdict::None, "a fast hand starts a swing, no verdict yet");
	Check(d.swinging, "and is swinging");
	Check(StepSwing(d, 2.5f, t) == SwingVerdict::None, "still swinging");
	Check(StepSwing(d, 0.3f, t) == SwingVerdict::Light, "slowing down ends it as a light swing");
	Check(!d.swinging, "and the detector is idle again");

	Check(StepSwing(d, 2.0f, t) == SwingVerdict::None, "a second swing starts");
	Check(StepSwing(d, 4.0f, t) == SwingVerdict::None, "peaks above the heavy speed");
	Check(StepSwing(d, 0.2f, t) == SwingVerdict::Heavy, "and ends heavy");

	HeldControl h;
	Check(!StepHeld(h, 0.016f), "nothing held reports nothing");
	HoldFor(h, 0.04f);
	Check(StepHeld(h, 0.016f), "held on the first frame");
	Check(StepHeld(h, 0.016f), "and the second");
	Check(StepHeld(h, 0.016f), "and the third");
	Check(!StepHeld(h, 0.016f), "released after the time is spent");
	HoldFor(h, 0.6f);
	HoldFor(h, 0.05f);
	Check(h.secondsLeft > 0.5f, "a shorter hold never cuts a longer one short");
}

void TestEdges() {
	std::printf("Edges\n");
	TriggerEdge e;
	Check(!StepTrigger(e, 0.4f), "a resting finger is not a press");
	Check(StepTrigger(e, 0.6f), "past the press point it is");
	Check(StepTrigger(e, 0.45f), "and stays pressed inside the hysteresis");
	Check(!StepTrigger(e, 0.3f), "until released");

	ButtonEdge b;
	Check(StepRisingEdge(b, true), "the frame a button goes down");
	Check(!StepRisingEdge(b, true), "not while it stays down");
	Check(!StepRisingEdge(b, false), "not when it comes up");
	Check(StepRisingEdge(b, true), "and again on the next press");

	StickDirections d = StickToDirections(0.1f, 0.9f, 0.4f);
	Check(d.forward && !d.back && !d.left && !d.right, "a stick pushed forward is forward");
	d = StickToDirections(-0.8f, -0.8f, 0.4f);
	Check(d.back && d.left && !d.forward && !d.right, "a diagonal is two directions");
	d = StickToDirections(0.2f, -0.2f, 0.4f);
	Check(!d.forward && !d.back && !d.left && !d.right, "inside the dead zone is nothing");
}

void TestPlanner() {
	std::printf("Planner\n");
	HandFrameInput in;
	in.rightValid = true;
	in.leftValid = true;
	in.rightTrigger = true;
	in.leftGrip = true;
	in.blockGesture = true;
	in.leftThumbY = 1.0f;
	in.rightThumbX = -0.5f;
	HandControlsWanted w = PlanHandControls(in, 0.4f);
	Check(w.attack, "the right trigger attacks");
	Check(w.activate, "the left grip activates");
	Check(w.block, "the raised left hand blocks");
	Check(w.move.forward, "the left stick walks");
	Check(Near(w.turn, -0.5f), "the right stick turns");
	Check(!w.menuClick && !w.cast && !w.grab, "nothing else is pressed");

	in.rightTrigger = false;
	in.swingAttackHeld = true;
	w = PlanHandControls(in, 0.4f);
	Check(w.attack, "a swing attacks without the trigger");

	HandFrameInput menu;
	menu.menuMode = true;
	menu.rightValid = true;
	menu.leftValid = true;
	menu.rightTrigger = true;
	menu.leftThumbY = 1.0f;
	menu.blockGesture = true;
	menu.leftMenuButton = true;
	w = PlanHandControls(menu, 0.4f);
	Check(w.menuClick && !w.attack, "in a menu the trigger clicks and does not attack");
	Check(!w.move.forward && !w.block, "and neither walks nor blocks");
	Check(w.menu, "the left menu button closes the menu");

	HandFrameInput oneHand;
	oneHand.rightValid = true;
	oneHand.leftValid = false;
	oneHand.leftGrip = true;
	oneHand.blockGesture = true;
	w = PlanHandControls(oneHand, 0.4f);
	Check(!w.activate && !w.block, "an untracked left hand presses nothing");
}

void TestLaser() {
	std::printf("Laser\n");
	// A quad one metre ahead, facing the origin, half a metre wide, a
	// quarter high, showing 1000x500 pixels.
	const NiPoint3 centre{0.0f, 0.0f, -1.0f};
	const NiPoint3 right{1.0f, 0.0f, 0.0f};
	const NiPoint3 up{0.0f, 1.0f, 0.0f};
	LaserHit hit = LaserOnQuad(NiPoint3{0, 0, 0}, NiPoint3{0, 0, -1}, centre, right, up, 0.5f,
	                           0.25f, 1000.0f, 500.0f);
	Check(hit.hit && Near(hit.pixelX, 500.0f) && Near(hit.pixelY, 250.0f),
	      "straight ahead hits the middle pixel");
	hit = LaserOnQuad(NiPoint3{0.2f, 0.1f, 0}, NiPoint3{0, 0, -1}, centre, right, up, 0.5f, 0.25f,
	                  1000.0f, 500.0f);
	Check(hit.hit && Near(hit.pixelX, 900.0f) && Near(hit.pixelY, 50.0f),
	      "right and up on the quad is right and up in pixels");
	hit = LaserOnQuad(NiPoint3{0.4f, 0, 0}, NiPoint3{0, 0, -1}, centre, right, up, 0.5f, 0.25f,
	                  1000.0f, 500.0f);
	Check(!hit.hit, "past the edge misses");
	hit = LaserOnQuad(NiPoint3{0, 0, 0}, NiPoint3{0, 0, 1}, centre, right, up, 0.5f, 0.25f,
	                  1000.0f, 500.0f);
	Check(!hit.hit, "pointing away misses");
	hit = LaserOnQuad(NiPoint3{0, 0, 0}, NiPoint3{1, 0, 0}, centre, right, up, 0.5f, 0.25f,
	                  1000.0f, 500.0f);
	Check(!hit.hit, "a ray along the quad misses");

	Check(CursorStep(100.0f, 200.0f, 0.5f, 60.0f) == 50, "half the way, under the cap");
	Check(CursorStep(0.0f, 1000.0f, 0.5f, 60.0f) == 60, "capped");
	Check(CursorStep(500.0f, 100.0f, 0.5f, 60.0f) == -60, "capped the other way");
	Check(CursorStep(10.0f, 10.0f, 0.5f, 60.0f) == 0, "no distance, no step");
}

void TestWristTransform() {
	std::printf("Wrist transform\n");
	const openvr::HmdMatrix34 m = WristOverlayTransform(0.06f, 0.12f, 0.0f);
	// With no tilt the quad's front (+z) turns to face straight up (+y).
	const float frontY = m.m[1][2];
	const float frontZ = m.m[2][2];
	Check(Near(frontY, 1.0f) && Near(frontZ, 0.0f), "the front faces up");
	Check(Near(m.m[1][3], 0.06f) && Near(m.m[2][3], 0.12f), "sits up and back on the wrist");
	Check(Near(m.m[0][0], 1.0f), "keeps the controller's right axis");
	const openvr::HmdMatrix34 tilted = WristOverlayTransform(0.0f, 0.0f, 90.0f);
	Check(Near(tilted.m[2][2], 1.0f), "at ninety degrees of tilt it faces forward again");
}

}  // namespace

int main() {
	TestGestures();
	TestSpeedAndSwing();
	TestEdges();
	TestPlanner();
	TestLaser();
	TestWristTransform();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

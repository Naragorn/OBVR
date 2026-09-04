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

	HandFrameInput gated;
	gated.rightValid = true;
	gated.rightTrigger = true;
	gated.drawBlocked = true;
	w = PlanHandControls(gated, 0.4f);
	Check(!w.attack, "a blocked draw does not attack on the trigger");
	gated.swingAttackHeld = true;
	w = PlanHandControls(gated, 0.4f);
	Check(w.attack, "but a swing still does");

	HandFrameInput motion;
	motion.rightValid = true;
	motion.rightTrigger = true;
	motion.meleeByMotion = true;
	w = PlanHandControls(motion, 0.4f);
	Check(!w.attack, "with a swung weapon striking by motion the trigger does not attack");
	Check(w.turn == 0.0f && !w.grab, "and nothing else changes");
	motion.rightGrip = true;
	w = PlanHandControls(motion, 0.4f);
	Check(w.grab, "the grip still grabs");
}

void TestStrikeByMotion() {
	std::printf("The swing for the strikes by motion\n");
	HandSettings settings;
	settings.enabled = true;
	HandModeFrame frame;
	frame.headValid = true;
	frame.dtSeconds = 0.01f;
	frame.right.valid = true;
	frame.right.position = NiPoint3{0.3f, -0.2f, -0.5f};
	frame.meleeInHand = true;
	HandMode mode;

	HandModeResult r = mode.Update(frame, settings);
	Check(r.strikeByMotion && !r.swingActive && r.swingSerial == 0,
	      "a melee weapon in hand: strikes by motion, no swing yet");

	// Three centimetres in ten milliseconds: three metres a second, a swing.
	frame.right.position = NiPoint3{0.33f, -0.2f, -0.5f};
	r = mode.Update(frame, settings);
	Check(r.swingActive && r.swingSerial == 1 && !r.swingHeavy, "a fast hand starts swing one, light");
	Check(!r.controls.attack && r.swing == SwingVerdict::None,
	      "and presses no attack control");
	frame.right.position = NiPoint3{0.37f, -0.2f, -0.5f};  // four metres a second
	r = mode.Update(frame, settings);
	Check(r.swingActive && r.swingHeavy && r.swingSerial == 1,
	      "past the heavy speed the same swing is heavy");
	frame.right.position = NiPoint3{0.371f, -0.2f, -0.5f};  // slowed down
	r = mode.Update(frame, settings);
	Check(!r.swingActive && r.swing == SwingVerdict::Heavy && !r.controls.attack,
	      "the swing ends heavy without holding the control");
	r = mode.Update(frame, settings);
	Check(!r.controls.attack, "and nothing is held after it either");
	frame.right.position = NiPoint3{0.40f, -0.2f, -0.5f};
	r = mode.Update(frame, settings);
	Check(r.swingActive && r.swingSerial == 2, "the next swing is number two");

	// The same swing with the attack control: no melee weapon in hand (a
	// bow, say) or strikes by motion switched off.
	frame.meleeInHand = false;
	HandMode byControl;
	frame.right.position = NiPoint3{0.3f, -0.2f, -0.5f};
	byControl.Update(frame, settings);
	frame.right.position = NiPoint3{0.34f, -0.2f, -0.5f};
	r = byControl.Update(frame, settings);
	Check(!r.strikeByMotion && r.swingActive, "without a melee weapon the swing is by control");
	frame.right.position = NiPoint3{0.341f, -0.2f, -0.5f};
	r = byControl.Update(frame, settings);
	Check(r.swing == SwingVerdict::Heavy && r.controls.attack,
	      "and a heavy swing holds the attack control as before");

	settings.motionHits = false;
	frame.meleeInHand = true;
	HandMode switchedOff;
	frame.right.position = NiPoint3{0.3f, -0.2f, -0.5f};
	switchedOff.Update(frame, settings);
	frame.right.position = NiPoint3{0.32f, -0.2f, -0.5f};
	r = switchedOff.Update(frame, settings);
	Check(!r.strikeByMotion && r.swingActive, "switched off, a melee weapon swings by control");
	frame.right.position = NiPoint3{0.321f, -0.2f, -0.5f};
	r = switchedOff.Update(frame, settings);
	Check(r.swing == SwingVerdict::Light && r.controls.attack, "and a light swing taps it");
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

void TestPoke() {
	std::printf("Poke\n");
	// The same quad the laser test uses: one metre ahead, facing the origin,
	// half a metre wide, a quarter high, 1000x500 pixels. Its front is +z,
	// towards the origin.
	const NiPoint3 centre{0.0f, 0.0f, -1.0f};
	const NiPoint3 right{1.0f, 0.0f, 0.0f};
	const NiPoint3 up{0.0f, 1.0f, 0.0f};
	const auto sample = [&](float x, float y, float z) {
		return PokeOnQuad(NiPoint3{x, y, z}, centre, right, up, 0.5f, 0.25f, 1000.0f, 500.0f);
	};

	PokeSample s = sample(0.0f, 0.0f, -0.95f);
	Check(s.inside && Near(s.depth, 0.05f) && Near(s.pixelX, 500.0f) && Near(s.pixelY, 250.0f),
	      "a tip five centimetres before the middle is inside, at the middle pixel");
	s = sample(0.2f, 0.1f, -1.02f);
	Check(s.inside && Near(s.depth, -0.02f) && Near(s.pixelX, 900.0f) && Near(s.pixelY, 50.0f),
	      "pushed through by two centimetres is a negative depth at the right pixel");
	s = sample(0.4f, 0.0f, -0.99f);
	Check(!s.inside, "past the edge is outside");
	s = PokeOnQuad(NiPoint3{0, 0, -0.99f}, centre, right, up, 0.0f, 0.25f, 1000.0f, 500.0f);
	Check(!s.inside, "a quad with no width takes no poke");

	// The state machine.
	PokeThresholds t;  // hover 0.10, press 0.015, release 0.04, through 0.06
	PokeState state;
	PokeVerdict v = StepPoke(state, sample(0.0f, 0.0f, -0.5f), t);
	Check(!v.hover && !v.press && !v.held, "half a metre out: nothing");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.92f), t);
	Check(v.hover && !v.press && !v.held, "within hover: the cursor follows, no click");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.99f), t);
	Check(v.hover && v.press && v.held, "touching: one press");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.995f), t);
	Check(v.hover && !v.press && v.held, "held on the surface: no second press");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.97f), t);
	Check(v.hover && !v.press && v.held, "three centimetres out is still held (hysteresis)");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.95f), t);
	Check(v.hover && !v.press && !v.held, "past release: let go, still hovering");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.99f), t);
	Check(v.press, "and touching again presses again");
	v = StepPoke(state, sample(0.0f, 0.0f, -1.1f), t);
	Check(!v.hover && !v.press && !v.held, "ten centimetres behind the quad is a hand behind it");
	v = StepPoke(state, sample(0.0f, 0.0f, -0.99f), t);
	Check(v.press, "coming back from behind presses afresh");
	v = StepPoke(state, sample(0.4f, 0.0f, -0.99f), t);
	Check(!v.hover && !v.held, "sliding off the edge releases");
	state = PokeState{};
	v = StepPoke(state, sample(0.0f, 0.0f, -1.03f), t);
	Check(v.press && v.held, "arriving already three centimetres through still presses");
}

void TestStickChord() {
	std::printf("Stick chord\n");
	StickChordState s;
	StickChordVerdict v = StepStickChord(s, true, false);
	Check(!v.rightClick && !v.leftClick && !v.both, "the right stick down alone fires nothing yet");
	v = StepStickChord(s, false, false);
	Check(v.rightClick && !v.leftClick && !v.both, "released alone: the right click");
	v = StepStickChord(s, false, true);
	v = StepStickChord(s, false, false);
	Check(v.leftClick && !v.rightClick, "the left stick likewise");

	v = StepStickChord(s, true, false);
	v = StepStickChord(s, true, true);
	Check(v.both && !v.rightClick && !v.leftClick, "the left joining the right is the chord");
	v = StepStickChord(s, true, true);
	Check(!v.both, "held together it fires once");
	v = StepStickChord(s, true, false);
	Check(!v.leftClick && !v.both, "the left releasing after a chord is no click");
	v = StepStickChord(s, false, false);
	Check(!v.rightClick && !v.both, "nor is the right");
	v = StepStickChord(s, true, true);
	Check(v.both, "and both down again from nothing is a chord again");
	v = StepStickChord(s, false, false);
	v = StepStickChord(s, false, false);
	Check(!v.rightClick && !v.leftClick && !v.both, "nothing down, nothing fires");
}

void TestStickNav() {
	std::printf("Stick navigation\n");
	StickNavState s;
	StickNavVerdict v = StepStickNav(s, 0.0f, 0.9f, 0.4f);
	Check(v.up && !v.down && !v.left && !v.right, "pushed up: up once");
	v = StepStickNav(s, 0.0f, 0.9f, 0.4f);
	Check(!v.up, "held up: nothing more");
	v = StepStickNav(s, 0.0f, 0.1f, 0.4f);
	Check(!v.up && !v.down, "in the dead zone: nothing");
	v = StepStickNav(s, 0.0f, 0.9f, 0.4f);
	Check(v.up, "up again after coming back");
	v = StepStickNav(s, -0.9f, -0.9f, 0.4f);
	Check(v.down && v.left && !v.up && !v.right, "down-left: both directions at once");
	v = StepStickNav(s, 0.9f, 0.0f, 0.4f);
	Check(v.right && !v.left && !v.down, "swinging to the right: right, the others let go");
}

void TestMenuHandAndSettingsMenu() {
	std::printf("Menu hand and OBVR's menu\n");
	HandSettings settings;
	settings.enabled = true;
	HandModeFrame frame;
	frame.headValid = true;
	frame.menuMode = true;
	frame.right.valid = true;
	frame.left.valid = true;
	HandMode mode;

	HandModeResult r = mode.Update(frame, settings);
	Check(r.menuOnWrist && r.menuWristRight, "by default the menu hangs on the right wrist");
	settings.menuOnRight = false;
	r = mode.Update(frame, settings);
	Check(r.menuOnWrist && !r.menuWristRight, "MenuOnRight=0 hangs it on the left");
	frame.left.valid = false;
	r = mode.Update(frame, settings);
	Check(!r.menuOnWrist, "and with that hand lost the menu leaves the wrist");

	// Both sticks: the chord toggles OBVR's menu; while it is open a stick
	// scrolls and nothing reaches the game.
	frame.left.valid = true;
	frame.menuMode = false;
	frame.right.buttonsPressed = 1ull << openvr::kButtonAxis0;
	r = mode.Update(frame, settings);
	Check(!r.settingsMenuToggle && !r.controls.readyWeapon,
	      "one stick down: no toggle, and its own click waits");
	frame.left.buttonsPressed = 1ull << openvr::kButtonAxis0;
	r = mode.Update(frame, settings);
	Check(r.settingsMenuToggle, "both down: the toggle");
	frame.right.buttonsPressed = 0;
	frame.left.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(!r.settingsMenuToggle && !r.controls.readyWeapon && !r.controls.quickMenu,
	      "released after a chord: nothing else fires");

	frame.settingsMenuOpen = true;
	frame.left.thumbY = 0.9f;
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.up && !r.controls.move.forward && !r.controls.attack,
	      "menu open: the stick is an arrow key, the trigger and the walk stay off");
	Check(r.controlsActive, "and the controls are actively released rather than left");
	frame.settingsMenuOpen = false;
	frame.left.thumbY = 0.0f;
	frame.right.trigger = 0.0f;
	r = mode.Update(frame, settings);
	Check(!r.settingsNav.up && !r.settingsNav.down, "menu closed: no arrows");
}

void TestHandPoses() {
	std::printf("Hand poses for the bone pin\n");
	HandSettings settings;
	settings.enabled = true;
	HandModeFrame frame;
	frame.headValid = true;
	frame.unitsPerMetre = 70.0f;
	frame.right.valid = true;
	frame.right.position = NiPoint3{0.3f, -0.2f, -0.5f};  // OpenVR: right, down, ahead
	frame.left.valid = false;
	HandMode mode;

	HandModeResult r = mode.Update(frame, settings);
	Check(r.rightHandValid && !r.leftHandValid, "a tracked right hand and an untracked left");
	// Game axes: x right, y forward, z up; the offset is metres times units.
	Check(Near(r.rightHandOffsetUnits.x, 21.0f) && Near(r.rightHandOffsetUnits.y, 35.0f) &&
	          Near(r.rightHandOffsetUnits.z, -14.0f),
	      "the right hand's offset is in game axes and units");
	Check(Near(r.rightHandRotation.data[0][0], 1.0f) && Near(r.rightHandRotation.data[1][1], 1.0f),
	      "an unturned hand relative to an unturned head is the identity");

	frame.left.valid = true;
	frame.left.position = NiPoint3{-0.3f, 0.0f, -0.4f};
	frame.menuMode = true;
	r = mode.Update(frame, settings);
	Check(r.leftHandValid && Near(r.leftHandOffsetUnits.x, -21.0f) &&
	          Near(r.leftHandOffsetUnits.y, 28.0f),
	      "the left hand too, and in a menu as well");
	Check(!r.armsValid, "while the arms placement stays off in a menu");

	frame.firstPerson = false;
	r = mode.Update(frame, settings);
	Check(!r.rightHandValid && !r.leftHandValid, "no hand poses in third person");
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
	TestPoke();
	TestStickChord();
	TestStickNav();
	TestMenuHandAndSettingsMenu();
	TestHandPoses();
	TestWristTransform();
	TestStrikeByMotion();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

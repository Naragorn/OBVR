// Checks the hand-tracked mode's pure decisions: the gestures, the swing
// detector, the held heavy attack, the trigger and button edges, the stick
// directions, the control planner in and out of menus, the laser's hit on
// a wrist quad, the cursor step, and the wrist transform.

#include <cstdio>

#include "game/FirstPersonDepth.h"
#include "game/KeyScanCodes.h"
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
	Check(w.grab, "the left grip grabs");
	Check(!w.activate, "and does not also activate, which would take the object");
	Check(w.block, "the raised left hand blocks");
	Check(w.move.forward, "the left stick walks");
	Check(Near(w.turn, -0.5f), "the right stick turns");
	Check(!w.menuClick && !w.cast && !w.sneak && !w.quickMenu, "nothing else is pressed");

	HandFrameInput right;
	right.rightValid = true;
	right.leftValid = true;
	right.rightGrip = true;
	w = PlanHandControls(right, 0.4f);
	Check(w.grab && !w.activate, "the right grip grabs too");

	HandFrameInput left;
	left.rightValid = true;
	left.leftValid = true;
	left.leftA = true;
	left.leftStickClick = true;
	left.leftStickHeld = true;
	left.leftTrackpadClick = true;
	w = PlanHandControls(left, 0.4f);
	Check(!w.activate && !w.grab, "left A no longer activates");
	HandFrameInput rightA;
	rightA.rightValid = true;
	rightA.leftValid = true;
	rightA.rightA = true;
	Check(PlanHandControls(rightA, 0.4f).activate, "right A activates, for a right-handed player");
	rightA.rightValid = false;
	Check(!PlanHandControls(rightA, 0.4f).activate, "not from an untracked right hand");
	HandFrameInput lefty;
	lefty.rightValid = true;
	lefty.leftValid = true;
	lefty.leftHanded = true;
	lefty.rightA = true;
	Check(!PlanHandControls(lefty, 0.4f).activate, "left-handed: the right A no longer activates");
	lefty.rightA = false;
	lefty.leftA = true;
	Check(PlanHandControls(lefty, 0.4f).activate, "the left A does");
	lefty.leftValid = false;
	Check(!PlanHandControls(lefty, 0.4f).activate, "not from an untracked left hand");
	Check(w.run && !w.sneak, "the left stick held in runs, and its click no longer sneaks");
	Check(w.quickMenu, "the left trackpad click opens the quick menu");

	HandFrameInput flicks;
	flicks.rightValid = true;
	flicks.leftValid = true;
	flicks.rightA = true;
	flicks.rightStickUp = true;
	w = PlanHandControls(flicks, 0.4f);
	Check(w.jump && !w.sneak, "the right stick flicked up jumps");
	flicks.rightStickUp = false;
	w = PlanHandControls(flicks, 0.4f);
	Check(!w.jump, "and right A no longer does");
	flicks.rightStickDown = true;
	w = PlanHandControls(flicks, 0.4f);
	Check(w.sneak && !w.jump, "flicked down sneaks");
	flicks.rightValid = false;
	w = PlanHandControls(flicks, 0.4f);
	Check(!w.sneak, "an untracked right hand neither jumps nor sneaks");

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
	oneHand.leftA = true;
	oneHand.blockGesture = true;
	w = PlanHandControls(oneHand, 0.4f);
	Check(!w.activate && !w.block && !w.grab, "an untracked left hand presses nothing");

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

void TestGamepadPlanner() {
	std::printf("The gamepad layout\n");
	GamepadInput in;
	in.rightValid = true;
	in.leftValid = true;
	in.rightTrigger = true;
	in.leftTrigger = true;
	in.rightGrip = true;
	in.leftGrip = true;
	in.rightA = true;
	in.leftA = true;
	in.rightB = true;
	in.leftB = true;
	in.rightStickClick = true;
	in.leftStickClick = true;
	in.rightTrackpadClick = true;
	in.leftTrackpadClick = true;
	in.leftThumbX = -0.9f;
	in.leftThumbY = 0.0f;
	in.rightThumbX = 0.7f;
	HandControlsWanted w = PlanGamepadControls(in, 0.4f);
	Check(w.attack && w.block, "the triggers attack and block");
	Check(w.cast && w.grab, "the grips cast and grab");
	Check(w.jump && w.activate, "the A buttons jump and activate");
	Check(w.escape && w.menu, "the B buttons are Escape and Tab");
	Check(w.togglePov && w.sneak, "the stick clicks switch the view and sneak");
	Check(w.readyWeapon && w.quickMenu, "the trackpad clicks ready the weapon and open the quick menu");
	Check(w.move.left && !w.move.right && !w.move.forward, "the left stick walks");
	Check(Near(w.turn, 0.7f), "the right stick turns");
	Check(!w.menuClick, "no menu click in the world");

	GamepadInput one;
	one.rightValid = true;
	one.leftTrigger = true;
	one.leftA = true;
	one.leftThumbY = 1.0f;
	w = PlanGamepadControls(one, 0.4f);
	Check(!w.block && !w.activate && !w.move.forward, "an untracked left hand presses nothing");
	GamepadInput none;
	w = PlanGamepadControls(none, 0.4f);
	Check(!w.attack && !w.jump && w.turn == 0.0f, "nothing tracked presses nothing");

	// The gamepad in the world through the mode itself: the tapped buttons
	// are edges, held ones are held, the stick chord still opens OBVR's menu.
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = false;
	HandModeFrame frame;
	frame.headValid = true;
	frame.menusOnly = true;
	frame.menuMode = false;
	frame.right.valid = true;
	frame.left.valid = true;
	HandMode mode;
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexB;
	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexA;
	HandModeResult r = mode.Update(frame, settings);
	Check(r.controlsActive && r.controls.escape && r.controls.activate, "right B is Escape, left A activates");
	r = mode.Update(frame, settings);
	Check(!r.controls.escape && r.controls.activate, "Escape once, activate held");
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	frame.left.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(!r.controls.togglePov, "a stick click waits for its release");
	frame.right.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(r.controls.togglePov, "released alone, it switches the view");
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexTrackpad;
	r = mode.Update(frame, settings);
	Check(r.controls.readyWeapon, "the right trackpad click readies the weapon");
	frame.right.buttonsPressed = 0;
	mode.Update(frame, settings);
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	r = mode.Update(frame, settings);
	Check(r.settingsMenuToggle && !r.controls.togglePov && !r.controls.sneak,
	      "both sticks together: OBVR's menu, no view switch, no sneak");
	frame.right.buttonsPressed = 0;
	frame.left.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(!r.controls.togglePov && !r.controls.sneak, "and their release fires nothing");
}

void TestStrikeByMotion() {
	std::printf("The swing for the strikes by motion\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
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
	// bow, say) or strikes by motion switched off. Drawn: a sheathed weapon
	// is readied by the attack control, so there a swing presses nothing.
	frame.meleeInHand = false;
	frame.weaponSeen = WeaponSeen::Sheathed;
	{
		HandMode sheathed;
		frame.right.position = NiPoint3{0.3f, -0.2f, -0.5f};
		sheathed.Update(frame, settings);
		frame.right.position = NiPoint3{0.34f, -0.2f, -0.5f};
		sheathed.Update(frame, settings);
		frame.right.position = NiPoint3{0.341f, -0.2f, -0.5f};
		r = sheathed.Update(frame, settings);
		Check(r.swing == SwingVerdict::Heavy && !r.controls.attack,
		      "sheathed: the same swing does not attack, so it cannot draw the fists");
	}
	frame.weaponSeen = WeaponSeen::Drawn;
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
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = true;
	settings.wristMenu = true;  // off by default; this is the wrist's own test
	settings.wristHud = true;
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
	Check(r.laserVisible && r.laserRight, "the right hand points at it, with the beam");
	frame.left.valid = false;
	r = mode.Update(frame, settings);
	Check(!r.menuOnWrist, "and with that hand lost the menu leaves the wrist");
	frame.left.valid = true;
	frame.inWorld = false;
	r = mode.Update(frame, settings);
	Check(!r.menuOnWrist, "before a game is loaded the menu stays off the wrist");
	Check(r.laserVisible && r.laserRight, "and the right hand points at the big quad");
	frame.inWorld = true;

	// Both sticks: the chord toggles OBVR's menu; while it is open a stick
	// scrolls and nothing reaches the game.
	frame.left.valid = true;
	frame.menuMode = false;
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	r = mode.Update(frame, settings);
	Check(!r.settingsMenuToggle && !r.controls.readyWeapon,
	      "one stick down: no toggle, and its own click waits");
	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
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

// A quad a metre ahead of an unturned head, 0.8 wide, showing 800x600.
HandModeFrame BigQuadFrame() {
	HandModeFrame frame;
	frame.headValid = true;
	frame.menuMode = true;
	frame.inWorld = false;
	frame.layerPixelsWidth = 800.0f;
	frame.layerPixelsHeight = 600.0f;
	frame.cursorValid = true;
	frame.cursorX = 400.0f;
	frame.cursorY = 300.0f;
	frame.menuQuad.valid = true;
	frame.menuQuad.centre = NiPoint3{0.0f, 0.0f, -1.0f};
	frame.menuQuad.right = NiPoint3{1.0f, 0.0f, 0.0f};
	frame.menuQuad.up = NiPoint3{0.0f, 1.0f, 0.0f};
	frame.menuQuad.width = 0.8f;
	frame.menuQuad.height = 0.6f;
	frame.right.valid = true;
	frame.right.position = NiPoint3{0.1f, -0.1f, 0.0f};  // a little right and below the eyes
	return frame;
}

void TestLaserOnBigQuad() {
	std::printf("The laser on the big quad\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = true;
	HandModeFrame frame = BigQuadFrame();
	HandMode mode;

	// Pointing straight ahead from 0.1 right, 0.1 down: the hit is 0.1 right
	// of and 0.1 below the quad's centre - pixel 500, 400 - and the cursor
	// at the middle steps towards it by the gain.
	HandModeResult r = mode.Update(frame, settings);
	Check(r.laserHit, "the ray meets the quad");
	Check(r.cursorDx >= 49 && r.cursorDx <= 50 && r.cursorDy >= 49 && r.cursorDy <= 50,
	      "and the cursor walks half the way there");
	Check(r.laserVisible && r.laserRight && Near(r.laserLengthMetres, 1.0f, 0.01f),
	      "the beam is the right hand's and reaches the quad");
	Check(!r.menuOnWrist, "nothing on a wrist");

	frame.right.valid = false;
	frame.left.valid = true;
	frame.left.position = NiPoint3{0.0f, 0.0f, 0.0f};
	r = mode.Update(frame, settings);
	Check(r.laserHit && !r.laserRight && r.cursorDx == 0 && r.cursorDy == 0,
	      "only the left hand tracked: it points, dead centre");

	// The right hand back, its trigger pulled: that pull takes the pointer
	// back from the left hand and is spent on the move. The menu buttons are
	// Tab and Escape regardless.
	frame.right.valid = true;
	frame.right.trigger = 1.0f;
	frame.left.buttonsPressed = 1ull << openvr::kButtonApplicationMenu;
	r = mode.Update(frame, settings);
	Check(r.laserRight && !r.controls.menuClick && r.controls.menu,
	      "the pull brings the pointer back without clicking; the left menu button is Tab");
	frame.right.trigger = 0.0f;
	frame.left.buttonsPressed = 0;
	mode.Update(frame, settings);
	frame.right.trigger = 1.0f;
	frame.left.buttonsPressed = 1ull << openvr::kButtonApplicationMenu;
	r = mode.Update(frame, settings);
	Check(r.controls.menuClick && r.controls.menu, "the trigger clicks, the left menu button is Tab");

	// The left stick is the wheel: a notch on the flick, quiet, then repeats.
	frame.right.trigger = 0.0f;
	frame.left.buttonsPressed = 0;
	frame.left.thumbY = 1.0f;
	frame.dtSeconds = 0.016f;
	r = mode.Update(frame, settings);
	Check(r.menuScroll == 1, "the stick up is a notch up");
	r = mode.Update(frame, settings);
	Check(r.menuScroll == 0, "and then waits");
	frame.dtSeconds = 0.4f;
	r = mode.Update(frame, settings);
	Check(r.menuScroll == 1, "until the first delay is over");
	frame.left.thumbY = -1.0f;
	r = mode.Update(frame, settings);
	Check(r.menuScroll == -1, "down is a notch down");
	frame.left.thumbY = 0.0f;
	frame.menuMode = false;
	r = mode.Update(frame, settings);
	Check(!r.laserHit && r.menuScroll == 0, "no menu: no laser hit, no wheel");

	// A quad the frame does not know: nothing to point at, the beam still
	// shows at its default length.
	frame.menuMode = true;
	frame.menuQuad.valid = false;
	r = mode.Update(frame, settings);
	Check(!r.laserHit && r.laserVisible && Near(r.laserLengthMetres, 1.5f, 0.01f),
	      "no quad: no hit, a beam of default length");
}

void TestMenusOnly() {
	std::printf("The mode off, the controllers on the menus\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = false;
	HandModeFrame frame = BigQuadFrame();
	frame.menusOnly = true;
	HandMode mode;

	HandModeResult r = mode.Update(frame, settings);
	Check(r.laserHit && r.cursorDx >= 49 && r.cursorDx <= 50 && r.laserVisible,
	      "the laser points at the game's menu");
	Check(!r.aimValid && !r.armsValid && !r.rightHandValid, "with no aim, no arms, no hand poses");
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.controls.menuClick && r.controlsActive, "the trigger clicks");
	Check(!r.controls.attack && !r.controls.grab, "and nothing else is pressed");

	// Out of the menu, in the world: nothing at all reaches the game.
	frame.menuMode = false;
	frame.right.trigger = 1.0f;
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexGrip;
	frame.left.valid = true;
	frame.left.thumbY = 1.0f;
	settings.gamepadLayout = false;
	r = mode.Update(frame, settings);
	Check(!r.controlsActive && !r.controls.attack && !r.controls.grab &&
	          !r.controls.move.forward && !r.laserVisible,
	      "in the world, without the gamepad layout, the controllers press nothing and the beam is off");

	// With the gamepad layout the same hands play: the trigger attacks, the
	// right grip casts, the left stick walks.
	settings.gamepadLayout = true;
	r = mode.Update(frame, settings);
	Check(r.controlsActive && r.controls.attack && r.controls.cast && r.controls.move.forward &&
	          !r.controls.grab && !r.laserVisible,
	      "with the gamepad layout: attack, cast, walk - no grab, no beam");
	frame.right.trigger = 0.0f;
	frame.right.buttonsPressed = 0;
	frame.left.thumbY = 0.0f;
	mode.Update(frame, settings);

	// Both sticks: OBVR's menu; in it the sticks and buttons steer.
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	frame.left.thumbY = 0.0f;
	r = mode.Update(frame, settings);
	Check(r.settingsMenuToggle, "both sticks clicked: the toggle");
	frame.right.buttonsPressed = 0;
	frame.left.buttonsPressed = 0;
	frame.settingsMenuOpen = true;
	frame.right.trigger = 0.0f;
	r = mode.Update(frame, settings);
	Check(!r.settingsNav.right && !r.settingsMenuToggle, "open, nothing pressed: nothing");
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.right, "the trigger is Right");
	r = mode.Update(frame, settings);
	Check(!r.settingsNav.right, "once, while it stays pulled");
	frame.right.trigger = 0.0f;
	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexGrip;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.left, "a grip is Left");
	frame.left.buttonsPressed = 1ull << openvr::kButtonA;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.right, "an A is Right");
	frame.left.buttonsPressed = 0;
	frame.right.thumbY = -1.0f;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.down && r.laserVisible, "the stick is Down, and the beam shows");
	frame.right.thumbY = 0.0f;
	frame.right.buttonsPressed = 1ull << openvr::kButtonApplicationMenu;
	r = mode.Update(frame, settings);
	Check(r.settingsMenuToggle, "a menu button closes it");

	// Switched off entirely: nothing.
	frame.menusOnly = false;
	frame.settingsMenuOpen = false;
	frame.menuMode = true;
	r = mode.Update(frame, settings);
	Check(!r.laserHit && !r.laserVisible && !r.controlsActive, "ControllerMenus off: nothing");
}

void TestHandPoses() {
	std::printf("Hand poses for the bone pin\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
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

// The main menu: no quad, the cinema screen two metres ahead of an
// unturned anchor at the origin, showing 1600x900 from pixel 0,100.
HandModeFrame MainMenuFrame() {
	HandModeFrame frame;
	frame.headValid = true;
	frame.menuMode = true;
	frame.inWorld = false;
	frame.menusOnly = true;
	frame.cursorValid = true;
	frame.cursorX = 800.0f;
	frame.cursorY = 550.0f;
	frame.menuQuad.valid = false;
	frame.flat.valid = true;
	frame.flat.tanHalfWidth = 0.5f;
	frame.flat.tanHalfHeight = 0.28f;
	frame.flat.pixelLeft = 0.0f;
	frame.flat.pixelTop = 100.0f;
	frame.flat.pixelWidth = 1600.0f;
	frame.flat.pixelHeight = 900.0f;
	frame.right.valid = true;
	frame.right.position = NiPoint3{0.3f, 0.0f, 0.0f};  // held out to the right
	frame.left.valid = true;
	frame.left.position = NiPoint3{-0.3f, 0.0f, 0.0f};
	return frame;
}

void TestMainMenuLaser() {
	std::printf("The laser on the main menu's cinema screen, and the hand that holds it\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = false;
	settings.laserGain = 1.0f;
	settings.laserMaxStep = 4096.0f;
	HandModeFrame frame = MainMenuFrame();
	HandMode mode;

	// With no quad the beam used to end in the air; now it meets the picture.
	HandModeResult r = mode.Update(frame, settings);
	Check(r.laserHit && r.laserVisible && r.laserRight, "the right hand's beam hits the picture");
	Check(Near(r.laserLengthMetres, 2.0f, 0.01f), "and ends at the stand-in plane");
	Check(r.cursorDx == 240 && r.cursorDy == 0,
	      "the cursor walks to where the head sees the beam's end: 240 right of centre");
	Check(!r.pokeHover, "no finger on a picture at infinity");

	// The right trigger: the click, from the right hand.
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.controls.menuClick && r.laserRight, "the right trigger clicks");
	frame.right.trigger = 0.0f;
	r = mode.Update(frame, settings);
	Check(!r.controls.menuClick, "and releases");

	// The left trigger takes the pointer to the left hand. That pull is not
	// a click: the cursor is still where the right hand left it.
	frame.left.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(!r.laserRight && r.laserVisible, "a pull on the left takes the pointer left");
	Check(!r.controls.menuClick, "and that pull is not a click");
	Check(r.cursorDx == -240, "the cursor now walks to the left hand's beam");
	r = mode.Update(frame, settings);
	Check(!r.controls.menuClick, "nor is holding it");
	frame.left.trigger = 0.0f;
	r = mode.Update(frame, settings);
	Check(!r.laserRight && !r.controls.menuClick, "released, the left hand keeps the pointer");
	frame.left.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(!r.laserRight && r.controls.menuClick, "pulled again, the left hand clicks");
	frame.left.trigger = 0.0f;
	mode.Update(frame, settings);

	// The right trigger while the left holds the pointer: takes it back,
	// without clicking.
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.laserRight && !r.controls.menuClick, "the right trigger takes it back, no click");
	frame.right.trigger = 0.0f;
	r = mode.Update(frame, settings);

	// The pointing hand goes untracked: the other one points.
	frame.right.valid = false;
	r = mode.Update(frame, settings);
	Check(!r.laserRight && r.laserHit, "the right hand lost: the left points");
	frame.right.valid = true;
	r = mode.Update(frame, settings);
	Check(!r.laserRight, "back, but the left keeps the pointer until a pull");

	// Both triggers on one frame: no change of hands.
	frame.right.trigger = 1.0f;
	frame.left.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(!r.laserRight, "both pulled at once: the hand stays");
	frame.right.trigger = 0.0f;
	frame.left.trigger = 0.0f;
	mode.Update(frame, settings);

	// No head pose: the mode stands down as a whole (its first gate), so
	// neither hit nor beam - the picture's pixel could not be told anyway.
	frame.headValid = false;
	r = mode.Update(frame, settings);
	Check(!r.laserHit && !r.laserVisible && r.cursorDx == 0,
	      "no head pose: nothing points and nothing moves");
	frame.headValid = true;

	// A quad present wins over the picture.
	frame.menuQuad = BigQuadFrame().menuQuad;
	frame.layerPixelsWidth = 800.0f;
	frame.layerPixelsHeight = 600.0f;
	r = mode.Update(frame, settings);
	Check(r.laserHit && Near(r.laserLengthMetres, 1.0f, 0.05f), "with a quad, the quad is the target");

	// The full mode, in a menu: the same pointer hand and click.
	settings.enabled = true;
	frame.menusOnly = false;
	frame.menuQuad.valid = false;
	frame.layerPixelsWidth = 0.0f;
	frame.layerPixelsHeight = 0.0f;
	HandMode full;
	r = full.Update(frame, settings);
	Check(r.laserHit && r.laserRight, "the full mode points at the picture too");
	frame.left.trigger = 1.0f;
	r = full.Update(frame, settings);
	Check(!r.laserRight && !r.controls.menuClick, "and switches hands on the left trigger, no click");
	frame.left.trigger = 0.0f;
	full.Update(frame, settings);
	frame.left.trigger = 1.0f;
	r = full.Update(frame, settings);
	Check(!r.laserRight && r.controls.menuClick, "the next pull clicks");
	frame.left.trigger = 0.0f;
	frame.menuMode = false;
	r = full.Update(frame, settings);
	Check(!r.laserVisible, "out of the menu: no beam");
}

void TestLaserOnOwnPanel() {
	std::printf("The laser on OBVR's own panel\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = false;
	HandModeFrame frame = MainMenuFrame();
	frame.settingsMenuOpen = true;
	// The panel a metre ahead, 0.8 wide, painted on 1024x768.
	frame.settingsQuad.valid = true;
	frame.settingsQuad.centre = NiPoint3{0.0f, 0.0f, -1.0f};
	frame.settingsQuad.right = NiPoint3{1.0f, 0.0f, 0.0f};
	frame.settingsQuad.up = NiPoint3{0.0f, 1.0f, 0.0f};
	frame.settingsQuad.width = 0.8f;
	frame.settingsQuad.height = 0.6f;
	frame.settingsPixelsWidth = 1024.0f;
	frame.settingsPixelsHeight = 768.0f;
	// The right hand at the eyes, pointing a little down and right.
	frame.right.position = NiPoint3{0.0f, 0.0f, 0.0f};
	HandMode mode;

	HandModeResult r = mode.Update(frame, settings);
	Check(r.settingsPointerValid && r.laserVisible && r.laserRight,
	      "the right hand's beam lands on the panel");
	Check(Near(r.settingsPointerX, 512.0f, 1.0f) && Near(r.settingsPointerY, 384.0f, 1.0f),
	      "straight ahead is the panel's middle");
	Check(Near(r.laserLengthMetres, 1.0f, 0.01f), "the beam ends on the panel");
	Check(!r.settingsClick && !r.settingsNav.right, "nothing pulled: no click, no Right");

	// A pull on the pointing hand is a click on the row, not the stick's Right.
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.settingsClick && !r.settingsNav.right, "the pull is a click, and not Right");
	r = mode.Update(frame, settings);
	Check(!r.settingsClick, "once, while it stays pulled");
	frame.right.trigger = 0.0f;
	mode.Update(frame, settings);

	// The left trigger moves the pointer to the left hand without clicking
	// or stepping; the left hand, held out left, misses the panel.
	frame.left.position = NiPoint3{-2.0f, 0.0f, 0.0f};
	frame.left.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(!r.laserRight && !r.settingsClick && !r.settingsNav.right,
	      "a pull on the left moves the pointer: no click, no Right");
	Check(!r.settingsPointerValid && Near(r.laserLengthMetres, 1.0f, 0.01f),
	      "the left hand misses the panel: a metre of beam, no pixel");
	frame.left.trigger = 0.0f;
	mode.Update(frame, settings);

	// Off the panel, a pull is the stick's Right, as before.
	frame.left.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.right && !r.settingsClick, "off the panel the pull is Right");
	frame.left.trigger = 0.0f;
	mode.Update(frame, settings);

	// Back to the right hand with a pull: the pointer moves, nothing fires.
	frame.right.trigger = 1.0f;
	r = mode.Update(frame, settings);
	Check(r.laserRight && !r.settingsClick && !r.settingsNav.right,
	      "the right pull takes the pointer back, nothing fires");
	frame.right.trigger = 0.0f;
	mode.Update(frame, settings);

	// Pointing at the lower right of the panel: the pixel follows.
	frame.right.position = NiPoint3{0.2f, -0.15f, 0.0f};
	r = mode.Update(frame, settings);
	Check(r.settingsPointerValid && r.settingsPointerX > 700.0f && r.settingsPointerY > 500.0f,
	      "a hand out to the right and below lands right and below");

	// The A button is still Right, panel or no panel.
	frame.right.buttonsPressed = 1ull << openvr::kButtonA;
	r = mode.Update(frame, settings);
	Check(r.settingsNav.right, "A is Right");
	frame.right.buttonsPressed = 0;

	// The beam on the panel's bottom band scrolls down: a notch at once,
	// quiet through the first delay, then repeats; the top band scrolls up.
	// Down and right of the eyes by enough to land in the help band: the
	// quad is 0.6 high a metre out, so 0.28 below is 7% from the bottom.
	frame.dtSeconds = 0.05f;
	frame.right.position = NiPoint3{0.0f, -0.28f, 0.0f};
	r = mode.Update(frame, settings);
	Check(r.settingsPointerValid && r.settingsPointerY > 700.0f, "the beam rests on the help band");
	Check(r.settingsNav.down && !r.settingsNav.up, "and that is a notch down");
	r = mode.Update(frame, settings);
	Check(!r.settingsNav.down, "quiet through the first delay");
	for (int i = 0; i < 12; ++i) {
		r = mode.Update(frame, settings);
		if (r.settingsNav.down) {
			break;
		}
	}
	Check(r.settingsNav.down, "then it repeats");
	frame.right.position = NiPoint3{0.0f, 0.28f, 0.0f};
	r = mode.Update(frame, settings);
	Check(r.settingsPointerValid && r.settingsPointerY < 70.0f && r.settingsNav.up,
	      "the title band is a notch up");
	frame.right.position = NiPoint3{0.0f, 0.0f, 0.0f};
	r = mode.Update(frame, settings);
	Check(!r.settingsNav.up && !r.settingsNav.down, "the middle scrolls nothing");
}

void TestGrabHand() {
	std::printf("The grab follows the hand whose grip holds it\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = true;
	HandModeFrame frame;
	frame.headValid = true;
	frame.inWorld = true;
	frame.unitsPerMetre = 70.0f;
	frame.right.valid = true;
	frame.right.position = NiPoint3{0.3f, 0.0f, -0.4f};  // 0.5 m from the eyes
	frame.left.valid = true;
	frame.left.position = NiPoint3{-0.6f, 0.0f, -0.8f};  // 1.0 m
	HandMode mode;

	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexGrip;
	HandModeResult r = mode.Update(frame, settings);
	Check(r.grabWanted && r.grabWithLeftHand && Near(r.grabDistanceMetres, 1.0f, 0.01f),
	      "the left grip: the left hand holds it, at the left hand's distance");
	Check(r.leftAimValid, "and the left hand has an aim to carry it along");
	Check(!r.controls.activate, "the left grip does not also activate");

	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexGrip;
	r = mode.Update(frame, settings);
	Check(r.grabWanted && !r.grabWithLeftHand && Near(r.grabDistanceMetres, 0.5f, 0.01f),
	      "both grips: the right hand wins, at its own distance");

	frame.left.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(r.grabWanted && !r.grabWithLeftHand, "the right grip alone");

	frame.right.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(!r.grabWanted, "no grip: no grab");
}

void TestLeftButtonsInHandMode() {
	std::printf("The left hand's buttons in the hand-tracked mode\n");
	HandSettings settings;
	settings.laserPitchDegrees = 0.0f;  // the rays below are laid along the tracked -z
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	settings.laserDragScroll = false;  // these check the click on the pull; see TestLaserPress
	settings.enabled = true;
	HandModeFrame frame;
	frame.headValid = true;
	frame.inWorld = true;
	frame.unitsPerMetre = 70.0f;
	frame.right.valid = true;
	frame.left.valid = true;
	HandMode mode;

	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexTrackpad;
	HandModeResult r = mode.Update(frame, settings);
	Check(r.controls.quickMenu, "the trackpad click opens the quick menu");
	frame.left.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(!r.controls.sneak,
	      "and its release is not a stick click - nothing sneaks");

	frame.dtSeconds = 1.0f / 90.0f;
	frame.left.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	r = mode.Update(frame, settings);
	Check(r.controls.run && !r.controls.sneak, "the left stick pressed in runs");
	for (int i = 0; i < 45; ++i) {
		r = mode.Update(frame, settings);
	}
	Check(r.controls.run, "for as long as it is held");
	frame.right.buttonsPressed = 1ull << openvr::kButtonIndexJoystick;
	r = mode.Update(frame, settings);
	Check(!r.settingsMenuToggle, "a right click half a second into running is not OBVR's menu");
	frame.right.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(r.controls.readyWeapon, "it readies the weapon on release");
	frame.left.buttonsPressed = 0;
	r = mode.Update(frame, settings);
	Check(!r.controls.run && !r.controls.sneak, "let go: walking again, nothing else");

	frame.right.thumbY = 0.9f;
	r = mode.Update(frame, settings);
	Check(r.controls.jump && !r.controls.sneak, "the right stick flicked up jumps");
	r = mode.Update(frame, settings);
	Check(!r.controls.jump, "once per flick");
	frame.right.thumbY = -0.9f;
	r = mode.Update(frame, settings);
	Check(r.controls.sneak && !r.controls.jump, "flicked down sneaks");
	frame.right.thumbY = 0.0f;
	mode.Update(frame, settings);

	frame.right.buttonsPressed = 1ull << openvr::kButtonA;
	r = mode.Update(frame, settings);
	Check(r.controls.activate && !r.controls.grab, "right A activates");
}

void TestStickFlick() {
	std::printf("Stick flicks\n");
	StickFlickState s;
	StickFlickVerdict v = StepStickFlick(s, 0.0f, 0.89f);
	Check(!v.up, "short of 0.9: nothing");
	v = StepStickFlick(s, 0.5f, 0.95f);
	Check(!v.up, "0.95 up with half as much sideways: still a turn, not a jump");
	v = StepStickFlick(s, 0.0f, 0.9f);
	Check(v.up && !v.down, "at 0.9 straight up: up");
	v = StepStickFlick(s, 0.0f, 1.0f);
	Check(!v.up, "held: once");
	v = StepStickFlick(s, 0.0f, 0.5f);
	v = StepStickFlick(s, 0.0f, 0.9f);
	Check(!v.up, "back only to 0.5, above half: not re-armed");
	v = StepStickFlick(s, 0.0f, 0.2f);
	v = StepStickFlick(s, 0.0f, 0.9f);
	Check(v.up, "back to 0.2, then up again: up again");
	s = StickFlickState{};
	v = StepStickFlick(s, 0.9f, 0.8f);
	Check(!v.up, "more sideways than up is a turn, not a jump");
	v = StepStickFlick(s, 0.0f, -0.95f);
	Check(v.down && !v.up, "straight down: down");
	s = StickFlickState{};
	volatile float zero = 0.0f;
	v = StepStickFlick(s, zero / zero, zero / zero);
	Check(!v.up && !v.down, "a NaN stick is a centred stick");
}

void TestLaserTilt() {
	std::printf("The laser's tilt\n");
	const NiPoint3 straight = LaserDirectionLocal(0.0f);
	Check(Near(straight.x, 0.0f) && Near(straight.y, 0.0f) && Near(straight.z, -1.0f),
	      "no tilt: the tracked forward, -z");
	const NiPoint3 sixty = LaserDirectionLocal(60.0f);
	Check(Near(sixty.x, 0.0f) && Near(sixty.y, -0.8660f) && Near(sixty.z, -0.5f),
	      "60 degrees: turned down about the controller's x");
	const NiPoint3 up = LaserDirectionLocal(-30.0f);
	Check(up.y > 0.0f, "a negative tilt turns it up");
	Check(Near(sixty.x * sixty.x + sixty.y * sixty.y + sixty.z * sixty.z, 1.0f),
	      "and it stays a unit direction");

	// On the big quad: a hand level with the quad's centre, held so that its
	// tracked forward points 60 degrees ABOVE the quad, hits the centre
	// once the laser is tilted down by 60.
	HandSettings settings;
	settings.laserPitchDegrees = 60.0f;
	HandModeFrame frame = BigQuadFrame();
	frame.menusOnly = true;
	settings.enabled = false;
	const float half = 0.5f * 60.0f * 0.01745329252f;
	frame.right.orientation = Quaternion{obvr::math::Sin(half), 0.0f, 0.0f, obvr::math::Cos(half)};
	HandMode mode;
	const HandModeResult r = mode.Update(frame, settings);
	Check(r.laserHit, "a hand tipped up 60 degrees still hits the quad ahead with the tilt");
}

void TestLaserPress() {
	std::printf("The laser's trigger as a finger on a touch screen\n");
	const float h = 1000.0f;  // layer height: drag start 20 px, a notch 40 px
	LaserPressState s;
	LaserPressVerdict v = StepLaserPress(s, true, true, 500.0f, 500.0f, h);
	Check(!v.mouseDown && v.wheel == 0, "pulled: nothing yet");
	v = StepLaserPress(s, true, true, 505.0f, 510.0f, h);
	Check(!v.mouseDown && v.wheel == 0, "a wobble under the threshold: still nothing");
	v = StepLaserPress(s, false, true, 505.0f, 510.0f, h);
	Check(v.mouseDown && v.wheel == 0, "let go without a drag: the click goes down");
	v = StepLaserPress(s, false, true, 505.0f, 510.0f, h);
	Check(!v.mouseDown, "and comes up the next frame");

	s = LaserPressState{};
	StepLaserPress(s, true, true, 500.0f, 500.0f, h);
	v = StepLaserPress(s, true, true, 500.0f, 530.0f, h);
	Check(!v.mouseDown && v.wheel == 0, "dragged down 30 px: scrolling, not a notch yet");
	v = StepLaserPress(s, true, true, 500.0f, 590.0f, h);
	Check(v.wheel == 2 && !v.mouseDown, "90 px down: two notches up, the list follows the beam");
	v = StepLaserPress(s, true, true, 500.0f, 460.0f, h);
	Check(v.wheel == -3, "back to 40 px above the start: three notches the other way");
	v = StepLaserPress(s, false, true, 500.0f, 460.0f, h);
	Check(!v.mouseDown && v.wheel == 0, "let go after a drag: no click");

	s = LaserPressState{};
	StepLaserPress(s, true, true, 500.0f, 500.0f, h);
	v = StepLaserPress(s, true, true, 540.0f, 505.0f, h);
	Check(v.mouseDown && v.wheel == 0, "dragged sideways: the button is held, for a slider");
	v = StepLaserPress(s, true, true, 700.0f, 600.0f, h);
	Check(v.mouseDown && v.wheel == 0, "and stays held, whatever the beam does");
	v = StepLaserPress(s, false, true, 700.0f, 600.0f, h);
	Check(!v.mouseDown, "let go: up, and no extra click");

	s = LaserPressState{};
	v = StepLaserPress(s, true, false, 0.0f, 0.0f, h);
	Check(!v.mouseDown && s.phase == LaserPressPhase::Idle, "pulled off every target: nothing");
	StepLaserPress(s, true, true, 500.0f, 500.0f, h);
	v = StepLaserPress(s, false, false, 0.0f, 0.0f, h);
	Check(!v.mouseDown, "pulled on it, let go off it: no click");

	// Through the mode: a pull is no click until it is let go.
	HandSettings settings;
	settings.enabled = false;
	settings.laserPitchDegrees = 0.0f;
	settings.laserYawDegrees = 0.0f;
	settings.laserOriginMetres = 0.0f;
	HandModeFrame frame = BigQuadFrame();
	frame.menusOnly = true;
	HandMode mode;
	mode.Update(frame, settings);
	frame.right.trigger = 1.0f;
	HandModeResult r = mode.Update(frame, settings);
	Check(!r.controls.menuClick, "in the mode: the pull alone does not click");
	frame.right.trigger = 0.0f;
	r = mode.Update(frame, settings);
	Check(r.controls.menuClick, "its release does");
	r = mode.Update(frame, settings);
	Check(!r.controls.menuClick, "for one frame");
}

void TestReadyWeapon() {
	std::printf("Ready weapon: the click followed until the game shows it\n");
	const float dt = 1.0f / 90.0f;
	const SInt32 none = -1;
	{
		ReadyWeaponState s;
		ReadyWeaponVerdict v = StepReadyWeapon(s, false, WeaponSeen::Drawn, none, dt);
		Check(!v.key && !v.dropBlock && !s.pending, "no click: nothing");
		v = StepReadyWeapon(s, true, WeaponSeen::Unknown, none, dt);
		Check(v.key && !v.dropBlock && !s.pending, "state unreadable: a plain tap, block untouched");
		int frames = 1;
		while (StepReadyWeapon(s, false, WeaponSeen::Unknown, none, dt).key) {
			++frames;
		}
		Check(frames >= 10 && frames <= 12, "held for the tap's tenth of a second");
		v = StepReadyWeapon(s, true, WeaponSeen::Unknown, none, 0.0f);
		Check(v.key, "a frame without a time still taps");
	}
	{
		ReadyWeaponState s;
		ReadyWeaponVerdict v = StepReadyWeapon(s, true, WeaponSeen::Drawn, none, dt);
		Check(v.key && v.dropBlock && s.pending && !s.wantDrawn,
		      "drawn: the click wants it sheathed, the key goes down at once, block off");
		for (int i = 0; i < 5; ++i) {
			v = StepReadyWeapon(s, false, WeaponSeen::Drawn, none, dt);
		}
		Check(v.key && v.dropBlock, "the key held for the tap");
		v = StepReadyWeapon(s, false, WeaponSeen::Drawn, kPlayerActionUnequipWeapon, dt);
		v = StepReadyWeapon(s, false, WeaponSeen::Sheathed, kPlayerActionUnequipWeapon, dt);
		for (int i = 0; i < 20; ++i) {
			v = StepReadyWeapon(s, false, WeaponSeen::Sheathed, kPlayerActionUnequipWeapon, dt);
		}
		Check(!v.key && v.dropBlock && s.pending, "while the game sheathes: waiting, block still off");
		v = StepReadyWeapon(s, false, WeaponSeen::Sheathed, none, dt);
		Check(v.reached && !v.key && !v.dropBlock && !s.pending, "sheathed and done: the wish ends");
	}
	{
		ReadyWeaponState s;
		ReadyWeaponVerdict v = StepReadyWeapon(s, true, WeaponSeen::Sheathed, kPlayerActionBlock, dt);
		Check(!v.key && v.dropBlock && s.wantDrawn, "blocking: no key yet, the block let go");
		for (int i = 0; i < 30; ++i) {
			v = StepReadyWeapon(s, false, WeaponSeen::Sheathed, kPlayerActionBlock, dt);
		}
		Check(!v.key && v.dropBlock, "not while the block action lasts");
		v = StepReadyWeapon(s, false, WeaponSeen::Sheathed, none, dt);
		Check(v.key, "the key the frame the block is over");
	}
	{
		ReadyWeaponState s;
		StepReadyWeapon(s, true, WeaponSeen::Sheathed, none, dt);
		int presses = 1;
		bool was = true;
		bool gaveUp = false;
		for (int i = 0; i < 400 && !gaveUp; ++i) {
			const ReadyWeaponVerdict v = StepReadyWeapon(s, false, WeaponSeen::Sheathed, none, dt);
			if (v.key && !was) {
				++presses;
			}
			was = v.key;
			gaveUp = v.gaveUp;
		}
		Check(presses == 4 && gaveUp && !s.pending,
		      "ignored: pressed again every 0.6 s, given up after 2.5 s");
	}
	{
		ReadyWeaponState s;
		StepReadyWeapon(s, true, WeaponSeen::Sheathed, kPlayerActionBlock, dt);
		ReadyWeaponVerdict v = StepReadyWeapon(s, true, WeaponSeen::Sheathed, kPlayerActionBlock, dt);
		Check(!s.wantDrawn && v.reached && !s.pending,
		      "a second click takes the wish back: already there, done");
		StepReadyWeapon(s, true, WeaponSeen::Drawn, none, dt);
		StepReadyWeapon(s, false, WeaponSeen::Drawn, kPlayerActionUnequipWeapon, dt);
		v = StepReadyWeapon(s, true, WeaponSeen::Drawn, kPlayerActionUnequipWeapon, dt);
		Check(s.pending && s.wantDrawn && !v.reached,
		      "taken back mid-animation: waits for the animation before calling it done");
	}
}

void TestSwingPressesAttack() {
	std::printf("Swing presses attack only with a drawn weapon and open grips\n");
	for (UInt32 mask = 0; mask < 8; ++mask) {
		const bool motion = (mask & 1) != 0;
		const bool drawn = (mask & 2) != 0;
		const bool grip = (mask & 4) != 0;
		Check(SwingPressesAttack(motion, drawn, grip) == (!motion && drawn && !grip),
		      "attack only when not striking by motion, drawn, and no grip closed");
	}
}

void TestRunToggle() {
	std::printf("Run held or toggled\n");
	bool latched = true;
	Check(StepRunToggle(latched, false, true, false) == false && !latched,
	      "hold mode: the stick's own level, a latch forgotten");
	Check(StepRunToggle(latched, false, false, true), "hold mode: pressed in, running");
	Check(StepRunToggle(latched, true, true, false) && latched, "toggle: a click switches running on");
	Check(StepRunToggle(latched, true, false, false), "and it stays on without the stick");
	Check(StepRunToggle(latched, true, false, true), "the stick held alone changes nothing");
	Check(!StepRunToggle(latched, true, true, false) && !latched, "the next click switches it off");
}

void TestLeftHandedMirror() {
	std::printf("Left-handed: the controllers swap roles as a whole\n");
	HandPose right;
	right.valid = true;
	right.position = NiPoint3{0.3f, 0.0f, 0.0f};
	right.trigger = 0.25f;
	right.buttonsPressed = 1;
	HandPose left;
	left.valid = false;
	left.position = NiPoint3{-0.3f, 0.0f, 0.0f};
	left.trigger = 0.75f;
	left.buttonsPressed = 2;
	HandPose r = right;
	HandPose l = left;
	Check(!AssignHandRoles(r, l, false) && r.position.x > 0.0f && r.trigger == 0.25f &&
	          l.position.x < 0.0f && !l.valid,
	      "right-handed: every controller keeps its role");
	Check(AssignHandRoles(r, l, true) && r.position.x < 0.0f && r.trigger == 0.75f &&
	          r.buttonsPressed == 2 && !r.valid && l.position.x > 0.0f && l.trigger == 0.25f &&
	          l.buttonsPressed == 1 && l.valid,
	      "left-handed: pose, buttons, trigger and tracking all move to the other role");
	Check(HandDeviceForRole(true, false) && !HandDeviceForRole(false, false),
	      "unswapped: the right role is the right controller");
	Check(!HandDeviceForRole(true, true) && HandDeviceForRole(false, true),
	      "swapped: the right role hangs on the left controller");

	// The whole mode on swapped roles: the left controller's trigger attacks.
	HandSettings settings;
	settings.enabled = true;
	settings.motionHits = false;
	HandModeFrame frame;
	frame.headValid = true;
	frame.dtSeconds = 0.01f;
	frame.right.valid = true;
	frame.left.valid = true;
	frame.right.position = NiPoint3{0.3f, -0.2f, -0.5f};
	frame.left.position = NiPoint3{-0.3f, -0.6f, -0.5f};
	frame.left.trigger = 1.0f;  // the physical left trigger
	frame.right.thumbY = 1.0f;  // the physical right stick
	AssignHandRoles(frame.right, frame.left, true);
	HandMode mode;
	const HandModeResult result = mode.Update(frame, settings);
	Check(result.controls.attack && !result.controls.cast, "the left trigger attacks");
	Check(result.controls.move.forward, "the right stick walks");
	Check(result.rightHandValid && result.rightHandOffsetUnits.x < 0.0f,
	      "the weapon hand is pinned where the left controller is");
}

void TestWeaponGuard() {
	std::printf("The weapon's guard blocks\n");
	GestureThresholds t;
	const NiPoint3 raised{0.1f, 0.35f, -0.1f};
	const NiPoint3 across{-0.9f, 0.3f, 0.2f};
	Check(IsWeaponGuard(raised, across, false, t), "raised, the blade across the body: a guard");
	Check(!IsWeaponGuard(raised, across, true, t), "not while swinging through the same place");
	Check(!IsWeaponGuard(NiPoint3{0.1f, 0.35f, -0.4f}, across, false, t), "held low: no guard");
	Check(!IsWeaponGuard(NiPoint3{0.1f, 0.05f, -0.1f}, across, false, t), "at the chest: no guard");
	Check(!IsWeaponGuard(raised, NiPoint3{0.1f, 0.99f, 0.0f}, false, t),
	      "the blade pointing ahead, as when thrusting: no guard");
	Check(!IsWeaponGuard(raised, NiPoint3{0.6f, 0.0f, 0.8f}, false, t),
	      "the blade upright: no guard");
	Check(IsWeaponGuard(raised, NiPoint3{0.8f, 0.2f, -0.45f}, false, t),
	      "either way across, a little tilted: a guard");

	HandSettings settings;
	settings.enabled = true;
	HandModeFrame frame;
	frame.headValid = true;
	frame.dtSeconds = 0.01f;
	frame.right.valid = true;
	frame.left.valid = true;
	frame.right.position = NiPoint3{0.1f, -0.1f, -0.35f};  // up and ahead (OpenVR: -z ahead)
	frame.left.position = NiPoint3{-0.3f, -0.6f, -0.2f};   // left hand down
	// The controller turned a quarter to the left: its forward now across.
	const float half = 0.70710678f;
	frame.right.orientation = obvr::vr::Quaternion{0.0f, half, 0.0f, half};
	frame.weaponSeen = WeaponSeen::Drawn;
	HandMode mode;
	HandModeResult r = mode.Update(frame, settings);
	Check(r.blocking, "in the mode: the drawn weapon held across blocks");
	frame.weaponSeen = WeaponSeen::Sheathed;
	HandMode sheathed;
	r = sheathed.Update(frame, settings);
	Check(!r.blocking, "sheathed: the same hand does not block");
}

void TestFirstPersonDepthBranch() {
	std::printf("First-person depth: the branch before the clear\n");
	using obvr::game::FirstPersonDepthBranchByte;
	Check(FirstPersonDepthBranchByte(0x75, true) == 0xEB, "vanilla's jne becomes a jmp to keep the depth");
	Check(FirstPersonDepthBranchByte(0xEB, true) == 0xEB, "already a jmp: stays");
	Check(FirstPersonDepthBranchByte(0xEB, false) == 0x75, "switched off: the jne comes back");
	Check(FirstPersonDepthBranchByte(0x75, false) == 0x75, "vanilla stays vanilla");
	Check(FirstPersonDepthBranchByte(0x90, true) == 0 && FirstPersonDepthBranchByte(0xE9, false) == 0,
	      "any other byte is not this branch: left alone");
}

void TestUsScanCodes() {
	std::printf("Keys by US scan code, whatever the layout\n");
	// The game's own [Controls] block (Oblivion.ini) for its defaults.
	Check(obvr::game::UsScanCode('Z') == 0x2C, "Z is 0x2C: the game's Grab=002CFFFF");
	Check(obvr::game::UsScanCode('Y') == 0x15, "Y is 0x15");
	Check(obvr::game::UsScanCode('W') == 0x11 && obvr::game::UsScanCode('S') == 0x1F &&
	          obvr::game::UsScanCode('A') == 0x1E && obvr::game::UsScanCode('D') == 0x20,
	      "WASD as the game binds them");
	Check(obvr::game::UsScanCode('C') == 0x2E && obvr::game::UsScanCode('F') == 0x21 &&
	          obvr::game::UsScanCode('E') == 0x12 && obvr::game::UsScanCode('R') == 0x13,
	      "cast, ready, jump, view");
	Check(obvr::game::UsScanCode(0x20) == 0x39 && obvr::game::UsScanCode(0x11) == 0x1D &&
	          obvr::game::UsScanCode(0x10) == 0x2A && obvr::game::UsScanCode(0x09) == 0x0F &&
	          obvr::game::UsScanCode(0x70) == 0x3B && obvr::game::UsScanCode(0x1B) == 0x01,
	      "space, ctrl, shift, tab, F1, esc");
	Check(obvr::game::UsScanCode('1') == 0x02 && obvr::game::UsScanCode('9') == 0x0A &&
	          obvr::game::UsScanCode('0') == 0x0B,
	      "digits: Quick1=0002 in the game's block");
	Check(obvr::game::UsScanCode(0x79) == 0x44 && obvr::game::UsScanCode(0x7A) == 0x57 &&
	          obvr::game::UsScanCode(0x7B) == 0x58,
	      "F10, F11, F12");
	Check(obvr::game::UsScanCode(0x08) == 0x0E && obvr::game::UsScanCode(0x0D) == 0x1C &&
	          obvr::game::UsScanCode(0xA0) == 0x2A && obvr::game::UsScanCode(0xA1) == 0x36 &&
	          obvr::game::UsScanCode(0xA2) == 0x1D && obvr::game::UsScanCode(0x12) == 0x38 &&
	          obvr::game::UsScanCode(0xA4) == 0x38 && obvr::game::UsScanCode(0x14) == 0x3A,
	      "backspace, enter, the shifts, ctrl, alt, caps lock");
	Check(obvr::game::UsScanCode(0x01) == 0 && obvr::game::UsScanCode(0x26) == 0 &&
	          obvr::game::UsScanCode('a') == 0,
	      "anything else is left to the layout");
}

void TestGrabReach() {
	std::printf("Grab by reach: grip arms, the reach decides, the grip lets go\n");
	{
		GrabReachState s;
		GrabReachVerdict v = StepGrabReach(s, false, true);
		Check(!v.reachPick && !v.key, "an open grip does nothing, whatever is in reach");
		// The first frames only turn the pick: what it found was found along
		// the laser, not through the hand.
		for (int i = 0; i < kGrabReachSettleFrames; ++i) {
			v = StepGrabReach(s, true, true);
			Check(v.reachPick && !v.key, "the pick settles through the hand before anything is taken");
		}
		v = StepGrabReach(s, true, true);
		Check(v.reachPick && v.key && s.grabbing,
		      "then something in reach is taken - the pick stays on it while the key is read");
		v = StepGrabReach(s, true, false);
		Check(v.key && v.reachPick, "and held, wherever it is, while the grip stays closed");
		v = StepGrabReach(s, false, false);
		Check(!v.key && !v.reachPick && !s.grabbing && s.armedFrames == 0,
		      "the grip opening lets go - the throw - and starts over");
	}
	{
		GrabReachState s;
		GrabReachVerdict v{};
		for (int i = 0; i < 50; ++i) {
			v = StepGrabReach(s, true, false);
		}
		Check(v.reachPick && !v.key && s.armedFrames == kGrabReachSettleFrames,
		      "a grip closed on nothing keeps reaching and takes nothing");
		v = StepGrabReach(s, true, true);
		Check(v.key, "until the hand is moved to something");
	}
	{
		// The direction: forward and down, to the right.
		float yaw = 0.0f;
		float pitch = 0.0f;
		Check(ReachDirection(NiPoint3{0.0f, 0.4f, -0.3f}, NiPoint3{0.0f, -0.3f, -0.4f}, yaw, pitch) &&
		          Near(yaw, 0.0f) && Near(pitch, -0.6f),
		      "straight ahead and below: no turn, the pitch from the room's vertical");
		Check(ReachDirection(NiPoint3{0.5f, 0.5f, 0.0f}, NiPoint3{0.5f, 0.0f, -0.5f}, yaw, pitch) &&
		          Near(yaw, -0.7853982f) && Near(pitch, 0.0f),
		      "to the right is a clockwise turn, negative");
		Check(ReachDirection(NiPoint3{-0.5f, 0.5f, 0.0f}, NiPoint3{-0.5f, 0.0f, -0.5f}, yaw, pitch) &&
		          Near(yaw, 0.7853982f),
		      "to the left is counter-clockwise, positive");
		Check(!ReachDirection(NiPoint3{0.0f, 0.0f, 0.3f}, NiPoint3{0.0f, 0.3f, 0.0f}, yaw, pitch),
		      "straight above the head has no heading");
		Check(!ReachDirection(NiPoint3{0.0f, 0.1f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, yaw, pitch),
		      "no room offset, no pitch");
	}
}

void TestSneakTap() {
	std::printf("Sneak: toggled by a flick, or held on the stick\n");
	// Toggle mode passes the flick through, whatever the game says.
	for (UInt32 mask = 0; mask < 8; ++mask) {
		SneakHoldState s;
		s.wait = 0.3f;
		const bool flick = (mask & 1) != 0;
		Check(StepSneakTap(s, false, flick, (mask & 2) != 0, (mask & 4) != 0, 0.01f) == flick &&
		          s.wait == 0.0f,
		      "toggle mode: the flick is the key, and no wait is kept");
	}
	// Hold mode, stick and game agree: nothing to tap.
	{
		SneakHoldState s;
		Check(!StepSneakTap(s, true, false, false, false, 0.01f), "hold: standing, stick up - no tap");
		Check(!StepSneakTap(s, true, true, true, true, 0.01f), "hold: sneaking, stick down - no tap");
	}
	// Hold mode: pushing down taps once, waits for the game, then rests.
	{
		SneakHoldState s;
		Check(StepSneakTap(s, true, true, true, false, 0.01f), "hold: stick down while standing taps");
		Check(!StepSneakTap(s, true, false, true, false, 0.01f), "the game has not followed yet - no second tap");
		Check(!StepSneakTap(s, true, false, true, true, 0.01f) && s.wait == 0.0f, "the game followed - the wait ends");
		Check(!StepSneakTap(s, true, false, true, true, 0.01f), "held and sneaking - rests");
		Check(StepSneakTap(s, true, false, false, true, 0.01f), "releasing the stick taps it off");
		Check(!StepSneakTap(s, true, false, false, true, 0.01f), "and waits again");
		Check(!StepSneakTap(s, true, false, false, false, 0.01f), "until the game has stood up");
	}
	// Hold mode: a tap the game ignored is repeated after the retry time, not before.
	{
		SneakHoldState s;
		Check(StepSneakTap(s, true, false, true, false, 0.1f), "first tap");
		bool again = false;
		int frames = 0;
		while (!again && frames < 100) {
			again = StepSneakTap(s, true, false, true, false, 0.1f);
			++frames;
		}
		Check(again && frames >= 6 && frames <= 7, "a lost tap is retried once the wait has run out");
		SneakHoldState z;
		z.wait = 0.2f;
		Check(!StepSneakTap(z, true, false, true, false, 0.0f) && z.wait == 0.2f,
		      "a frame without time does not use up the wait");
	}
}

void TestTapHold() {
	std::printf("Toggled controls held for a moment\n");
	TapHoldState s;
	const float dt = 1.0f / 90.0f;
	Check(StepTapHold(s, true, dt), "the tap itself is down");
	int frames = 1;
	while (StepTapHold(s, false, dt) && frames < 100) {
		++frames;
	}
	Check(frames == 11, "and stays down about 0.12 s at 90 Hz (11 frames)");
	Check(!StepTapHold(s, false, dt), "then up");
	TapHoldState t;
	StepTapHold(t, true, 0.0f);
	Check(!StepTapHold(t, false, 0.0f) || !StepTapHold(t, false, 0.0f),
	      "with no time to count it does not stay down");
}

void TestLaserCoast() {
	std::printf("A flick coasts on\n");
	const float h = 1000.0f;
	const float dt = 1.0f / 90.0f;
	LaserPressState s;
	StepLaserPress(s, true, true, 500.0f, 500.0f, h, dt);
	// A fast flick down: 30 px a frame, 2700 px a second.
	float y = 500.0f;
	for (int i = 0; i < 6; ++i) {
		y += 30.0f;
		StepLaserPress(s, true, true, 500.0f, y, h, dt);
	}
	int coasted = 0;
	int frames = 0;
	LaserPressVerdict v = StepLaserPress(s, false, true, 500.0f, y, h, dt);
	Check(!v.mouseDown, "let go mid-flick: no click");
	coasted += v.wheel;
	while (s.coastVelocity != 0.0f && frames < 1000) {
		v = StepLaserPress(s, false, true, 500.0f, y, h, dt);
		coasted += v.wheel;
		++frames;
	}
	Check(coasted > 5, "the list goes on scrolling up after the release");
	Check(frames > 10 && frames < 1000, "and comes to a stop by itself");

	LaserPressState slow;
	StepLaserPress(slow, true, true, 500.0f, 500.0f, h, dt);
	StepLaserPress(slow, true, true, 500.0f, 525.0f, h, dt);
	for (int i = 0; i < 30; ++i) {
		StepLaserPress(slow, true, true, 500.0f, 525.0f, h, dt);  // held still
	}
	StepLaserPress(slow, false, true, 500.0f, 525.0f, h, dt);
	Check(slow.coastVelocity == 0.0f, "let go after holding still: no coasting");

	LaserPressState stop;
	StepLaserPress(stop, true, true, 500.0f, 500.0f, h, dt);
	for (int i = 0; i < 6; ++i) {
		StepLaserPress(stop, true, true, 500.0f, 500.0f + 30.0f * (i + 1), h, dt);
	}
	StepLaserPress(stop, false, true, 500.0f, 680.0f, h, dt);
	Check(stop.coastVelocity != 0.0f, "coasting");
	v = StepLaserPress(stop, true, true, 500.0f, 680.0f, h, dt);
	Check(stop.coastVelocity == 0.0f && v.wheel == 0, "a new pull stops it, like a finger on it");

	LaserPressState bar;
	v = StepLaserPress(bar, true, true, 500.0f, 500.0f, h, dt, true);
	Check(v.mouseDown, "pulled on a scroll bar: the button goes down at once");
	v = StepLaserPress(bar, true, true, 500.0f, 700.0f, h, dt, true);
	Check(v.mouseDown && v.wheel == 0, "and stays down while dragging, the bar's own way");
}

void TestLaserYaw() {
	std::printf("The laser turned inwards\n");
	const NiPoint3 left = LaserDirectionLocal(0.0f, 5.0f);
	Check(left.x < 0.0f && Near(left.y, 0.0f) && left.z < 0.0f, "a positive yaw turns it left");
	const NiPoint3 both = LaserDirectionLocal(40.0f, 5.0f);
	Check(Near(both.x * both.x + both.y * both.y + both.z * both.z, 1.0f), "still a unit direction");
	const NiPoint3 right = LaserRightLocal(5.0f);
	Check(Near(right.x * both.x + right.y * both.y + right.z * both.z, 0.0f),
	      "the beam's right stays square to it");
}

void TestChordWindow() {
	std::printf("The chord only when both come together\n");
	StickChordState s;
	StickChordVerdict v = StepStickChord(s, false, true, 0.011f);
	for (int i = 0; i < 30; ++i) {
		v = StepStickChord(s, false, true, 0.011f);
	}
	v = StepStickChord(s, true, true, 0.011f);
	Check(!v.both, "the right a quarter second after the left: no chord");
	v = StepStickChord(s, false, true, 0.011f);
	Check(v.rightClick && !v.both, "the right released: its own click");
	v = StepStickChord(s, false, false, 0.011f);
	Check(v.leftClick, "and the left released: its own click");

	v = StepStickChord(s, false, true, 0.011f);
	v = StepStickChord(s, false, true, 0.011f);
	v = StepStickChord(s, true, true, 0.011f);
	Check(v.both, "the right a couple of frames after the left: the chord");
	v = StepStickChord(s, false, false, 0.011f);
	Check(!v.rightClick && !v.leftClick, "and no clicks after it");
}

void TestLegacyButtons() {
	std::printf("The legacy controller state, normalised\n");
	const UInt64 trackpad = 1ull << openvr::kButtonIndexTrackpad;
	const UInt64 stick = 1ull << openvr::kButtonIndexJoystick;
	const UInt64 b = 1ull << openvr::kButtonIndexB;
	Check(NormalizeLegacyButtons(trackpad) == stick, "Axis0's click is the stick click there");
	Check(NormalizeLegacyButtons(trackpad | b) == (stick | b), "and the other bits stay");
	Check(NormalizeLegacyButtons(stick) == stick, "a real Axis3 click stays as it is");
	Check(NormalizeLegacyButtons(0) == 0, "nothing stays nothing");
	Check(StickClickDown(NormalizeLegacyButtons(trackpad)) &&
	          !TrackpadClickDown(NormalizeLegacyButtons(trackpad)),
	      "so a legacy click is a stick click and never both");
}

void TestPickHand() {
	std::printf("The pick follows the hand that moves\n");
	using obvr::vr::PickHandState;
	using obvr::vr::StepPickHand;
	const float dt = 1.0f / 90.0f;
	PickHandState s;
	Check(!StepPickHand(s, true, true, 0.0f, 0.0f, dt), "both still: the right hand, as before");
	bool left = false;
	for (int i = 0; i < 20; ++i) {
		left = StepPickHand(s, true, true, 0.0f, 1.0f, dt);
	}
	Check(left, "the left hand reaching (1 m/s, a fifth of a second): the pick goes over");
	for (int i = 0; i < 90; ++i) {
		left = StepPickHand(s, true, true, 0.0f, 0.0f, dt);
	}
	Check(left, "then both still: it stays with the left hand");
	for (int i = 0; i < 3; ++i) {
		left = StepPickHand(s, true, true, 0.3f, 0.0f, dt);
	}
	Check(left, "a small twitch of the right hand does not take it back");
	for (int i = 0; i < 30; ++i) {
		left = StepPickHand(s, true, true, 1.0f, 0.0f, dt);
	}
	Check(!left, "the right hand moving takes it back");
	PickHandState both;
	for (int i = 0; i < 30; ++i) {
		left = StepPickHand(both, true, true, 1.0f, 1.0f, dt);
	}
	Check(!left, "both moving alike: no switch");
	PickHandState lost;
	lost.left = true;
	Check(!StepPickHand(lost, true, false, 0.0f, 0.0f, dt), "the left hand not tracked: the right");
	Check(StepPickHand(lost, false, true, 0.0f, 0.0f, dt), "only the left tracked: the left");
	PickHandState paused;
	paused.leftMotion = 0.2f;
	StepPickHand(paused, true, true, 0.0f, 0.0f, 0.0f);
	Check(Near(paused.leftMotion, 0.2f), "a frame of no time changes nothing");
	PickHandState hitch;
	hitch.leftMotion = 0.2f;
	StepPickHand(hitch, true, true, 0.0f, 0.0f, 1.0f);
	Check(Near(hitch.leftMotion, 0.0f), "a long hitch forgets the motion, never negative");
}

int main() {
	TestPickHand();
	TestGestures();
	TestGrabHand();
	TestLeftButtonsInHandMode();
	TestLegacyButtons();
	TestStickFlick();
	TestChordWindow();
	TestLaserTilt();
	TestLaserPress();
	TestLaserYaw();
	TestTapHold();
	TestLaserCoast();
	TestSpeedAndSwing();
	TestEdges();
	TestPlanner();
	TestGamepadPlanner();
	TestLaser();
	TestPoke();
	TestStickChord();
	TestStickNav();
	TestMenuHandAndSettingsMenu();
	TestHandPoses();
	TestWristTransform();
	TestStrikeByMotion();
	TestLaserOnBigQuad();
	TestMenusOnly();
	TestMainMenuLaser();
	TestLaserOnOwnPanel();
	TestSneakTap();
	TestGrabReach();
	TestReadyWeapon();
	TestSwingPressesAttack();
	TestRunToggle();
	TestUsScanCodes();
	TestFirstPersonDepthBranch();
	TestWeaponGuard();
	TestLeftHandedMirror();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

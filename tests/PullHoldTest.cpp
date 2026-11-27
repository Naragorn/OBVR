// Checks a pull to the hand under the grip toggle (game/PullHold.h) and what a
// release does to the body's speed (game/GrabPhysics.h ReleaseSpeedFor): with
// [Hands] GripToggle one grip pulls, takes and keeps a weapon, a bow or a
// torch; a pull given up stops where it was, it is no launch.

#include <cstdio>

#include "game/GrabPhysics.h"
#include "game/PullHold.h"
#include "vr/GripToggle.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kFrame = 1.0f / 90.0f;

void TestHandsOf() {
	std::printf("The hands a pull keeps\n");
	PullHoldInput in;
	PullHands h = PullHandsOf(in);
	Check(!h.right && !h.left, "no grab: none");
	in.keyDown = true;
	h = PullHandsOf(in);
	Check(!h.right && !h.left, "the grab on a potion or a body: none - the grip's, as before");
	in.weaponPullGrab = true;
	h = PullHandsOf(in);
	Check(h.right && !h.left, "the grab on a weapon for the weapon hand: the weapon hand");
	in.weaponGrabLeft = true;
	h = PullHandsOf(in);
	Check(!h.right && h.left, "the grab on a bow, the other hand's: the other hand");
	in = PullHoldInput{};
	in.weaponPullGrab = true;
	h = PullHandsOf(in);
	Check(!h.right && !h.left, "a weapon's grab with the key up: none");
	in.weaponArrived = true;
	h = PullHandsOf(in);
	Check(h.right && !h.left, "arrived, the key forced up while the engine lets go: still the weapon hand");
	in = PullHoldInput{};
	in.weaponTaking = true;
	h = PullHandsOf(in);
	Check(h.right && !h.left, "the take until it hangs on the hand: the weapon hand");
	in.weaponTakingBow = true;
	h = PullHandsOf(in);
	Check(!h.right && h.left, "a bow's take: the other hand");
	in = PullHoldInput{};
	in.keyDown = true;
	in.torchPullGrab = true;
	h = PullHandsOf(in);
	Check(!h.right && h.left, "a torch's grab: the other hand");
	in.keyDown = false;
	h = PullHandsOf(in);
	Check(!h.right && !h.left, "a torch's grab with the key up: none");
	in.torchArrived = true;
	Check(PullHandsOf(in).left, "a torch arrived: the other hand");
	in = PullHoldInput{};
	in.torchTaking = true;
	h = PullHandsOf(in);
	Check(!h.right && h.left, "a torch's take: the other hand");
}

void TestGrabOf() {
	std::printf("The grab a pull keeps on\n");
	PullHoldInput in;
	Check(!PullGrabOf(in).pulling, "no grab: none");
	in.keyDown = true;
	Check(!PullGrabOf(in).pulling, "a grab on a loose thing: none");
	in.weaponPullGrab = true;
	PullGrab g = PullGrabOf(in);
	Check(g.pulling && !g.left, "a weapon: the weapon hand's");
	in.weaponGrabLeft = true;
	g = PullGrabOf(in);
	Check(g.pulling && g.left, "a bow: the other hand's");
	in = PullHoldInput{};
	in.keyDown = true;
	in.torchPullGrab = true;
	g = PullGrabOf(in);
	Check(g.pulling && g.left, "a torch: the other hand's");
	in.keyDown = false;
	in.weaponArrived = true;
	Check(!PullGrabOf(in).pulling, "arrived with the key up: no grab kept - the engine must let go");
}

void TestKeepsGrab() {
	std::printf("Whether the grab stays on with the grip open\n");
	PullGrab right;
	right.pulling = true;
	PullGrab left = right;
	left.left = true;
	Check(!PullKeepsGrab(false, right, true, true), "the toggle off: never - the pull is the grip's");
	Check(PullKeepsGrab(true, right, true, false), "on, the weapon hand's hold closed: kept");
	Check(!PullKeepsGrab(true, right, false, true), "on, the weapon hand's hold open (the next grip): given up");
	Check(PullKeepsGrab(true, left, false, true), "on, the other hand's pull reads that hand's hold");
	Check(!PullKeepsGrab(true, PullGrab{}, true, true), "no pull: nothing kept");
}

// The weapon hand over a pull: the toggle's hold (vr::StepGripToggle) with the
// pull's keep, and whether the grab stays on - the hand mode's and the frame's
// steps in one. `countPull` false is the code before 2026-11-27: the pull was
// not the hand's.
struct PullRun {
	vr::GripToggleState toggle;
	bool keyDown = false;
	bool hold = false;
	bool grab = false;
};

void StepRun(PullRun& r, bool grip, bool toggleOn, bool countPull) {
	PullHoldInput in;
	in.keyDown = r.keyDown;
	in.weaponPullGrab = r.keyDown;
	vr::GripToggleInput t;
	t.hold = grip;
	t.keeps = countPull && PullHandsOf(in).right;
	t.valid = true;
	t.world = true;
	t.dt = kFrame;
	const vr::GripToggleOut out = vr::StepGripToggle(r.toggle, t, toggleOn);
	r.hold = out.hold;
	const bool click = grip && !out.takesClick;
	r.grab = click || (countPull && PullKeepsGrab(toggleOn, PullGrabOf(in), r.hold, false));
	r.keyDown = r.grab;  // the key stays down while the grab is wanted (vr::StepGrabReach, grabbing)
}

// Frames of one grip; true when the grab stayed on in every one.
bool GrabFrames(PullRun& r, bool grip, int frames, bool toggleOn, bool countPull) {
	bool all = true;
	for (int i = 0; i < frames; ++i) {
		StepRun(r, grip, toggleOn, countPull);
		all = all && r.grab;
	}
	return all;
}

void TestOneGripTakes() {
	std::printf("Toggle on: one short grip pulls the sword in and takes it\n");
	PullRun r;
	Check(GrabFrames(r, true, 5, true, true), "the grip closed: the grab on");
	Check(r.toggle.latched, "the pull's grab down: the hand's hold latched for it");
	Check(GrabFrames(r, false, 80, true, true), "the grip opened in flight: the grab stays on all the way in (0.9 s)");
	Check(r.hold, "at the arrival the hold reads closed: the take goes ahead");
	PullRun before;
	GrabFrames(before, true, 5, true, false);
	const bool kept = GrabFrames(before, false, 80, true, false);
	Check(!kept && !before.hold, "the code before: the grip opened let the grab go and the hold opened - no take");
}

void TestNextGripGivesUp() {
	std::printf("Toggle on: the next grip in flight gives the pull up\n");
	PullRun r;
	GrabFrames(r, true, 5, true, true);
	GrabFrames(r, false, 10, true, true);
	StepRun(r, true, true, true);
	Check(!r.grab && !r.hold, "the next grip: the hold open, the grab let go");
	Check(!GrabFrames(r, true, 5, true, true) && !r.grab, "held on: swallowed, no new grab");
	StepRun(r, false, true, true);
	Check(!r.grab && !r.hold && !r.toggle.latched, "opened: nothing kept");
}

void TestToggleOff() {
	std::printf("Toggle off: the pull is the grip's\n");
	PullRun r;
	Check(GrabFrames(r, true, 5, false, true), "the grip closed: the grab on");
	StepRun(r, false, false, true);
	Check(!r.grab && !r.hold, "the grip opened in flight: the grab let go, as before");
}

void TestReleaseSpeed() {
	std::printf("What a release does to the body's speed\n");
	Check(ReleaseSpeedFor(true, false) == ReleaseSpeed::Thrown, "thrown from the hand: the hand's speed");
	Check(ReleaseSpeedFor(true, true) == ReleaseSpeed::Thrown, "swung while it floated in: still a throw");
	Check(ReleaseSpeedFor(false, true) == ReleaseSpeed::Stopped,
	      "let go of on its way in, the hand still: stopped - not launched past the hand");
	Check(ReleaseSpeedFor(false, false) == ReleaseSpeed::Kept, "set down from the hand: its own speed, as before");
}

}  // namespace

int main() {
	TestHandsOf();
	TestGrabOf();
	TestKeepsGrab();
	TestOneGripTakes();
	TestNextGripGivesUp();
	TestToggleOff();
	TestReleaseSpeed();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

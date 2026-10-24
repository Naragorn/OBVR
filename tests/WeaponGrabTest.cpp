// Checks the world weapon taken at its grip (game/WeaponGrab.h): where the
// grip point lies on a model, what a closing grip does on a weapon - equip
// at the grip, carry at a touch elsewhere, nothing out of reach - and the
// take's steps after the activation: the equip once, the ready click when
// the weapon shows sheathed, the give-up.

#include <cmath>
#include <cstdio>

#include "game/WeaponGrab.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 0.001f; }

bool Near(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

void TestGripPoint() {
	std::printf("The grip point on a weapon's model\n");
	Check(Near(GripAxialFor(0), -0.9f) && Near(GripAxialFor(1), -0.9f) && Near(GripAxialFor(2), -0.9f) &&
	          Near(GripAxialFor(3), -0.9f),
	      "blades and blunts: the palm 0.9 below the model's origin");
	Check(Near(GripAxialFor(4), -37.6f), "a staff: 37.6 below");
	Check(Near(GripAxialFor(5), -0.9f), "a bow: as a blade (unmeasured)");
	const NiPoint3 origin{100.0f, 200.0f, 50.0f};
	Check(Near(GripPointOf(NiMatrix33::Identity(), origin, -0.9f), NiPoint3{100.0f, 199.1f, 50.0f}),
	      "upright model: 0.9 down its y");
	// A model lying along world x: its +y turned onto +x (a quarter turn about z).
	NiMatrix33 lying = NiMatrix33::Identity();
	lying.data[0][0] = 0.0f;
	lying.data[0][1] = 1.0f;
	lying.data[1][0] = -1.0f;
	lying.data[1][1] = 0.0f;
	const NiPoint3 axis = lying * NiPoint3{0.0f, 1.0f, 0.0f};
	Check(Near(axis, NiPoint3{1.0f, 0.0f, 0.0f}), "the lying model's y is world x");
	Check(Near(GripPointOf(lying, origin, -37.6f), NiPoint3{62.4f, 200.0f, 50.0f}),
	      "a staff lying along x: its grip 37.6 back along x");
}

void TestChoice() {
	std::printf("What a closing grip does on a weapon\n");
	const float reach = 8.4f;  // 0.12 m
	const float touch = 3.5f;
	Check(ChooseWeaponGrab(false, true, true, 1.0f, 1.0f, reach, touch, false) == WeaponGrabChoice::NotAWeapon,
	      "not a weapon: the grab as today");
	Check(ChooseWeaponGrab(true, true, true, 5.0f, 20.0f, reach, touch, false) == WeaponGrabChoice::Equip,
	      "the weapon hand at the grip: equip");
	Check(ChooseWeaponGrab(true, true, true, 8.4f, -1.0f, reach, touch, false) == WeaponGrabChoice::Equip,
	      "at the reach's edge, the mesh distance unknown: equip");
	Check(ChooseWeaponGrab(true, true, true, 5.0f, 1.0f, reach, touch, false) == WeaponGrabChoice::Equip,
	      "at the grip and touching: the grip wins");
	Check(ChooseWeaponGrab(true, true, true, 30.0f, 2.0f, reach, touch, false) == WeaponGrabChoice::Carry,
	      "the weapon hand touching the blade, far from the grip: carry");
	Check(ChooseWeaponGrab(true, true, true, 30.0f, 10.0f, reach, touch, false) == WeaponGrabChoice::Nothing,
	      "the weapon hand near but not touching, off the grip: nothing - no pull");
	Check(ChooseWeaponGrab(true, true, false, 5.0f, 20.0f, reach, touch, false) == WeaponGrabChoice::Nothing,
	      "the weapon hand at the grip but not allowed (a weapon drawn): nothing");
	Check(ChooseWeaponGrab(true, true, false, 5.0f, 2.0f, reach, touch, false) == WeaponGrabChoice::Carry,
	      "not allowed but touching: carry");
	Check(ChooseWeaponGrab(true, false, true, 5.0f, 20.0f, reach, touch, false) == WeaponGrabChoice::Carry,
	      "the other hand at the grip: carry, never equip");
	Check(ChooseWeaponGrab(true, false, true, 30.0f, 2.0f, reach, touch, false) == WeaponGrabChoice::Carry,
	      "the other hand touching: carry");
	Check(ChooseWeaponGrab(true, false, true, 30.0f, 10.0f, reach, touch, false) == WeaponGrabChoice::Nothing,
	      "the other hand off the weapon: nothing");
	Check(ChooseWeaponGrab(true, true, true, -1.0f, -1.0f, reach, touch, false) == WeaponGrabChoice::Nothing,
	      "both distances unknown: far");
	// The pull to the hand, an option default on (the tester, 2026-10-23: "es
	// soll weiterhin möglich sein das zu mir zu teleporten wie es vorher war").
	Check(WeaponGrabSettings{}.pullToHand, "the pull is on by default");
	Check(ChooseWeaponGrab(true, true, true, 30.0f, 10.0f, reach, touch, true) == WeaponGrabChoice::Pull,
	      "the weapon hand off the weapon, the pull on: pull");
	Check(ChooseWeaponGrab(true, false, true, 30.0f, 10.0f, reach, touch, true) == WeaponGrabChoice::Pull,
	      "the other hand off the weapon, the pull on: pull too");
	Check(ChooseWeaponGrab(true, true, true, 5.0f, 20.0f, reach, touch, true) == WeaponGrabChoice::Equip,
	      "at the grip with the pull on: still the equip");
	Check(ChooseWeaponGrab(true, true, true, 30.0f, 2.0f, reach, touch, true) == WeaponGrabChoice::Carry,
	      "touching with the pull on: still the carry");
	Check(ChooseWeaponGrab(true, true, true, -1.0f, -1.0f, reach, touch, true) == WeaponGrabChoice::Pull,
	      "distances unknown with the pull on: the pull decides by its own reach");
}

void TestTrigger() {
	std::printf("The take while the hold is closed, once per closing\n");
	WeaponHoldLatch l;
	Check(!StepWeaponTakeTrigger(l, false, true), "the hand open at the grip: nothing");
	Check(StepWeaponTakeTrigger(l, true, true), "the hand closes at the grip: take");
	Check(!StepWeaponTakeTrigger(l, true, true), "still closed: no second take");
	Check(!StepWeaponTakeTrigger(l, true, false), "still closed, the weapon gone: nothing");
	Check(!StepWeaponTakeTrigger(l, false, true), "the hand opens: nothing, the latch released");
	Check(StepWeaponTakeTrigger(l, true, true), "closed again: take");
	WeaponHoldLatch arriving;
	Check(!StepWeaponTakeTrigger(arriving, true, false), "closed on the way, no grip yet: nothing");
	Check(StepWeaponTakeTrigger(arriving, true, true), "arrives closed at the grip: take (run 3 of the tester, 2026-10-23)");
	Check(WeaponHoldBlocksGrab(arriving), "and while it stays closed the grip grabs nothing else");
	WeaponHoldLatch pulled;
	Check(!WeaponHoldBlocksGrab(pulled), "a fresh hold blocks nothing");
	LatchWeaponHold(pulled);
	Check(WeaponHoldBlocksGrab(pulled), "a take by the pull latches the hold too");
	Check(!StepWeaponTakeTrigger(pulled, true, true), "...so no take while it stays closed");
	StepWeaponTakeTrigger(pulled, false, false);
	Check(!WeaponHoldBlocksGrab(pulled), "opened: the grab is free again");
}

WeaponPullInput Held(UInt32 ref, bool arrived = true, bool weaponHand = true, bool closed = true, float dt = 1.0f / 90.0f) {
	WeaponPullInput in;
	in.allowed = true;
	in.heldRef = ref;
	in.heldIsWeapon = ref != 0;
	in.heldForm = ref != 0 ? 0x4400u : 0u;
	in.heldType = 0;
	in.weaponHandHolds = weaponHand;
	in.arrived = arrived;
	in.holdClosed = closed;
	in.pullGrab = true;
	in.dt = dt;
	return in;
}

void TestPullTake() {
	std::printf("A pulled weapon equipped on arrival\n");
	WeaponPullState s;
	WeaponPullVerdict v = StepWeaponPull(s, Held(0));
	Check(!v.forceGrabUp && v.takeNow == 0 && s.phase == WeaponPullPhase::Idle, "nothing held: idle");
	v = StepWeaponPull(s, Held(0x77u, false));
	Check(!v.forceGrabUp && s.phase == WeaponPullPhase::Idle, "a weapon still floating in: idle");
	v = StepWeaponPull(s, Held(0x77u, true, false));
	Check(!v.forceGrabUp && s.phase == WeaponPullPhase::Idle, "arrived in the other hand: idle - it only carries");
	v = StepWeaponPull(s, Held(0x77u, true, true, false));
	Check(!v.forceGrabUp && s.phase == WeaponPullPhase::Idle, "arrived with the hand open: idle");
	WeaponPullInput carry = Held(0x77u);
	carry.pullGrab = false;
	v = StepWeaponPull(s, carry);
	Check(!v.forceGrabUp && s.phase == WeaponPullPhase::Idle, "a carry, not a pull: idle - the blade stays a carry");
	WeaponPullInput other = Held(0x77u);
	other.heldIsWeapon = false;
	v = StepWeaponPull(s, other);
	Check(!v.forceGrabUp && s.phase == WeaponPullPhase::Idle, "a pulled cup: idle");
	v = StepWeaponPull(s, Held(0x77u));
	Check(v.forceGrabUp && s.phase == WeaponPullPhase::Releasing && s.ref == 0x77u && s.form == 0x4400u,
	      "arrived in the weapon hand, closed, pulled: the grab let go, the weapon remembered");
	v = StepWeaponPull(s, Held(0x77u));
	Check(v.forceGrabUp && v.takeNow == 0, "the engine still holds it: the key stays up, no take yet");
	v = StepWeaponPull(s, Held(0));
	Check(v.takeNow == 0x77u && v.form == 0x4400u && v.type == 0 && s.phase == WeaponPullPhase::Idle,
	      "the engine let go: taken now, with the form and the type");
	// Never let go: given up after a second.
	StepWeaponPull(s, Held(0x78u));
	int frames = 0;
	bool gaveUp = false;
	for (; frames < 200 && !gaveUp; ++frames) {
		gaveUp = StepWeaponPull(s, Held(0x78u)).gaveUp;
	}
	Check(gaveUp && frames >= 89 && frames <= 92 && s.phase == WeaponPullPhase::Idle, "held for a second more: given up");
	// A menu cancels.
	StepWeaponPull(s, Held(0x79u));
	WeaponPullInput menu = Held(0x79u);
	menu.allowed = false;
	v = StepWeaponPull(s, menu);
	Check(v.cancelled && s.phase == WeaponPullPhase::Idle, "a menu: cancelled");
	// Another thing held meanwhile: not ours, cancelled.
	StepWeaponPull(s, Held(0x7Au));
	v = StepWeaponPull(s, Held(0x7Bu));
	Check(v.cancelled && s.phase == WeaponPullPhase::Idle, "another reference in the hand: cancelled, nothing taken");
}

WeaponTakeInput World(vr::EquippedKind equipped, vr::WeaponSeen seen, float dt = 1.0f / 90.0f) {
	WeaponTakeInput in;
	in.allowed = true;
	in.equipped = equipped;
	in.seen = seen;
	in.dt = dt;
	return in;
}

void TestTake() {
	std::printf("The take's steps\n");
	WeaponTakeState s;
	WeaponTakeVerdict v = StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	Check(!v.equipNow && !v.done && !v.gaveUp && !v.cancelled, "idle: nothing");
	BeginWeaponTake(s, 0x12345678u, vr::EquippedKind::OneHand, 2.0f);
	v = StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	Check(v.equipNow && v.form == 0x12345678u && !v.done, "begun: the equip sent once, with the form");
	v = StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	Check(!v.equipNow && !v.done && !v.gaveUp, "the next frame: waiting, no second equip");
	v = StepWeaponTake(s, World(vr::EquippedKind::Bow, vr::WeaponSeen::Sheathed));
	Check(!v.done && !v.readyClick, "another kind still equipped: waiting");
	v = StepWeaponTake(s, World(vr::EquippedKind::OneHand, vr::WeaponSeen::Unknown));
	Check(!v.done, "the kind equipped but not yet seen: waiting");
	v = StepWeaponTake(s, World(vr::EquippedKind::OneHand, vr::WeaponSeen::Sheathed));
	Check(v.done && v.readyClick && s.phase == WeaponTakePhase::Idle,
	      "equipped and sheathed: the ready click draws it, the take is done");
	v = StepWeaponTake(s, World(vr::EquippedKind::OneHand, vr::WeaponSeen::Sheathed));
	Check(!v.done && !v.readyClick, "idle again: nothing more");

	// Drawn by the game itself: done without a click.
	BeginWeaponTake(s, 7u, vr::EquippedKind::TwoHand, 2.0f);
	StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	v = StepWeaponTake(s, World(vr::EquippedKind::TwoHand, vr::WeaponSeen::Drawn));
	Check(v.done && !v.readyClick, "the weapon shows drawn: done, no click");

	// Never shows: given up after the wait.
	BeginWeaponTake(s, 7u, vr::EquippedKind::Bow, 1.0f);
	StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	int frames = 0;
	bool gaveUp = false;
	for (; frames < 200 && !gaveUp; ++frames) {
		gaveUp = StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown)).gaveUp;
	}
	Check(gaveUp && frames >= 89 && frames <= 92 && s.phase == WeaponTakePhase::Idle,
	      "nothing equipped for a second: given up at the second");

	// A menu opening cancels it.
	BeginWeaponTake(s, 7u, vr::EquippedKind::Bow, 1.0f);
	StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	WeaponTakeInput menu = World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown);
	menu.allowed = false;
	v = StepWeaponTake(s, menu);
	Check(v.cancelled && !v.gaveUp && s.phase == WeaponTakePhase::Idle, "a menu: cancelled, the state cleared");

	// Frames of no number count no time.
	BeginWeaponTake(s, 7u, vr::EquippedKind::Bow, 1.0f);
	StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown));
	for (int i = 0; i < 300; ++i) {
		v = StepWeaponTake(s, World(vr::EquippedKind::Nothing, vr::WeaponSeen::Unknown, std::nanf("")));
	}
	Check(!v.gaveUp && s.phase == WeaponTakePhase::Equipping, "300 frames that are not a number: still waiting");
	// And a wait that is no number is the default.
	BeginWeaponTake(s, 7u, vr::EquippedKind::Bow, std::nanf(""));
	Check(Near(s.wait, 2.0f), "a wait that is not a number: two seconds");
}

}  // namespace

int main() {
	TestGripPoint();
	TestChoice();
	TestTake();
	TestTrigger();
	TestPullTake();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

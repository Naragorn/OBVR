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
	Check(ChooseWeaponGrab(false, true, true, 1.0f, 1.0f, reach, touch) == WeaponGrabChoice::NotAWeapon,
	      "not a weapon: the grab as today");
	Check(ChooseWeaponGrab(true, true, true, 5.0f, 20.0f, reach, touch) == WeaponGrabChoice::Equip,
	      "the weapon hand at the grip: equip");
	Check(ChooseWeaponGrab(true, true, true, 8.4f, -1.0f, reach, touch) == WeaponGrabChoice::Equip,
	      "at the reach's edge, the mesh distance unknown: equip");
	Check(ChooseWeaponGrab(true, true, true, 5.0f, 1.0f, reach, touch) == WeaponGrabChoice::Equip,
	      "at the grip and touching: the grip wins");
	Check(ChooseWeaponGrab(true, true, true, 30.0f, 2.0f, reach, touch) == WeaponGrabChoice::Carry,
	      "the weapon hand touching the blade, far from the grip: carry");
	Check(ChooseWeaponGrab(true, true, true, 30.0f, 10.0f, reach, touch) == WeaponGrabChoice::Nothing,
	      "the weapon hand near but not touching, off the grip: nothing - no pull");
	Check(ChooseWeaponGrab(true, true, false, 5.0f, 20.0f, reach, touch) == WeaponGrabChoice::Nothing,
	      "the weapon hand at the grip but not allowed (a weapon drawn): nothing");
	Check(ChooseWeaponGrab(true, true, false, 5.0f, 2.0f, reach, touch) == WeaponGrabChoice::Carry,
	      "not allowed but touching: carry");
	Check(ChooseWeaponGrab(true, false, true, 5.0f, 20.0f, reach, touch) == WeaponGrabChoice::Carry,
	      "the other hand at the grip: carry, never equip");
	Check(ChooseWeaponGrab(true, false, true, 30.0f, 2.0f, reach, touch) == WeaponGrabChoice::Carry,
	      "the other hand touching: carry");
	Check(ChooseWeaponGrab(true, false, true, 30.0f, 10.0f, reach, touch) == WeaponGrabChoice::Nothing,
	      "the other hand off the weapon: nothing");
	Check(ChooseWeaponGrab(true, true, true, -1.0f, -1.0f, reach, touch) == WeaponGrabChoice::Nothing,
	      "both distances unknown: far");
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
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

// Checks that a bow is the other hand's (game/BowHand.h; free-weapons-spec
// "A bow is the other hand's"): which things the other hand takes, which hold
// takes a weapon at its grip, which hand's grab is a weapon's pull, who
// takes the thing nearest which hand (IsTakingHand), and
// the bow's grip point.

#include <cmath>
#include <cstdio>

#include "game/BowHand.h"
#include "game/TorchGrab.h"
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

void TestWhatTheOtherHandTakes() {
	std::printf("What the other hand takes\n");
	Check(IsBowType(5) && kWeaponTypeBow == 5, "the weapon type 5 is a bow");
	for (SInt32 t = -1; t <= 4; ++t) {
		Check(!IsBowType(t), "no other weapon type is a bow");
	}
	Check(IsOtherHandsItem(true, -1), "a torch: the other hand's");
	Check(IsOtherHandsItem(false, 5), "a bow: the other hand's");
	Check(!IsOtherHandsItem(false, 0) && !IsOtherHandsItem(false, 1) && !IsOtherHandsItem(false, 4),
	      "a blade, a two-hander, a staff: the weapon hand's");
	Check(!IsOtherHandsItem(false, -1), "neither a weapon nor a torch: nobody's");
}

void TestTakingHandForABow() {
	std::printf("Which hand takes a bow near it\n");
	const bool bow = IsOtherHandsItem(false, 5);
	Check(IsTakingHand(bow, true), "a bow near the other hand: that hand takes");
	Check(!IsTakingHand(bow, false), "a bow near the weapon hand: it only carries");
	const bool sword = IsOtherHandsItem(false, 0);
	Check(IsTakingHand(sword, false) && !IsTakingHand(sword, true), "a sword: the weapon hand takes, as before");
	const float reach = 10.0f;
	const float touch = 3.0f;
	Check(ChooseWeaponGrab(true, IsTakingHand(bow, true), true, 5.0f, 2.0f, reach, touch, true) ==
	          WeaponGrabChoice::Equip,
	      "the other hand's hold at a bow's grip: takes it");
	Check(ChooseWeaponGrab(true, IsTakingHand(bow, false), true, 5.0f, 2.0f, reach, touch, true) ==
	          WeaponGrabChoice::Carry,
	      "the weapon hand at a bow's grip: carries it");
	Check(ChooseWeaponGrab(true, IsTakingHand(bow, true), true, 30.0f, 20.0f, reach, touch, true) ==
	          WeaponGrabChoice::Pull,
	      "the other hand's grip on a bow out of touch: pulls it");
	Check(ChooseWeaponGrab(true, IsTakingHand(bow, false), true, 30.0f, 20.0f, reach, touch, true) ==
	          WeaponGrabChoice::Pull,
	      "the weapon hand's grip on a bow out of touch: the pick's own pull (the grab then only carries it)");
}

void TestWhichHoldTakes() {
	std::printf("Which hold takes at the grip\n");
	Check(TakingHoldClosed(5, false, true), "a bow: the other hand's hold closed takes");
	Check(!TakingHoldClosed(5, true, false), "a bow: the weapon hand's hold closed alone does not");
	Check(TakingHoldClosed(0, true, false), "a sword: the weapon hand's hold takes");
	Check(!TakingHoldClosed(0, false, true), "a sword: the other hand's hold does not");
	Check(TakingHoldClosed(4, true, false) && TakingHoldClosed(1, true, true), "a staff, a two-hander: the weapon hand's");
	Check(!TakingHoldClosed(5, false, false) && !TakingHoldClosed(0, false, false), "no hold closed: nothing takes");
}

void TestWhichGrabIsAPull() {
	std::printf("Which hand's grab is a weapon's pull\n");
	Check(GrabHandTakes(5, true), "a bow grabbed by the other hand: a pull to be taken");
	Check(!GrabHandTakes(5, false), "a bow grabbed by the weapon hand: a carry");
	Check(GrabHandTakes(0, false), "a sword grabbed by the weapon hand: a pull");
	Check(!GrabHandTakes(0, true), "a sword grabbed by the other hand: a carry");
	Check(!GrabHandTakes(-1, false) && !GrabHandTakes(-1, true), "not a weapon: neither");
	Check(GrabHandTakes(4, false) && GrabHandTakes(1, false) && GrabHandTakes(3, false) && GrabHandTakes(2, false),
	      "a staff, a two-hander, a blunt: the weapon hand's");
}

void TestGripPoint() {
	std::printf("The bow's grip point\n");
	Check(Near(GripAxialFor(kWeaponTypeBow), kGripAxialBow) && Near(kGripAxialBow, -2.7f),
	      "a bow's grip 2.7 below its origin, the palm's place measured on the other hand's Torch node");
	Check(Near(kGripAxialBow, kGripAxialTorch), "the bow hangs where the torch does: the same palm");
	Check(Near(GripAxialFor(0), -0.9f) && Near(GripAxialFor(4), -37.6f), "the blade's and the staff's stay");
}

}  // namespace

int main() {
	TestWhatTheOtherHandTakes();
	TestTakingHandForABow();
	TestWhichHoldTakes();
	TestWhichGrabIsAPull();
	TestGripPoint();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}

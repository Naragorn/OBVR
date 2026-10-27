// Checks a torch in the world taken at its grip (game/TorchGrab.h): what
// counts as a torch, which hand takes it, what keeps the other hand from
// taking it, what a closing grip does with it (ChooseWeaponGrab with the
// other hand as the taking one), and the take's steps after the activation:
// the equip once, done when worn, the give-up and the cancel.

#include <cmath>
#include <cstdio>

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

void TestWhatIsATorch() {
	std::printf("What counts as a torch\n");
	Check(IsTorchLight(0x1A, 0x2B), "Torch02's light, flags 0x2B (carried): a torch");
	Check(IsTorchLight(0x1A, 0x02), "carried alone: a torch");
	Check(!IsTorchLight(0x1A, 0x29), "a light not carried (a lamp of the cell): no torch");
	Check(!IsTorchLight(0x29, 0x02), "another form type with the bit: no torch");
	Check(Near(GripAxialFor(0), -0.9f) && Near(kGripAxialTorch, -2.7f), "the torch's grip 2.7 below its model's origin, as the palm was measured");
}

void TestTakingHand() {
	std::printf("Which hand takes\n");
	Check(IsTakingHand(false, false), "a weapon near the weapon hand: that hand takes");
	Check(!IsTakingHand(false, true), "a weapon near the other hand: it only carries");
	Check(IsTakingHand(true, true), "a torch near the other hand: that hand takes");
	Check(!IsTakingHand(true, false), "a torch near the weapon hand: it only carries");
}

void TestGate() {
	std::printf("What keeps the other hand from taking a torch\n");
	Check(TorchTakeGateFor(true, true, false, false, false) == TorchTakeGate::Takes, "all clear: takes");
	Check(TorchTakeGateFor(false, true, false, false, false) == TorchTakeGate::Off, "the setting off");
	Check(TorchTakeGateFor(true, false, false, false, false) == TorchTakeGate::NotNow, "a menu or a grab in flight");
	Check(TorchTakeGateFor(true, true, true, false, false) == TorchTakeGate::BowDrawn, "a bow drawn holds that hand");
	Check(TorchTakeGateFor(true, true, false, true, false) == TorchTakeGate::HandFull,
	      "a shield or a torch on that hand");
	Check(TorchTakeGateFor(true, true, false, false, true) == TorchTakeGate::Holster, "the holster has the grip");
	Check(TorchTakeGateFor(false, false, true, true, true) == TorchTakeGate::Off, "the setting is asked first");
	Check(TorchTakeGateFor(true, false, true, true, true) == TorchTakeGate::NotNow, "then the moment");
	Check(TorchTakeGateFor(true, true, true, true, true) == TorchTakeGate::BowDrawn, "then the bow");
	Check(TorchTakeGateFor(true, true, false, true, true) == TorchTakeGate::HandFull, "then the hand");
	bool allWords = true;
	for (UInt8 g = 0; g <= static_cast<UInt8>(TorchTakeGate::Holster); ++g) {
		allWords = allWords && TorchTakeGateWords(static_cast<TorchTakeGate>(g))[0] != '?';
	}
	Check(allWords, "every gate has its words for the log");
}

void TestChoice() {
	std::printf("What a closing grip does on a torch\n");
	const float reach = 12.6f;  // 0.18 m at 70 units a metre
	const float touch = 3.0f;
	// The other hand is the taking hand for a torch (IsTakingHand(true, true)).
	Check(ChooseWeaponGrab(true, IsTakingHand(true, true), true, 5.0f, 2.0f, reach, touch, true) ==
	          WeaponGrabChoice::Equip,
	      "the other hand at the grip: take and equip");
	Check(ChooseWeaponGrab(true, IsTakingHand(true, true), false, 5.0f, 2.0f, reach, touch, true) ==
	          WeaponGrabChoice::Carry,
	      "the other hand at the grip, the take gated: carried");
	Check(ChooseWeaponGrab(true, IsTakingHand(true, true), true, 30.0f, 2.0f, reach, touch, true) ==
	          WeaponGrabChoice::Carry,
	      "the other hand touching the head, off the grip: carried");
	Check(ChooseWeaponGrab(true, IsTakingHand(true, true), true, 30.0f, 20.0f, reach, touch, true) ==
	          WeaponGrabChoice::Pull,
	      "the other hand out of touch, the pull on: pulled, equipped on arrival");
	Check(ChooseWeaponGrab(true, IsTakingHand(true, true), true, 30.0f, 20.0f, reach, touch, false) ==
	          WeaponGrabChoice::Nothing,
	      "the other hand out of touch, the pull off: nothing");
	Check(ChooseWeaponGrab(true, IsTakingHand(true, false), true, 5.0f, -1.0f, reach, touch, true) ==
	          WeaponGrabChoice::Carry,
	      "the weapon hand at the torch's grip: carried");
}

void TestTake() {
	std::printf("The take's steps\n");
	TorchTakeState s;
	TorchTakeInput in;
	in.allowed = true;
	in.dt = 0.02f;
	Check(!StepTorchTake(s, in).equipNow, "idle: nothing");
	BeginTorchTake(s, 0x1234, 2.0f);
	TorchTakeVerdict v = StepTorchTake(s, in);
	Check(v.equipNow && v.form == 0x1234 && !v.done, "first frame: the equip sent");
	v = StepTorchTake(s, in);
	Check(!v.equipNow && !v.done && !v.gaveUp, "not worn yet: waits, no second equip");
	in.wornTorch = 0x9999;
	v = StepTorchTake(s, in);
	Check(!v.done, "another torch worn (the old one still): waits");
	in.wornTorch = 0x1234;
	v = StepTorchTake(s, in);
	Check(v.done && s.phase == TorchTakePhase::Idle, "worn: done");

	BeginTorchTake(s, 0x1234, 0.1f);
	in.wornTorch = 0;
	StepTorchTake(s, in);
	bool gaveUp = false;
	for (int i = 0; i < 10 && !gaveUp; ++i) {
		gaveUp = StepTorchTake(s, in).gaveUp;
	}
	Check(gaveUp && s.phase == TorchTakePhase::Idle, "never worn: given up after the wait");

	BeginTorchTake(s, 0x1234, 2.0f);
	StepTorchTake(s, in);
	in.allowed = false;
	v = StepTorchTake(s, in);
	Check(v.cancelled && s.phase == TorchTakePhase::Idle, "a menu: cancelled");

	BeginTorchTake(s, 0x1234, 2.0f);
	v = StepTorchTake(s, in);
	Check(v.cancelled && !v.equipNow, "a menu before the equip: cancelled, nothing sent");

	BeginTorchTake(s, 0x1234, -1.0f);
	in.allowed = true;
	Check(Near(s.wait, 2.0f), "a nonsense wait: two seconds");
	BeginTorchTake(s, 0, 2.0f);
	StepTorchTake(s, in);
	in.wornTorch = 0;
	Check(!StepTorchTake(s, in).done, "no form: never done by an empty slot");
}

}  // namespace

int main() {
	TestWhatIsATorch();
	TestTakingHand();
	TestGate();
	TestChoice();
	TestTake();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

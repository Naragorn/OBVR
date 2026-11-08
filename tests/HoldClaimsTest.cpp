// Who holds a fighter (game/HoldClaims.h): the off hand's grab and a stuck
// blade claim him, each in its own right; he is let go only when neither
// does - every order of letting go.

#include <cstdio>

#include "game/HoldClaims.h"

namespace {

using namespace obvr;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kFoe = 0x1000u;
constexpr UInt32 kOther = 0x2000u;

void TestAlone() {
	std::printf("one holder\n");
	HoldClaims c;
	Check(HoldEffectOf(c).still == 0 && HoldEffectOf(c).foe == 0, "nobody claims: nobody held");
	Claim(c, HoldOwner::Grab, kFoe, true);
	Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == kFoe, "the grab by the neck: legs and blows held");
	Release(c, HoldOwner::Grab);
	Check(HoldEffectOf(c).still == 0 && HoldEffectOf(c).foe == 0, "the grab lets go: free");
	Claim(c, HoldOwner::Grab, kFoe, false);
	Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == 0,
	      "the grab by a part that does not stop blows: legs held, blows land");
	Release(c, HoldOwner::Grab);
	Claim(c, HoldOwner::Blade, kFoe, true);
	Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == kFoe, "a stuck blade alone: legs and blows held");
}

void TestBothOrders() {
	std::printf("both on the same fighter, every order\n");
	{
		HoldClaims c;
		Claim(c, HoldOwner::Grab, kFoe, true);
		Claim(c, HoldOwner::Blade, kFoe, true);
		Release(c, HoldOwner::Blade);
		Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == kFoe,
		      "grab, stab, the blade out: still held by the grab");
		Release(c, HoldOwner::Grab);
		Check(HoldEffectOf(c).still == 0 && HoldEffectOf(c).foe == 0, "then the left opens: free");
	}
	{
		HoldClaims c;
		Claim(c, HoldOwner::Grab, kFoe, true);
		Claim(c, HoldOwner::Blade, kFoe, true);
		Release(c, HoldOwner::Grab);
		Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == kFoe,
		      "grab, stab, the left opens: still held by the blade");
		Release(c, HoldOwner::Blade);
		Check(HoldEffectOf(c).still == 0 && HoldEffectOf(c).foe == 0, "then the blade out: free");
	}
	{
		HoldClaims c;
		Claim(c, HoldOwner::Blade, kFoe, true);
		Claim(c, HoldOwner::Grab, kFoe, false);
		Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == kFoe,
		      "stab, then a grab that does not stop blows: the blade still stops them");
		Release(c, HoldOwner::Blade);
		Check(HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == 0,
		      "the blade out: the grab holds the legs, his blows land again");
		Release(c, HoldOwner::Grab);
		Check(HoldEffectOf(c).still == 0, "the left opens: free");
	}
	{
		HoldClaims c;
		Claim(c, HoldOwner::Grab, kFoe, true);
		Release(c, HoldOwner::Blade);
		Check(HoldEffectOf(c).still == kFoe, "a release by one that held nothing changes nothing");
	}
}

void TestTwoFighters() {
	std::printf("two fighters: one slot\n");
	HoldClaims c;
	Claim(c, HoldOwner::Grab, kFoe, true);
	Check(!Claim(c, HoldOwner::Blade, kOther, true) && HoldEffectOf(c).still == kFoe && HoldEffectOf(c).foe == kFoe,
	      "the grab holds one, a blade sticks in another: refused, the grab's stays held");
	Release(c, HoldOwner::Grab);
	Check(HoldEffectOf(c).still == 0, "the grab lets go: nobody (the refused claim was never taken)");
	HoldClaims d;
	Check(Claim(d, HoldOwner::Blade, kOther, true), "a blade alone: taken");
	Check(Claim(d, HoldOwner::Grab, kFoe, true) && HoldEffectOf(d).still == kFoe,
	      "a grab of another while a blade holds: the grab wins the slot (the hand placing him needs his legs)");
	Release(d, HoldOwner::Grab);
	Check(HoldEffectOf(d).still == kOther && HoldEffectOf(d).foe == kOther,
	      "the grab lets go: the blade's fighter is held again");
	Check(!Claim(d, HoldOwner::Blade, 0u, true) || HoldEffectOf(d).still == 0, "a claim of nobody is a release");
}

}  // namespace

int main() {
	TestAlone();
	TestBothOrders();
	TestTwoFighters();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

// Checks the weight of every held item (vr/HeldItemWeight.h): what each hand
// holds that weighs - a swung weapon or a staff in the weapon hand; the bow,
// the shield or the torch in the off hand, the bow first - the weight each
// lags by (the form's own, the torch's constant, none unread or not a number),
// when the off hand does not lag (no hand, adjusting, a menu, the tablet, a
// weapon's handle), the off hand's tuning (the weapon's curve, never the
// two-hander's, exhaustion as for the weapon) and the log's words.

#include <cstdio>
#include <cstring>
#include <limits>

#include "vr/HeldItemWeight.h"

namespace {

using namespace obvr::vr;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.0001f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestWeaponHand() {
	std::printf("The weapon hand\n");
	Check(WeaponHandWeighed(true, false) == WeighedItem::Weapon, "a swung weapon drawn weighs");
	Check(WeaponHandWeighed(false, true) == WeighedItem::Staff, "a staff drawn weighs");
	Check(WeaponHandWeighed(false, false) == WeighedItem::None, "nothing drawn: nothing weighs");
	Check(WeaponHandWeighed(true, true) == WeighedItem::Weapon, "a swung weapon is never a staff");
	Check(WeaponHandLags(24.0f, true, false), "a weight, a tracked hand: it lags");
	Check(!WeaponHandLags(0.0f, true, false), "no weight: on the controller");
	Check(!WeaponHandLags(kNaN, true, false), "a weight that is not a number: on the controller");
	Check(!WeaponHandLags(24.0f, false, false), "no drawn hand (untracked, third person): nothing lags");
	Check(!WeaponHandLags(24.0f, true, true), "the hands being adjusted: the fit measures the controller");
}

void TestOffHand() {
	std::printf("The off hand\n");
	Check(OffHandWeighed(true, OffHandItem::Nothing) == WeighedItem::Bow, "the drawn bow weighs");
	Check(OffHandWeighed(true, OffHandItem::Shield) == WeighedItem::Bow, "the drawn bow first, a shield worn or not");
	Check(OffHandWeighed(true, OffHandItem::Torch) == WeighedItem::Bow, "and a torch worn or not");
	Check(OffHandWeighed(false, OffHandItem::Shield) == WeighedItem::Shield, "the worn shield weighs");
	Check(OffHandWeighed(false, OffHandItem::Torch) == WeighedItem::Torch, "the worn torch weighs");
	Check(OffHandWeighed(false, OffHandItem::Nothing) == WeighedItem::None,
	      "an empty hand or a spell: nothing weighs");
}

void TestLagWeight() {
	std::printf("The weight each lags by\n");
	Check(Near(LagWeightOf(WeighedItem::Shield, true, 18.0f), 18.0f), "the shield its form's weight");
	Check(Near(LagWeightOf(WeighedItem::Bow, true, 8.0f), 8.0f), "the bow its form's weight");
	Check(Near(LagWeightOf(WeighedItem::Weapon, true, 24.0f), 24.0f), "a weapon its form's weight");
	Check(Near(LagWeightOf(WeighedItem::Staff, true, 10.0f), 10.0f), "a staff its form's weight");
	Check(Near(LagWeightOf(WeighedItem::Torch, true, 1.0f), kTorchLagWeight), "the torch the constant, not its form's");
	Check(Near(LagWeightOf(WeighedItem::Torch, false, 0.0f), kTorchLagWeight), "the torch with no form read: the constant");
	Check(kTorchLagWeight > kWeaponLagLightWeight, "the torch's constant lags at all (over the dagger's)");
	Check(WeightFactor(kTorchLagWeight) < 0.5f, "and lightly (under half the heaviest)");
	Check(Near(LagWeightOf(WeighedItem::Shield, false, 18.0f), 0.0f), "a weight not read: none");
	Check(Near(LagWeightOf(WeighedItem::Shield, true, kNaN), 0.0f), "a weight that is not a number: none");
	Check(Near(LagWeightOf(WeighedItem::Shield, true, -1.0f), 0.0f), "a negative weight: none");
	Check(Near(LagWeightOf(WeighedItem::Shield, true, 5000.0f), 0.0f), "a weight no item has: none");
	Check(Near(LagWeightOf(WeighedItem::None, true, 18.0f), 0.0f), "nothing held: none");
}

OffHandLagGate Lagging() {
	OffHandLagGate g;
	g.weight = 18.0f;
	g.handValid = true;
	return g;
}

void TestOffHandGate() {
	std::printf("When the off hand lags\n");
	Check(OffHandLags(Lagging()), "a weight, a tracked hand in first person: it lags");
	OffHandLagGate g = Lagging();
	g.weight = 0.0f;
	Check(!OffHandLags(g), "no weight (an empty hand, a spell): on the controller");
	g = Lagging();
	g.weight = kNaN;
	Check(!OffHandLags(g), "a weight that is not a number: on the controller");
	g = Lagging();
	g.handValid = false;
	Check(!OffHandLags(g), "no drawn hand: nothing lags");
	g = Lagging();
	g.adjusting = true;
	Check(!OffHandLags(g), "the hands being adjusted: the fit measures the controller");
	g = Lagging();
	g.menuMode = true;
	Check(!OffHandLags(g), "a menu up: the laser and the menus are the controller's");
	g = Lagging();
	g.tabletUp = true;
	Check(!OffHandLags(g), "the tablet in that hand: held where the hand is");
	g = Lagging();
	g.onHandle = true;
	Check(!OffHandLags(g), "the hand on a weapon's handle: pinned to the handle, not lagged");
}

void TestOffHandTuning() {
	std::printf("The off hand's tuning\n");
	const WeaponLagTuning shield = OffHandLagTuning(18.0f, 40.0f, true, 0.0f, 1.0f);
	const WeaponLagTuning weapon = WeaponLagFor(18.0f, 40.0f, false, true);
	Check(Near(shield.timeConstantSeconds, weapon.timeConstantSeconds) && Near(shield.capMetres, weapon.capMetres) &&
	          Near(shield.capRadians, weapon.capRadians) && shield.swingThrough,
	      "the weapon's curve, one-handed, the same slider");
	Check(!OffHandLagTuning(18.0f, 40.0f, false, 0.0f, 1.0f).swingThrough, "the swing-through off: the plain lag");
	const WeaponLagTuning torch = OffHandLagTuning(kTorchLagWeight, 40.0f, true, 0.0f, 1.0f);
	Check(torch.timeConstantSeconds > 0.0f && torch.timeConstantSeconds < shield.timeConstantSeconds,
	      "the torch lags, less than an 18 shield");
	const WeaponLagTuning tired = OffHandLagTuning(18.0f, 40.0f, true, 1.0f, 3.0f);
	const WeaponLagTuning tiredWeapon = WeaponLagExhausted(weapon, 1.0f, 3.0f);
	Check(Near(tired.timeConstantSeconds, tiredWeapon.timeConstantSeconds) && Near(tired.capMetres, tiredWeapon.capMetres),
	      "exhausted, heavier as the weapon is");
	Check(Near(OffHandLagTuning(2.0f, 40.0f, true, 0.0f, 1.0f).timeConstantSeconds, 0.0f),
	      "a buckler under the dagger's weight: no lag");
}

void TestWords() {
	std::printf("The log's words\n");
	Check(std::strcmp(WeighedItemWords(WeighedItem::Weapon), "a weapon") == 0, "a weapon");
	Check(std::strcmp(WeighedItemWords(WeighedItem::Staff), "a staff") == 0, "a staff");
	Check(std::strcmp(WeighedItemWords(WeighedItem::Bow), "a bow") == 0, "a bow");
	Check(std::strcmp(WeighedItemWords(WeighedItem::Shield), "a shield") == 0, "a shield");
	Check(std::strcmp(WeighedItemWords(WeighedItem::Torch), "a torch") == 0, "a torch");
	Check(std::strcmp(WeighedItemWords(WeighedItem::None), "nothing") == 0, "nothing");
}

}  // namespace

int main() {
	TestWeaponHand();
	TestOffHand();
	TestLagWeight();
	TestOffHandGate();
	TestOffHandTuning();
	TestWords();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all held item weight checks passed\n");
	return 0;
}

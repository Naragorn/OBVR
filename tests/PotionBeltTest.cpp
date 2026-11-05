// Potions kept at the holster places (vr/PotionBelt.h): every flow of the
// place a potion may go to, the places a weapon takes over, and the reach.
#include <cstdio>

#include "vr/PotionBelt.h"

namespace {

using namespace obvr::vr;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kPotion = 0x00098296;
constexpr UInt32 kOther = 0x00098297;
constexpr UInt32 kSword = 0x00025AFB;

}  // namespace

int main() {
	std::printf("Potions at the places\n");
	UInt32 holster[kHolsterSlotCount] = {};
	UInt32 potions[kHolsterSlotCount] = {};
	Check(PotionPlaceFor(HolsterSlot::HipLeft, holster, potions, kPotion) == HolsterSlot::HipLeft,
	      "a free place takes a potion");
	Check(PotionPlaceFor(HolsterSlot::None, holster, potions, kPotion) == HolsterSlot::None, "no place: none");
	Check(PotionPlaceFor(HolsterSlot::HipLeft, holster, potions, 0) == HolsterSlot::None, "no potion: none");
	holster[static_cast<UInt32>(HolsterSlot::HipLeft)] = kSword;
	Check(PotionPlaceFor(HolsterSlot::HipLeft, holster, potions, kPotion) == HolsterSlot::None,
	      "a place keeping a weapon takes no potion");
	potions[static_cast<UInt32>(HolsterSlot::BackRight)] = kOther;
	Check(PotionPlaceFor(HolsterSlot::BackRight, holster, potions, kPotion) == HolsterSlot::None,
	      "a place keeping another potion takes no second one");
	Check(PotionPlaceFor(HolsterSlot::BackRight, holster, potions, kOther) == HolsterSlot::BackRight,
	      "the same potion again: the same place");

	KeepPotionAt(potions, HolsterSlot::HipRight, kOther);
	Check(potions[static_cast<UInt32>(HolsterSlot::HipRight)] == kOther &&
	          potions[static_cast<UInt32>(HolsterSlot::BackRight)] == 0,
	      "a potion kept at a new place leaves the old one: one place per potion");
	KeepPotionAt(potions, HolsterSlot::None, kPotion);
	KeepPotionAt(potions, HolsterSlot::BackLeft, 0);
	Check(potions[static_cast<UInt32>(HolsterSlot::BackLeft)] == 0 && potions[0] == 0,
	      "no place or no potion: nothing kept");

	UInt32 cleared[kHolsterSlotCount] = {};
	cleared[static_cast<UInt32>(HolsterSlot::HipLeft)] = kPotion;
	cleared[static_cast<UInt32>(HolsterSlot::BackLeft)] = kOther;
	Check(ForgetPotionsUnderItems(cleared, holster) == 1 && cleared[static_cast<UInt32>(HolsterSlot::HipLeft)] == 0 &&
	          cleared[static_cast<UInt32>(HolsterSlot::BackLeft)] == kOther,
	      "a weapon kept where a potion was: the potion forgotten (it stays in the pack)");

	std::printf("The reach for a potion\n");
	UInt32 kept[kHolsterSlotCount] = {};
	kept[static_cast<UInt32>(HolsterSlot::HipRight)] = kPotion;
	PotionReachInput in;
	in.allowed = true;
	in.closedNow = true;
	in.slot = HolsterSlot::HipRight;
	Check(PotionReachAt(in, kept) == kPotion, "the grip closes at a place keeping a potion: that potion drawn");
	in.closedNow = false;
	Check(PotionReachAt(in, kept) == 0, "a grip held, not closing now: nothing");
	in.closedNow = true;
	in.handBusy = true;
	Check(PotionReachAt(in, kept) == 0, "the hand holds something or the holster acted there: nothing");
	in.handBusy = false;
	in.allowed = false;
	Check(PotionReachAt(in, kept) == 0, "not allowed (a menu, not Full VR): nothing");
	in.allowed = true;
	in.slot = HolsterSlot::HipLeft;
	Check(PotionReachAt(in, kept) == 0, "a place keeping no potion: nothing");
	in.slot = HolsterSlot::None;
	Check(PotionReachAt(in, kept) == 0, "no place: nothing");
	in.slot = static_cast<HolsterSlot>(9);
	Check(PotionReachAt(in, kept) == 0, "a place past the list: nothing");
	Check(!HolsterTookTheReach(SlotOutcome::Empty) && !HolsterTookTheReach(SlotOutcome::None) &&
	          HolsterTookTheReach(SlotOutcome::Sheathe) && HolsterTookTheReach(SlotOutcome::Take) &&
	          HolsterTookTheReach(SlotOutcome::WrongHand),
	      "the holster's reach that found the place empty leaves it to the potion; any other is the holster's");

	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

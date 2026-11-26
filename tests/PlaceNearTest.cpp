#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "vr/PlaceNear.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestOpacity() {
	std::printf("The opacity\n");
	Check(Near(PlaceHintOpacityOf(0.25f), 0.25f), "within its bounds: as set");
	Check(Near(PlaceHintOpacityOf(0.0f), kPlaceHintOpacityLeast) && Near(PlaceHintOpacityOf(-3.0f), kPlaceHintOpacityLeast),
	      "too low: the least");
	Check(Near(PlaceHintOpacityOf(7.0f), kPlaceHintOpacityMost), "too high: the most");
	Check(Near(PlaceHintOpacityOf(kNaN), kPlaceHintOpacity), "no number: the default");
	Check(kPlaceHintOpacity <= 0.3f, "the default is faint");
}

void TestTarget() {
	std::printf("How visible a place should be\n");
	const PlaceNearSettings s;
	Check(Near(PlaceNearTarget(s, true, 0.0f, 0.2f), 0.25f), "at its middle: its opacity");
	Check(Near(PlaceNearTarget(s, true, 0.2f, 0.2f), 0.25f), "at its edge: its opacity");
	Check(Near(PlaceNearTarget(s, true, 0.2f + kPlaceNearMarginMetres * 0.5f, 0.2f), 0.125f), "half the margin outside: half of it");
	Check(Near(PlaceNearTarget(s, true, 0.2f + kPlaceNearMarginMetres, 0.2f), 0.0f), "the margin outside: nothing");
	Check(Near(PlaceNearTarget(s, true, 2.0f, 0.2f), 0.0f), "far away: nothing");
	Check(Near(PlaceNearTarget(s, false, 0.0f, 0.2f), 0.0f), "nothing held: nothing, even at its middle");
	PlaceNearSettings off;
	off.enabled = false;
	Check(Near(PlaceNearTarget(off, true, 0.0f, 0.2f), 0.0f), "switched off: nothing");
	Check(Near(PlaceNearTarget(s, true, kNaN, 0.2f), 0.0f), "no distance: nothing");
	Check(Near(PlaceNearTarget(s, true, 0.0f, 0.0f), 0.0f) && Near(PlaceNearTarget(s, true, 0.0f, kNaN), 0.0f), "no radius: nothing");
	PlaceNearSettings strong;
	strong.opacity = 0.8f;
	Check(Near(PlaceNearTarget(strong, true, 0.1f, 0.2f), 0.8f), "a stronger setting: that opacity");
	PlaceNearSettings wild;
	wild.opacity = 9.0f;
	Check(Near(PlaceNearTarget(wild, true, 0.1f, 0.2f), 1.0f), "a setting past the most: the most");
}

void TestFade() {
	std::printf("The fade\n");
	float a = 0.0f;
	a = StepPlaceNearAlpha(a, 0.25f, kPlaceNearFadeSeconds * 0.5f, 0.25f);
	Check(Near(a, 0.125f), "half the fade's time: half way in");
	a = StepPlaceNearAlpha(a, 0.25f, kPlaceNearFadeSeconds, 0.25f);
	Check(Near(a, 0.25f), "the rest and more: at the target, not past it");
	a = StepPlaceNearAlpha(a, 0.0f, kPlaceNearFadeSeconds * 0.5f, 0.25f);
	Check(Near(a, 0.125f), "and out again the same way");
	a = StepPlaceNearAlpha(a, 0.0f, 5.0f, 0.25f);
	Check(Near(a, 0.0f), "a long frame: at nothing, not below");
	Check(Near(StepPlaceNearAlpha(0.1f, 0.25f, 0.0f, 0.25f), 0.1f) && Near(StepPlaceNearAlpha(0.1f, 0.25f, -1.0f, 0.25f), 0.1f) &&
	          Near(StepPlaceNearAlpha(0.1f, 0.25f, kNaN, 0.25f), 0.1f),
	      "no time passed, or none readable: as it was");
	Check(Near(StepPlaceNearAlpha(kNaN, 0.0f, 0.01f, 0.25f), 0.0f), "no number to start from: from nothing");
	Check(!PlaceNearShown(0.0f) && !PlaceNearShown(0.005f) && PlaceNearShown(0.01f) && PlaceNearShown(0.25f), "shown from a hundredth");
}

HeldForPlaces HeldOf(WeaponSeen seen, EquippedKind equipped, OffHandItem offHand = OffHandItem::Nothing,
                     SInt32 potionHand = -1) {
	HeldForPlacesInput in;
	in.seen = seen;
	in.equipped = equipped;
	in.offHand = offHand;
	in.potionHand = potionHand;
	return HeldForPlacesOf(in);
}

constexpr UInt32 kWeapon = 0;
constexpr UInt32 kOther = 1;

void TestHeld() {
	std::printf("What is held that belongs at a place\n");
	HeldForPlaces h = HeldOf(WeaponSeen::Drawn, EquippedKind::OneHand);
	Check(h.hand[kWeapon].item == SlotItem::OneHand && h.hand[kOther].item == SlotItem::Nothing,
	      "a drawn one-hander: the weapon hand's");
	const HandHeldForPlace& w = h.hand[kWeapon];
	Check(PlaceTakesHeld(HolsterSlot::HipLeft, w, false) && PlaceTakesHeld(HolsterSlot::HipRight, w, false) &&
	          PlaceTakesHeld(HolsterSlot::BackLeft, w, false) && PlaceTakesHeld(HolsterSlot::BackRight, w, false),
	      "it shows the hips and the shoulders (2026-11-26)");
	h = HeldOf(WeaponSeen::Drawn, EquippedKind::TwoHand);
	Check(h.hand[kWeapon].item == SlotItem::TwoHand, "a drawn two-hander: the weapon hand's");
	Check(!PlaceTakesHeld(HolsterSlot::HipLeft, h.hand[kWeapon], false) &&
	          PlaceTakesHeld(HolsterSlot::BackLeft, h.hand[kWeapon], false) &&
	          PlaceTakesHeld(HolsterSlot::BackRight, h.hand[kWeapon], false),
	      "it shows the back, not the hips");
	h = HeldOf(WeaponSeen::Drawn, EquippedKind::Bow);
	Check(h.hand[kOther].item == SlotItem::Bow && h.hand[kWeapon].item == SlotItem::Nothing, "a drawn bow: the other hand's");
	Check(PlaceTakesHeld(HolsterSlot::BackLeft, h.hand[kOther], false) &&
	          !PlaceTakesHeld(HolsterSlot::HipRight, h.hand[kOther], false),
	      "it shows the back");
	h = HeldOf(WeaponSeen::Drawn, EquippedKind::Nothing);
	Check(h.hand[kWeapon].item == SlotItem::Nothing && !PlaceTakesHeld(HolsterSlot::HipLeft, h.hand[kWeapon], false),
	      "the fists up: nothing held for a place");
	h = HeldOf(WeaponSeen::Sheathed, EquippedKind::OneHand);
	Check(h.hand[kWeapon].item == SlotItem::Nothing && !PlaceTakesHeld(HolsterSlot::HipLeft, h.hand[kWeapon], false),
	      "sheathed: nothing held");
	h = HeldOf(WeaponSeen::Unknown, EquippedKind::TwoHand);
	Check(h.hand[kWeapon].item == SlotItem::Nothing, "not known (a menu): nothing held");
	Check(!PlaceTakesHeld(HolsterSlot::None, HeldOf(WeaponSeen::Drawn, EquippedKind::OneHand).hand[kWeapon], true),
	      "no place: never");
}

// Nadi, 2026-11-26: the torch showed the sign's words where a weapon shows the circle - the
// circle wanted for it too, and maybe for potions.
void TestOtherHandAndPotions() {
	std::printf("The torch, the shield and potions get the circle as the weapons do\n");
	HeldForPlaces h = HeldOf(WeaponSeen::Sheathed, EquippedKind::OneHand, OffHandItem::Torch);
	Check(h.hand[kOther].item == SlotItem::Torch && h.hand[kWeapon].item == SlotItem::Nothing,
	      "a torch in the other hand, the sword sheathed: the other hand's torch");
	Check(PlaceTakesHeld(HolsterSlot::HipLeft, h.hand[kOther], false) &&
	          PlaceTakesHeld(HolsterSlot::HipRight, h.hand[kOther], false) &&
	          !PlaceTakesHeld(HolsterSlot::BackLeft, h.hand[kOther], false) &&
	          !PlaceTakesHeld(HolsterSlot::BackRight, h.hand[kOther], false),
	      "the torch shows the hips, not the back");
	h = HeldOf(WeaponSeen::Drawn, EquippedKind::OneHand, OffHandItem::Torch);
	Check(h.hand[kWeapon].item == SlotItem::OneHand && h.hand[kOther].item == SlotItem::Torch,
	      "the sword drawn beside the torch: each hand its own");
	h = HeldOf(WeaponSeen::Drawn, EquippedKind::Nothing, OffHandItem::Shield);
	Check(h.hand[kOther].item == SlotItem::Shield, "a shield worn: the other hand's");
	Check(PlaceTakesHeld(HolsterSlot::HipLeft, h.hand[kOther], false) &&
	          PlaceTakesHeld(HolsterSlot::BackRight, h.hand[kOther], false),
	      "the shield shows the hips and the back, as it is kept at all four");
	h = HeldOf(WeaponSeen::Drawn, EquippedKind::Bow, OffHandItem::Shield);
	Check(h.hand[kOther].item == SlotItem::Bow, "the bow drawn: the other hand's place is the bow's, as the sign says");
	h = HeldOf(WeaponSeen::Sheathed, EquippedKind::Nothing, OffHandItem::Nothing, 1);
	Check(h.hand[kOther].potion && !h.hand[kWeapon].potion && h.hand[kOther].item == SlotItem::Nothing,
	      "a potion in the other hand: that hand's");
	Check(PlaceTakesHeld(HolsterSlot::BackLeft, h.hand[kOther], true) &&
	          !PlaceTakesHeld(HolsterSlot::HipLeft, h.hand[kOther], false),
	      "a potion: the places a let go would keep it at, no other");
	h = HeldOf(WeaponSeen::Sheathed, EquippedKind::Nothing, OffHandItem::Nothing, 0);
	Check(h.hand[kWeapon].potion && !h.hand[kOther].potion, "a potion in the weapon hand: that hand's");
	h = HeldOf(WeaponSeen::Sheathed, EquippedKind::Nothing, OffHandItem::Nothing, 5);
	Check(!h.hand[kWeapon].potion && !h.hand[kOther].potion, "no hand read for the potion: nobody's");
	Check(!PlaceTakesHeld(HolsterSlot::None, h.hand[kWeapon], true), "no place, a potion held: never");
}

void TestBothHands() {
	std::printf("A place's circle from both hands\n");
	const PlaceNearSettings s;
	PlaceNearHand hands[2];
	hands[kWeapon] = PlaceNearHand{true, 1.0f, true};
	hands[kOther] = PlaceNearHand{true, 0.1f, true};
	PlaceNearOne one = PlaceNearFromHands(s, hands, 0.2f);
	Check(Near(one.target, 0.25f) && one.lit, "the other hand's torch inside: shown and lit, the sword far away");
	hands[kOther].takes = false;
	one = PlaceNearFromHands(s, hands, 0.2f);
	Check(Near(one.target, 0.0f) && !one.lit, "the hand inside holds nothing the place takes: nothing");
	hands[kWeapon] = PlaceNearHand{true, 0.2f + kPlaceNearMarginMetres * 0.5f, true};
	hands[kOther] = PlaceNearHand{true, 0.25f, true};
	one = PlaceNearFromHands(s, hands, 0.2f);
	Check(Near(one.target, 0.25f * (1.0f - 0.05f / kPlaceNearMarginMetres)) && !one.lit,
	      "both outside: the nearer one's fade, not lit");
	hands[kWeapon] = PlaceNearHand{false, 0.0f, true};
	hands[kOther] = PlaceNearHand{false, 0.0f, true};
	one = PlaceNearFromHands(s, hands, 0.2f);
	Check(Near(one.target, 0.0f) && !one.lit, "neither hand tracked: nothing");
	hands[kWeapon] = PlaceNearHand{true, kNaN, true};
	hands[kOther] = PlaceNearHand{true, 0.05f, true};
	one = PlaceNearFromHands(s, hands, 0.2f);
	Check(Near(one.target, 0.25f) && one.lit, "one hand's distance unread: the other still counts");
}

void TestHeldWords() {
	std::printf("What the log names as held\n");
	char words[96];
	PlaceNearHeldWords(HeldOf(WeaponSeen::Drawn, EquippedKind::OneHand), words, sizeof(words));
	Check(std::strcmp(words, "the weapon hand's weapon") == 0, "the drawn sword: as the log has said since 2026-11-16");
	PlaceNearHeldWords(HeldOf(WeaponSeen::Sheathed, EquippedKind::OneHand), words, sizeof(words));
	Check(std::strcmp(words, "the weapon hand's nothing") == 0, "nothing: the weapon hand's nothing, as before");
	PlaceNearHeldWords(HeldOf(WeaponSeen::Drawn, EquippedKind::Bow), words, sizeof(words));
	Check(std::strcmp(words, "the other hand's bow") == 0, "the drawn bow: the other hand's, as before");
	PlaceNearHeldWords(HeldOf(WeaponSeen::Drawn, EquippedKind::OneHand, OffHandItem::Torch), words, sizeof(words));
	Check(std::strcmp(words, "the weapon hand's weapon, the other hand's torch") == 0, "both: both named");
	PlaceNearHeldWords(HeldOf(WeaponSeen::Sheathed, EquippedKind::Nothing, OffHandItem::Shield, 0), words,
	                   sizeof(words));
	Check(std::strcmp(words, "the weapon hand's potion, the other hand's shield") == 0, "a potion and a shield");
	char tiny[8];
	PlaceNearHeldWords(HeldOf(WeaponSeen::Drawn, EquippedKind::OneHand, OffHandItem::Torch), tiny, sizeof(tiny));
	Check(std::strlen(tiny) < sizeof(tiny), "a short buffer: cut, still ended");
}

void TestSaid() {
	std::printf("What the log says of a place's showing\n");
	Check(PlaceNearToSay(true, false, 3), "shown and not yet said: said");
	Check(PlaceNearToSay(false, true, 3), "hidden again: said");
	Check(!PlaceNearToSay(true, true, 3) && !PlaceNearToSay(false, false, 3), "no change: nothing");
	Check(!PlaceNearToSay(true, false, 0) && !PlaceNearToSay(false, true, 0), "no line left: nothing");
}

}  // namespace

int main() {
	TestOpacity();
	TestTarget();
	TestFade();
	TestHeld();
	TestOtherHandAndPotions();
	TestBothHands();
	TestHeldWords();
	TestSaid();
	std::printf(g_failures == 0 ? "All place near checks passed\n" : "%d place near checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

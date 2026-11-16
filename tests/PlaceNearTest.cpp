#include <cmath>
#include <cstdio>
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

void TestHeld() {
	std::printf("What is held that belongs at a place\n");
	HeldForPlaces h = HeldForPlacesOf(WeaponSeen::Drawn, EquippedKind::OneHand);
	Check(h.item == SlotItem::OneHand && !h.otherHand, "a drawn one-hander: the weapon hand's");
	Check(PlaceTakesHeld(HolsterSlot::HipLeft, h) && PlaceTakesHeld(HolsterSlot::HipRight, h) &&
	          !PlaceTakesHeld(HolsterSlot::BackLeft, h) && !PlaceTakesHeld(HolsterSlot::BackRight, h),
	      "it shows the hips, not the back");
	h = HeldForPlacesOf(WeaponSeen::Drawn, EquippedKind::TwoHand);
	Check(h.item == SlotItem::TwoHand && !h.otherHand, "a drawn two-hander: the weapon hand's");
	Check(!PlaceTakesHeld(HolsterSlot::HipLeft, h) && PlaceTakesHeld(HolsterSlot::BackLeft, h) && PlaceTakesHeld(HolsterSlot::BackRight, h),
	      "it shows the back, not the hips");
	h = HeldForPlacesOf(WeaponSeen::Drawn, EquippedKind::Bow);
	Check(h.item == SlotItem::Bow && h.otherHand, "a drawn bow: the other hand's");
	Check(PlaceTakesHeld(HolsterSlot::BackLeft, h) && !PlaceTakesHeld(HolsterSlot::HipRight, h), "it shows the back");
	h = HeldForPlacesOf(WeaponSeen::Drawn, EquippedKind::Nothing);
	Check(h.item == SlotItem::Nothing && !PlaceTakesHeld(HolsterSlot::HipLeft, h), "the fists up: nothing held for a place");
	h = HeldForPlacesOf(WeaponSeen::Sheathed, EquippedKind::OneHand);
	Check(h.item == SlotItem::Nothing && !PlaceTakesHeld(HolsterSlot::HipLeft, h), "sheathed: nothing held");
	h = HeldForPlacesOf(WeaponSeen::Unknown, EquippedKind::TwoHand);
	Check(h.item == SlotItem::Nothing, "not known (a menu): nothing held");
	Check(!PlaceTakesHeld(HolsterSlot::None, HeldForPlacesOf(WeaponSeen::Drawn, EquippedKind::OneHand)), "no place: never");
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
	TestSaid();
	std::printf(g_failures == 0 ? "All place near checks passed\n" : "%d place near checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

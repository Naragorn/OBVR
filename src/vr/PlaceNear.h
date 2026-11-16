#pragma once

// A place shown faintly while what belongs there is held near it (Nadi,
// 2026-11-16: "wenn man eine waffe in die nähe der gesetzten holster hält die
// holster kreise mit einer bestimmten opacity sich einblenden ... das selbe
// auch für den inv stow spot. beide opacity sehr dezent"): a weapon place's
// circle while the drawn weapon comes near it, the inventory ring while a held
// item comes near it. Nothing shows from afar - the circles shown whenever the
// player looked down (2026-11-14) were taken out again the next day.
//
// A place is at its opacity from its own radius in, fades to nothing over
// kPlaceNearMarginMetres outside it, and follows that over kPlaceNearFadeSeconds.
// [Hands] HolsterNear and [Hands] StowSpotVisible switch the two, [Hands]
// PlaceHintOpacity is how strongly both show. Pure, covered by place_near_test.

#include "core/Types.h"
#include "vr/Holster.h"

namespace obvr::vr {

inline constexpr float kPlaceNearMarginMetres = 0.15f;  // how far outside its radius a place begins to show
inline constexpr float kPlaceNearFadeSeconds = 0.2f;    // from nothing to its opacity and back
inline constexpr float kPlaceNearShownFrom = 0.01f;
inline constexpr float kPlaceHintOpacity = 0.25f;       // [Hands] PlaceHintOpacity's default: very faint
inline constexpr float kPlaceHintOpacityLeast = 0.05f;
inline constexpr float kPlaceHintOpacityMost = 1.0f;

struct PlaceNearSettings {
	bool enabled = true;                  // [Hands] HolsterNear, or [Hands] StowSpotVisible
	float opacity = kPlaceHintOpacity;    // [Hands] PlaceHintOpacity
};

// The opacity as it is used: within its bounds, the default for no number.
inline float PlaceHintOpacityOf(float opacity) {
	if (!(opacity == opacity)) {
		return kPlaceHintOpacity;
	}
	return opacity < kPlaceHintOpacityLeast ? kPlaceHintOpacityLeast
	                                        : (opacity > kPlaceHintOpacityMost ? kPlaceHintOpacityMost : opacity);
}

// How visible a place should be. `held`: something that belongs at the place
// is held, by a hand that is tracked, in the world with no menu up.
// `distanceMetres` from the hand to the place's middle, `radiusMetres` the
// place's own.
inline float PlaceNearTarget(const PlaceNearSettings& s, bool held, float distanceMetres, float radiusMetres) {
	if (!s.enabled || !held || !(distanceMetres == distanceMetres) || !(radiusMetres > 0.0f)) {
		return 0.0f;
	}
	const float opacity = PlaceHintOpacityOf(s.opacity);
	const float outside = distanceMetres - radiusMetres;
	if (outside <= 0.0f) {
		return opacity;
	}
	if (outside >= kPlaceNearMarginMetres) {
		return 0.0f;
	}
	return opacity * (1.0f - outside / kPlaceNearMarginMetres);
}

// One frame of the fade toward `target`; `opacity` the full one, which takes
// kPlaceNearFadeSeconds.
inline float StepPlaceNearAlpha(float now, float target, float dtSeconds, float opacity) {
	const float from = now == now ? now : 0.0f;
	if (!(dtSeconds > 0.0f)) {
		return from;
	}
	const float step = PlaceHintOpacityOf(opacity) * dtSeconds / kPlaceNearFadeSeconds;
	if (from < target) {
		return from + step < target ? from + step : target;
	}
	return from - step > target ? from - step : target;
}

inline bool PlaceNearShown(float alpha) { return alpha >= kPlaceNearShownFrom; }

// What is held that belongs at a weapon place, and by which hand: the drawn
// one- or two-hander by the weapon hand, the drawn bow by the other hand.
// Nothing while sheathed or with the fists up.
struct HeldForPlaces {
	SlotItem item = SlotItem::Nothing;
	bool otherHand = false;
};

inline HeldForPlaces HeldForPlacesOf(WeaponSeen seen, EquippedKind equipped) {
	HeldForPlaces h;
	if (seen != WeaponSeen::Drawn) {
		return h;
	}
	h.item = SlotItemOf(equipped);
	h.otherHand = h.item == SlotItem::Bow;
	if (!WeaponHandItem(h.item) && h.item != SlotItem::Bow) {
		h = HeldForPlaces{};
	}
	return h;
}

// Whether a place's circle may show for what is held: only a place that takes it.
inline bool PlaceTakesHeld(HolsterSlot slot, const HeldForPlaces& held) {
	return held.item != SlotItem::Nothing && SlotAccepts(slot, held.item);
}

}  // namespace obvr::vr

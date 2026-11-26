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

// Whether the log says a place's showing now: when it changed, while lines
// are left (the weapon places' "Holster near" and the ring's "Stow ring near").
inline bool PlaceNearToSay(bool shown, bool said, UInt32 linesLeft) { return shown != said && linesLeft > 0; }

// What each hand holds that a place could take - the same things the places'
// sign names (camera/hands/Holster.cpp StepHolsterSign): the weapon hand's
// drawn one- or two-hander; the other hand's drawn bow, else its shield or
// torch; and a potion held in either hand, which a free place keeps
// (vr/PotionBelt.h). Nadi, 2026-11-26: the torch showed the sign's words
// where a weapon shows the circle - "hätte lieber kreis", and for potions
// "vll ähnlich". Before, only the drawn weapon and the bow had a circle.
struct HandHeldForPlace {
	SlotItem item = SlotItem::Nothing;
	bool potion = false;  // a potion (no poison) held in this hand, [Hands] PotionBelt on
};

inline constexpr UInt32 kPlaceWeaponHand = 0;
inline constexpr UInt32 kPlaceOtherHand = 1;

struct HeldForPlaces {
	HandHeldForPlace hand[2];  // kPlaceWeaponHand, kPlaceOtherHand
};

// A potion held for the places: its base form id (0 none) and the hand
// (kPlaceWeaponHand, kPlaceOtherHand), as the held object's step read it.
struct PotionHeld {
	UInt32 id = 0;
	SInt32 hand = -1;
};

struct HeldForPlacesInput {
	WeaponSeen seen = WeaponSeen::Unknown;
	EquippedKind equipped = EquippedKind::Nothing;
	OffHandItem offHand = OffHandItem::Nothing;  // the other hand's shield or torch, worn
	SInt32 potionHand = -1;  // the hand holding a potion: kPlaceWeaponHand, kPlaceOtherHand, else none
};

inline HeldForPlaces HeldForPlacesOf(const HeldForPlacesInput& in) {
	HeldForPlaces h;
	const bool drawn = in.seen == WeaponSeen::Drawn;
	const SlotItem equipped = SlotItemOf(in.equipped);
	if (drawn && WeaponHandItem(equipped)) {
		h.hand[kPlaceWeaponHand].item = equipped;
	}
	h.hand[kPlaceOtherHand].item = drawn && equipped == SlotItem::Bow ? SlotItem::Bow : SlotItemOf(in.offHand);
	if (in.potionHand == static_cast<SInt32>(kPlaceWeaponHand) || in.potionHand == static_cast<SInt32>(kPlaceOtherHand)) {
		h.hand[in.potionHand].potion = true;
	}
	return h;
}

// Whether a place's circle may show for what a hand holds: only a place that
// takes it - for a potion, a place a let go would keep it at (`potionFits`,
// vr/PotionBelt.h PotionPlaceFor).
inline bool PlaceTakesHeld(HolsterSlot slot, const HandHeldForPlace& held, bool potionFits) {
	if (slot == HolsterSlot::None) {
		return false;
	}
	return (held.item != SlotItem::Nothing && SlotAccepts(slot, held.item)) || (held.potion && potionFits);
}

// One place's hint from both hands: each counts while tracked and holding
// what the place takes; the place shows the larger target and is lit while
// such a hand is inside it (a let go or a grip there puts it away).
struct PlaceNearHand {
	bool tracked = false;
	float distanceMetres = 0.0f;  // from the hand to the place's middle
	bool takes = false;           // the place takes what this hand holds (PlaceTakesHeld)
};

struct PlaceNearOne {
	float target = 0.0f;
	bool lit = false;
};

inline PlaceNearOne PlaceNearFromHands(const PlaceNearSettings& s, const PlaceNearHand (&hands)[2], float radiusMetres) {
	PlaceNearOne one;
	for (const PlaceNearHand& h : hands) {
		const bool mine = h.tracked && h.takes;
		const float target = PlaceNearTarget(s, mine, h.distanceMetres, radiusMetres);
		one.target = target > one.target ? target : one.target;
		one.lit = one.lit || (mine && h.distanceMetres <= radiusMetres);
	}
	return one;
}

// The log's words for what the hands hold for the places: "the weapon hand's
// weapon", "the other hand's torch", both joined; "the weapon hand's nothing"
// for none, as the line has read since 2026-11-16.
inline void PlaceNearHeldWords(const HeldForPlaces& held, char* out, UInt32 size) {
	if (out == nullptr || size == 0) {
		return;
	}
	out[0] = '\0';
	UInt32 at = 0;
	const auto add = [&](const char* text) {
		for (; *text != '\0' && at + 1 < size; ++text) {
			out[at++] = *text;
		}
		out[at] = '\0';
	};
	for (UInt32 i = 0; i < 2; ++i) {
		const HandHeldForPlace& h = held.hand[i];
		const char* const what = WeaponHandItem(h.item)          ? "weapon"
		                         : h.item == SlotItem::Bow    ? "bow"
		                         : h.item == SlotItem::Shield ? "shield"
		                         : h.item == SlotItem::Torch  ? "torch"
		                         : h.potion                   ? "potion"
		                                                      : nullptr;
		if (what == nullptr) {
			continue;
		}
		add(at > 0 ? ", " : "");
		add(i == kPlaceWeaponHand ? "the weapon hand's " : "the other hand's ");
		add(what);
	}
	if (at == 0) {
		add("the weapon hand's nothing");
	}
}

}  // namespace obvr::vr

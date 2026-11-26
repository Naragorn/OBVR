#include "camera/hands/HolsterNear.h"

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/HolsterSlots.h"
#include "render/GameDevice.h"
#include "ui/StowSpotPainter.h"
#include "vr/HolsterPlace.h"
#include "vr/PlaceNear.h"
#include "vr/PotionBelt.h"

namespace obvr::camera::hands {

void StepHolsterNear(HolsterState& state, const Config& config, vr::OpenVRBackend& backend,
                     const vr::HandModeFrame& frame, bool allowed) {
	const vr::HolsterSettings& h = config.hands.holster;
	vr::PlaceNearSettings s;
	s.enabled = h.enabled && h.showNear;
	s.opacity = h.hintOpacity;
	// Each hand's thing for the places (vr/PlaceNear.h): the drawn weapon, the
	// bow, the shield or the torch, a potion (the held step's reading of last
	// frame, camera/hands/Held.cpp) - each hand near a place that takes it.
	vr::HeldForPlacesInput hin;
	hin.seen = frame.weaponSeen;
	hin.equipped = frame.equipped;
	hin.offHand = frame.offHand;
	const UInt32 potion = config.hands.potionBelt ? state.potionHeld.id : 0;
	hin.potionHand = potion != 0 ? state.potionHeld.hand : -1;
	const vr::HeldForPlaces held = vr::HeldForPlacesOf(hin);
	const vr::HandPose* const hands[2] = {&frame.right, &frame.left};
	bool tracked[2] = {};
	bool holds = false;
	for (UInt32 i = 0; i < 2; ++i) {
		tracked[i] = allowed && frame.headValid && hands[i]->valid;
		holds = holds || (tracked[i] && (held.hand[i].item != vr::SlotItem::Nothing || held.hand[i].potion));
	}
	if ((!s.enabled || !holds) && !state.circlesUp) {
		return;  // nothing held near a place, and no circle left to fade out
	}
	const vr::HolsterPlaces places = vr::PlacesOf(h);
	NiPoint3 at[2] = {};
	for (UInt32 i = 0; i < 2; ++i) {
		at[i] = tracked[i] ? vr::PlaceRelative(frame.body, frame.head, frame.headPosition, hands[i]->position)
		                   : NiPoint3{0.0f, 0.0f, 0.0f};
	}
	bool repaint = false;
	bool any = false;
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		const vr::HolsterSlot slot = vr::SlotOfPlace(i);
		const bool potionFits =
			potion != 0 && vr::PotionPlaceFor(slot, game::HolsterSlotIds(), game::PotionPlaceIds(), potion) == slot;
		vr::PlaceNearHand byHand[2];
		for (UInt32 hand = 0; hand < 2; ++hand) {
			byHand[hand].tracked = tracked[hand];
			byHand[hand].distanceMetres = math::Sqrt((at[hand] - places.at[i]).LengthSquared());
			byHand[hand].takes = vr::PlaceTakesHeld(slot, held.hand[hand], potionFits);
		}
		const vr::PlaceNearOne one = vr::PlaceNearFromHands(s, byHand, places.radius[i]);
		state.nearAlpha[i] = vr::StepPlaceNearAlpha(state.nearAlpha[i], one.target, frame.dtSeconds, s.opacity);
		any = any || vr::PlaceNearShown(state.nearAlpha[i]);
		// lit while a hand that holds what it takes is inside: a let go or a grip there puts it away
		ui::StowSpotView& view = state.placeViews[i];
		repaint = repaint || view.lit != one.lit || view.name != vr::kHolsterPlaceNames[i];
		view.lit = one.lit;
		view.name = vr::kHolsterPlaceNames[i];
	}
	if (vr::PlaceNearToSay(any, state.nearSaid, state.nearLines)) {
		--state.nearLines;
		char words[96];
		vr::PlaceNearHeldWords(held, words, sizeof(words));
		OBVR_LOG("Holster near: the places %s (%s, opacity %.2f)", any ? "shown" : "hidden", words,
		         static_cast<double>(vr::PlaceHintOpacityOf(s.opacity)));
	}
	state.nearSaid = any;
	state.circlesUp = any;
	state.placeRevision += repaint ? 1u : 0u;
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		if (!frame.headValid || !vr::PlaceNearShown(state.nearAlpha[i])) {
			state.placeLayers[i].Hide(backend);
			continue;
		}
		const NiPoint3 room = vr::PlaceInTracking(frame.body, frame.head, frame.headPosition, places.at[i]);
		state.placeLayers[i].Show(backend, render::GetGameDevice(), StowSpotPose(room, frame.headPosition),
		                          2.0f * places.radius[i], state.placeRevision, ui::PaintStowSpotFor,
		                          &state.placeViews[i]);
		state.placeLayers[i].SetAlpha(backend, state.nearAlpha[i]);
	}
}

void EndHolsterNear(HolsterState& state, vr::OpenVRBackend& backend) {
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		state.nearAlpha[i] = 0.0f;
		state.placeLayers[i].SetAlpha(backend, 1.0f);
	}
	state.circlesUp = true;  // the window's circles: hidden by the hint's next step once the window is gone
	state.nearSaid = false;
}

}  // namespace obvr::camera::hands

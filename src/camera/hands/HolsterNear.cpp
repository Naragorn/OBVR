#include "camera/hands/HolsterNear.h"

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "render/GameDevice.h"
#include "ui/StowSpotPainter.h"
#include "vr/HolsterPlace.h"
#include "vr/PlaceNear.h"

namespace obvr::camera::hands {

void StepHolsterNear(HolsterState& state, const Config& config, vr::OpenVRBackend& backend,
                     const vr::HandModeFrame& frame, bool allowed) {
	const vr::HolsterSettings& h = config.hands.holster;
	vr::PlaceNearSettings s;
	s.enabled = h.enabled && h.showNear;
	s.opacity = h.hintOpacity;
	const vr::HeldForPlaces held = vr::HeldForPlacesOf(frame.weaponSeen, frame.equipped);
	const vr::HandPose& hand = held.otherHand ? frame.left : frame.right;
	const bool tracked = allowed && frame.headValid && hand.valid;
	if ((!s.enabled || !tracked || held.item == vr::SlotItem::Nothing) && !state.circlesUp) {
		return;  // nothing held near a place, and no circle left to fade out
	}
	const vr::HolsterPlaces places = vr::PlacesOf(h);
	const NiPoint3 at = tracked ? vr::PlaceRelative(frame.body, frame.head, frame.headPosition, hand.position)
	                            : NiPoint3{0.0f, 0.0f, 0.0f};
	bool repaint = false;
	bool any = false;
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		const float distance = math::Sqrt((at - places.at[i]).LengthSquared());
		const bool mine = tracked && vr::PlaceTakesHeld(vr::SlotOfPlace(i), held);
		const float target = vr::PlaceNearTarget(s, mine, distance, places.radius[i]);
		state.nearAlpha[i] = vr::StepPlaceNearAlpha(state.nearAlpha[i], target, frame.dtSeconds, s.opacity);
		any = any || vr::PlaceNearShown(state.nearAlpha[i]);
		// lit while the hand is inside the place: a grip here puts the weapon away
		const bool lit = mine && distance <= places.radius[i];
		ui::StowSpotView& view = state.placeViews[i];
		repaint = repaint || view.lit != lit || view.name != vr::kHolsterPlaceNames[i];
		view.lit = lit;
		view.name = vr::kHolsterPlaceNames[i];
	}
	if (any != state.nearSaid && state.nearLines > 0) {
		--state.nearLines;
		OBVR_LOG("Holster near: the places %s (the %s hand's %s, opacity %.2f)", any ? "shown" : "hidden",
		         held.otherHand ? "other" : "weapon",
		         held.item == vr::SlotItem::Nothing ? "nothing" : (held.item == vr::SlotItem::Bow ? "bow" : "weapon"),
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

#include "camera/hands/HolsterGlance.h"

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"
#include "ui/StowSpotPainter.h"
#include "vr/HolsterGlance.h"
#include "vr/HolsterPlace.h"

namespace obvr::camera::hands {

namespace {

// The head's pitch in degrees, up positive: its forward against the level.
float PitchDegrees(const vr::Quaternion& head) {
	const NiPoint3 forward = ToMatrix(head) * NiPoint3{0.0f, 0.0f, -1.0f};
	float up = forward.y;
	up = up > 1.0f ? 1.0f : (up < -1.0f ? -1.0f : up);
	return math::Asin(up) * (180.0f / math::kPi);
}

void HideAll(HolsterState& state, vr::OpenVRBackend& backend) {
	for (ui::CanvasOverlay& layer : state.placeLayers) {
		layer.Hide(backend);
	}
}

}  // namespace

void StepHolsterGlance(HolsterState& state, const Config& config, vr::OpenVRBackend& backend,
                       const vr::HandModeFrame& frame, bool allowed) {
	vr::HolsterGlanceSettings s;
	s.enabled = config.hands.holster.glance;
	const float pitch = frame.headValid ? PitchDegrees(frame.head) : 0.0f;
	const float target = vr::GlanceTarget(s, pitch, allowed && frame.headValid);
	const bool wasShown = vr::GlanceShown(state.glanceAlpha);
	state.glanceAlpha = vr::StepGlanceAlpha(state.glanceAlpha, target, frame.dtSeconds);
	const bool shown = vr::GlanceShown(state.glanceAlpha);
	if (shown != wasShown && state.glanceLines > 0) {
		--state.glanceLines;
		OBVR_LOG("Holster glance: the places %s (the head %.0f degrees %s the level)", shown ? "shown" : "hidden",
		         static_cast<double>(pitch < 0.0f ? -pitch : pitch), pitch < 0.0f ? "below" : "above");
	}
	if (test::HandScriptMarkedThisFrame()) {
		OBVR_LOG("Holster glance: at the mark %s - opacity %.2f, the head %.0f degrees", test::HandScriptMarkName(),
		         static_cast<double>(state.glanceAlpha), static_cast<double>(pitch));
	}
	if (!shown) {
		HideAll(state, backend);
		return;
	}
	const vr::HolsterPlaces places = vr::PlacesOf(config.hands.holster);
	bool repaint = false;
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		ui::StowSpotView& view = state.placeViews[i];
		repaint = repaint || view.lit || view.name != vr::kHolsterPlaceNames[i];
		view.lit = false;
		view.name = vr::kHolsterPlaceNames[i];
	}
	state.placeRevision += repaint ? 1u : 0u;
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		const NiPoint3 at = vr::PlaceInTracking(frame.body, frame.head, frame.headPosition, places.at[i]);
		state.placeLayers[i].Show(backend, render::GetGameDevice(), StowSpotPose(at, frame.headPosition),
		                          2.0f * places.radius[i], state.placeRevision, ui::PaintStowSpotFor,
		                          &state.placeViews[i]);
		state.placeLayers[i].SetAlpha(backend, state.glanceAlpha);
	}
}

void EndHolsterGlance(HolsterState& state, vr::OpenVRBackend& backend) {
	if (state.glanceAlpha <= 0.0f) {
		return;
	}
	state.glanceAlpha = 0.0f;
	for (ui::CanvasOverlay& layer : state.placeLayers) {
		layer.SetAlpha(backend, 1.0f);
	}
}

}  // namespace obvr::camera::hands

#include "camera/hands/ReachRing.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/CrosshairTarget.h"
#include "game/TargetMarker.h"
#include "test/HandScriptRuntime.h"
#include "vr/LaserGeometry.h"

namespace obvr::camera::hands {

void StepReachRing(ReachRingState& state, const HandStep& step, const HandShared& shared, const ReachRingLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	bool markerShown = false;
	UInt32 markerRef = 0;
	vr::openvr::HmdMatrix34 markerPose{};
	vr::openvr::HmdMatrix34 head{};
	if (active && !menuIsUp && (config.hands.reachTooltip || config.hands.reachRing) &&
	    shared.cyclopeanCameraValid &&
	    backend.GetRenderPoseMatrix(head)) {
		const game::CrosshairTarget target = game::ReadCrosshairTarget();
		NiPoint3 hit = target.position;
		game::ReadPickHit(hit);
		// Shown on the thing the pick has settled on (game/PickHold.h): a
		// thing the ray crossed for a few frames does not take the ring;
		// and the ring eases over the thing rather than following every
		// tremor of the ray's hit, and glides over to another thing
		// (the tester, 2026-10-08: "der übergang könnte generell weicher
		// sein") - what the grip takes is the pick's own, not the ring's
		// drawn place.
		const UInt32 wantedRef = target.haveRef ? target.refAddress : 0;
		const UInt32 shownRef = state.ringSettle.Step(wantedRef, dt, links.nearItem.valid ? links.nearItem.ref : 0);
		if (shownRef != wantedRef && state.ringAnchor.valid && state.ringAnchor.ref == shownRef) {
			hit = state.ringAnchor.point;
		}
		hit = game::StepAnchor(state.ringAnchor, shownRef, hit, dt, game::kAnchorSeconds, game::kAnchorGlideSeconds);
		const float reachUnits = config.hands.reachMarkerMetres * config.tracker.unitsPerMetre;
		const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
		const NiPoint3& camPos = shared.cyclopeanCamera.pos;
		const bool nearRight =
			hand.rightHandValid &&
			vr::WithinReach(camPos + camRot * hand.rightHandOffsetUnits, hit,
			                reachUnits);
		const bool nearLeft =
			hand.leftHandValid &&
			vr::WithinReach(camPos + camRot * hand.leftHandOffsetUnits, hit,
			                reachUnits);
		if (game::ReachMarkerWanted(shownRef != 0, game::RefBaseFormType(shownRef), nearRight, nearLeft)) {
			markerShown = true;
			markerRef = shownRef;
			markerPose = vr::FacingHeadAt(
				head, vr::WorldPointInTracking(head, camRot, camPos, hit,
				                               config.tracker.unitsPerMetre));
		}
		// When the ring comes and goes, and at a hand script's mark: how far
		// the thing is from the eyes and from the nearer hand - next to the
		// Info element's lines, whether the name comes with the ring (Nadi,
		// 2026-10-25: "momentan kommt der ring zuerst dann bücke ich mich
		// dann kommt erst der text").
		const bool marked = test::HandScriptMarkedThisFrame();
		if ((markerRef != state.ringRef && state.ringLines > 0) || marked) {
			if (!marked) {
				--state.ringLines;
			}
			const auto units = [](const NiPoint3& a, const NiPoint3& b) {
				return static_cast<double>(math::Sqrt((a - b).LengthSquared()));
			};
			const double rightUnits =
				hand.rightHandValid ? units(camPos + camRot * hand.rightHandOffsetUnits, hit) : 1e9;
			const double leftUnits =
				hand.leftHandValid ? units(camPos + camRot * hand.leftHandOffsetUnits, hit) : 1e9;
			OBVR_LOG("Reach: %s%s%s - the ring %s on %08X, the pick on %08X (HUDInfo state %u); it is %.0f units "
			         "from the eyes, %.0f from the nearer hand",
			         marked ? "at \"" : "", marked ? test::HandScriptMarkName() : "the ring changed",
			         marked ? "\"" : "", markerShown ? "shown" : "away", markerRef, wantedRef, target.menuState,
			         units(camPos, hit), rightUnits < leftUnits ? rightUnits : leftUnits);
		}
		state.ringRef = markerRef;
	}
	state.marker.Submit(backend, markerShown && config.hands.reachRing, markerPose,
	                     config.hands.reachMarkerOpacity);
	// The crosshair's icon moves onto the object, into the ring's middle.
	state.iconShown = markerShown && config.hands.reachTooltip;
	state.shown = markerShown;
	state.shownRef = markerRef;
	// And the thing itself marked: an outline, or a glow (game/TargetMarker.h).
	game::StepTargetMarker(markerShown ? markerRef : 0, active && config.hands.targetOutline,
	                       active && config.hands.targetGlow);
	state.iconPose = render::ReachIconPose(markerPose);
}

}  // namespace obvr::camera::hands

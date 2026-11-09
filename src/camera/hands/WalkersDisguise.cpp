#include "camera/hands/WalkersDisguise.h"

#include "camera/hands/HandGeometry.h"
#include "core/Log.h"
#include "game/CorpseTouch.h"
#include "game/HandBlood.h"
#include "game/WalkersLink.h"
#include "render/GameDevice.h"
#include "vr/Holster.h"
#include "vr/Stow.h"

namespace obvr::camera::hands {
namespace {

constexpr float kScanSeconds = 0.1f;
// The engine fades its decals after fDecalLifetime (10 s here, docs/blade-penetration-spec.md);
// laid again before that.
constexpr float kBloodAgainSeconds = 8.0f;
// The ring's clear middle and the shade's (degrees from the centre of view).
constexpr float kRingClearDegrees = 22.0f;
constexpr float kShadeClearDegrees = 1.0f;
// The ring's colour: a dark blood red on the white ring (render/VignetteLayer.h).
constexpr float kRingRed = 0.35f;
constexpr float kRingGreen = 0.0f;
constexpr float kRingBlue = 0.02f;

void SetIdentity(WalkersDisguiseState& state) {
	if (state.identitySet) {
		return;
	}
	state.identitySet = true;
	state.ring.SetIdentity("obvr.disguisering", "OBVR Disguise Ring", true);
	state.shade.SetIdentity("obvr.disguiseshade", "OBVR Disguise Shade", false);
}

}  // namespace

void HideWalkersDisguise(WalkersDisguiseState& state, vr::OpenVRBackend& backend) {
	SetIdentity(state);
	state.ring.UpdateHeld(backend, render::GetGameDevice(), false, 0.0f, kRingClearDegrees, 0.0f, 0.0f, 0.0f);
	state.shade.UpdateHeld(backend, render::GetGameDevice(), false, 0.0f, kShadeClearDegrees, 0.0f, 0.0f, 0.0f);
	state.gesture = vr::DisguiseGestureState{};
	state.look = vr::DisguiseLook{};
	state.sinceBlood = -1.0f;
}

void StepWalkersDisguise(WalkersDisguiseState& state, const HandStep& step, const HandShared& shared,
                         const vr::HandModeFrame& frame, bool disguised) {
	SetIdentity(state);
	const Config& config = step.config;
	const vr::DisguiseSettings& set = config.hands.disguise;
	const float dt = step.dt == step.dt && step.dt > 0.0f ? step.dt : 0.0f;
	const bool inWorld = step.active && frame.inWorld;
	const bool allowed = inWorld && !step.menuIsUp && shared.cyclopeanCameraValid && game::ReadWalkersNow().linked;

	// The hands: in the world (game units) and against the chest (the stow spot).
	const vr::HandModeResult& hand = shared.hand;
	NiPoint3 world[2];
	bool valid[2];
	vr::DisguiseHandInput in[2];
	for (UInt32 h = 0; h < 2; ++h) {
		const vr::HandPose& pose = h == 1 ? frame.right : frame.left;
		valid[h] = pose.valid && frame.headValid && (h == 1 ? hand.rightHandValid : hand.leftHandValid);
		world[h] = shared.cyclopeanCamera.pos +
		           shared.cyclopeanCamera.rot * (h == 1 ? hand.rightHandOffsetUnits : hand.leftHandOffsetUnits);
		in[h].valid = valid[h];
		in[h].atChest = valid[h] && vr::InStowZone(vr::BodyRelative(frame.head, frame.headPosition,
		                                                            DrawnHandTracking(pose, h == 1, config.hands)),
		                                           config.hands.stow);
	}
	if (allowed && set.enabled) {
		state.sinceScan += dt;
		if (state.sinceScan >= kScanSeconds) {
			state.sinceScan = 0.0f;
			const UInt32 was[2] = {state.underHand[0], state.underHand[1]};
			game::DeadBodiesUnderHands(world, valid, set.corpseReachUnits, state.underHand);
			for (UInt32 h = 0; h < 2; ++h) {
				if (state.underHand[h] != was[h] && state.underHand[h] != 0 && state.lines > 0) {
					--state.lines;
					OBVR_LOG("Walkers disguise: the %s hand in the dead body %08X", h == 0 ? "left" : "right",
					         state.underHand[h]);
				}
			}
		}
	} else {
		state.underHand[0] = 0;
		state.underHand[1] = 0;
	}
	in[0].deadBody = state.underHand[0];
	in[1].deadBody = state.underHand[1];
	const UInt32 body = vr::StepDisguiseGesture(state.gesture, in, allowed, dt, set);
	if (body != 0) {
		// a short pulse at both hands: the smear felt
		game::SendWalkersBlood(body);
		step.backend.Pulse(true, 0.08f, 90.0f, 0.6f);
		step.backend.Pulse(false, 0.08f, 90.0f, 0.6f);
	}

	// The look while Walkers says disguised.
	const vr::DisguiseLookOut look = vr::StepDisguiseLook(state.look, disguised && inWorld, dt, set);
	const bool show = inWorld && !step.menuIsUp;
	state.ring.UpdateHeld(step.backend, render::GetGameDevice(), show, look.vignetteAlpha, kRingClearDegrees, kRingRed,
	                      kRingGreen, kRingBlue);
	state.shade.UpdateHeld(step.backend, render::GetGameDevice(), show, look.darkenAlpha, kShadeClearDegrees, 0.0f,
	                       0.0f, 0.0f);
	if (disguised != state.disguisedSaid && state.lines > 0) {
		--state.lines;
		state.disguisedSaid = disguised;
		OBVR_LOG("Walkers disguise: %s", disguised ? "on - the vignette and the darker view fade in" : "over - they fade out");
	}

	// Blood on the hands from every side (vr::HandStainDirection), one side a
	// frame for both hands - the engine lays at most iMaxDecalsPerFrame -
	// and the round again before the engine fades it.
	if (look.bloodyHands && inWorld) {
		if (state.sinceBlood < 0.0f || state.sinceBlood >= kBloodAgainSeconds) {
			state.sinceBlood = 0.0f;
			state.stainSide = 0;
			state.stainGained = 0;
		}
		if (state.stainSide < vr::kHandStainSides) {
			state.stainGained += game::StainPlayerHands(shared.cyclopeanCamera.pos, valid, state.stainSide++);
			if (state.stainSide == vr::kHandStainSides && state.lines > 0) {
				--state.lines;
				OBVR_LOG("Walkers disguise: blood laid on the hands from %u sides - %u decal(s) gained", vr::kHandStainSides,
				         state.stainGained);
			}
		}
		state.sinceBlood += dt;
	} else {
		state.sinceBlood = -1.0f;
	}
}

}  // namespace obvr::camera::hands

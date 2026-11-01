#include "camera/hands/WalkersShake.h"

#include <cstdio>
#include <cstring>

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "game/WalkersLink.h"
#include "platform/PluginPath.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"
#include "vr/GripHold.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

void StepWalkersShake(WalkersShakeState& state, const HandStep& step, const HandShared& shared,
                      const vr::HandModeFrame& frame, const WalkersShakeLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	vr::HandModeResult& hand = shared.hand;
	game::SetWalkersTestHold(config.hands.testWalkersHold);
	// 'WGVR' once more at the first world frames while no Walkers took it.
	if (state.vrAsks > 0 && frame.inWorld && config.hands.walkersShake) {
		state.vrAsks = game::SayVrToWalkers() ? 0 : state.vrAsks - 1;
	}
	if (frame.inWorld) {
		game::StepDismemberToWalkers(config.hands.dismember.enabled);
	}
	const game::WalkersNow walkers = game::ReadWalkersNow();
	const bool shakeOn = config.hands.walkersShake && active && !menuIsUp && frame.inWorld;
	const float shakeSeconds = dt == dt && dt > 0.0f ? dt : 0.0f;
	if (walkers.biteSerial != state.biteSerial) {
		state.biteSerial = walkers.biteSerial;
		const UInt32 at = walkers.bite.arm == vr::kWalkersArmLeft ? 0 : 1;
		state.bites[at] = walkers.bite.number;
		state.toDie = walkers.bite.toDie;
		state.biteSince = 0.0f;
		state.biteFlashLethal = walkers.bite.lethal;
		if (config.hands.walkersShake) {
			backend.Pulse(walkers.bite.arm == vr::kWalkersArmRight, 0.3f, 50.0f, 1.0f);
		}
	} else if (state.biteSince >= 0.0f) {
		state.biteSince += shakeSeconds;
	}
	state.biteFlashAlpha = config.hands.walkersShake ? vr::BiteFlashAlpha(state.biteSince, state.biteFlashLethal) : 0.0f;
	for (UInt32 at = 0; at < 2; ++at) {
		const UInt32 arm = at == 0 ? vr::kWalkersArmLeft : vr::kWalkersArmRight;
		const UInt32 holder = vr::HolderOf(walkers.holds, arm);
		const bool physicalRight = arm == vr::kWalkersArmRight;
		const vr::HandPose& pose = vr::ShakePromptHand(arm, links.rolesSwapped) ? frame.right : frame.left;
		if (holder != state.holders[at]) {
			if (holder != 0 && config.hands.walkersShake) {
				backend.Pulse(physicalRight, 0.15f, 70.0f, 0.9f);
			}
			if (state.lines > 0) {
				--state.lines;
				OBVR_LOG("Walkers shake: the %s arm %s %08X", physicalRight ? "right" : "left",
				         holder != 0 ? "held by" : "let go by", holder != 0 ? holder : state.holders[at]);
			}
			state.holders[at] = holder;
			state.strokeCount[at] = 0;
			vr::ResetShakeStroke(state.strokes[at]);
			if (holder == 0) {
				state.bites[at] = 0;
			}
		}
		const bool held = holder != 0 && shakeOn;
		if (held && vr::StepShakeStroke(state.strokes[at], config.hands.shake, pose.position, shakeSeconds, pose.valid)) {
			++state.strokeCount[at];
			const bool sent = game::SendWalkersStroke(arm);
			backend.Pulse(physicalRight, 0.02f, 200.0f, 0.4f);
			if (state.lines > 0) {
				--state.lines;
				OBVR_LOG("Walkers shake: stroke %u of the %s arm - %s", state.strokeCount[at],
				         physicalRight ? "right" : "left", sent ? "sent to Walkers" : "no Walkers to take it");
			}
		} else if (!held) {
			vr::ResetShakeStroke(state.strokes[at]);
		}
		ui::CanvasOverlay& layer = state.signLayers[at];
		if (held && pose.valid && frame.headValid) {
			ui::HolsterSignView view;
			std::snprintf(view.item, sizeof(view.item), "%s", vr::kShakePromptText);
			vr::ShakePromptText(state.bites[at], state.toDie, view.hint, sizeof(view.hint));
			if (std::strcmp(view.item, state.signViews[at].item) != 0 ||
			    std::strcmp(view.hint, state.signViews[at].hint) != 0) {
				state.signViews[at] = view;
				++state.signRevisions[at];
			}
			const NiPoint3 above{pose.position.x, pose.position.y + kShakeSignAboveMetres, pose.position.z};
			const bool wasVisible = layer.IsVisible();
			layer.Show(backend, render::GetGameDevice(), StowSpotPose(above, frame.headPosition), kShakeSignMetres,
			           state.signRevisions[at], ui::PaintHolsterSignFor, &state.signViews[at]);
			if (!wasVisible && layer.IsVisible() && state.lines > 0) {
				--state.lines;
				OBVR_LOG("Walkers shake: \"%s\" shown at the %s hand%s%s", state.signViews[at].item,
				         physicalRight ? "right" : "left", state.signViews[at].hint[0] != '\0' ? " - " : "",
				         state.signViews[at].hint);
			}
		} else {
			layer.Hide(backend);
		}
		// For the test runner: the call as painted, at each mark it is up for.
		if (test::HandScriptMarkedThisFrame() && layer.IsVisible()) {
			char name[96];
			char path[512];
			std::snprintf(name, sizeof(name), "OBVR-ShakeSign-%s-%s.bmp", physicalRight ? "right" : "left",
			              test::HandScriptMarkName());
			if (platform::BuildGamePath(name, path, sizeof(path)) && layer.SaveBmp(path)) {
				OBVR_LOG("HandScript: the shake sign's picture saved as %s", name);
			}
		}
	}
	if (vr::WalkLocked(walkers.holds, config.hands.walkersShake && active)) {
		const vr::StickDirections& move = hand.controls.move;
		if ((move.forward || move.back || move.left || move.right) && !state.lockSaid) {
			state.lockSaid = true;
			OBVR_LOG("Walkers shake: held - the stick does not walk");
		}
		hand.controls.move = vr::StickDirections{};
	}
}

}  // namespace obvr::camera::hands

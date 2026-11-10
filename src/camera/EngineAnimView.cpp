#include "camera/EngineAnimView.h"

#include <limits>

#include "core/Config.h"
#include "core/Log.h"
#include "game/GameTypes.h"
#include "game/MeleeHits.h"
#include "game/PlayerBody.h"
#include "game/PlayerTeleport.h"
#include "test/HandScriptRuntime.h"
#include "render/VignetteLayer.h"
#include "vr/EngineAnimViewLogic.h"

namespace obvr::camera {

namespace {
// The vignette's alpha, owned here (StepEngineAnimViewFrame writes it).
float& AnimVignetteAlpha() {
	static float alpha = 0.0f;
	return alpha;
}
}  // namespace

float EngineAnimVignetteAlpha() { return AnimVignetteAlpha(); }

render::VignetteLayer& EngineAnimVignetteLayer() {
	static render::VignetteLayer layer;
	static const bool named = (layer.SetIdentity("obvr.animvignette", "OBVR Animation Vignette", true), true);
	(void)named;
	return layer;
}

void SubmitEngineAnimVignette(vr::OpenVRBackend& backend, void* gameDevice, bool worldFrame) {
	const float alpha = AnimVignetteAlpha();
	// Warmed once, hidden, at the first world frame with it on (render/
	// OverlayWarmup.h): SteamVR's import of its texture not paid at the
	// first chair.
	static bool s_warmed = false;
	if (!s_warmed && worldFrame && alpha == 0.0f && GetConfig().look.animationVignette) {
		s_warmed = true;
		EngineAnimVignetteLayer().Warm(backend, gameDevice);
	}
	EngineAnimVignetteLayer().UpdateHeld(backend, gameDevice, alpha > 0.0f && worldFrame, alpha,
	                                     GetConfig().look.snapTurnVignetteRadius, 0.0f, 0.0f, 0.0f);
}

bool StepEngineAnimViewFrame(NiAVObject* cameraNode, bool thirdPerson, bool headset, float dtSeconds) {
	static vr::EngineAnimViewState s_state;
	vr::EngineAnimViewInput in;
	in.enabled = GetConfig().look.firstPersonAnimations;
	in.headset = headset;
	in.thirdPerson = thirdPerson;
	in.dead = game::PlayerIsDead();
	in.sitMountBytes = game::PlayerSitMountBytes();
	in.sitSleepState = game::PlayerSitSleepState();
	in.knockState = game::PlayerKnockState();
	// The vignette over the moment, whichever view shows it.
	static vr::EngineAnimMomentState s_moment;
	static bool s_vignetteLogged = false;
	const bool moment = vr::EngineAnimMomentHeld(s_moment, in, dtSeconds);
	const Config& config = GetConfig();
	vr::StepAnimVignette(AnimVignetteAlpha(), moment && config.look.animationVignette,
	                     config.look.animationVignetteStrength, dtSeconds);
	if (moment != s_vignetteLogged) {
		s_vignetteLogged = moment;
		OBVR_LOG("Camera: the engine's own animation %s - %s, vignette %s",
		         moment ? "begins" : "is over", config.look.firstPersonAnimations ? "in the head" : "in third person",
		         config.look.animationVignette ? "on" : "off");
	}
	if (moment && test::HandScriptMarkedThisFrame()) {
		OBVR_LOG("Camera: at the mark the animation vignette at alpha %.2f", static_cast<double>(AnimVignetteAlpha()));
	}
	const bool wanted = cameraNode != nullptr && vr::EngineAnimViewHeld(s_state, in, dtSeconds);
	NiPoint3 face{0.0f, 0.0f, 0.0f};
	if (wanted && !game::PlayerFaceCentre(face)) {
		face.x = std::numeric_limits<float>::quiet_NaN();
	}
	const bool wasActive = s_state.active;
	NiPoint3 eyes{0.0f, 0.0f, 0.0f};
	const bool on = vr::StepEngineAnimView(s_state, wanted, face, dtSeconds, eyes);
	if (on != wasActive) {
		OBVR_LOG("Camera: the eyes %s the body's head (the engine's third person - sit/mount bytes %04X, sit/sleep "
		         "state %u, knock state %d)%s",
		         on ? "put into" : "let go from", static_cast<unsigned>(in.sitMountBytes),
		         static_cast<unsigned>(in.sitSleepState), static_cast<int>(in.knockState),
		         wanted && !on ? " - the face's centre not read" : "");
	}
	if (!on) {
		return false;
	}
	cameraNode->localTransform.pos = eyes;
	if (test::HandScriptMarkedThisFrame()) {
		OBVR_LOG("Camera: at the mark the eyes held in the body's head at %.1f %.1f %.1f (the face's centre %.1f %.1f "
		         "%.1f)",
		         static_cast<double>(eyes.x), static_cast<double>(eyes.y), static_cast<double>(eyes.z),
		         static_cast<double>(face.x), static_cast<double>(face.y), static_cast<double>(face.z));
	}
	game::HidePlayerHeadForThisDraw();
	return true;
}

}  // namespace obvr::camera

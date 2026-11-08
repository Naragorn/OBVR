#include "camera/EngineAnimView.h"

#include <limits>

#include "core/Config.h"
#include "core/Log.h"
#include "game/GameTypes.h"
#include "game/MeleeHits.h"
#include "game/PlayerBody.h"
#include "game/PlayerTeleport.h"
#include "test/HandScriptRuntime.h"
#include "vr/EngineAnimViewLogic.h"

namespace obvr::camera {

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

#include "camera/StandingEyeFrame.h"

#include "camera/StandingEyeHold.h"
#include "core/Log.h"
#include "game/BodyIkLogic.h"
#include "game/KickLogic.h"
#include "game/MenuMode.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/Shove.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera {

void ApplyStandingEyeHold(NiAVObject* cameraNode, bool base, float dtSeconds) {
	static StandingEyeHoldState s_hold;
	static bool s_said = false;
	if (cameraNode == nullptr) {
		return;
	}
	StandingEyeHoldInput in;
	in.eye = cameraNode->localTransform.pos;
	game::PlayerRotation rot{};
	const bool read = game::PlayerWorldPosition(in.feet) && game::ReadPlayerRotation(rot);
	const UInt16 flags = read ? game::ReadPlayerMovementFlags() : 0;
	in.live = base && read && !game::PlayerRiding() && !game::IsMenuMode() &&
	          !game::KickSwimming(game::PlayerCharacterState());
	in.heightFree = game::IsPlayerSneaking();
	in.standing = game::FeetStand(flags);
	in.yaw = rot.yaw;
	in.dt = dtSeconds;
	const NiPoint3 held = StepStandingEyeHold(s_hold, in);
	if (in.live && !s_said) {
		s_said = true;
		OBVR_LOG("Camera: the view held against the first-person animation while standing - eased over %.1f s "
		         "(walking %.1f s), a jump over %.0f units taken at once (camera/StandingEyeHold.h)",
		         static_cast<double>(kStandingEyeSeconds), static_cast<double>(kWalkingEyeSeconds),
		         static_cast<double>(kStandingEyeSnapUnits));
	}
	if (test::HandScriptMarkedThisFrame()) {
		OBVR_LOG("Camera: standing eye at the mark - %s, the engine's eye %.2f %.2f %.2f over the feet, held %.2f "
		         "%.2f %.2f",
		         in.live ? (in.heightFree ? "held across the ground, sneaking" : (in.standing ? "held, standing" : "held, walking")) : "not held",
		         static_cast<double>(in.eye.x - in.feet.x), static_cast<double>(in.eye.y - in.feet.y),
		         static_cast<double>(in.eye.z - in.feet.z), static_cast<double>(held.x - in.feet.x),
		         static_cast<double>(held.y - in.feet.y), static_cast<double>(held.z - in.feet.z));
	}
	cameraNode->localTransform.pos = held;
}

}  // namespace obvr::camera

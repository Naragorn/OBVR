#include "camera/RoomFollowFrame.h"

#include "core/Log.h"
#include "game/FlyCam.h"
#include "game/MenuMode.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/RoomFollow.h"
#include "test/HandScriptRuntime.h"
#include "vr/RoomFollowLogic.h"

namespace obvr::camera {

namespace {
struct RoomFollowMemory {
	vr::RoomFollowState walk;  // whether a walk after the head is under way
	NiMatrix33 asked = NiMatrix33::Identity();  // the base rotation the last request was asked in
};
RoomFollowMemory& Memory() {
	static RoomFollowMemory m;
	return m;
}
}  // namespace

bool RoomFollowWalking() { return Memory().walk.following; }

void StepRoomFollowFrame(vr::HeadTracker& tracker, const Config& config, const NiMatrix33& baseRotation,
                         bool isThirdPerson, bool controlsActive) {
	float achievedX = 0.0f;
	float achievedY = 0.0f;
	if (game::TakeRoomStepAchieved(achievedX, achievedY)) {
		// Back into the head offset's space: the transpose of the base
		// rotation the step was asked in (a snap turn between the two
		// frames would turn it otherwise).
		const NiMatrix33& r = Memory().asked;
		tracker.SetFollowed(tracker.GetFollowed() + NiPoint3{r.data[0][0] * achievedX + r.data[1][0] * achievedY,
		                                                     r.data[0][1] * achievedX + r.data[1][1] * achievedY,
		                                                     r.data[0][2] * achievedX + r.data[1][2] * achievedY});
	}
	vr::RoomFollowGate gate;
	gate.fullVr = config.fullVrMode && controlsActive;
	gate.headset = tracker.IsHeadsetConnected() && config.tracker.positionalTracking;
	gate.firstPerson = !isThirdPerson;
	gate.inWorld = game::PlayerInWorld();
	gate.menuUp = game::IsMenuMode();
	gate.riding = game::PlayerRiding();
	game::FlyCamPose fly;
	gate.flyCam = game::ReadFlyCam(fly) && fly.on;
	gate.knockedDown = game::PlayerKnockState() > 0;
	// The neck, not the eyes: a head that only turns walks nothing.
	const float units = config.tracker.EffectiveUnitsPerMetre();
	const NiPoint3 eye{0.0f, vr::kNeckToEyeForwardMetres * units, vr::kNeckToEyeUpMetres * units};
	const NiPoint3 neck = vr::NeckPivotOffset(tracker.GetRawOffset(), tracker.GetCameraRotation(), eye);
	const bool allowed = vr::RoomFollowAllowed(config.hands.roomFollow, gate);
	if (allowed) {
		const NiPoint3 head = baseRotation * neck;
		const NiPoint3 followed = baseRotation * tracker.GetFollowed();
		const NiPoint3 step = vr::RoomStepWanted(head, followed, config.hands.roomFollow, Memory().walk);
		game::RequestRoomStep(step.x, step.y);
		Memory().asked = baseRotation;
	} else {
		game::RequestRoomStep(0.0f, 0.0f);
		Memory().walk.following = false;
	}
	if (test::HandScriptMarkedThisFrame()) {
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		game::ReadPlayerFeet(feet);
		const NiPoint3 headWorld = baseRotation * tracker.GetRawOffset();
		const NiPoint3 neckWorld = baseRotation * neck;
		const NiPoint3 lean = baseRotation * tracker.GetCameraOffset();
		OBVR_LOG("RoomFollow: at the mark - %s, the feet at %.1f %.1f %.1f, the head's lead %.1f %.1f, the neck's "
		         "%.1f %.1f, the lean left %.1f %.1f (world units)",
		         allowed ? "following" : "not following", static_cast<double>(feet.x), static_cast<double>(feet.y),
		         static_cast<double>(feet.z), static_cast<double>(headWorld.x), static_cast<double>(headWorld.y),
		         static_cast<double>(neckWorld.x), static_cast<double>(neckWorld.y), static_cast<double>(lean.x),
		         static_cast<double>(lean.y));
	}
}

}  // namespace obvr::camera

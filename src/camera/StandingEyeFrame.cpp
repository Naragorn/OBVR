#include "camera/StandingEyeFrame.h"

#include <cstdio>

#include "camera/StandingEyeHold.h"
#include "core/Log.h"
#include "game/BodyIkLogic.h"
#include "game/KickLogic.h"
#include "game/MenuMode.h"
#include "game/PlayerAim.h"
#include "game/PlayerLookAt.h"
#include "game/PlayerTeleport.h"
#include "game/Shove.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera {

namespace {

// What the engine's eye did under a menu that is gone, against the view kept
// there: the measurement for docs/inventory-tablet-spec.md 28. A line a menu,
// some dozen a session.
void SayMenuEyeNote(const MenuEyeNote& note) {
	static UInt32 s_linesLeft = 40;
	if (!note.ready || s_linesLeft == 0) {
		return;
	}
	--s_linesLeft;
	char first[64] = "never 2 units off";
	if (note.firstFarFrame != 0) {
		std::snprintf(first, sizeof(first), "2 units off from its frame %u", note.firstFarFrame);
	}
	OBVR_LOG("Camera: under a menu with the world running the view was kept where it stood over the feet for %u "
	         "frame(s) - the engine's eye stood up to %.1f units from it (%.1f right, %.1f forward, %.1f up at the "
	         "furthest; %s), the feet went %.1f units meanwhile",
	         note.frames, static_cast<double>(note.farUnits), static_cast<double>(note.far.x),
	         static_cast<double>(note.far.y), static_cast<double>(note.far.z), first,
	         static_cast<double>(note.feetMovedUnits));
}

}  // namespace

void ApplyStandingEyeHold(NiAVObject* cameraNode, bool base, float dtSeconds) {
	static StandingEyeHoldState s_hold;
	static bool s_said = false;
	if (cameraNode == nullptr) {
		return;
	}
	StandingEyeHoldInput in;
	in.eye = cameraNode->localTransform.pos;
	game::PlayerRotation rot{};
	StandingEyeGates gates;
	gates.base = base;
	gates.playerRead = game::PlayerWorldPosition(in.feet) && game::ReadPlayerRotation(rot);
	const UInt16 flags = gates.playerRead ? game::ReadPlayerMovementFlags() : 0;
	gates.riding = game::PlayerRiding();
	gates.swimming = game::KickSwimming(game::PlayerCharacterState());
	// A menu's frame comes here only with the world running behind the menu:
	// the view is kept where it stood (camera/StandingEyeHold.h).
	gates.menuUp = game::IsMenuMode();
	gates.talking = gates.menuUp && game::PlayerDialogActive();
	gates.knockedDown = gates.menuUp && game::PlayerKnockState() > 0;
	in.live = StandingEyeLive(gates);
	in.underMenu = StandingEyeUnderMenu(gates);
	in.heightFree = game::IsPlayerSneaking();
	in.standing = game::FeetStand(flags);
	in.yaw = rot.yaw;
	in.dt = dtSeconds;
	const NiPoint3 held = StepStandingEyeHold(s_hold, in);
	SayMenuEyeNote(TakeMenuEyeNote(s_hold));
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
		         !in.live ? "not held"
		         : in.underMenu ? "kept under a menu"
		         : in.heightFree ? "held across the ground, sneaking"
		         : in.standing ? "held, standing"
		                       : "held, walking",
		         static_cast<double>(in.eye.x - in.feet.x), static_cast<double>(in.eye.y - in.feet.y),
		         static_cast<double>(in.eye.z - in.feet.z), static_cast<double>(held.x - in.feet.x),
		         static_cast<double>(held.y - in.feet.y), static_cast<double>(held.z - in.feet.z));
	}
	cameraNode->localTransform.pos = held;
}

}  // namespace obvr::camera

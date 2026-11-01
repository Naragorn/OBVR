#include "camera/hands/HandsInMenus.h"

#include "core/Log.h"
#include "game/FirstPersonDepth.h"
#include "game/FirstPersonHide.h"
#include "game/GrabNearBody.h"
#include "game/HandAdjust.h"
#include "game/HandBones.h"
#include "game/MenuPausePolicy.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "render/InterfaceRenderHook.h"
#include "test/HandScriptRuntime.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

void StepHandsInMenus(HandsInMenusState& state, const HandStep& step, const HandsInMenusLinks& links) {
	const Config& config = step.config;
	const bool menuIsUp = step.menuIsUp;
	// In a menu the hands are not pinned (the arms placement stands down
	// there), and with the game paused nothing animates them either: they
	// stayed where the controllers last put them, two hands hanging in the
	// room through a dialogue (2026-09-25). So in a menu the hand meshes
	// ("Hand", hand.nif's shape) are hidden along with the arms.
	// And from the conversation's first frame, not its menu's: see
	// game::StepDialogApproach.
	state.awayForDialog =
		game::StepDialogApproach(state.approach, game::TakeDialogCameraCall(), menuIsUp);
	// Or they stay: in the game's own menus, with HandsInMenus (the tester,
	// 2026-10-11), pinned and drawn as in the world; only the Esc menu and
	// its kin take them (HandsAwayNow).
	const bool handsAway = HandsAwayNow(config, menuIsUp, state.awayForDialog);
	state.stayInMenu = menuIsUp && !handsAway;
	state.stumpHandsAway = handsAway;
	{
		const UInt32 top = menuIsUp ? game::TopVisibleMenu() : game::kMenuIdNone;
		const bool changed = !state.menuLogged || menuIsUp != state.menuWasUp || handsAway != state.menuWasAway ||
		                     (menuIsUp && top != state.menuLastTop);
		if (changed && state.menuLines > 0) {
			--state.menuLines;
			if (menuIsUp) {
				OBVR_LOG("Hands: menu %s (0x%03X) - the hands %s", game::MenuIdName(top), top,
				         handsAway ? "go" : "stay - pinned and drawn as in the world");
			} else if (state.menuLogged) {
				OBVR_LOG("Hands: no menu - the hands as in the world%s",
				         handsAway ? " (away for the conversation)" : "");
			}
		}
		state.menuLogged = true;
		state.menuWasUp = menuIsUp;
		state.menuWasAway = handsAway;
		state.menuLastTop = top;
	}
	// The sheaths: a weapon's scabbard is its own node, "Scb", which the
	// engine hangs on the skeleton's side-weapon bone (cs.uesp.net,
	// NifSkope Comprehensive Guide, "Scabbards"), and a sheathed weapon
	// hangs there with it. In the first-person skeleton that bone moved
	// with the right hand, so the scabbard floated in the room beside it
	// (2026-09-25). The bones a sheathed weapon, a bow or a quiver hang on
	// are hidden with everything under them; "Scb" is named too, for a
	// scabbard hung anywhere else.
	// The weapon gone from the hand while it is sheathed: the engine's
	// animation carries it away from the controller (ComposeHandsHideList).
	const bool sheathing = game::ReadPlayerAction() == vr::kPlayerActionUnequipWeapon;
	game::HideFirstPersonNodes(!links.thirdPerson(),
	                           HandsHideList(config.hands, handsAway, sheathing, links.stumpShown && !handsAway),
	                           HandsCuirassKeep(config.hands));
	// The hands in the world rather than on top of it (FirstPersonDepth.h).
	game::KeepFirstPersonDepth(true);
	// And closed: their inside their own surface, darker - not the room
	// (BackfacePass.h).
	render::SetFirstPersonBackfaces(config.hands.closedHands, config.hands.closedHandsBrightness,
	                                config.hands.closedHands && game::FirstPersonHandsBare());
	if (test::HandScriptMarkedThisFrame()) {
		render::TraceFirstPersonBackfaces(16);
	}
	// Held objects may come up to the mouth and the body (GrabNearBody.h).
	game::AllowGrabNearBody(true);
	// The real controllers in the eyes while the hands are being adjusted.
	links.headsetRenderer.SetControllersWanted(
		(config.hands.adjustHands || game::HandAdjustActive()) && !menuIsUp);
}

}  // namespace obvr::camera::hands

#include "camera/hands/ControlsOut.h"

#include "camera/FrameLogic.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/AnalogWalk.h"
#include "game/FlyCam.h"
#include "game/GameSound.h"
#include "game/HandControls.h"
#include "game/MeleeHits.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "vr/FlyCamLogic.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::camera::hands {

void StepControlsOut(ControlsOutState& state, const HandStep& step, const HandShared& shared,
                     const vr::HandModeFrame& frame, const ControlsOutLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	const HandStep& handStep = step;
	const HandShared& handShared = shared;
	// The laser is the pointer: the game's own cursor sprite goes for as long
	// as the hand-tracked mode runs - it showed on the HUD in game as well -
	// and, with only the menus on the controllers, while a laser points at a
	// game menu. It comes back when neither holds.
	game::SetMenuCursorHidden(
		(active || (menuIsUp && hand.laserVisible)) && !frame.settingsMenuOpen);
	
	if (menuIsUp && !state.quadLaserMenuWasUp) {
		state.quadLaserLinesLeft = 4;
	}
	state.quadLaserMenuWasUp = menuIsUp;
	if (menuIsUp && !frame.flat.valid && state.quadLaserLinesLeft > 0 &&
	    (hand.laserVisible || hand.menuOnWrist)) {
		// Once on the first frame, then only while the trigger is pulled, so
		// the lines that are left show what a click saw.
		const bool pulled = frame.right.trigger > 0.5f || frame.left.trigger > 0.5f;
		if (state.quadLaserLinesLeft == 4 || pulled) {
			--state.quadLaserLinesLeft;
			OBVR_LOG("Hands: menu laser - %s quad %s (%.0fx%.0f layer px), cursor %s %.0f,%.0f, "
			         "%s hand points, hit %s %.0f,%.0f, step %d,%d, triggers %.2f/%.2f, click %d, "
			         "controls %s",
			         hand.menuOnWrist ? "wrist" : "big", frame.menuQuad.valid || hand.menuOnWrist
			                                                   ? "valid" : "MISSING",
			         static_cast<double>(frame.layerPixelsWidth),
			         static_cast<double>(frame.layerPixelsHeight),
			         frame.cursorValid ? "at" : "UNREADABLE", static_cast<double>(frame.cursorX),
			         static_cast<double>(frame.cursorY), hand.laserRight ? "right" : "left",
			         hand.laserHit ? "at" : "NONE", static_cast<double>(hand.laserPixelX),
			         static_cast<double>(hand.laserPixelY), hand.cursorDx, hand.cursorDy,
			         static_cast<double>(frame.right.trigger), static_cast<double>(frame.left.trigger),
			         hand.controls.menuClick ? 1 : 0,
			         hand.controlsActive ? "active" : "INACTIVE - nothing is sent");
		}
	}
	
	if (hand.controlsActive) {
		// Snap turning: the stick's turn becomes whole steps written into the
		// player's heading, and the continuous mouse turn is dropped. Written
		// here, at Present, so the next camera pass builds the view on it.
		vr::HandControlsWanted controls = hand.controls;
		// The engine's fly camera (game/FlyCam.h, vr/FlyCamLogic.h): the
		// sticks fly and turn it, the snap turn's steps turning its yaw, not
		// the player's.
		game::FlyCamPose fly;
		const bool flying = !menuIsUp && game::ReadFlyCam(fly) && fly.on;
		const SnapTurnStep snap =
			StepSnapTurn(state.snapTurn, flying ? hand.flyTurnX : controls.turn, config.look, links.deltaSeconds,
			             !menuIsUp && game::PlayerInWorld());
		if (snap.ownsStick) {
			controls.turn = 0.0f;
		}
		if (snap.fired && config.look.snapTurnVignette) {
			shared.vignette.Trigger();
		}
		if (flying) {
			vr::FlyCamInput in;
			in.moveX = hand.flyMoveX;
			in.moveY = hand.flyMoveY;
			in.turnX = hand.flyTurnX;
			in.liftY = hand.flyLiftY;
			Heading head{};
			if (HeadingOf(links.cameraRotation, head)) {
				in.headYaw = math::Atan2(head.sine, head.cosine);
			}
			in.snapYaw = snap.yawRadians;
			in.snapOwnsStick = snap.ownsStick;
			in.dtSeconds = links.deltaSeconds;
			const vr::FlyCamStep flyStep = vr::StepFlyCam(in, fly.yaw, config.hands.flyCam);
			if (flyStep.move.x != 0.0f || flyStep.move.y != 0.0f || flyStep.move.z != 0.0f || flyStep.yaw != 0.0f) {
				const NiPoint3 at{fly.at.x + flyStep.move.x, fly.at.y + flyStep.move.y, fly.at.z + flyStep.move.z};
				const bool wrote = game::WriteFlyCam(at, math::WrapAngle(fly.yaw + flyStep.yaw));
				if (state.flyLines > 0) {
					--state.flyLines;
					OBVR_LOG("FlyCam: the sticks %s - by %.1f %.1f %.1f, the yaw by %.1f degrees",
					         wrote ? "fly it" : "could not write it", static_cast<double>(flyStep.move.x),
					         static_cast<double>(flyStep.move.y), static_cast<double>(flyStep.move.z),
					         static_cast<double>(flyStep.yaw) * 57.29578);
				}
			}
		} else if (snap.yawRadians != 0.0f) {
			game::PlayerRotation rotation{};
			// rotZ grows clockwise and PlayerYawForGaze subtracts its step,
			// so a turn to the right goes in negated.
			const bool turned = game::ReadPlayerRotation(rotation) &&
			                    game::WritePlayerYaw(PlayerYawForGaze(rotation.yaw, -snap.yawRadians));
			if (snap.fired && !state.snapTurnReported) {
				state.snapTurnReported = true;
				OBVR_LOG("Look: snap turn %.0f degrees %s", static_cast<double>(config.look.snapTurnAngle),
				         turned ? "written into the player's heading"
				                : "could not be written - the player could not be reached");
			}
		}
	
		// The ready weapon tap, the first several times, with where the
		// weapon stood: "the button does nothing" was reported on
		// 2026-09-25, and this says whether the press left OBVR at all.
		if (controls.readyWeapon && !state.readyWasWanted && state.readyLinesLeft > 0) {
			--state.readyLinesLeft;
			const game::WeaponState weapon = game::ReadPlayerWeaponState();
			OBVR_LOG("Hands: ready weapon sent (the weapon was %s)",
			         weapon == game::WeaponState::Drawn      ? "drawn"
			         : weapon == game::WeaponState::Sheathed ? "sheathed"
			                                                 : "unknown");
		}
		// How long the draw or sheathe took, from the key to the game showing
		// it with no animation left: the length of the animation, which
		// WeaponDrawSpeed shortens.
		if (controls.readyWeapon && !state.readyWasWanted) {
			state.readySeconds = 0.0f;
		} else {
			state.readySeconds += links.deltaSeconds;
		}
		state.readyWasWanted = controls.readyWeapon;
		if ((hand.ready.reached || hand.ready.gaveUp) && state.readyEndLinesLeft > 0) {
			--state.readyEndLinesLeft;
			// With the player's action code: a give-up while blocking or in an
			// equip animation throughout never sent the key at all.
			// And the weapon's type: two-handers were still seen settling for
			// about a second after the draw (2026-09-27).
			SInt32 weaponType = -1;
			game::EquippedWeaponForm(&weaponType);
			OBVR_LOG("Hands: ready weapon %s after %.2f s (player action %d, weapon type %d)",
			         hand.ready.reached ? "done - the game shows the wanted state"
			                              : "given up - the game did not follow within 2.5 s",
			         static_cast<double>(state.readySeconds), static_cast<int>(game::ReadPlayerAction()),
			         static_cast<int>(weaponType));
		}
	
		// The staff's draw and sheathe sounds, which the game
		// does not play (vr::DrawSoundForm): once the weapon shows drawn or
		// sheathed, the sound of the change.
		{
			SInt32 soundType = -1;
			game::EquippedWeaponForm(&soundType);
			const game::WeaponState weaponNow = game::ReadPlayerWeaponState();
			if (weaponNow != game::WeaponState::Unknown && state.lastWeapon != game::WeaponState::Unknown &&
			    weaponNow != state.lastWeapon && !menuIsUp) {
				// Sheathed, the weapon is still equipped: its own type.
				const SInt32 type = soundType >= 0 ? soundType : state.lastType;
				const UInt32 form = vr::DrawSoundForm(type, weaponNow == game::WeaponState::Drawn);
				if (form != 0) {
					const bool played = game::PlaySoundForm(form);
					if (state.soundLines > 0) {
						--state.soundLines;
						OBVR_LOG("Hands: the %s's %s sound (%08X) %s", vr::IsStaffWeaponType(type) ? "staff" : "weapon",
						         weaponNow == game::WeaponState::Drawn ? "draw" : "sheathe", form,
						         played ? "played" : "not played");
					}
				}
			}
			state.lastWeapon = weaponNow;
			state.lastType = soundType;
		}
	
		// Every click the laser sends into a game menu, the first several
		// dozen: a message box that could not be clicked away (2026-09-27)
		// left no trace of whether a click was sent at all.
		if (controls.menuClick && !state.clickWasSent && menuIsUp && state.clickLinesLeft > 0) {
			--state.clickLinesLeft;
			char tileName[64] = {};
			game::ActiveTileName(tileName, sizeof(tileName));
			OBVR_LOG("Hands: click sent to the %s menu (on top: %s) at cursor %.0f,%.0f, the tile under it \"%s\"",
			         game::MenuIdName(game::ActiveMenuId()), game::MenuIdName(game::TopVisibleMenu()),
			         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY), tileName);
		}
		// The click's down-edge and the tile under the cursor as it went:
		// what the SteamVR keyboard opens on (vr/VrKeyboard.h, StepVrKeyboard).
		state.menuClickEdge = controls.menuClick && !state.clickWasSent && menuIsUp;
		if (!state.menuClickEdge || !game::ActiveTileName(state.menuClickTile, sizeof(state.menuClickTile))) {
			state.menuClickTile[0] = '\0';
		}
		// A tick in the pointing hand as the laser comes onto one of the
		// game's tiles, a firmer one as it clicks (vr/MenuHaptics.h).
		{
			const bool pointing = menuIsUp && (hand.laserHit || hand.pokeHover);
			const vr::MenuPulse pulse =
				vr::StepTileHaptic(state.menuHaptic, pointing, pointing ? game::ActiveTile() : 0, state.menuClickEdge);
			shared.menuPulse(hand.laserRight, pulse);
		}
		state.clickWasSent = controls.menuClick;
		// Every jump sent, the first several dozen: the right stick's flick
		// and a diagonal turn must be told apart (docs/controls-spec.md 2).
		if (controls.jump && !state.jumpWasSent && state.jumpLinesLeft > 0) {
			--state.jumpLinesLeft;
			OBVR_LOG("Hands: jump sent (right stick flicked up)");
		}
		state.jumpWasSent = controls.jump;
		// The menu key after a death: whether the game had shown a menu of
		// its own by then (the tester, 2026-10-08: "das Esc menü kam nicht,
		// musste es mit B herholen" - his logs cannot tell a menu he opened
		// from one the game opened).
		{
			const bool dead = game::PlayerIsDead();
			state.deadSeconds = dead ? state.deadSeconds + links.deltaSeconds : 0.0f;
			if (controls.menu && !state.menuWasSent && dead && state.deathMenuLines > 0) {
				--state.deathMenuLines;
				OBVR_LOG("Hands: the menu key %.1f s after death - the game %s", static_cast<double>(state.deadSeconds),
				         menuIsUp ? "had a menu up already" : "had shown no menu of its own by then");
			}
			state.menuWasSent = controls.menu;
		}
		// The left stick's analog walk (vr/AnalogWalkLogic.h, game/AnalogWalk.h):
		// the engine's pace factor written for the push, the heading kept for
		// the player's move, and the run key turned over under Always Run so
		// the legs show the gait the push chose.
		StepAnalogWalk(links.locomotion, handStep, handShared, flying, links.deltaSeconds, controls);
		game::ApplyHandControls(controls, config.handKeys, config.hands.turnSpeed);
		state.controlsHeld = true;
		// Not while the sticks walk the player under a reached container's
		// menu: the player's input runs then and would take the cursor's
		// steps as a turn of the body.
		if ((hand.laserHit || hand.pokeHover || hand.cursorMade) && !hand.walkingUnderMenu &&
		    (hand.cursorDx != 0 || hand.cursorDy != 0)) {
			game::MoveMouseBy(hand.cursorDx, hand.cursorDy);
			// Whether the steps arrive: the engine's cursor read the same
			// through many frames of steps means the mouse moves are not
			// reaching it (the tester, 2026-10-07: "kann im hauptmenü nicht
			// mehr buttons anvisieren oder klicken" with the cursor at 0,0
			// through every step in the log).
			if (frame.cursorValid && frame.cursorX == state.stuckX && frame.cursorY == state.stuckY) {
				++state.stuckFrames;
			} else {
				state.stuckFrames = 0;
				state.stuckX = frame.cursorX;
				state.stuckY = frame.cursorY;
			}
			if (state.stuckFrames == 90 && !state.stuckSaid) {
				state.stuckSaid = true;
				OBVR_LOG("Menu cursor: the engine's cursor stayed at %.0f,%.0f through 90 frames of mouse steps - the "
				         "moves are not reaching the game (its window not in front, or its cursor clamped)",
				         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY));
			}
		}
		if (hand.menuScroll != 0) {
			game::ScrollMouseWheel(hand.menuScroll);
			// Said a few times: the headset run of 2026-09-25 had the lists not
			// scroll, and whether the notches are sent at all is the first
			// question.
			if (state.wheelLinesLeft > 0) {
				--state.wheelLinesLeft;
				OBVR_LOG("Hands: mouse wheel %+d notch(es) sent to the %s menu at cursor %.0f,%.0f",
				         hand.menuScroll, game::MenuIdName(game::ActiveMenuId()),
				         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY));
			}
		}
		if (hand.pokePress && state.pokeLinesLeft > 0) {
			--state.pokeLinesLeft;
			OBVR_LOG("Hands: the finger pressed the menu at cursor %.0f, %.0f", frame.cursorX,
			         frame.cursorY);
		}
	} else if (state.controlsHeld) {
		game::ReleaseHandControls(config.handKeys);
		state.controlsHeld = false;
	}
	if (!hand.controlsActive) {
		// No hands, no analog walk: the pace factor back to the engine's.
		float pace = 1.0f;
		game::StepAnalogWalk(game::AnalogWalkRequest{}, pace);
	}
	
	if (hand.menuOnWrist) {
		links.hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(hand.menuWristRight, links.rolesSwapped)),
		                             hand.menuTransform, config.hands.wristMenuWidth);
	} else if (hand.hudOnRightWrist) {
		links.hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(true, links.rolesSwapped)), hand.hudTransform,
		                             config.hands.wristHudWidth);
	} else {
		links.hudLayer.ClearWristPlacement();
	}
}

}  // namespace obvr::camera::hands

#include "camera/hands/ControlsOut.h"

#include "camera/FrameLogic.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/AnalogWalk.h"
#include "game/EngineInput.h"
#include "game/FlyCam.h"
#include "game/GameSound.h"
#include "game/OptionsMenuVr.h"
#include "game/HandControls.h"
#include "game/MeleeHits.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "vr/FlyCamLogic.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::camera::hands {

namespace {

// One line a press of the drop button in a menu (2026-11-13: "A links ging
// nicht mehr" in the headset, the harness dropping): which menu was active,
// whether the drop began, whether the game's window was in front (the keys
// then go through the system, behind it through the engine's own input), and
// the tile under the cursor.
void LogDropPress(ControlsOutState& state, vr::DropGate gate) {
	if (gate == vr::DropGate::None || state.dropLinesLeft == 0) {
		return;
	}
	--state.dropLinesLeft;
	char tileName[64] = {};
	game::ActiveTileName(tileName, sizeof(tileName));
	OBVR_LOG("Inventory drop: the left A in the %s menu - %s; the game's window %s; the tile under the cursor \"%s\"",
	         game::MenuIdName(game::ActiveMenuId()),
	         gate == vr::DropGate::Drop ? "Shift and a click on the item" : "nothing: not the inventory",
	         game::GameWindowInFront() ? "in front" : "behind another window", tileName);
}

// Every jump sent and its input, the first several dozen: a flick and a
// diagonal turn told apart (docs/controls-spec.md 2), OBVR_Controls.ini seen.
// And every bound action the frame it fires, with its input, the first
// vr::kBoundFireLines of each (the controls matrix reads them).
void LogJumpAndBoundFires(ControlsOutState& state, bool jump, const vr::HandModeResult& hand) {
	if (jump && !state.jumpWasSent && state.jumpLinesLeft > 0) {
		--state.jumpLinesLeft;
		OBVR_LOG("Hands: jump sent (%s)", hand.jumpBy);
	}
	state.jumpWasSent = jump;
	for (UInt32 i = 0; i < vr::kControlActionCount; ++i) {
		const vr::ControlAction action = static_cast<vr::ControlAction>(i);
		if (vr::StepBoundFireLine(state.boundFireLog, action, hand.firedBy[i])) {
			OBVR_LOG("Buttons: %s from the %s", vr::ControlActionName(action), vr::ControlSourcePhrase(hand.firedBy[i]));
		}
	}
}

// The colours of the figure's own page (vr::SetFigurePage): a dark edge and
// the paper, opaque, near the page's own parchment.
constexpr UInt32 kFigurePageEdge = 0xFF4A3A28u;
constexpr UInt32 kFigurePagePaper = 0xFFE4D5B1u;

void Piece(render::TabletQuadPiece& piece, const vr::TabletCorners& corners, bool textured, UInt32 colour) {
	piece.valid = corners.valid;
	piece.textured = textured;
	piece.colour = colour;
	for (int i = 0; i < 4; ++i) {
		piece.corners[i] = corners.at[i];
		piece.u[i] = corners.u[i];
		piece.v[i] = corners.v[i];
	}
}

constexpr UInt32 kTabChosen = 0xFFE4D5B1u;  // the paper's colour: the tab in front
constexpr UInt32 kTabBehind = 0xFFA8946Cu;  // darker: the tab behind

// A small picture of `source` in the middle of a tab, its height 0.7 of the tab's.
vr::UiRect TabPicture(const vr::UiRect& tab, const vr::UiRect& source) {
	vr::UiRect r;
	const float height = 0.7f * (tab.bottom - tab.top);
	const float sourceHeight = source.bottom - source.top;
	if (!tab.valid || !source.valid || !(sourceHeight > 0.0f)) {
		return r;
	}
	const float width = height * (source.right - source.left) / sourceHeight;
	const float middle = 0.5f * (tab.left + tab.right);
	r.left = middle - 0.5f * width;
	r.right = middle + 0.5f * width;
	r.top = 0.5f * (tab.top + tab.bottom) - 0.5f * height;
	r.bottom = r.top + height;
	r.valid = true;
	return r;
}

// The figure behind a tab (vr::SetFigureTab): the two tabs, the chosen one in
// the paper's colour, each with its small picture; with the figure's chosen,
// the figure on paper over the page's rectangle.
void TabPieces(render::TabletQuadFrame& quad, const vr::TabletPlace& place, const vr::TabletFrame& t, bool showFigure,
               float textureWidth, float textureHeight, float share) {
	const vr::UiRect none;
	const vr::UiRect* sources[2] = {&t.paper, &t.figure};
	for (int i = 0; i < 2; ++i) {
		const bool chosen = (i == 1) == showFigure;
		Piece(quad.pieces[1 + 2 * i],
		      vr::RectOnTablet(place, t.crop, t.tabs[i], none, textureWidth, textureHeight, share), false,
		      chosen ? kTabChosen : kTabBehind);
		Piece(quad.pieces[2 + 2 * i],
		      vr::RectOnTablet(place, t.crop, TabPicture(t.tabs[i], *sources[i]), *sources[i], textureWidth,
		                       textureHeight, share),
		      true, 0);
	}
	if (showFigure) {
		Piece(quad.pieces[5], vr::RectOnTablet(place, t.crop, t.paper, none, textureWidth, textureHeight, share), false,
		      kFigurePagePaper);
		Piece(quad.pieces[6],
		      vr::RectOnTablet(place, t.crop, t.figureFit, t.figure, textureWidth, textureHeight, share), true, 0);
	}
}

// The tablet as drawn into the eyes: the page and its popups where the game
// draws them, and with the figure on its own page that page's edge, its paper
// and the figure - all rising from the lower edge together.
render::TabletQuadFrame TabletPieces(const vr::HandModeResult& hand, const vr::TabletFrame& t, float textureWidth,
                                     float textureHeight, float share) {
	render::TabletQuadFrame quad;
	vr::TabletPlace place;
	place.valid = true;
	place.transform = hand.menuTransform;
	place.width = hand.tabletWidth;
	place.height = hand.tabletHeight;
	const vr::UiRect page = t.figurePage || t.figureTab ? t.pageCrop : t.crop;
	Piece(quad.pieces[0], vr::RectOnTablet(place, t.crop, page, page, textureWidth, textureHeight, share), true, 0);
	if (t.figurePage) {
		const float edge = 0.006f * (t.paper.right - t.paper.left);
		vr::UiRect border = t.backing;
		border.top -= edge;
		border.bottom += edge;
		border.right += edge;
		const vr::UiRect none;
		Piece(quad.pieces[1], vr::RectOnTablet(place, t.crop, border, none, textureWidth, textureHeight, share), false,
		      kFigurePageEdge);
		Piece(quad.pieces[2], vr::RectOnTablet(place, t.crop, t.backing, none, textureWidth, textureHeight, share), false,
		      kFigurePagePaper);
		Piece(quad.pieces[3],
		      vr::RectOnTablet(place, t.crop, t.figureAt, t.figure, textureWidth, textureHeight, share), true, 0);
	}
	if (t.figureTab) {
		TabPieces(quad, place, t, hand.tabletShowsFigure, textureWidth, textureHeight, share);
	}
	quad.valid = quad.pieces[0].valid;
	return quad;
}

// The game's menus and the HUD on the hands: the tablet in the off hand -
// drawn into the eyes (render/TabletQuad.h) or, when that is off or did not
// reach both eyes last frame, the overlay on the holding controller showing
// the menus' cut - or a wrist, or neither.
void PlaceMenusOnHands(ControlsOutState& state, const Config& config, vr::OpenVRBackend& backend,
                       const vr::HandModeResult& hand, const vr::HandModeFrame& frame, const ControlsOutLinks& links) {
	const bool drawnBoth = links.headsetRenderer.TakeTabletDrawnBoth();
	state.tabletSeconds = hand.menuOnTablet ? state.tabletSeconds + links.deltaSeconds : 0.0f;
	render::TabletQuadFrame quad;
	const float textureWidth = static_cast<float>(links.hudLayer.CaptureWidth());
	const float textureHeight = static_cast<float>(links.hudLayer.CaptureHeight());
	if (hand.menuOnTablet && config.hands.tablet.inScene) {
		quad = TabletPieces(hand, frame.tablet, textureWidth, textureHeight, vr::TabletOpenShare(state.tabletSeconds));
		quad.texture = links.hudLayer.CaptureTexture();
	}
	links.headsetRenderer.SetTablet(quad);
	if (hand.menuOnTablet && hand.tabletShowsFigure != state.tabletShowedFigure) {
		OBVR_LOG("Tablet: the %s tab chosen", hand.tabletShowsFigure ? "figure's" : "page's");
	}
	state.tabletShowedFigure = hand.menuOnTablet && hand.tabletShowsFigure;
	if (hand.menuOnTablet != state.tabletWasUp) {
		state.tabletWasUp = hand.menuOnTablet;
		OBVR_LOG("Tablet: %s", hand.menuOnTablet ? (config.hands.tablet.inScene ? "in the off hand, drawn into the eyes"
		                                                                       : "in the off hand, as an overlay")
		                                         : "put down - the menus as before");
	}
	if (hand.menuOnTablet && textureWidth > 0.0f && textureHeight > 0.0f) {
		const vr::openvr::VRTextureBounds bounds{frame.tablet.crop.left / textureWidth, frame.tablet.crop.top / textureHeight,
		                                         frame.tablet.crop.right / textureWidth,
		                                         frame.tablet.crop.bottom / textureHeight};
		links.hudLayer.SetTabletPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(false, links.rolesSwapped)),
		                                  hand.menuTransform, hand.tabletWidth, bounds,
		                                  config.hands.tablet.inScene && drawnBoth);
	} else if (hand.menuOnWrist) {
		links.hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(hand.menuWristRight, links.rolesSwapped)),
		                             hand.menuTransform, config.hands.wristMenuWidth);
	} else if (hand.hudOnRightWrist) {
		links.hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(true, links.rolesSwapped)), hand.hudTransform,
		                             config.hands.wristHudWidth);
	} else {
		links.hudLayer.ClearWristPlacement();
	}
}

}  // namespace

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
	
		// Every click the laser sends into a game menu, the first several dozen: a message box
		// that could not be clicked away (2026-09-27) left no trace of whether a click was sent.
		LogDropPress(state, hand.dropGate);
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
		if (state.menuClickEdge) {
			game::NoteMenuClickTile(state.menuClickTile);  // the Options menu's VR Options and VR Guide
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
		LogJumpAndBoundFires(state, controls.jump, hand);
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
	
	PlaceMenusOnHands(state, config, backend, hand, frame, links);
}

}  // namespace obvr::camera::hands

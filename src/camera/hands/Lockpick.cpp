#include "camera/hands/Lockpick.h"

#include <cstdio>

#include "core/Log.h"
#include "game/HudTiles.h"
#include "game/LockpickTrace.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "vr/HandHud.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {
namespace {

// The minigame's tiles out of its tile tree, and the tumbler the pick
// stands under.
vr::LockTiles ReadLockTiles(UInt32& pickTumbler, bool& pickRead, float& pickY) {
	vr::LockTiles t;
	pickTumbler = 0;
	pickRead = false;
	vr::HudTile tiles[96];
	const UInt32 count = game::ReadHudTiles(game::kMenuIdLockPick, tiles, 96);
	for (UInt32 i = 0; i < count; ++i) {
		const vr::HudTile& tile = tiles[i];
		if (vr::SameTileName(tile.name, "lockpick_background")) {
			t.background = true;
			t.backgroundX = tile.x;
			t.backgroundY = tile.y;
			t.backgroundWidth = tile.width;
			t.backgroundHeight = tile.height;
		} else if (vr::SameTileName(tile.name, "lockpick_exit")) {
			t.buttons = true;
			t.buttonsY = tile.y;
		} else if (vr::SameTileName(tile.name, "lockpick_pick")) {
			pickTumbler = game::PickTumblerFor(game::PickPlaceFromTileX(tile.x));
			pickRead = true;
			pickY = tile.y;
		} else {
			for (UInt32 n = 0; n < 5; ++n) {
				char name[40];
				std::snprintf(name, sizeof(name), "lockpick_tumbler_%u_cover", n + 1);
				if (vr::SameTileName(tile.name, name)) {
					t.cover[n] = true;
					t.coverX[n] = tile.x;
					t.coverWidth[n] = tile.width;
				}
			}
		}
	}
	return t;
}

}  // namespace

void StepLockpickByHand(LockpickHandState& state, const HandStep& step, const HandShared& shared,
                        const vr::HandModeFrame& frame) {
	vr::HandModeResult& hand = shared.hand;
	const bool ours = step.active && step.menuIsUp && step.config.hands.lockpickByHand &&
	                  game::TopVisibleMenu() == game::kMenuIdLockPick;
	vr::LockpickFrame f;
	f.lockOnTop = ours;
	if (ours) {
		bool pickRead = false;
		float pickY = 0.0f;
		f.geometry = vr::LockGeometryFrom(ReadLockTiles(f.pickTumbler, pickRead, pickY), frame.layerPixelsWidth);
		if (pickRead) {
			state.pickLowY = !state.pickSeen || pickY < state.pickLowY ? pickY : state.pickLowY;
			state.pickHighY = !state.pickSeen || pickY > state.pickHighY ? pickY : state.pickHighY;
			state.pickSeen = true;
		}
		f.beamValid = hand.cursorWanted;
		f.beamX = hand.cursorWantedX;
		f.beamY = hand.cursorWantedY;
		f.stickX = vr::LockStick(frame.left.valid ? frame.left.thumbX : 0.0f, frame.right.valid ? frame.right.thumbX : 0.0f);
		f.stickY = vr::LockStick(frame.left.valid ? frame.left.thumbY : 0.0f, frame.right.valid ? frame.right.thumbY : 0.0f);
		f.cursorValid = frame.cursorValid;
		f.cursorY = frame.cursorY;
	}
	const vr::LockpickCursor c = vr::StepLockpick(state.logic, f);
	if (ours != state.up) {
		state.up = ours;
		if (ours) {
			OBVR_LOG("Lockpick: the minigame is up - the beam chooses the tumbler, a stick left or right steps, a stick "
			         "up pushes, the trigger or A secures (%s; the pick under tumbler %u)",
			         f.geometry.valid ? "its tiles read" : "its tiles NOT read, the beam's own cursor stays",
			         f.pickTumbler);
		}
	}
	if (!ours) {
		return;
	}
	if (step.scriptMarked) {
		OBVR_LOG("Lockpick: at the mark - chosen %u, the pick under %u, risen %.0f units since the last mark, the "
		         "cursor %s",
		         state.logic.selected, f.pickTumbler,
		         static_cast<double>(state.pickSeen ? state.pickHighY - state.pickLowY : 0.0f),
		         c.active ? "made here" : "the beam's own");
		state.pickSeen = false;
	}
	// The sticks are the pick's here, not the wheel's.
	hand.menuScroll = 0;
	if ((c.beamChose || c.stepped || c.pushed) && state.linesLeft > 0) {
		--state.linesLeft;
		OBVR_LOG("Lockpick: %s - tumbler %u, the cursor at %.0f %.0f (the pick under %u)",
		         c.pushed ? "a stick pushed up, the push sent" : c.stepped ? "a stick's flick stepped" : "the beam chose",
		         state.logic.selected, static_cast<double>(c.x), static_cast<double>(c.y), f.pickTumbler);
	}
	if (!c.active) {
		return;
	}
	hand.cursorMade = true;
	hand.cursorWanted = true;
	hand.cursorWantedX = c.x;
	hand.cursorWantedY = c.y;
	// The sticks alone at work: a trigger secures without a beam on the menu;
	// either hand's A does with the beam or without.
	if (vr::LockTriggerClicks(hand.laserHit || hand.pokeHover, frame.right.valid ? frame.right.trigger : 0.0f,
	                          frame.left.valid ? frame.left.trigger : 0.0f) ||
	    vr::LockAClicks(frame.right.valid && vr::ButtonADown(frame.right.buttonsPressed),
	                    frame.left.valid && vr::ButtonADown(frame.left.buttonsPressed))) {
		hand.controls.menuClick = true;
	}
	// Still on the buttons' row on its way up: no click there.
	if (c.settling) {
		hand.controls.menuClick = false;
	}
	if (frame.cursorValid) {
		hand.cursorDx = vr::CursorStep(frame.cursorX, c.x, step.config.hands.laserGain, step.config.hands.laserMaxStep);
		hand.cursorDy = vr::CursorStep(frame.cursorY, c.y, step.config.hands.laserGain, step.config.hands.laserMaxStep);
	}
}

}  // namespace obvr::camera::hands

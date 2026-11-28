#pragma once

// The hand mode's controls sent to the game, the frame's last step: the
// game's cursor sprite hidden while the laser points, the snap turn's steps
// written into the player's heading (camera/SnapTurn.h) or the fly camera
// flown (vr/FlyCamLogic.h), the analog walk, the keys applied
// (game/HandControls.h), the laser's mouse steps, the wheel and the finger's
// press; the click's edge and the tile under it for the VR keyboard, the
// tick at the tiles (vr/MenuHaptics.h); the ready weapon, the click, the
// jump and the menu key after a death logged the first several times; the
// staff's draw and sheathe sounds; the HUD placed on a wrist.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.17): the code as it was, its
// nine file globals and twenty statics now ControlsOutState's members (the
// VR keyboard reads the click's edge and tile as g_controlsOut.<name>).
// Harness: flycam.txt, analog-walk.txt, stick-diagonal.txt, main-menu-click.txt,
// quick-menu-assign.txt, reach-walk-wheel.txt.

#include "camera/SnapTurn.h"
#include "camera/hands/HandStep.h"
#include "camera/hands/Locomotion.h"
#include "game/PlayerAim.h"
#include "render/HeadsetRenderer.h"
#include "render/HudLayer.h"
#include "render/VignetteLayer.h"
#include "vr/MenuHaptics.h"
#include "vr/TabletClose.h"
#include "vr/TabletOverlayGate.h"

namespace obvr::camera::hands {

struct ControlsOutState {
	// The menu laser's lines, a few at each menu that opens: the first Full VR
	// headset run (2026-09-25) had the beam and neither a cursor nor a click.
	UInt32 quadLaserLinesLeft = 0;
	bool quadLaserMenuWasUp = false;
	// Snap turning, stepped once per frame with the controls (camera/SnapTurn.h).
	SnapTurnState snapTurn;
	bool snapTurnReported = false;
	bool controlsHeld = false;  // the keys applied, to be released when the hands go
	// The click's down-edge and the tile under the cursor as it went: what the
	// SteamVR keyboard opens on (vr/VrKeyboard.h).
	bool menuClickEdge = false;
	char menuClickTile[64] = {};
	// The tick at the buttons (vr/MenuHaptics.h): the game's tiles, OBVR's
	// panel rows and the quick menu's ring, each remembered on its own.
	vr::MenuHapticState menuHaptic;
	UInt32 pokeLinesLeft = 20;
	// The block's function statics, as they were.
	UInt32 flyLines = 6;
	bool readyWasWanted = false;
	UInt32 readyLinesLeft = 60;
	float readySeconds = 0.0f;
	UInt32 readyEndLinesLeft = 60;
	game::WeaponState lastWeapon = game::WeaponState::Unknown;
	SInt32 lastType = -1;
	UInt32 soundLines = 8;
	bool clickWasSent = false;
	UInt32 clickLinesLeft = 40;
	// The left A's drop presses in menus, each with what it did (vr::JudgeDropPress).
	UInt32 dropLinesLeft = 40;
	bool jumpWasSent = false;
	UInt32 jumpLinesLeft = 40;
	// The bound actions' lines, "Buttons: <action> from the <input>" (vr::StepBoundFireLine).
	vr::BoundFireLog boundFireLog;
	bool menuWasSent = false;
	float deadSeconds = 0.0f;
	UInt32 deathMenuLines = 8;
	float stuckX = -1.0f;
	float stuckY = -1.0f;
	UInt32 stuckFrames = 0;
	bool stuckSaid = false;
	UInt32 wheelLinesLeft = 6;
	// How long the tablet has been up (vr::TabletUnrollShare), and its lines.
	float tabletSeconds = 0.0f;
	bool tabletWasUp = false;
	vr::TabletCloseState tabletClose;  // the frames its menus close on (vr/TabletClose.h)
	vr::OverlayReturnState overlayReturn;  // the room's panel back after the tablet (vr/TabletOverlayGate.h)
	vr::HeldPlace heldSaid;            // the place in the hand as last said (vr::HeldPlace), and its lines
	bool heldSaidOnce = false;
	UInt32 heldLinesLeft = 40;
	vr::TabletTab tabletShowedTab = vr::TabletTab::Page;  // the tab shown, as last said
	bool tabletWasBetween = false;     // the scroll between the hands, as last said
	bool tabletWasRight = false;       // kept by the right hand, as last said
	UInt32 scrollRefusalLinesLeft = 40;  // "the scroll not begun" lines left (vr::TabletScrollRefusal)
	// A walk with the tablet up (docs/inventory-tablet-spec.md 4.6): where it
	// began and the clicks sent on the way, said as it ends.
	bool tabletWalking = false;
	bool tabletWalkFeetKnown = false;
	NiPoint3 tabletWalkFrom{0.0f, 0.0f, 0.0f};
	UInt32 tabletWalkClicks = 0;
	UInt32 tabletWalkLinesLeft = 8;
	// The page key the right stick's side sent (vr/TabletPages.h), held a few frames.
	UInt32 pageKey = 0;
	UInt32 pageKeyFrames = 0;
	// The finger on the tablet's glass (vr/TabletTouch.h), said as it goes: the notches of its scroll summed until
	// the list rests.
	UInt32 touchLinesLeft = 200;
	int touchNotches = 0;
	bool touchScrolling = false;  // a drag started and its notches not yet said
	UInt32 touchQuietFrames = 0;
};

// What the step shares with the rest of the frame.
struct ControlsOutLinks {
	render::HudLayer& hudLayer;            // the HUD, placed on a wrist or not
	render::HeadsetRenderer& headsetRenderer;  // the tablet drawn into the eyes
	LocomotionState& locomotion;           // the analog walk's state
	const NiMatrix33& cameraRotation;      // the head tracker's camera rotation (the fly camera's heading)
	bool rolesSwapped;                     // the hands' roles swapped (left-handed)
	float deltaSeconds;                    // the frame's (g_deltaSeconds)
};

// The hand mode stopped: the keys OBVR holds let go, a tablet's close forgotten.
void ResetControlsOut(ControlsOutState& state, const Config& config);

void StepControlsOut(ControlsOutState& state, const HandStep& step, const HandShared& shared,
                     const vr::HandModeFrame& frame, const ControlsOutLinks& links);

}  // namespace obvr::camera::hands

#include "camera/hands/LaserTargets.h"

#include "core/Config.h"
#include "core/Log.h"
#include "game/FirstPersonHide.h"
#include "game/MenuCursor.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "vr/HandInput.h"
#include "vr/LaserGeometry.h"
#include "vr/OpenVRTypes.h"

namespace obvr::camera::hands {

namespace {

// The tablet held at the palms' middles read last frame (vr/FingerTip.h
// PalmBetween), turned like the sword in that hand (vr::HeldAxes), before
// the hand as it is drawn (vr::HandReach) - whose read is told whether the
// tablet is up.
void PalmsToTablet(LaserTargetsState& state, vr::TabletFrame& tablet) {
	state.tabletUp = tablet.up;
	tablet.reach[0] = state.reach.shown[0];
	tablet.reach[1] = state.reach.shown[1];
	tablet.palmValid[0] = state.leftPalm.valid;
	tablet.palm[0] = state.leftPalm.local;
	tablet.palmValid[1] = state.rightPalm.valid;
	tablet.palm[1] = state.rightPalm.local;
	tablet.held[0] = state.leftHeld;
	tablet.held[1] = state.rightHeld;
}

// Up, or its menus fading out after the close, the left stick walks under
// the tablet's menus (docs/inventory-tablet-spec.md 4.6 and 26.3; game/MenuPause.h).
bool WalkUnderTablet(LaserTargetsState& state, const vr::TabletFrame& tablet) {
	const bool closing = vr::StepTabletClose(state.tabletClose, tablet.up, tablet.menusGone);
	return vr::TabletWalkFrame(tablet.up, closing, game::TabletWalkRefused());
}

}  // namespace

void FillLaserTargets(LaserTargetsState& state, const HandStep& step, vr::HandModeFrame& frame,
                      const LaserTargetsLinks& links) {
	vr::OpenVRBackend& backend = step.backend;
	const bool menuIsUp = step.menuIsUp;
	frame.rightIndexTip = state.rightIndexTip;
	frame.leftIndexTip = state.leftIndexTip;
	frame.cursorValid = game::InterfaceCursorPosition(frame.cursorX, frame.cursorY);
	// Whether the engine's tile search found a tile under the pixel placed
	// last frame (the sticky button, vr::StickyCursor).
	{
		char tileName[64] = {};
		game::ActiveTileName(tileName, sizeof(tileName));
		frame.cursorOnTile = tileName[0] != '\0';
	}
	// With the laser's pixel written for the tile search (game/MenuCursor.h)
	// the two floats hold that pixel; the mouse steps walk the engine's own
	// cursor, which a drag moves with - they are reckoned from where it is.
	float engineCursorX = 0.0f;
	float engineCursorY = 0.0f;
	if (frame.cursorValid && game::EngineMenuCursor(engineCursorX, engineCursorY)) {
		frame.cursorX = engineCursorX;
		frame.cursorY = engineCursorY;
	}
	if (menuIsUp) {
		char tileName[48];
		game::CursorOverScrollBar(tileName, sizeof(tileName));
		frame.cursorOnScrollKnob = game::IsScrollKnobName(tileName);
		frame.knobSnapValid = game::MenuCursorKnobOffset(frame.knobSnapDx, frame.knobSnapDy);
		frame.menuIsDragSurface = game::ActiveMenuId() == game::kMenuIdMap ||
		                          game::TopVisibleMenu() == game::kMenuIdMap;
		// Which tile a pull lands on, the first several times: the scroll bar
		// is recognised by its name, and the log is where a name that is
		// missed shows up.
		const bool pulled = (frame.right.valid && frame.right.trigger > 0.5f) ||
		                    (frame.left.valid && frame.left.trigger > 0.5f);
		if (pulled && !state.wasPulled && state.pullLinesLeft > 0) {
			--state.pullLinesLeft;
			OBVR_LOG("Hands: pulled over tile \"%s\" in the %s menu - %s", tileName,
			         game::MenuIdName(game::ActiveMenuId()),
			         frame.cursorOnScrollKnob ? "a slider's or scroll bar's knob, the button is held"
			                                 : "not a knob");
		}
		state.wasPulled = pulled;
	}
	// The quad the game's menus hang on when they are not on a wrist, for
	// the laser: where the layer last hung it, in tracking space.
	{
		vr::openvr::HmdMatrix34 quadPose{};
		float quadWidth = 0.0f;
		if (links.hudLayer.QuadInTracking(backend, quadPose, quadWidth)) {
			frame.menuQuad = vr::QuadFromPose(quadPose, quadWidth, frame.layerPixelsWidth,
			                                  frame.layerPixelsHeight);
		}
	}
	// The tablet's menus and the page they show (game/TabletMenu.h), in Full
	// VR in the world.
	frame.tablet = game::ReadTabletMenus(state.tabletMenu,
	                                     step.active && step.config.hands.tablet.enabled && frame.inWorld,
	                                     frame.layerPixelsWidth, frame.layerPixelsHeight,
	                                     step.config.hands.tablet.inScene, step.config.hands.tablet.figure,
	                                     step.scriptMarked);
	PalmsToTablet(state, frame.tablet);
	frame.walkUnderTablet = WalkUnderTablet(state, frame.tablet);
	frame.walkUnderMenu = frame.walkUnderMenu || frame.walkUnderTablet;
	// And the cinema screen when the frame is a flat one - the main menu,
	// character creation, a loading screen - which has no quad: the laser
	// meets the picture the head sees at infinity instead.
	links.headsetRenderer.FlatPictureInTracking(frame.flat);
	// OBVR's own panel, whichever is open, so the laser can put the
	// highlight on the row it points at.
	if (frame.settingsMenuOpen) {
		const ui::SettingsMenuLayer& panel =
			links.onboardingOpen ? links.onboardingLayer : links.settingsMenuLayer;
		vr::openvr::HmdMatrix34 panelPose{};
		float panelWidth = 0.0f;
		UInt32 canvasWidth = 0;
		UInt32 canvasHeight = 0;
		ui::SettingsMenuLayer::CanvasSize(canvasWidth, canvasHeight);
		frame.settingsPixelsWidth = static_cast<float>(canvasWidth);
		frame.settingsPixelsHeight = static_cast<float>(canvasHeight);
		if (panel.QuadInTracking(backend, panelPose, panelWidth)) {
			frame.settingsQuad = vr::QuadFromPose(panelPose, panelWidth, frame.settingsPixelsWidth,
			                                      frame.settingsPixelsHeight);
		}
	}
	if (frame.flat.valid != state.flatReported) {
		state.flatReported = frame.flat.valid;
		if (frame.flat.valid) {
			OBVR_LOG("Hands: the laser has the flat picture to point at (%.0fx%.0f frame pixels "
			         "from %.0f,%.0f; half-extents tan %.3f x %.3f)",
			         static_cast<double>(frame.flat.pixelWidth),
			         static_cast<double>(frame.flat.pixelHeight),
			         static_cast<double>(frame.flat.pixelLeft),
			         static_cast<double>(frame.flat.pixelTop),
			         static_cast<double>(frame.flat.tanHalfWidth),
			         static_cast<double>(frame.flat.tanHalfHeight));
		}
	}
}

void StepIndexTips(LaserTargetsState& state, bool rightFollows, bool leftFollows, const NiMatrix33& cameraRot,
                   const NiPoint3& cameraPos, float unitsPerMetre, const vr::HandModeResult& hand) {
	const auto step = [&](bool right, bool follows, const NiMatrix33& handRot, const NiPoint3& handOffsetUnits,
	                      vr::IndexTip& tip) {
		const NiAVObject* const middle = game::FindFirstPersonNode(right ? "Bip01 R Finger11" : "Bip01 L Finger11");
		const NiAVObject* const last = game::FindFirstPersonNode(right ? "Bip01 R Finger12" : "Bip01 L Finger12");
		const bool read = follows && middle != nullptr && last != nullptr && unitsPerMetre > 0.0f;
		NiPoint3 local{0.0f, 0.0f, 0.0f};
		if (read) {
			const NiPoint3 world = vr::IndexTipBeyond(middle->worldTransform.pos, last->worldTransform.pos);
			const NiPoint3 handWorld = cameraPos + cameraRot * handOffsetUnits;
			local = vr::ControllerLocalOf(cameraRot, handRot, world - handWorld) * (1.0f / unitsPerMetre);
		}
		const bool was = tip.valid;
		vr::StepIndexTip(tip, read, local);
		if (tip.valid != was && state.tipLinesLeft > 0) {
			--state.tipLinesLeft;
			if (tip.valid) {
				OBVR_LOG("Hands: the %s index finger's tip read - %.3f %.3f %.3f m in its controller's frame; the "
				         "tablet is tapped with it",
				         right ? "right" : "left", static_cast<double>(tip.local.x),
				         static_cast<double>(tip.local.y), static_cast<double>(tip.local.z));
			} else {
				const char* const why = !follows                              ? "the drawn fingers do not follow the controller's"
				                        : middle == nullptr || last == nullptr ? "its bones not found"
				                                                               : "too far from its controller";
				OBVR_LOG("Hands: the %s index finger's tip no longer read (%s) - the controller's tip taps",
				         right ? "right" : "left", why);
			}
		}
	};
	step(true, rightFollows && hand.rightHandValid, hand.rightHandRotation, hand.rightHandOffsetUnits,
	     state.rightIndexTip);
	step(false, leftFollows && hand.leftHandValid, hand.leftHandRotation, hand.leftHandOffsetUnits,
	     state.leftIndexTip);
	// The palms' middles, from the same hands whatever their fingers do: the tablet is held there.
	const auto palm = [&](bool right, bool valid, const NiMatrix33& handRot, const NiPoint3& handOffsetUnits,
	                      vr::IndexTip& at) {
		const NiAVObject* const wrist = game::FindFirstPersonNode(right ? "Bip01 R Hand" : "Bip01 L Hand");
		const NiAVObject* const middle = game::FindFirstPersonNode(right ? "Bip01 R Finger2" : "Bip01 L Finger2");
		const bool read = valid && wrist != nullptr && middle != nullptr && unitsPerMetre > 0.0f;
		NiPoint3 local{0.0f, 0.0f, 0.0f};
		if (read) {
			const NiPoint3 world = vr::PalmBetween(wrist->worldTransform.pos, middle->worldTransform.pos);
			const NiPoint3 handWorld = cameraPos + cameraRot * handOffsetUnits;
			local = vr::ControllerLocalOf(cameraRot, handRot, world - handWorld) * (1.0f / unitsPerMetre);
		}
		const bool was = at.valid;
		vr::StepIndexTip(at, read, local);
		if (at.valid && !was && state.tipLinesLeft > 0) {
			--state.tipLinesLeft;
			OBVR_LOG("Hands: the %s palm's middle read - %.3f %.3f %.3f m in its controller's frame; the tablet is held there",
			         right ? "right" : "left", static_cast<double>(at.local.x), static_cast<double>(at.local.y),
			         static_cast<double>(at.local.z));
		}
	};
	palm(true, hand.rightHandValid, hand.rightHandRotation, hand.rightHandOffsetUnits, state.rightPalm);
	palm(false, hand.leftHandValid, hand.leftHandRotation, hand.leftHandOffsetUnits, state.leftPalm);
	// The held-item nodes, from the same hands: the right's "Weapon", where the
	// sword hangs, the left's "Torch", where the torch and the bow do - their
	// y and x in the controller's frame (vr::HeldAxes). Directions only, so a
	// mirrored skeleton (left-handed) reads its own hands the same way.
	const auto held = [&](bool right, bool valid, const NiMatrix33& handRot, vr::HeldAxes& at) {
		const char* const name = right ? "Weapon" : "Torch";
		const NiAVObject* const node = game::FindFirstPersonNode(name);
		const bool read = valid && node != nullptr;
		NiPoint3 blade{0.0f, 1.0f, 0.0f};
		NiPoint3 edge{1.0f, 0.0f, 0.0f};
		if (read) {
			blade = vr::ControllerLocalOf(cameraRot, handRot, node->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f});
			edge = vr::ControllerLocalOf(cameraRot, handRot, node->worldTransform.rot * NiPoint3{1.0f, 0.0f, 0.0f});
		}
		const bool was = at.valid;
		vr::StepHeldAxes(at, read, blade, edge);
		if (at.valid && !was && state.heldLinesLeft > 0) {
			--state.heldLinesLeft;
			OBVR_LOG("Hands: the %s hand's %s node read - blade %.3f %.3f %.3f, knuckles %.3f %.3f %.3f in its "
			         "controller's frame; the tablet is held like the sword there",
			         right ? "right" : "left", name, static_cast<double>(at.blade.x), static_cast<double>(at.blade.y),
			         static_cast<double>(at.blade.z), static_cast<double>(at.edge.x), static_cast<double>(at.edge.y),
			         static_cast<double>(at.edge.z));
		}
	};
	held(true, hand.rightHandValid, hand.rightHandRotation, state.rightHeld);
	held(false, hand.leftHandValid, hand.leftHandRotation, state.leftHeld);
	// And how far each hand as drawn reaches out of the flat the page lies in, while the tablet is up.
	StepHandReachRead(state.reach, state.tabletUp, hand, unitsPerMetre, GetConfig().hands.tablet.held);
}

}  // namespace obvr::camera::hands

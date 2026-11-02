#include "camera/hands/LaserTargets.h"

#include "core/Log.h"
#include "game/MenuCursor.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"

namespace obvr::camera::hands {

void FillLaserTargets(LaserTargetsState& state, const HandStep& step, vr::HandModeFrame& frame,
                      const LaserTargetsLinks& links) {
	vr::OpenVRBackend& backend = step.backend;
	const bool menuIsUp = step.menuIsUp;
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

}  // namespace obvr::camera::hands

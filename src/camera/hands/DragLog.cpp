#include "camera/hands/DragLog.h"

#include "core/Log.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"

namespace obvr::camera::hands {

void LogMenuDrags(DragLogState& state, const HandShared& shared, const vr::HandModeFrame& frame) {
	const vr::HandModeResult& hand = shared.hand;
	// On the map, its pan as well: what the drag did.
	if (hand.menuDragging != state.dragWas && state.dragLines > 0) {
		--state.dragLines;
		float dragX = 0.0f;
		float dragY = 0.0f;
		float panX = 0.0f;
		float panY = 0.0f;
		const bool onMap = game::MapMenuPan(dragX, dragY, panX, panY);
		if (hand.menuDragging) {
			state.dragFromX = frame.cursorX;
			state.dragFromY = frame.cursorY;
			state.dragOnMap = onMap;
			state.panFromX = panX;
			state.panFromY = panY;
			OBVR_LOG("Hands: a drag held in the %s menu - the engine's own cursor at %.0f,%.0f, the beam at %.0f,%.0f",
			         game::MenuIdName(game::ActiveMenuId()), static_cast<double>(frame.cursorX),
			         static_cast<double>(frame.cursorY), static_cast<double>(hand.laserPixelX),
			         static_cast<double>(hand.laserPixelY));
		} else if (state.dragOnMap && onMap) {
			OBVR_LOG("Hands: the drag let go - the engine's own cursor went from %.0f,%.0f to %.0f,%.0f, the map's "
			         "pan from %.1f,%.1f to %.1f,%.1f",
			         static_cast<double>(state.dragFromX), static_cast<double>(state.dragFromY),
			         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY),
			         static_cast<double>(state.panFromX), static_cast<double>(state.panFromY), static_cast<double>(panX),
			         static_cast<double>(panY));
		} else {
			OBVR_LOG("Hands: the drag let go - the engine's own cursor went from %.0f,%.0f to %.0f,%.0f",
			         static_cast<double>(state.dragFromX), static_cast<double>(state.dragFromY),
			         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY));
		}
	}
	state.dragWas = hand.menuDragging;
}

}  // namespace obvr::camera::hands

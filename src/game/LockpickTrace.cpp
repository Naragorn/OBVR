#include "game/LockpickTrace.h"

#include "core/Log.h"
#include "game/HudTiles.h"
#include "game/MenuCursor.h"
#include "game/MenuType.h"
#include "vr/HandHud.h"

namespace obvr::game {

void StepLockpickTrace(LockpickTraceState& state, UInt32 wantedLines) {
	if (state.written >= wantedLines) {
		return;
	}
	vr::HudTile tiles[96];
	const UInt32 count = ReadHudTiles(kMenuIdLockPick, tiles, 96);
	if (count == 0) {
		return;
	}
	const vr::HudTile* pick = nullptr;
	const vr::HudTile* background = nullptr;
	for (UInt32 i = 0; i < count; ++i) {
		if (vr::SameTileName(tiles[i].name, "lockpick_pick")) {
			pick = &tiles[i];
		} else if (vr::SameTileName(tiles[i].name, "lockpick_background")) {
			background = &tiles[i];
		}
	}
	float cursorX = 0.0f;
	float cursorY = 0.0f;
	const bool cursor = InterfaceCursorPosition(cursorX, cursorY);
	float engineX = 0.0f;
	float engineY = 0.0f;
	const bool engine = EngineMenuCursor(engineX, engineY);
	float laserX = 0.0f;
	float laserY = 0.0f;
	const bool laser = MenuCursorWantedNow(laserX, laserY);
	const float place = pick != nullptr ? PickPlaceFromTileX(pick->x) : 0.0f;
	OBVR_LOG("Lockpick trace: f=%u cursor %d %.1f %.1f engine %d %.1f %.1f laser %d %.1f %.1f | pick %d x %.1f y %.1f "
	         "place %.1f tumbler %u | background %d x %.1f y %.1f w %.1f h %.1f",
	         state.written, cursor ? 1 : 0, static_cast<double>(cursorX), static_cast<double>(cursorY), engine ? 1 : 0,
	         static_cast<double>(engineX), static_cast<double>(engineY), laser ? 1 : 0, static_cast<double>(laserX),
	         static_cast<double>(laserY), pick != nullptr ? 1 : 0, static_cast<double>(pick != nullptr ? pick->x : 0.0f),
	         static_cast<double>(pick != nullptr ? pick->y : 0.0f), static_cast<double>(place),
	         pick != nullptr ? PickTumblerFor(place) : 0u, background != nullptr ? 1 : 0,
	         static_cast<double>(background != nullptr ? background->x : 0.0f),
	         static_cast<double>(background != nullptr ? background->y : 0.0f),
	         static_cast<double>(background != nullptr ? background->width : 0.0f),
	         static_cast<double>(background != nullptr ? background->height : 0.0f));
	++state.written;
}

}  // namespace obvr::game

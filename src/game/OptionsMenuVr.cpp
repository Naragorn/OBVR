#include "game/OptionsMenuVr.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/Log.h"
#include "game/ConsoleLine.h"
#include "game/GameAddresses.h"
#include "game/GuideTrack.h"
#include "game/HudTiles.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/NativeMenuPrototype.h"
#include "ui/OptionsVrButtons.h"

namespace obvr::game {

namespace {

// The traits read and written (xOBSE's GameTiles.h ids).
constexpr UInt16 kTraitVisible = 0x0FA1;
constexpr UInt16 kTraitY = 0x0FAC;
constexpr UInt16 kTraitUser1 = 0x0FAF;  // generic_background.xml: the frame's height is its user1
constexpr UInt16 kTraitHeight = 0x0FCA;

// What the buttons keep from frame to frame, owned here.
struct ButtonState {
	bool guideAsked = false;
	ui::OptionsVrTile handClick = ui::OptionsVrTile::None;
	ui::OptionsVrState buttons;
	float bottomMargin = ui::kOptionsDefaultMargin;  // under the last row, before OBVR's came
	UInt32 ownRows = 0;
	UInt32 rowsMoved = 0;      // how often the rows were put right in this menu
	UInt32 saidLayoutFor = 0;  // the menu whose layout was said after the buttons came (0: none)
	UInt32 framesSince = 0;
	bool mainWasLoaded = false;
};
ButtonState& State() {
	static ButtonState s_state;
	return s_state;
}

// The mouse's left button, only while the game's own window is in front.
bool MouseDownInGame() {
	if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
		return false;
	}
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}

float NumberOr(const void* tile, UInt16 trait, float otherwise) {
	float value = otherwise;
	return ReadTileNumber(tile, trait, value) ? value : otherwise;
}

void WriteTile(const void* tile, UInt16 trait, float value) {
	using UpdateFloatFn = void(__thiscall*)(void* self, UInt32 trait, float value);
	reinterpret_cast<UpdateFloatFn>(addr::kTileUpdateFloat)(const_cast<void*>(tile), trait, value);
}

// The menu's rows as they stand. False when one of the game's rows is not
// there by its name.
bool ReadRows(const void* frame, ui::OptionsRows& rows, const void** tiles) {
	if (!ReadTileNumber(frame, kTraitUser1, rows.frameHeight)) {
		return false;
	}
	for (UInt32 i = 0; i < ui::kOptionsGameRows; ++i) {
		tiles[i] = FindTileNamed(frame, ui::kOptionsGameRowTiles[i]);
		if (tiles[i] == nullptr || !ReadTileNumber(tiles[i], kTraitY, rows.y[i])) {
			return false;
		}
		float number = 0.0f;
		const char* text = nullptr;
		const bool has = ReadTileTrait(tiles[i], kTraitVisible, number, text);
		rows.visible[i] = ui::OptionsRowShown(has, number, text);
	}
	return true;
}

// The rows put where they belong (ui::LayOutOptionsRows), only what is off
// written: the game's rows first, OBVR's after them - a row of OBVR's reads
// its place from the game's (its prefab) and would follow their move.
void PlaceRows(ButtonState& s, const void* frame) {
	ui::OptionsRows rows;
	const void* tiles[ui::kOptionsGameRows] = {};
	const void* const own[2] = {FindTileNamed(frame, ui::kVrOptionsTile), FindTileNamed(frame, ui::kVrGuideTile)};
	if (own[0] == nullptr || (s.ownRows > 1 && own[1] == nullptr) || !ReadRows(frame, rows, tiles)) {
		return;  // MenuQue's insert is still to come
	}
	rows.ownRows = s.ownRows;
	const ui::OptionsLayout layout = ui::LayOutOptionsRows(rows, s.bottomMargin);
	bool moved = false;
	for (UInt32 i = 0; i < ui::kOptionsGameRows; ++i) {
		if (rows.visible[i] && std::fabs(layout.y[i] - rows.y[i]) > 0.5f) {
			WriteTile(tiles[i], kTraitY, layout.y[i]);
			moved = true;
		}
	}
	for (UInt32 i = 0; i < s.ownRows && i < 2; ++i) {
		if (std::fabs(NumberOr(own[i], kTraitY, -10000.0f) - layout.ownY[i]) > 0.5f) {
			WriteTile(own[i], kTraitY, layout.ownY[i]);
			moved = true;
		}
	}
	if (moved && ++s.rowsMoved <= 6) {
		OBVR_LOG("Options menu: the rows put in place (%u. time in this menu) - the frame %.0f high, the game's rows "
		         "from %.0f (they stood from %.0f), %s, VR Options at %.0f%s",
		         s.rowsMoved, static_cast<double>(rows.frameHeight), static_cast<double>(layout.y[0]),
		         static_cast<double>(rows.y[0]), rows.visible[5] ? "another mod's row in the Downloads place" : "no sixth row",
		         static_cast<double>(layout.ownY[0]), s.ownRows > 1 ? ", VR Guide under it" : "");
	}
}

// Where the menu's rows stand a second after the buttons came, said once a
// menu: what a UI mod's or LINK's own changes left of the layout is read
// from this line.
void SayLayout(const void* frame) {
	static const char* const kRows[] = {"options_return_button", "options_controls_button", "options_downloads_button",
	                                    ui::kVrOptionsTile, ui::kVrGuideTile};
	char rows[320] = {};
	for (const char* name : kRows) {
		const void* const tile = FindTileNamed(frame, name);
		const UInt32 len = static_cast<UInt32>(std::strlen(rows));
		if (tile == nullptr) {
			std::snprintf(rows + len, sizeof(rows) - len, "%s%s none", len > 0 ? ", " : "", name);
			continue;
		}
		float number = 0.0f;
		const char* text = nullptr;
		const bool has = ReadTileTrait(tile, kTraitVisible, number, text);
		std::snprintf(rows + len, sizeof(rows) - len, "%s%s y %.0f %s", len > 0 ? ", " : "", name,
		              static_cast<double>(NumberOr(tile, kTraitY, -1.0f)),
		              ui::OptionsRowShown(has, number, text) ? "shown" : "hidden");
	}
	OBVR_LOG("Options menu: the rows as they stand - the frame's y %.0f, height %.0f (its user1 %.0f); %s",
	         static_cast<double>(NumberOr(frame, kTraitY, -1.0f)), static_cast<double>(NumberOr(frame, kTraitHeight, -1.0f)),
	         static_cast<double>(NumberOr(frame, kTraitUser1, -1.0f)), rows);
}

}  // namespace

void RequestStartGuide() { State().guideAsked = true; }
bool StartGuideAsked() { return State().guideAsked; }
void ClearStartGuideAsked() { State().guideAsked = false; }

void NoteMenuClickTile(const char* tileName) {
	const ui::OptionsVrTile tile = ui::OptionsVrTileOf(tileName);
	if (tile != ui::OptionsVrTile::None) {
		State().handClick = tile;
	}
}

void StepOptionsVrButtons(bool enabled) {
	ButtonState& s = State();
	const void* const menu = enabled ? MenuRootTile(kMenuIdOptions) : nullptr;
	ui::OptionsVrInput in;
	in.optionsTile = reinterpret_cast<UInt32>(menu);
	in.handClickOn = s.handClick;
	s.handClick = ui::OptionsVrTile::None;
	const bool mainLoaded = MainMenuRootTile() != 0;
	if (s.mainWasLoaded && !mainLoaded) {
		s.guideAsked = false;  // an ask never taken does not wait for the next main menu
	}
	s.mainWasLoaded = mainLoaded;
	if (menu == nullptr) {
		if (s.buttons.seenFor != 0) {
			ui::StepOptionsVrButtons(s.buttons, in);
		}
		s.saidLayoutFor = 0;
		return;  // no Options menu: nothing more a frame
	}
	const void* const frame = FindTileNamed(menu, "options_background");
	ui::OptionsRows rows;
	const void* tiles[ui::kOptionsGameRows] = {};
	in.frameFound = frame != nullptr && ReadRows(frame, rows, tiles);
	in.frameHeight = rows.frameHeight;
	in.optionsOnTop = TopVisibleMenu() == kMenuIdOptions;
	in.fromMainMenu = mainLoaded;
	in.guideShowing = StartGuideShowing();
	in.settingsOpen = NativeSettingsOpen();
	in.mouseDown = MouseDownInGame();
	char tile[64] = {};
	in.pointerOn = in.optionsOnTop && ActiveTileName(tile, sizeof(tile)) ? ui::OptionsVrTileOf(tile) : ui::OptionsVrTile::None;
	const ui::OptionsVrStep step = ui::StepOptionsVrButtons(s.buttons, in);
	if (step.foreignLayout) {
		OBVR_LOG("Options menu: the menu %08X is not laid out as the game's (no \"options_background\" with the game's "
		         "six rows and a height) - VR Options and VR Guide are not put into it; both sticks or Insert open "
		         "OBVR's menu as ever",
		         in.optionsTile);
	}
	if (step.insertOptions) {
		s.bottomMargin = ui::OptionsBottomMargin(rows);
		s.ownRows = step.insertGuide ? 2 : 1;
		s.rowsMoved = 0;
		s.framesSince = 0;
		const bool options = RequestConsoleLine(ui::kVrOptionsInsertLine);
		const bool guide = !step.insertGuide || RequestConsoleLine(ui::kVrGuideInsertLine);
		WriteTile(frame, kTraitUser1, step.frameHeight);
		OBVR_LOG("Options menu: VR Options%s %s the Options menu %08X (MenuQue's InsertXML, OBVR\\options_vr_*.xml), "
		         "opened from the %s; its frame %.0f -> %.0f high, %.0f under its last row",
		         step.insertGuide ? " and VR Guide" : "", options && guide ? "asked into" : "could NOT be asked into",
		         in.optionsTile, in.fromMainMenu ? "main menu" : "game", static_cast<double>(in.frameHeight),
		         static_cast<double>(step.frameHeight), static_cast<double>(s.bottomMargin));
	}
	if (s.buttons.inserted) {
		PlaceRows(s, frame);
		// The layout a second after the ask: MenuQue's insert and another mod's own changes have settled.
		if (s.saidLayoutFor != in.optionsTile && ++s.framesSince >= 60) {
			s.saidLayoutFor = in.optionsTile;
			SayLayout(frame);
		}
	}
	const char* const by = in.handClickOn != ui::OptionsVrTile::None ? "a hand's click" : "the mouse";
	if (step.openSettings) {
		RequestNativeSettingsToggle();
		OBVR_LOG("Options menu: VR Options clicked (%s) - OBVR's menu asked for", by);
	}
	if (step.startGuide) {
		RequestStartGuide();
		const bool queued = RequestConsoleLine(ui::kOptionsReturnLine);
		OBVR_LOG("Options menu: VR Guide clicked (%s) - the start guide asked for again, the Options menu's Return %s", by,
		         queued ? "clicked for it" : "could NOT be clicked");
	}
}

}  // namespace obvr::game

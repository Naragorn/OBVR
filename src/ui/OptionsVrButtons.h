#pragma once

// "VR Options" and "VR Guide" in the game's Options menu (Nadi, 2026-11-16:
// the main menu's "VR" button moves under Options as "VR Guide", with "VR
// Options" for OBVR's own menu; in the game, under the pause menu's Options,
// "VR Options" alone). The main menu and the pause menu open the same Options
// menu (options_menu.xml, menu type 1015): Return, Gameplay, Video, Audio,
// Controls, and an Xbox "Downloads" button that is invisible on the PC.
// OBVR ships no Options menu of its own - a UI mod's would lose its own - so
// the buttons are tiles put into the loaded menu's frame ("options_background")
// by MenuQue's InsertXML (cs.uesp.net/wiki/InsertXML: "file|component", the
// console's '@' for the pipe; the file under Data\Menus\Prefabs), once for
// each Options menu the game builds.
// They stand under the menu's last row that is shown - Controls, or the
// Downloads row when another mod shows it (LINK makes it its "Mods" button,
// LINK.esp LINKMenu1015UpdateMenuFunctionScript) - so OBVR's never lie on
// another mod's; the frame is made taller by OBVR's own rows, added to what
// stands. The rows' places are OBVR's own layout (LayOutOptionsRows below).
// The engine hands a click to the Options menu, which knows no such button;
// OBVR notices it itself: the hands' click with the cursor on the tile
// (camera/hands/ControlsOut.cpp), or the mouse's button going down while the
// tile is the interface's active one.
// Pure: one step a frame over plain values, game/OptionsMenuVr.cpp carries it
// out. Covered by options_vr_buttons_test.

#include <cstring>

#include "core/Types.h"

namespace obvr::ui {

inline constexpr const char* kVrOptionsTile = "obvr_vr_options";
inline constexpr const char* kVrGuideTile = "obvr_vr_guide";
// The console's lines (a path's backslash doubled for the C string only).
inline constexpr const char* kVrOptionsInsertLine = "InsertXML \"OBVR\\options_vr_options.xml@options_background\" 1015";
inline constexpr const char* kVrGuideInsertLine = "InsertXML \"OBVR\\options_vr_guide.xml@options_background\" 1015";
// The Options menu's own Return: the guide runs in front of the main menu, not of the Options menu.
inline constexpr const char* kOptionsReturnLine = "ClickMenuButton \"options_background\\options_return_button\" 1015";
// A button's row in the menu (button_floating_2.xml: height 64).
inline constexpr float kOptionsButtonHeight = 64.0f;

enum class OptionsVrTile : UInt8 { None, Options, Guide };

inline OptionsVrTile OptionsVrTileOf(const char* tileName) {
	if (tileName == nullptr) {
		return OptionsVrTile::None;
	}
	if (std::strcmp(tileName, kVrOptionsTile) == 0) {
		return OptionsVrTile::Options;
	}
	return std::strcmp(tileName, kVrGuideTile) == 0 ? OptionsVrTile::Guide : OptionsVrTile::None;
}

struct OptionsVrState {
	UInt32 seenFor = 0;     // the Options menu's tile that was looked at (0: none): asked into, or refused
	bool inserted = false;  // the buttons were asked into it
	bool mouseWas = false;
};

struct OptionsVrInput {
	UInt32 optionsTile = 0;     // the loaded Options menu's tile, 0 without one (or with the buttons switched off)
	bool frameFound = false;    // its frame and its Controls button are there by the game's names
	float frameHeight = 0.0f;   // the frame's height as it stands (its user1)
	bool optionsOnTop = false;  // the Options menu is the menu on top
	bool fromMainMenu = false;  // the main menu is loaded under it
	bool guideShowing = false;  // the start guide is up already
	bool settingsOpen = false;  // OBVR's own menu is up
	bool mouseDown = false;     // the mouse's left button, the game in front
	OptionsVrTile pointerOn = OptionsVrTile::None;    // the interface's active tile
	OptionsVrTile handClickOn = OptionsVrTile::None;  // a hand's click went out with the cursor on it
};

struct OptionsVrStep {
	bool insertOptions = false;  // ask MenuQue to put "VR Options" into the menu now
	bool insertGuide = false;    // and "VR Guide"
	float frameHeight = 0.0f;    // the frame's new height; 0: left as it is
	bool foreignLayout = false;  // this menu is not laid out as the game's: nothing put in (said once a menu)
	bool openSettings = false;   // VR Options clicked: OBVR's menu
	bool startGuide = false;     // VR Guide clicked: the menu's Return, then the start guide
};

inline OptionsVrStep StepOptionsVrButtons(OptionsVrState& s, const OptionsVrInput& in) {
	OptionsVrStep step;
	const bool mouseEdge = in.mouseDown && !s.mouseWas;
	s.mouseWas = in.mouseDown;
	if (in.optionsTile == 0) {
		s.seenFor = 0;  // the menu is gone: the next one gets its own buttons
		s.inserted = false;
		return step;
	}
	if (in.optionsTile != s.seenFor) {
		s.seenFor = in.optionsTile;
		s.inserted = in.frameFound;
		if (!in.frameFound) {
			step.foreignLayout = true;
			return step;
		}
		step.insertOptions = true;
		step.insertGuide = in.fromMainMenu;
		step.frameHeight = in.frameHeight + kOptionsButtonHeight * (in.fromMainMenu ? 2.0f : 1.0f);
		return step;  // nothing to click before they are there
	}
	if (!s.inserted || !in.optionsOnTop || in.settingsOpen) {
		return step;
	}
	const OptionsVrTile clicked =
		in.handClickOn != OptionsVrTile::None ? in.handClickOn : (mouseEdge ? in.pointerOn : OptionsVrTile::None);
	step.openSettings = clicked == OptionsVrTile::Options;
	step.startGuide = clicked == OptionsVrTile::Guide && in.fromMainMenu && !in.guideShowing;
	return step;
}

// --- Where the rows stand -------------------------------------------------
//
// Making the frame taller moves the game's rows: their top is the menu's own
// formula, (the frame's height - five rows) / 2, so every 64 units of height
// push them 32 down, and a value another mod wrote into a row is lost when
// the formula runs again (measured 2026-11-16, options-vr-sweep
// 20261116-152150: with LINK and OBVR's two rows the game's rows stood from
// 84, OBVR's first row on LINK's "Mods"; LINK alone, 20261116-152405: the
// frame 424, the rows from 20 - 20 under the last, as the game's own 360 and
// five rows have it). So OBVR lays the rows out itself, each frame the menu
// is up, and writes only what is off: the rows that are shown, in the game's
// order, end the same distance over the frame's end as before OBVR's came;
// OBVR's follow the last of them.

inline constexpr UInt32 kOptionsGameRows = 6;  // Return, Gameplay, Video, Audio, Controls, Downloads
inline constexpr const char* const kOptionsGameRowTiles[kOptionsGameRows] = {
	"options_return_button", "options_gameplay_button", "options_video_button",
	"options_audio_button",  "options_controls_button", "options_downloads_button"};
inline constexpr float kOptionsDefaultMargin = 20.0f;  // the game's: 360 high, five rows from 20

struct OptionsRows {
	float frameHeight = 0.0f;  // the frame's user1 as it stands
	float y[kOptionsGameRows] = {};
	bool visible[kOptionsGameRows] = {};
	UInt32 ownRows = 0;  // OBVR's rows in the menu: 1 (VR Options) or 2 (and VR Guide)
};

struct OptionsLayout {
	float y[kOptionsGameRows] = {};  // a hidden row's: as it was
	float ownY[2] = {};
};

// A row's visible trait: the menus' true is 2 and false 1; a row without the
// trait is shown; a string (LINK's script writes "&true;") counts by its text.
inline bool OptionsRowShown(bool hasTrait, float number, const char* text) {
	if (!hasTrait) {
		return true;
	}
	if (text != nullptr) {
		return std::strcmp(text, "&true;") == 0 || std::strcmp(text, "2") == 0;
	}
	return number == 2.0f;
}

// The space under the last row shown, read before OBVR's rows come: what the
// layout keeps. The game's 20 when the rows make no sense against the frame.
inline float OptionsBottomMargin(const OptionsRows& rows) {
	float bottom = -1.0f;
	for (UInt32 i = 0; i < kOptionsGameRows; ++i) {
		if (rows.visible[i] && rows.y[i] + kOptionsButtonHeight > bottom) {
			bottom = rows.y[i] + kOptionsButtonHeight;
		}
	}
	const float margin = rows.frameHeight - bottom;
	return bottom >= 0.0f && margin >= 0.0f && margin <= 2.0f * kOptionsButtonHeight ? margin : kOptionsDefaultMargin;
}

inline OptionsLayout LayOutOptionsRows(const OptionsRows& rows, float bottomMargin) {
	OptionsLayout layout;
	UInt32 shown = 0;
	for (UInt32 i = 0; i < kOptionsGameRows; ++i) {
		shown += rows.visible[i] ? 1u : 0u;
	}
	float at = rows.frameHeight - bottomMargin - kOptionsButtonHeight * static_cast<float>(shown + rows.ownRows);
	for (UInt32 i = 0; i < kOptionsGameRows; ++i) {
		layout.y[i] = rows.y[i];
		if (rows.visible[i]) {
			layout.y[i] = at;
			at += kOptionsButtonHeight;
		}
	}
	for (UInt32 i = 0; i < 2 && i < rows.ownRows; ++i) {
		layout.ownY[i] = at;
		at += kOptionsButtonHeight;
	}
	return layout;
}

}  // namespace obvr::ui

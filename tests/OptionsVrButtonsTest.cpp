#include <cstdio>
#include <cstring>

#include "ui/OptionsVrButtons.h"

using namespace obvr::ui;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// The Options menu up, opened from the main menu, the frame at the game's 360.
OptionsVrInput FromMain() {
	OptionsVrInput in;
	in.optionsTile = 0x1000;
	in.frameFound = true;
	in.frameHeight = 360.0f;
	in.optionsOnTop = true;
	in.fromMainMenu = true;
	return in;
}

// The same from the pause menu: no main menu under it, the frame 40 taller.
OptionsVrInput FromPause() {
	OptionsVrInput in = FromMain();
	in.fromMainMenu = false;
	in.frameHeight = 400.0f;
	return in;
}

OptionsVrState Inserted(const OptionsVrInput& in) {
	OptionsVrState s;
	StepOptionsVrButtons(s, in);
	return s;
}

void TestNames() {
	std::printf("The tiles and the lines\n");
	Check(OptionsVrTileOf("obvr_vr_options") == OptionsVrTile::Options, "the VR Options button's tile");
	Check(OptionsVrTileOf("obvr_vr_guide") == OptionsVrTile::Guide, "the VR Guide button's tile");
	Check(OptionsVrTileOf("options_controls_button") == OptionsVrTile::None && OptionsVrTileOf("") == OptionsVrTile::None &&
	          OptionsVrTileOf(nullptr) == OptionsVrTile::None && OptionsVrTileOf("obvr_vr") == OptionsVrTile::None &&
	          OptionsVrTileOf("obvr_vr_options_x") == OptionsVrTile::None,
	      "another tile, none, the old main menu button's name, a longer name: neither");
	Check(std::strcmp(kVrOptionsInsertLine, "InsertXML \"OBVR\\options_vr_options.xml@options_background\" 1015") == 0,
	      "VR Options: the prefab into the Options menu's frame (1015; the console's @ for the pipe)");
	Check(std::strcmp(kVrGuideInsertLine, "InsertXML \"OBVR\\options_vr_guide.xml@options_background\" 1015") == 0,
	      "VR Guide: its prefab into the same frame");
	Check(std::strcmp(kOptionsReturnLine, "ClickMenuButton \"options_background\\options_return_button\" 1015") == 0,
	      "the menu's own Return, clicked for the guide");
	Check(std::strlen(kVrOptionsInsertLine) < 95 && std::strlen(kVrGuideInsertLine) < 95 && std::strlen(kOptionsReturnLine) < 95,
	      "each short enough for the console line's queue");
}

void TestInsert() {
	std::printf("The buttons put into each Options menu once\n");
	OptionsVrState s;
	OptionsVrInput none;
	OptionsVrStep step = StepOptionsVrButtons(s, none);
	Check(!step.insertOptions && !step.insertGuide && step.frameHeight == 0.0f, "no Options menu: nothing");
	step = StepOptionsVrButtons(s, FromMain());
	Check(step.insertOptions && step.insertGuide, "from the main menu: both buttons");
	Check(step.frameHeight == 360.0f + 2.0f * kOptionsButtonHeight, "the frame two buttons taller than it was");
	step = StepOptionsVrButtons(s, FromMain());
	Check(!step.insertOptions && !step.insertGuide && step.frameHeight == 0.0f, "the same menu the next frame: not again");
	step = StepOptionsVrButtons(s, none);
	step = StepOptionsVrButtons(s, FromMain());
	Check(step.insertOptions && step.insertGuide, "closed and opened again (even at the same address): again");
	{
		OptionsVrState p;
		step = StepOptionsVrButtons(p, FromPause());
		Check(step.insertOptions && !step.insertGuide, "from the pause menu: VR Options alone");
		Check(step.frameHeight == 400.0f + kOptionsButtonHeight, "the frame one button taller");
	}
	{
		// LINK's "Mods" made the frame a button taller before OBVR came: OBVR adds to what stands.
		OptionsVrState p;
		OptionsVrInput in = FromMain();
		in.frameHeight = 424.0f;
		step = StepOptionsVrButtons(p, in);
		Check(step.frameHeight == 424.0f + 2.0f * kOptionsButtonHeight, "a frame another mod made taller: added to, not set back");
	}
	{
		OptionsVrState p;
		OptionsVrInput in = FromMain();
		in.frameFound = false;
		step = StepOptionsVrButtons(p, in);
		Check(!step.insertOptions && !step.insertGuide && step.frameHeight == 0.0f && step.foreignLayout,
		      "an Options menu without the game's frame and Controls button (a UI mod's): nothing put in, said once");
		step = StepOptionsVrButtons(p, in);
		Check(!step.foreignLayout && !step.insertOptions, "the next frame: not said again, still nothing");
		in.handClickOn = OptionsVrTile::Options;
		Check(!StepOptionsVrButtons(p, in).openSettings, "a tile of OBVR's name in such a menu is not OBVR's: no click taken");
	}
	{
		OptionsVrState p;
		OptionsVrInput another = FromMain();
		another.optionsTile = 0x2000;
		StepOptionsVrButtons(p, FromMain());
		step = StepOptionsVrButtons(p, another);
		Check(step.insertOptions, "another Options menu without a frame between: its own buttons");
	}
	{
		// The buttons come a frame or more after the ask: a click in the asking frame is no click on them.
		OptionsVrState p;
		OptionsVrInput in = FromMain();
		in.handClickOn = OptionsVrTile::Options;
		step = StepOptionsVrButtons(p, in);
		Check(step.insertOptions && !step.openSettings, "the frame the buttons are asked in: no click taken");
	}
}

void TestOptionsClick() {
	std::printf("VR Options clicked: OBVR's menu\n");
	{
		OptionsVrInput in = FromPause();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Options;
		const OptionsVrStep step = StepOptionsVrButtons(s, in);
		Check(step.openSettings && !step.startGuide, "a hand's click on it, in the game: OBVR's menu, no guide");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Options;
		Check(StepOptionsVrButtons(s, in).openSettings, "the same from the main menu");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.pointerOn = OptionsVrTile::Options;
		Check(!StepOptionsVrButtons(s, in).openSettings, "the pointer on it, no click: nothing");
		in.mouseDown = true;
		Check(StepOptionsVrButtons(s, in).openSettings, "the mouse button going down on it: OBVR's menu");
		Check(!StepOptionsVrButtons(s, in).openSettings, "the button held: once");
		in.mouseDown = false;
		StepOptionsVrButtons(s, in);
		in.mouseDown = true;
		Check(StepOptionsVrButtons(s, in).openSettings, "let go and down again: again");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.mouseDown = true;
		Check(!StepOptionsVrButtons(s, in).openSettings, "the mouse down somewhere else: nothing");
		in.pointerOn = OptionsVrTile::Options;
		Check(!StepOptionsVrButtons(s, in).openSettings, "dragged onto the button while held: no click");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Options;
		in.settingsOpen = true;
		Check(!StepOptionsVrButtons(s, in).openSettings, "OBVR's menu open already: not toggled shut by a click under it");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Options;
		in.optionsOnTop = false;
		Check(!StepOptionsVrButtons(s, in).openSettings, "another menu over the Options menu (Video, a message): nothing");
	}
	{
		OptionsVrState s = Inserted(FromMain());
		OptionsVrInput in;  // the menu gone
		in.handClickOn = OptionsVrTile::Options;
		Check(!StepOptionsVrButtons(s, in).openSettings, "no Options menu (or the buttons switched off): nothing");
	}
}

void TestGuideClick() {
	std::printf("VR Guide clicked: the start guide, the Options menu out of its way\n");
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Guide;
		const OptionsVrStep step = StepOptionsVrButtons(s, in);
		Check(step.startGuide && !step.openSettings, "a hand's click on it: the guide (the menu's Return with it), not OBVR's menu");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.pointerOn = OptionsVrTile::Guide;
		in.mouseDown = true;
		Check(StepOptionsVrButtons(s, in).startGuide, "the mouse's click on it: the guide");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Guide;
		in.guideShowing = true;
		Check(!StepOptionsVrButtons(s, in).startGuide, "the guide up already: not again");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Guide;
		in.settingsOpen = true;
		Check(!StepOptionsVrButtons(s, in).startGuide, "OBVR's menu over the Options menu: nothing");
	}
	{
		// The guide's button is never put into the pause menu's Options; a tile of that name there is not OBVR's.
		OptionsVrInput in = FromPause();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Guide;
		Check(!StepOptionsVrButtons(s, in).startGuide, "in the game (no main menu under it): no guide");
	}
	{
		OptionsVrInput in = FromMain();
		OptionsVrState s = Inserted(in);
		in.handClickOn = OptionsVrTile::Guide;
		in.optionsOnTop = false;
		Check(!StepOptionsVrButtons(s, in).startGuide, "another menu on top: nothing");
	}
}

// The game's five rows from `top`, the Downloads row after them, hidden unless `mods`.
OptionsRows Rows(float frameHeight, float top, bool mods, UInt32 own) {
	OptionsRows rows;
	rows.frameHeight = frameHeight;
	for (UInt32 i = 0; i < kOptionsGameRows; ++i) {
		rows.y[i] = top + kOptionsButtonHeight * static_cast<float>(i);
		rows.visible[i] = i < 5 || mods;
	}
	rows.ownRows = own;
	return rows;
}

void TestBottomMargin() {
	std::printf("The space under the last row, measured before OBVR's rows come\n");
	Check(OptionsBottomMargin(Rows(360.0f, 20.0f, false, 0)) == 20.0f, "the game's menu from the main menu: 360 high, five rows from 20 - 20 under them");
	Check(OptionsBottomMargin(Rows(400.0f, 60.0f, false, 0)) == 20.0f, "from the pause menu: 400 high, the rows from 60 - 20");
	Check(OptionsBottomMargin(Rows(424.0f, 20.0f, true, 0)) == 20.0f, "LINK's Mods row shown, the frame 424: 20 under the sixth row");
	Check(OptionsBottomMargin(Rows(360.0f, 400.0f, false, 0)) == kOptionsDefaultMargin,
	      "rows below the frame's end (a layout not understood): the game's 20");
	Check(OptionsBottomMargin(Rows(900.0f, 20.0f, false, 0)) == kOptionsDefaultMargin, "a frame far taller than its rows: the game's 20");
	OptionsRows none = Rows(360.0f, 20.0f, false, 0);
	for (bool& v : none.visible) {
		v = false;
	}
	Check(OptionsBottomMargin(none) == kOptionsDefaultMargin, "no row shown: the game's 20");
}

void TestLayout() {
	std::printf("Where the rows stand: the game's block moved up by OBVR's rows, OBVR's under the last one shown\n");
	{
		// The main menu's Options, OBVR's two rows: the frame 360 + 128; the engine's own formula has moved the
		// game's rows down by half of that (84).
		const OptionsLayout l = LayOutOptionsRows(Rows(488.0f, 84.0f, false, 2), 20.0f);
		Check(l.y[0] == 20.0f && l.y[4] == 276.0f, "the game's five rows from 20 again, 64 apart");
		Check(l.ownY[0] == 340.0f && l.ownY[1] == 404.0f, "VR Options under Controls, VR Guide under it");
		Check(488.0f - (l.ownY[1] + kOptionsButtonHeight) == 20.0f, "20 under the last row, as before");
		Check(l.y[5] == 84.0f + 5.0f * kOptionsButtonHeight, "the hidden Downloads row is left where it is");
	}
	{
		// With LINK: its Mods row shown, the frame 360 + 128 + 64.
		const OptionsLayout l = LayOutOptionsRows(Rows(552.0f, 84.0f, true, 2), 20.0f);
		Check(l.y[0] == 20.0f && l.y[5] == 340.0f, "the six rows from 20, Mods the sixth");
		Check(l.ownY[0] == 404.0f && l.ownY[1] == 468.0f, "VR Options under Mods, never on it; VR Guide under that");
	}
	{
		// The pause menu's Options: 400 + 64, one row of OBVR's, the rows start under the pause line.
		const OptionsLayout l = LayOutOptionsRows(Rows(464.0f, 92.0f, false, 1), 20.0f);
		Check(l.y[0] == 60.0f && l.ownY[0] == 60.0f + 5.0f * kOptionsButtonHeight, "the rows from 60 as the game has them, VR Options the sixth");
	}
	{
		// Rows that stand right already: the same values back (nothing to write).
		OptionsRows rows = Rows(552.0f, 20.0f, true, 2);
		const OptionsLayout l = LayOutOptionsRows(rows, 20.0f);
		bool same = true;
		for (UInt32 i = 0; i < kOptionsGameRows; ++i) {
			same = same && l.y[i] == rows.y[i];
		}
		Check(same, "rows already in place stay");
	}
	{
		// A row hidden in the middle takes no place.
		OptionsRows rows = Rows(488.0f, 84.0f, false, 2);
		rows.visible[2] = false;
		const OptionsLayout l = LayOutOptionsRows(rows, 20.0f);
		Check(l.y[1] + kOptionsButtonHeight == l.y[3] && l.y[2] == rows.y[2], "a hidden row in the middle: skipped, left alone");
		Check(l.ownY[0] == l.y[4] + kOptionsButtonHeight, "OBVR's first row under the last one shown");
	}
	std::printf("A row's visible trait\n");
	Check(OptionsRowShown(false, 0.0f, nullptr), "no visible trait: shown (the game's own rows have none)");
	Check(OptionsRowShown(true, 2.0f, nullptr) && !OptionsRowShown(true, 1.0f, nullptr) && !OptionsRowShown(true, 0.0f, nullptr),
	      "a number: 2 is the menus' true, 1 their false");
	Check(OptionsRowShown(true, 0.0f, "&true;") && OptionsRowShown(true, 0.0f, "2"), "a string (LINK writes one): true by its text");
	Check(!OptionsRowShown(true, 2.0f, "&false;") && !OptionsRowShown(true, 2.0f, "1") && !OptionsRowShown(true, 2.0f, ""),
	      "a string that says false, or nothing: hidden");
}

}  // namespace

int main() {
	TestBottomMargin();
	TestLayout();
	TestNames();
	TestInsert();
	TestOptionsClick();
	TestGuideClick();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

#pragma once

namespace obvr::game {

// Where the game itself shows the paused world tinted: its static menu
// background (bStaticMenuBackground) - the world drawn once into a texture
// (0x0040D160) and shown desaturated sepia from then on, while the menus
// stay up. OBVR's menu shade (render/MenuShade.h) stands in for that picture
// - on the held pair, and on the live pair once LiveMenuBackground has turned
// the snapshot off - so it falls only where the game's falls. It fell behind
// every menu frame the world was drawn for, the race menu among them, which
// the game draws live and in its colours: the character to be made, shown
// grey (Nadi, 2026-11-26: "beim racemenu am anfang ist das bild grau ...
// man muss den char ja sehen").
//
// Read in Oblivion.exe (objdump, 2026-11-26), the main loop at
// 0x0040D9D4-0x0040DB31, every frame, in this order: menu mode (0x00578F60);
// the interface manager's byte at +0x08 not 4 (xOBSE: currentMode; what 4
// stands for is unread); no conversation (0x0057AB70: the Dialog menu up with
// its byte at +0x64 set); none of the race menu (0x40C), the main menu
// (0x414), a Generic menu (0x3F3) or a message box (0x3E9) open (0x005790E0
// for each); the console closed (0x00579BC0, the byte at +0x31 of what
// 0x00585C10 returns - read as the console's because the console's print,
// 0x00579B60, asks the same byte before it prints; derived, not proven).
// With all of them and no snapshot kept, the snapshot is
// taken - unless the player stands in no cell (+0x40) or has no 3D (the
// virtual at +0x154): then 0x0040D160 returns before it and the next frame
// asks again. A kept snapshot is let go (0x0040DA78-0x0040DB31) when menu
// mode ends or the main menu, a conversation, the race menu or the console
// comes up, but not while the screen fader 2 is on (0x00572E70). So a message
// box or a Generic menu over the inventory keeps the inventory's tinted
// picture, and over the walking world shows the world as it is. Not mirrored:
// the drop after ten frames of an opaque fader 2 (0x0040D4D0, after a load),
// which falls where menu mode has ended.

struct MenuSnapshotFacts {
	bool menuMode = false;      // 0x00578F60
	bool modeFour = false;      // the interface manager's byte at +0x08 is 4
	bool conversation = false;  // 0x0057AB70
	bool raceMenu = false;      // 0x005790E0(0x40C, 0)
	bool mainMenu = false;      // 0x005790E0(0x414, 0)
	bool genericMenu = false;   // 0x005790E0(0x3F3, 0)
	bool messageBox = false;    // 0x005790E0(0x3E9, 0)
	bool console = false;       // 0x00579BC0
	bool worldReady = false;    // the player stands in a cell and has 3D
	bool faderTwo = false;      // 0x00572E70(2): the screen fader 2 is on
};

// The menus up want the game's snapshot (the first chain, 0x0040D9D4).
constexpr bool MenuSnapshotWanted(const MenuSnapshotFacts& f) {
	return f.menuMode && !f.modeFour && !f.conversation && !f.raceMenu && !f.mainMenu && !f.genericMenu &&
	       !f.messageBox && !f.console;
}

// A kept snapshot is let go (the second chain, 0x0040DA78).
constexpr bool MenuSnapshotLetGo(const MenuSnapshotFacts& f) {
	return (!f.menuMode || f.mainMenu || f.conversation || f.raceMenu || f.console) && !f.faderTwo;
}

// One frame: whether the game keeps its tinted snapshot after it.
constexpr bool StepMenuSnapshotKept(bool kept, const MenuSnapshotFacts& f) {
	if (MenuSnapshotWanted(f)) {
		return kept || f.worldReady;
	}
	return kept && !MenuSnapshotLetGo(f);
}

}  // namespace obvr::game

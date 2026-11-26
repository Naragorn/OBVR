// Checks the game's own rule for its tinted menu background
// (game/MenuSnapshotRule.h), read in Oblivion.exe's main loop: where it takes
// its snapshot of the paused world - shown desaturated sepia while the menus
// stay up - where it keeps it, and where it shows the world as it is. OBVR's
// menu shade stands in for that snapshot, so it falls only where the game's
// falls: not behind the race menu (Nadi, 2026-11-26: "beim racemenu am anfang
// ist das bild grau ... man muss den char ja sehen").

#include <cstdio>

#include "game/MenuSnapshotRule.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// A menu up, nothing the main loop checks for, the player in a cell with 3D.
MenuSnapshotFacts Menu() {
	MenuSnapshotFacts f;
	f.menuMode = true;
	f.worldReady = true;
	return f;
}

// No menu: the walking world.
MenuSnapshotFacts World() {
	MenuSnapshotFacts f;
	f.worldReady = true;
	return f;
}

// Frames of the same facts; the kept state after them.
bool Frames(bool kept, const MenuSnapshotFacts& f, int frames) {
	for (int i = 0; i < frames; ++i) {
		kept = StepMenuSnapshotKept(kept, f);
	}
	return kept;
}

struct FactCase {
	const char* what;
	bool MenuSnapshotFacts::*fact;
};

void TestWalkingWorld() {
	std::printf("No menu: no snapshot\n");
	Check(!MenuSnapshotWanted(World()), "the walking world wants none");
	Check(!StepMenuSnapshotKept(false, World()), "and keeps none");
	Check(!StepMenuSnapshotKept(true, World()), "a kept one is let go when the menus close");
	MenuSnapshotFacts fading = World();
	fading.faderTwo = true;
	Check(StepMenuSnapshotKept(true, fading), "but not while the screen fader 2 is on");
	Check(!StepMenuSnapshotKept(false, fading), "and none is taken then");
}

void TestPauseMenuTakesAndKeeps() {
	std::printf("A pause menu: the snapshot taken at once and kept\n");
	Check(MenuSnapshotWanted(Menu()), "the inventory wants one");
	Check(StepMenuSnapshotKept(false, Menu()), "taken at its first frame");
	Check(Frames(false, Menu(), 300), "kept while the menu stays up");
	MenuSnapshotFacts noWorld = Menu();
	noWorld.worldReady = false;
	Check(!Frames(false, noWorld, 30), "none taken while the player stands in no cell or has no 3D (0x0040D160 returns early)");
	Check(StepMenuSnapshotKept(false, Menu()), "taken once the player stands in the cell with 3D");
	Check(StepMenuSnapshotKept(true, noWorld), "a kept one stays when the 3D goes");
}

void TestEachCheckRefusesTheTake() {
	std::printf("Each of the main loop's checks keeps the snapshot from being taken\n");
	const FactCase cases[] = {
		{"the interface manager's mode 4", &MenuSnapshotFacts::modeFour},
		{"a conversation", &MenuSnapshotFacts::conversation},
		{"the race menu", &MenuSnapshotFacts::raceMenu},
		{"the main menu", &MenuSnapshotFacts::mainMenu},
		{"a Generic menu", &MenuSnapshotFacts::genericMenu},
		{"a message box", &MenuSnapshotFacts::messageBox},
		{"the console", &MenuSnapshotFacts::console},
	};
	for (const FactCase& c : cases) {
		MenuSnapshotFacts f = Menu();
		f.*c.fact = true;
		char what[160];
		std::snprintf(what, sizeof(what), "%s: none wanted, none taken in 60 frames", c.what);
		Check(!MenuSnapshotWanted(f) && !Frames(false, f, 60), what);
	}
}

void TestWhatLetsAKeptOneGo() {
	std::printf("A kept snapshot: what lets it go, what keeps it\n");
	struct Case {
		const char* what;
		bool MenuSnapshotFacts::*fact;
		bool letsGo;
	};
	const Case cases[] = {
		{"the race menu", &MenuSnapshotFacts::raceMenu, true},
		{"the main menu", &MenuSnapshotFacts::mainMenu, true},
		{"a conversation", &MenuSnapshotFacts::conversation, true},
		{"the console", &MenuSnapshotFacts::console, true},
		{"a message box over the menu", &MenuSnapshotFacts::messageBox, false},
		{"a Generic menu over the menu", &MenuSnapshotFacts::genericMenu, false},
		{"the interface manager's mode 4", &MenuSnapshotFacts::modeFour, false},
	};
	for (const Case& c : cases) {
		MenuSnapshotFacts f = Menu();
		f.*c.fact = true;
		char what[160];
		std::snprintf(what, sizeof(what), "%s: %s", c.what, c.letsGo ? "let go" : "kept");
		Check(StepMenuSnapshotKept(true, f) == !c.letsGo, what);
		f.faderTwo = true;
		std::snprintf(what, sizeof(what), "%s with the screen fader 2 on: kept", c.what);
		Check(StepMenuSnapshotKept(true, f), what);
	}
	MenuSnapshotFacts closed = Menu();
	closed.menuMode = false;
	Check(!StepMenuSnapshotKept(true, closed), "menu mode ends: let go");
}

void TestRaceMenuAtNewGame() {
	std::printf("The race menu at a new game: the world as it is (Nadi, 2026-11-26)\n");
	MenuSnapshotFacts race = Menu();
	race.raceMenu = true;
	Check(!Frames(false, race, 600), "never taken while the race menu is up");
	// A menu before it took one - the loading screen, once the player stood in
	// the cell: let go as soon as the screen fader 2 is off.
	const bool loaded = StepMenuSnapshotKept(false, Menu());
	race.faderTwo = true;
	const bool fading = Frames(loaded, race, 10);
	Check(loaded && fading, "one taken before it is kept while the screen fader 2 is on");
	race.faderTwo = false;
	Check(!StepMenuSnapshotKept(fading, race), "let go when the fader is off: the character in the world's colours");
	MenuSnapshotFacts box = race;
	box.messageBox = true;
	Check(!Frames(false, box, 60), "its message box (\"Are you sure you want to be ...?\") takes none either");
}

void TestMessageBoxes() {
	std::printf("A message box: the world as it is over the walking world, the tinted picture over a menu\n");
	MenuSnapshotFacts box = Menu();
	box.messageBox = true;
	Check(!Frames(false, box, 100), "over the walking world: never taken");
	const bool inventory = StepMenuSnapshotKept(false, Menu());
	const bool boxOver = Frames(inventory, box, 100);
	Check(inventory && boxOver, "over the inventory: the inventory's tinted picture stays");
	Check(StepMenuSnapshotKept(boxOver, Menu()), "and stays when the box closes");
}

}  // namespace

int main() {
	TestWalkingWorld();
	TestPauseMenuTakesAndKeeps();
	TestEachCheckRefusesTheTake();
	TestWhatLetsAKeptOneGo();
	TestRaceMenuAtNewGame();
	TestMessageBoxes();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

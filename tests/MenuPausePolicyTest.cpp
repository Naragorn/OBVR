// Checks the decision behind Render.UnpausedMenus: which menus keep the world
// running, that everything opened from a conversation still pauses, and
// that outside menu mode or with the option off the answer is vanilla's.

#include <cstdio>

#include "game/MenuPausePolicy.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestPlayerControls() {
	std::printf("The player's controls under a menu\n");
	Check(PlayerControlsRunUnderMenu(true, true, kMenuIdContainer), "the sticks walk under a container's menu: run");
	Check(!PlayerControlsRunUnderMenu(true, false, kMenuIdContainer), "the sticks still: gated as vanilla");
	Check(!PlayerControlsRunUnderMenu(true, true, kMenuIdInventory), "walking wanted under the inventory: gated");
	Check(!PlayerControlsRunUnderMenu(true, true, kMenuIdLockPick), "under the lock's minigame: gated");
	Check(!PlayerControlsRunUnderMenu(true, true, kMenuIdNone), "a stack that reads empty: gated");
	Check(PlayerControlsRunUnderMenu(false, false, kMenuIdNone), "outside menu mode: run, as they do");
	Check(PlayerControlsRunUnderMenu(false, true, kMenuIdContainer), "outside menu mode whatever is asked");
	Check(!PlayerControlsRunUnderMenu(true, true, kMenuIdContainer, true),
	      "a merchant's barter: gated as vanilla, the sticks do not walk off");
	Check(PlayerControlsRunUnderMenu(false, true, kMenuIdContainer, true), "outside menu mode a barter flag is moot");
}

void TestPlayerWheel() {
	std::printf("The mouse wheel the player's controls read\n");
	Check(!PlayerControlsReadAxis(true, kMouseAxisWheel), "under a menu the wheel is the menu's alone");
	Check(PlayerControlsReadAxis(false, kMouseAxisWheel), "outside menu mode the wheel zooms as vanilla");
	Check(PlayerControlsReadAxis(true, 1), "under a menu the mouse's x passes (OBVR sends none while walking)");
	Check(PlayerControlsReadAxis(true, 2), "under a menu the mouse's y passes");
	Check(PlayerControlsReadAxis(false, 1), "outside menu mode the x passes");
}

void TestWhichMenusRun() {
	std::printf("Which menus keep the world running\n");

	Check(MenuKeepsWorldRunning(kMenuIdBigFour), "the F1-F4 stack entry");
	Check(MenuKeepsWorldRunning(kMenuIdInventory), "inventory");
	Check(MenuKeepsWorldRunning(kMenuIdStats), "stats");
	Check(MenuKeepsWorldRunning(kMenuIdMagic), "magic");
	Check(MenuKeepsWorldRunning(kMenuIdMap), "map");
	Check(MenuKeepsWorldRunning(kMenuIdContainer), "a container");
	Check(MenuKeepsWorldRunning(kMenuIdBook), "a book");
	Check(MenuKeepsWorldRunning(kMenuIdQuantity), "the quantity popup");
	Check(MenuKeepsWorldRunning(kMenuIdMagicPopup), "the magic popup");

	Check(!MenuKeepsWorldRunning(kMenuIdDialog), "a dialogue pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdPersuasion), "persuasion pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdNegotiate), "haggling pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdRepair), "repair pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdPause), "the Esc menu pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdOptions), "options pause");
	Check(!MenuKeepsWorldRunning(kMenuIdLoading), "loading pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdMain), "the main menu pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdSleepWait), "sleeping and waiting pause");
	Check(!MenuKeepsWorldRunning(kMenuIdLockPick), "lockpicking pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdLevelUp), "level-up pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdMessage), "a message box pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdQuickKeys), "the quick keys wheel pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdNone), "an empty stack pauses");
}

void TestAnswer() {
	std::printf("The answer at a redirected site\n");

	Check(!WorldPausesForMenu(false, true, kMenuIdInventory),
	      "outside menu mode nothing pauses, whatever the stack holds");
	Check(!WorldPausesForMenu(false, false, kMenuIdNone), "and not with the option off either");
	Check(WorldPausesForMenu(true, false, kMenuIdInventory),
	      "with the option off a menu pauses as vanilla");
	Check(!WorldPausesForMenu(true, true, kMenuIdInventory),
	      "with the option on the inventory keeps the world running");
	Check(WorldPausesForMenu(true, true, kMenuIdDialog),
	      "and a dialogue still pauses it");
	Check(WorldPausesForMenu(true, true, kMenuIdNone),
	      "an unreadable stack pauses rather than guesses");

	std::printf("The container's own switch\n");
	Check(!WorldPausesForMenu(true, false, kMenuIdContainer, true),
	      "the container runs the world with the option off");
	Check(WorldPausesForMenu(true, false, kMenuIdInventory, true),
	      "... and the inventory still pauses as vanilla");
	Check(WorldPausesForMenu(true, false, kMenuIdContainer, false), "neither switch: the container pauses");
	Check(!WorldPausesForMenu(true, true, kMenuIdContainer, false), "the option alone runs it too");
	Check(!WorldPausesForMenu(false, false, kMenuIdContainer, true), "outside menu mode nothing pauses either way");

	std::printf("The lock's minigame with opening by reaching\n");
	Check(!WorldPausesForMenu(true, false, kMenuIdLockPick, false, true),
	      "the lockpicking runs the world with ReachOpens on");
	Check(WorldPausesForMenu(true, false, kMenuIdLockPick, true, false),
	      "... and pauses as vanilla with it off, the container's switch alone");
	Check(WorldPausesForMenu(true, true, kMenuIdLockPick, false, false),
	      "the unpaused menus leave the lockpicking paused (not one of the player's own screens)");
	Check(WorldPausesForMenu(true, false, kMenuIdInventory, false, true),
	      "the inventory is not the lock: paused as vanilla");
	Check(!WorldPausesForMenu(false, false, kMenuIdLockPick, false, true), "outside menu mode nothing pauses");

	std::printf("A merchant's barter pauses as vanilla\n");
	Check(WorldPausesForMenu(true, true, kMenuIdContainer, true, false, true),
	      "the barter's container menu pauses, both switches on");
	Check(WorldPausesForMenu(true, false, kMenuIdContainer, true, false, true),
	      "... and with the container's switch alone");
	Check(WorldPausesForMenu(true, true, kMenuIdQuantity, false, false, true),
	      "the barter's quantity box pauses too (the unpaused menus would run it)");
	Check(!WorldPausesForMenu(false, true, kMenuIdContainer, true, false, true),
	      "outside menu mode nothing pauses, barter or not");
	Check(!WorldPausesForMenu(true, true, kMenuIdContainer, true, false, false),
	      "a chest's container menu still runs the world");
}

void TestBarterInStack() {
	std::printf("Telling a merchant's barter from a chest by the menu stack\n");
	const UInt32 barter[] = {kMenuIdDialog, kMenuIdContainer};
	Check(BarterInMenuStack(barter, 2), "the dialogue under the container menu: a barter");
	const UInt32 quantity[] = {kMenuIdDialog, kMenuIdContainer, kMenuIdQuantity};
	Check(BarterInMenuStack(quantity, 3), "the quantity box over it: still the barter");
	const UInt32 chest[] = {kMenuIdContainer};
	Check(!BarterInMenuStack(chest, 1), "a chest, a body or a pickpocket: the container menu alone");
	const UInt32 talking[] = {kMenuIdDialog};
	Check(!BarterInMenuStack(talking, 1), "the dialogue alone: no barter");
	const UInt32 inventory[] = {kMenuIdBigFour, kMenuIdInventory};
	Check(!BarterInMenuStack(inventory, 2), "other menus: no barter");
	Check(!BarterInMenuStack(barter, 0), "an empty stack: no barter");
	Check(!BarterInMenuStack(nullptr, 2), "no stack read: no barter");
	Check(MenuStackHas(talking, 1, kMenuIdDialog), "the dialogue found in the stack");
	Check(!MenuStackHas(chest, 1, kMenuIdDialog), "... and not in a chest's");
	Check(!MenuStackHas(nullptr, 1, kMenuIdDialog), "no stack read: not found");
}

void TestForeignHookThunk() {
	std::printf("Recognising a foreign hook's IsMenuMode thunk\n");
	const UInt8 exact[] = {0xB8, 0x60, 0x8F, 0x57, 0x00, 0xFF, 0xE0};
	Check(IsAbsoluteJumpTo(exact, 0x00578F60), "the exact mov/jump thunk is recognised");
	for (UInt32 changed = 0; changed < sizeof(exact); ++changed) {
		UInt8 bytes[sizeof(exact)];
		for (UInt32 i = 0; i < sizeof(exact); ++i) bytes[i] = exact[i];
		bytes[changed] ^= 1;
		Check(!IsAbsoluteJumpTo(bytes, 0x00578F60), "every changed thunk byte is refused");
	}
	Check(!IsAbsoluteJumpTo(nullptr, 0x00578F60), "a null thunk is refused");
}

void TestStableMenuId() {
	std::printf("Stable menu identity during one menu episode\n");
	Check(StablePauseMenuId(true, kMenuIdBigFour, kMenuIdNone) == kMenuIdBigFour,
	      "a visible menu becomes the remembered menu");
	Check(StablePauseMenuId(true, kMenuIdNone, kMenuIdBigFour) == kMenuIdBigFour,
	      "a transient empty reading keeps the menu episode running");
	Check(StablePauseMenuId(true, kMenuIdDialog, kMenuIdBigFour) == kMenuIdDialog,
	      "a newly observed menu replaces the remembered one");
	Check(StablePauseMenuId(false, kMenuIdBigFour, kMenuIdBigFour) == kMenuIdNone,
	      "leaving menu mode clears the episode");
}

}  // namespace

int main() {
	TestWhichMenusRun();
	TestAnswer();
	TestBarterInStack();
	TestPlayerControls();
	TestPlayerWheel();
	TestForeignHookThunk();
	TestStableMenuId();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

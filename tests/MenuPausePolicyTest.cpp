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
	TestForeignHookThunk();
	TestStableMenuId();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

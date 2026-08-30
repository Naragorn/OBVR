#include <cstdio>
#include <cstring>

#include "game/MenuType.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// Every id this build claims to know, so the table can be walked rather than
// spot-checked. Adding an id to the header without a name here is exactly the
// mistake this catches.
struct Known {
	UInt32 id;
	const char* name;
};

constexpr Known kKnown[] = {
	{obvr::game::kMenuIdMessage, "Message"},
	{obvr::game::kMenuIdInventory, "Inventory"},
	{obvr::game::kMenuIdStats, "Stats"},
	{obvr::game::kMenuIdLoading, "Loading"},
	{obvr::game::kMenuIdContainer, "Container"},
	{obvr::game::kMenuIdDialog, "Dialog"},
	{obvr::game::kMenuIdGeneric, "Generic"},
	{obvr::game::kMenuIdSleepWait, "SleepWait"},
	{obvr::game::kMenuIdPause, "Pause"},
	{obvr::game::kMenuIdLockPick, "LockPick"},
	{obvr::game::kMenuIdOptions, "Options"},
	{obvr::game::kMenuIdMagic, "Magic"},
	{obvr::game::kMenuIdMap, "Map"},
	{obvr::game::kMenuIdNegotiate, "Negotiate"},
	{obvr::game::kMenuIdBook, "Book"},
	{obvr::game::kMenuIdLevelUp, "LevelUp"},
	{obvr::game::kMenuIdTraining, "Training"},
	{obvr::game::kMenuIdPersuasion, "Persuasion"},
	{obvr::game::kMenuIdRepair, "Repair"},
	{obvr::game::kMenuIdRaceSex, "RaceSex"},
	{obvr::game::kMenuIdLoad, "Load"},
	{obvr::game::kMenuIdSave, "Save"},
	{obvr::game::kMenuIdAlchemy, "Alchemy"},
	{obvr::game::kMenuIdMain, "Main"},
	{obvr::game::kMenuIdQuickKeys, "QuickKeys"},
	{obvr::game::kMenuIdCredits, "Credits"},
};

constexpr int kKnownCount = static_cast<int>(sizeof(kKnown) / sizeof(kKnown[0]));

}  // namespace

int main() {
	using obvr::game::MenuIdName;

	std::printf("The menu each id names\n");

	// Every named id gives back its own name. A table this long is written by
	// copying lines, and a copied line that kept the name above it would put
	// the wrong menu in a log somebody later reasons from.
	bool allNamed = true;
	bool allCorrect = true;
	for (int i = 0; i < kKnownCount; ++i) {
		if (std::strcmp(MenuIdName(kKnown[i].id), "unnamed") == 0) {
			allNamed = false;
			std::printf("        id 0x%03X has no name\n", kKnown[i].id);
		}
		if (std::strcmp(MenuIdName(kKnown[i].id), kKnown[i].name) != 0) {
			allCorrect = false;
			std::printf("        id 0x%03X is called %s, not %s\n", kKnown[i].id,
			            MenuIdName(kKnown[i].id), kKnown[i].name);
		}
	}
	Check(allNamed, "every id the header declares has a name");
	Check(allCorrect, "every id gives back its own name and not a neighbour's");

	// No two ids share a name, which is the other half of the copied-line
	// mistake: two entries naming the same menu read as one menu appearing
	// twice.
	bool allDistinct = true;
	for (int i = 0; i < kKnownCount; ++i) {
		for (int j = i + 1; j < kKnownCount; ++j) {
			if (std::strcmp(MenuIdName(kKnown[i].id), MenuIdName(kKnown[j].id)) == 0) {
				allDistinct = false;
				std::printf("        0x%03X and 0x%03X are both %s\n", kKnown[i].id,
				            kKnown[j].id, MenuIdName(kKnown[i].id));
			}
		}
	}
	Check(allDistinct, "no two ids answer to the same name");

	// Nothing read is its own answer. ActiveMenuId returns this when the
	// cursor is not over a menu, and it must not read as a menu.
	Check(std::strcmp(MenuIdName(obvr::game::kMenuIdNone), "none") == 0,
	      "an unread menu is called none, not a menu");

	// An id inside the range with no entry - the several unk ones, and the
	// menus OBVR has no reason to name.
	Check(std::strcmp(MenuIdName(0x407), "unnamed") == 0,
	      "an id with no entry is unnamed rather than mislabelled");

	// And one outside it entirely, which is what a wrong offset would produce.
	Check(std::strcmp(MenuIdName(0x12345678), "unnamed") == 0,
	      "a value that is no menu id at all is unnamed");

	// The persuasion minigame is why this file exists: it opens from inside a
	// dialogue, so only the type tells the two apart.
	Check(obvr::game::kMenuIdPersuasion != obvr::game::kMenuIdDialog,
	      "persuasion and dialogue are different menus");

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}

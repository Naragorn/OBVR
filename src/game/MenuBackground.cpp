#include "game/MenuBackground.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuSnapshotRule.h"
#include "game/MenuType.h"
#include "game/ScreenFaders.h"

namespace obvr::game {
namespace {

// The main loop's own checks for its snapshot (game/MenuSnapshotRule.h, read
// 2026-11-26); each answers in AL.
constexpr UInt32 kConversationUp = 0x0057AB70;  // cdecl ()
constexpr UInt32 kMenuOpen = 0x005790E0;        // cdecl (menu id, flags 0)
constexpr UInt32 kConsoleOpen = 0x00579BC0;     // cdecl ()
constexpr UInt32 kInterfaceModeOffset = 0x08;   // the byte the loop compares with 4
constexpr UInt32 kScreenFaderTwo = 2;

bool AskEngine(UInt32 function) { return reinterpret_cast<UInt8(__cdecl*)()>(function)() != 0; }

bool MenuOpen(UInt32 id) { return reinterpret_cast<UInt8(__cdecl*)(UInt32, UInt32)>(kMenuOpen)(id, 0) != 0; }

// 0x0040D160's own checks before it takes the snapshot: the player stands in
// a cell (+0x40, what 0x006ECC80 returns) and has 3D (the virtual at +0x154).
bool PlayerStandsInTheWorld() {
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(player) ||
	    *reinterpret_cast<const UInt32*>(player + addr::kRefParentCellOffset) == 0) {
		return false;
	}
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(player);
	const UInt32 nodeOf =
		mem::LooksLikeObjectAddress(vtable)
			? *reinterpret_cast<const UInt32*>(vtable + addr::kActorVtableGetNiNodeOffset)
			: 0;
	return mem::LooksLikeObjectAddress(nodeOf) &&
	       reinterpret_cast<void*(__thiscall*)(void*)>(nodeOf)(reinterpret_cast<void*>(player)) != nullptr;
}

MenuSnapshotFacts ReadMenuSnapshotFacts() {
	MenuSnapshotFacts f;
	f.faderTwo = ScreenFaderOn(kScreenFaderTwo);
	f.menuMode = IsMenuMode();
	if (!f.menuMode) {
		return f;  // the rest only counts with a menu up
	}
	const UInt32 manager = *reinterpret_cast<const UInt32*>(addr::kInterfaceManagerPointer);
	f.modeFour = mem::LooksLikeObjectAddress(manager) &&
	             *reinterpret_cast<const UInt8*>(manager + kInterfaceModeOffset) == 4;
	f.conversation = AskEngine(kConversationUp);
	f.raceMenu = MenuOpen(kMenuIdRaceSex);
	f.mainMenu = MenuOpen(kMenuIdMain);
	f.genericMenu = MenuOpen(kMenuIdGeneric);
	f.messageBox = MenuOpen(kMenuIdMessage);
	f.console = AskEngine(kConsoleOpen);
	f.worldReady = PlayerStandsInTheWorld();
	return f;
}

// Why the game shows the world as it is, for the log: the first check that
// says so, in the main loop's order.
const char* WhyNoSnapshot(const MenuSnapshotFacts& f) {
	if (f.modeFour) {
		return "the interface manager's mode 4";
	}
	if (f.conversation) {
		return "a conversation";
	}
	if (f.raceMenu) {
		return "the race menu";
	}
	if (f.mainMenu) {
		return "the main menu";
	}
	if (f.genericMenu) {
		return "a Generic menu over the world";
	}
	if (f.messageBox) {
		return "a message box over the world";
	}
	if (f.console) {
		return "the console";
	}
	return f.worldReady ? "kept by the screen fader 2" : "the player in no cell or without 3D";
}

// Whether the byte has been reported since the last change, so a setting left
// alone does not write a line every frame.
bool g_reported = false;
bool g_reportedValue = false;

// Whether OBVR is the one that cleared the byte, and what the game had in it
// before. Without this the feature could not be switched off again without
// also overriding a player who had set bStaticMenuBackground themselves: on
// the way back OBVR would write a 1 the game never asked for.
bool g_ourChange = false;
UInt8 g_valueBeforeUs = 1;

}  // namespace

void ApplyLiveMenuBackground(bool wantLive) {
	if (wantLive) {
		if (!g_ourChange) {
			g_valueBeforeUs = *reinterpret_cast<const UInt8*>(addr::kStaticMenuBackground);
			g_ourChange = true;
		}
		SetStaticMenuBackground(false);
		return;
	}

	// Not asked for, so the engine keeps whatever it had - and if OBVR changed
	// it earlier in this session, it gets its own value back rather than a
	// guessed default. Untouched otherwise: a player who set
	// bStaticMenuBackground in their own INI is entitled to it.
	if (g_ourChange) {
		g_ourChange = false;
		SetStaticMenuBackground(g_valueBeforeUs != 0);
	}
}

bool SetStaticMenuBackground(bool staticBackground) {
	auto* const flag = reinterpret_cast<UInt8*>(addr::kStaticMenuBackground);
	const UInt8 wanted = staticBackground ? 1 : 0;

	// Read first. The byte is written once during start-up and read only by
	// the snapshot guard, so a frame that changes nothing should touch
	// nothing - and a value that already matches needs no page protection
	// dance either.
	if (*flag != wanted) {
		if (!mem::SafeWrite(addr::kStaticMenuBackground, &wanted, sizeof(wanted))) {
			OBVR_LOG("Menu background: the static background byte at %08X could not be "
			         "written - menus keep whatever they had",
			         addr::kStaticMenuBackground);
			return false;
		}
	}

	if (!g_reported || g_reportedValue != staticBackground) {
		g_reported = true;
		g_reportedValue = staticBackground;
		OBVR_LOG("Menu background: the engine's static menu background is %s - the world "
		         "behind a menu is %s",
		         staticBackground ? "on" : "off",
		         staticBackground ? "one snapshot, held" : "rendered live, every frame");
	}
	return true;
}

bool MenuSnapshotIsValid() {
	return *reinterpret_cast<const UInt8*>(addr::kMenuSnapshotValid) != 0;
}

bool StepGameMenuSnapshot() {
	static bool s_kept = false;
	static UInt32 s_top = kMenuIdNone;
	static UInt32 s_lines = 40;
	const MenuSnapshotFacts f = ReadMenuSnapshotFacts();
	const bool kept = StepMenuSnapshotKept(s_kept, f);
	const UInt32 top = f.menuMode ? TopVisibleMenu() : kMenuIdNone;
	if (f.menuMode && (kept != s_kept || top != s_top) && s_lines > 0) {
		--s_lines;
		if (kept) {
			OBVR_LOG("Menu background: behind %s (0x%03X) the game shows its tinted snapshot - the menu shade may "
			         "stand in for it",
			         MenuIdName(top), top);
		} else {
			OBVR_LOG("Menu background: behind %s (0x%03X) the game shows the world as it is (%s) - no menu shade",
			         MenuIdName(top), top, WhyNoSnapshot(f));
		}
	}
	s_kept = kept;
	s_top = top;
	return kept;
}

}  // namespace obvr::game

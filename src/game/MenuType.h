#pragma once

#include "core/Types.h"

namespace obvr::game {

// Which menu Oblivion currently has open, rather than whether it has one.
//
// IsMenuMode answers a yes or no, and for most of OBVR that is the right
// question: the world is paused and the interface is what the wearer is
// looking at, whichever menu it is. But some menus are not that. The
// persuasion minigame is played by watching the NPC's face react, and a
// dialogue is watched as much as read - in both the world behind the menu is
// the thing being looked at, and freezing it takes the point away. Telling
// those apart from an inventory screen needs the type.
//
// The ids are Oblivion's own, counted from kMenuType_Message = 0x3E9 in
// xOBSE's GameMenus.h. The count is confirmed independently at one point:
// SleepWait lands on 0x3F4, which is the value measured from this binary
// during the menu background work, so the enum and this build agree.
enum : UInt32 {
	kMenuIdNone = 0,

	kMenuIdMessage = 0x3E9,
	kMenuIdInventory = 0x3EA,
	kMenuIdStats = 0x3EB,
	kMenuIdLoading = 0x3EF,
	kMenuIdContainer = 0x3F0,
	kMenuIdDialog = 0x3F1,
	kMenuIdGeneric = 0x3F3,
	kMenuIdSleepWait = 0x3F4,
	kMenuIdPause = 0x3F5,
	kMenuIdLockPick = 0x3F6,
	kMenuIdOptions = 0x3F7,
	kMenuIdMagic = 0x3FE,
	kMenuIdMap = 0x3FF,
	kMenuIdNegotiate = 0x401,
	kMenuIdBook = 0x402,
	kMenuIdLevelUp = 0x403,
	kMenuIdTraining = 0x404,
	kMenuIdPersuasion = 0x40A,
	kMenuIdRepair = 0x40B,
	kMenuIdRaceSex = 0x40C,
	kMenuIdLoad = 0x40E,
	kMenuIdSave = 0x40F,
	kMenuIdAlchemy = 0x410,
	kMenuIdMain = 0x414,
	kMenuIdQuickKeys = 0x416,
	kMenuIdCredits = 0x417,

	// The ends of the range, for the sanity check below.
	kMenuIdFirst = 0x3E9,
	kMenuIdLast = 0x41B,
};

// The values above were counted by hand along xOBSE's enum, which is exactly
// the kind of arithmetic that is wrong by one and looks right. These pin the
// count to its two fixed points: SleepWait is the eleventh entry and was
// measured at 0x3F4 in this binary during the menu background work, so a
// miscount anywhere before it breaks the build rather than mislabelling a log
// line. Persuasion is the thirty-third, and is the one this was written for.
static_assert(kMenuIdSleepWait == kMenuIdFirst + 11,
              "SleepWait is the twelfth menu id, and 0x3F4 in this build");
static_assert(kMenuIdDialog == kMenuIdFirst + 8, "Dialog is the ninth menu id");
static_assert(kMenuIdPersuasion == kMenuIdFirst + 33, "Persuasion is the thirty-fourth");
static_assert(kMenuIdMain == kMenuIdFirst + 43, "Main is the forty-fourth");

// The menu the cursor is over, or kMenuIdNone when it cannot be told.
//
// Read through the InterfaceManager singleton: activeMenu at +0x9C, and the
// menu's own id at +0x20. The neighbouring field is the second source rather
// than a second document - activeTile at +0x98 is already used by the cursor
// probe and has been right in the game for weeks, which places the pointer
// next to it - and the value is checked against the id range before it is
// believed, so a wrong offset reports nothing instead of a plausible number.
//
// "Cannot be told" is a real answer and not a failure. xOBSE's own
// GetActiveMenuMode records that activeMenu is null when a menu is being
// driven from the keyboard, since the field means "the menu the mouse is
// over". Anything reading this must treat 0 as no information, never as no
// menu - IsMenuMode is what answers that.
UInt32 ActiveMenuId();

// A name for the log, or "unnamed" for an id with no entry here. Pure, so the
// table can be checked without a game to read from.
const char* MenuIdName(UInt32 id);

}  // namespace obvr::game

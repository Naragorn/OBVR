#include "game/MenuType.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// Menu::activeMenu inside the InterfaceManager, from xOBSE's GameMenus.h; see
// the header for why the id it leads to is checked rather than trusted.
//
// The id's own offset moved to GameAddresses.h when the crosshair depth came
// to need it as well - one place to be wrong in rather than two that could
// drift apart.
constexpr UInt32 kActiveMenuOffset = 0x9C;

}  // namespace

bool InterfaceCursorRaw(float& x, float& y) {
	const auto* manager =
		*reinterpret_cast<const UInt8* const*>(addr::kInterfaceManagerPointer);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(manager))) {
		return false;
	}
	const float* pos = reinterpret_cast<const float*>(manager + addr::kInterfaceCursorPosOffset);
	x = pos[0];
	y = pos[1];
	return true;
}

bool InterfaceCursorPosition(float& x, float& y) {
	const auto* manager =
		*reinterpret_cast<const UInt8* const*>(addr::kInterfaceManagerPointer);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(manager))) {
		return false;
	}
	const float* pos = reinterpret_cast<const float*>(manager + addr::kInterfaceCursorPosOffset);
	// Finite and inside any screen the copy could describe; a NaN fails
	// every comparison and lands here too. A little above the top is
	// allowed: at the main menu the game parks the cursor at (0, -16),
	// measured 2026-09-07, and a laser that could not read it there could
	// not walk it onto the buttons.
	if (!(pos[0] >= -64.0f && pos[0] <= 16384.0f && pos[1] >= -64.0f && pos[1] <= 16384.0f)) {
		return false;
	}
	x = pos[0];
	y = pos[1];
	return true;
}

UInt32 ActiveMenuId() {
	auto* const manager = *reinterpret_cast<UInt8* const*>(addr::kInterfaceManagerPointer);
	if (manager == nullptr) {
		return kMenuIdNone;
	}

	auto* const menu = *reinterpret_cast<UInt8* const*>(manager + kActiveMenuOffset);
	if (menu == nullptr) {
		return kMenuIdNone;
	}

	const UInt32 id = *reinterpret_cast<const UInt32*>(menu + addr::kMenuIdOffset);

	// Out of range means the offsets are not what this build has, and a number
	// that is not a menu id is worse than none: it would name the wrong menu
	// in a log somebody later reasons from.
	if (id < kMenuIdFirst || id > kMenuIdLast) {
		return kMenuIdNone;
	}
	return id;
}

bool LoadingThreadActive() {
	return *reinterpret_cast<const UInt32*>(addr::kLoadingThreadHandle) != 0;
}

const char* MenuIdName(UInt32 id) {
	switch (id) {
	case kMenuIdNone: return "none";
	case kMenuIdBigFour: return "F1-F4 stack entry";
	case kMenuIdMessage: return "Message";
	case kMenuIdInventory: return "Inventory";
	case kMenuIdStats: return "Stats";
	case kMenuIdHudMain: return "HudMain";
	case kMenuIdHudInfo: return "HudInfo";
	case kMenuIdHudReticle: return "HudReticle";
	case kMenuIdLoading: return "Loading";
	case kMenuIdContainer: return "Container";
	case kMenuIdDialog: return "Dialog";
	case kMenuIdHudSubtitle: return "HudSubtitle";
	case kMenuIdGeneric: return "Generic";
	case kMenuIdSleepWait: return "SleepWait";
	case kMenuIdPause: return "Pause";
	case kMenuIdLockPick: return "LockPick";
	case kMenuIdOptions: return "Options";
	case kMenuIdQuantity: return "Quantity";
	case kMenuIdMagic: return "Magic";
	case kMenuIdMap: return "Map";
	case kMenuIdMagicPopup: return "MagicPopup";
	case kMenuIdNegotiate: return "Negotiate";
	case kMenuIdBook: return "Book";
	case kMenuIdLevelUp: return "LevelUp";
	case kMenuIdTraining: return "Training";
	case kMenuIdPersuasion: return "Persuasion";
	case kMenuIdRepair: return "Repair";
	case kMenuIdRaceSex: return "RaceSex";
	case kMenuIdLoad: return "Load";
	case kMenuIdSave: return "Save";
	case kMenuIdAlchemy: return "Alchemy";
	case kMenuIdMain: return "Main";
	case kMenuIdQuickKeys: return "QuickKeys";
	case kMenuIdCredits: return "Credits";
	default: return "unnamed";
	}
}

}  // namespace obvr::game

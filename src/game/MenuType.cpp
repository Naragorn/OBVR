#include "game/MenuType.h"

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

const char* MenuIdName(UInt32 id) {
	switch (id) {
	case kMenuIdNone: return "none";
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
	case kMenuIdMagic: return "Magic";
	case kMenuIdMap: return "Map";
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

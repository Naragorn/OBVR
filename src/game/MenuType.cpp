#include "game/MenuType.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "core/Log.h"

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
	x = *reinterpret_cast<const float*>(manager + addr::kInterfaceCursorXOffset);
	y = *reinterpret_cast<const float*>(manager + addr::kInterfaceCursorYOffset);
	return true;
}

bool InterfaceCursorPosition(float& x, float& y) {
	float readX = 0.0f;
	float readY = 0.0f;
	if (!InterfaceCursorRaw(readX, readY)) {
		return false;
	}
	// Finite and inside any screen the copy could describe; a NaN fails
	// every comparison and lands here too. The engine clamps these to the
	// screen itself for its hit test (see kInterfaceCursorXOffset), so a
	// little slack either side is only for a frame caught mid-update.
	if (!(readX >= -64.0f && readX <= 16384.0f && readY >= -64.0f && readY <= 16384.0f)) {
		return false;
	}
	x = readX;
	y = readY;
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


namespace {

// The cursor sprite OBVR hid, so it can be shown again - and only it.
UInt8* g_hiddenCursorNode = nullptr;
bool g_cursorHideReported = false;
bool g_cursorReshowReported = false;

constexpr UInt16 kHiddenBit = 0x0001;

UInt16& NodeFlags(UInt8* node) {
	return *reinterpret_cast<UInt16*>(node + addr::kNiFlagsOffset);
}

void ShowHiddenCursor() {
	if (g_hiddenCursorNode != nullptr &&
	    mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(g_hiddenCursorNode))) {
		NodeFlags(g_hiddenCursorNode) =
			static_cast<UInt16>(NodeFlags(g_hiddenCursorNode) & ~kHiddenBit);
	}
	g_hiddenCursorNode = nullptr;
}

// The cursor tile's render node: InterfaceManager+0x1C is the cursor tile,
// its node at kTileRenderNodeOffset - the pair the cursor probe read the
// sprite's position from during the hover-offset work.
UInt8* CursorNode() {
	auto* const manager = *reinterpret_cast<UInt8* const*>(addr::kInterfaceManagerPointer);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(manager))) {
		return nullptr;
	}
	auto* const tile = *reinterpret_cast<UInt8* const*>(manager + addr::kInterfaceCursorTileOffset);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(tile))) {
		return nullptr;
	}
	auto* const node = *reinterpret_cast<UInt8* const*>(tile + addr::kTileRenderNodeOffset);
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node)) ? node : nullptr;
}

}  // namespace

void SetMenuCursorHidden(bool hidden) {
	if (!hidden) {
		ShowHiddenCursor();
		return;
	}
	UInt8* const node = CursorNode();
	if (node != g_hiddenCursorNode) {
		ShowHiddenCursor();  // a different sprite now; the old one is let go
	} else if (node != nullptr && (NodeFlags(node) & kHiddenBit) == 0 && !g_cursorReshowReported) {
		g_cursorReshowReported = true;
		OBVR_LOG("Menu cursor: the engine showed the sprite again since the last frame - hidden "
		         "again every frame");
	}
	if (node == nullptr) {
		return;
	}
	NodeFlags(node) = static_cast<UInt16>(NodeFlags(node) | kHiddenBit);
	g_hiddenCursorNode = node;
	if (!g_cursorHideReported) {
		g_cursorHideReported = true;
		OBVR_LOG("Menu cursor: the sprite (node %08X) is hidden while a laser points",
		         reinterpret_cast<UInt32>(node));
	}
}

}  // namespace obvr::game

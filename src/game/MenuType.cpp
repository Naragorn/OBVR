#include "game/MenuType.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "core/Log.h"
#include "game/GameTypes.h"

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

// What OBVR hid of the cursor sprite, so exactly that is shown again: the
// sprite's node and the geometry directly under it. The node alone was not
// enough in game: the engine clears the node's hidden bit again every frame
// (the log's "the engine showed the sprite again") and, in game menus and
// the HUD, before the 2D pass draws it. The geometry under the node keeps
// what is written to it.
constexpr UInt32 kMaxHiddenCursorParts = 9;
UInt8* g_hiddenCursorParts[kMaxHiddenCursorParts]{};
UInt32 g_hiddenCursorCount = 0;
const UInt8* g_hiddenCursorNode = nullptr;
bool g_cursorHideWanted = false;
bool g_cursorHideReported = false;
bool g_cursorReshowReported = false;

constexpr UInt16 kHiddenBit = 0x0001;

UInt16& NodeFlags(UInt8* node) {
	return *reinterpret_cast<UInt16*>(node + addr::kNiFlagsOffset);
}

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

void ShowHiddenCursor() {
	for (UInt32 at = 0; at < g_hiddenCursorCount; ++at) {
		UInt8* const part = g_hiddenCursorParts[at];
		if (LooksLikeObject(part)) {
			NodeFlags(part) = static_cast<UInt16>(NodeFlags(part) & ~kHiddenBit);
		}
		g_hiddenCursorParts[at] = nullptr;
	}
	g_hiddenCursorCount = 0;
	g_hiddenCursorNode = nullptr;
}

void HidePart(UInt8* part) {
	if (!LooksLikeObject(part)) {
		return;
	}
	for (UInt32 at = 0; at < g_hiddenCursorCount; ++at) {
		if (g_hiddenCursorParts[at] == part) {
			NodeFlags(part) = static_cast<UInt16>(NodeFlags(part) | kHiddenBit);
			return;
		}
	}
	if (g_hiddenCursorCount >= kMaxHiddenCursorParts || (NodeFlags(part) & kHiddenBit) != 0) {
		return;  // hidden already by the engine: not OBVR's to show again later
	}
	NodeFlags(part) = static_cast<UInt16>(NodeFlags(part) | kHiddenBit);
	g_hiddenCursorParts[g_hiddenCursorCount++] = part;
}

// The cursor tile's render node: InterfaceManager+0x1C is the cursor tile,
// its node at kTileRenderNodeOffset - the pair the cursor probe read the
// sprite's position from during the hover-offset work.
UInt8* CursorNode() {
	auto* const manager = *reinterpret_cast<UInt8* const*>(addr::kInterfaceManagerPointer);
	if (!LooksLikeObject(manager)) {
		return nullptr;
	}
	auto* const tile = *reinterpret_cast<UInt8* const*>(manager + addr::kInterfaceCursorTileOffset);
	if (!LooksLikeObject(tile)) {
		return nullptr;
	}
	auto* const node = *reinterpret_cast<UInt8* const*>(tile + addr::kTileRenderNodeOffset);
	return LooksLikeObject(node) ? node : nullptr;
}

}  // namespace

void ReapplyMenuCursorHidden() {
	if (g_cursorHideWanted) {
		SetMenuCursorHidden(true);
	}
}

void SetMenuCursorHidden(bool hidden) {
	g_cursorHideWanted = hidden;
	if (!hidden) {
		ShowHiddenCursor();
		return;
	}
	UInt8* const node = CursorNode();
	if (node != g_hiddenCursorNode) {
		ShowHiddenCursor();  // a different sprite now; the old one is let go
	} else if (node != nullptr && (NodeFlags(node) & kHiddenBit) == 0 && !g_cursorReshowReported) {
		g_cursorReshowReported = true;
		OBVR_LOG("Menu cursor: the engine showed the sprite again since the last frame - its "
		         "geometry is hidden as well");
	}
	if (node == nullptr) {
		return;
	}
	g_hiddenCursorNode = node;
	HidePart(node);
	// And what hangs under it - a node's children, when the sprite is one.
	UInt8* const* const children =
		*reinterpret_cast<UInt8* const* const*>(node + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(node + addr::kNiChildCountOffset);
	if (LooksLikeObject(children) && count <= 64) {
		for (UInt16 at = 0; at < count; ++at) {
			// Only a real child: one whose parent is this node. If the sprite
			// is geometry rather than a node, these words are not a children
			// array, and nothing that does not point back is touched.
			UInt8* const child = children[at];
			if (LooksLikeObject(child) &&
			    reinterpret_cast<const UInt8*>(reinterpret_cast<NiAVObject*>(child)->parent) == node) {
				HidePart(child);
			}
		}
	}
	if (!g_cursorHideReported) {
		g_cursorHideReported = true;
		OBVR_LOG("Menu cursor: the sprite (node %08X, %u part(s)) is hidden while the "
		         "controllers point", reinterpret_cast<UInt32>(node), g_hiddenCursorCount);
	}
}

namespace {

// A tile's name, when it reads like one: printable, bounded.
const char* TileName(const UInt8* tile) {
	const char* const name = *reinterpret_cast<const char* const*>(tile + addr::kTileNameOffset);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(name))) {
		return nullptr;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		if (name[at] == '\0') {
			return at > 0 ? name : nullptr;
		}
		if (name[at] < 0x20 || name[at] > 0x7E) {
			return nullptr;
		}
	}
	return nullptr;
}

bool HasScroll(const char* name) {
	for (const char* at = name; *at != '\0'; ++at) {
		const char* a = at;
		const char* b = "scroll";
		while (*b != '\0' && *a != '\0' &&
		       ((*a >= 'A' && *a <= 'Z') ? *a + ('a' - 'A') : *a) == *b) {
			++a;
			++b;
		}
		if (*b == '\0') {
			return true;
		}
	}
	return false;
}

}  // namespace

bool CursorOverScrollBar(char* nameOut, UInt32 nameSize) {
	if (nameOut != nullptr && nameSize > 0) {
		nameOut[0] = '\0';
	}
	const auto* const manager =
		*reinterpret_cast<const UInt8* const*>(addr::kInterfaceManagerPointer);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(manager))) {
		return false;
	}
	const auto* tile =
		*reinterpret_cast<const UInt8* const*>(manager + addr::kInterfaceActiveTileOffset);
	// The tile itself and three above it: a scroll bar's marker sits inside
	// the bar, and either may be what the cursor is over.
	for (int level = 0; level < 4 && mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(tile));
	     ++level) {
		const char* const name = TileName(tile);
		if (level == 0 && name != nullptr && nameOut != nullptr && nameSize > 0) {
			UInt32 at = 0;
			for (; at + 1 < nameSize && name[at] != '\0'; ++at) {
				nameOut[at] = name[at];
			}
			nameOut[at] = '\0';
		}
		if (name != nullptr && HasScroll(name)) {
			return true;
		}
		tile = *reinterpret_cast<const UInt8* const*>(tile + addr::kTileParentOffset);
	}
	return false;
}

}  // namespace obvr::game

#include "game/HudTiles.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"

namespace obvr::game {

namespace {

// Tile's lists, from xOBSE's obse/GameTiles.h and NiTypes.h: the value list
// (an NiTListBase - vtable, start, end, count) at +0x14 and the child list at
// +0x30; each list node is {next, prev, data}. A value is {owning tile +0,
// number +4, string (BSStringT, characters first) +8, ..., id +0x18 (UInt16),
// "is a number" +0x1A}.
constexpr UInt32 kTileValueListStartOffset = 0x18;
constexpr UInt32 kTileChildListStartOffset = 0x34;
constexpr UInt32 kListNodeNextOffset = 0x00;
constexpr UInt32 kListNodeDataOffset = 0x08;
constexpr UInt32 kValueNumberOffset = 0x04;
constexpr UInt32 kValueStringOffset = 0x08;
constexpr UInt32 kValueIdOffset = 0x18;
constexpr UInt32 kValueIsNumberOffset = 0x1A;

// The trait ids (xOBSE's GameTiles.h table).
constexpr UInt16 kTraitVisible = 0x0FA1;
constexpr UInt16 kTraitLocus = 0x0FA6;
constexpr UInt16 kTraitAlpha = 0x0FA7;
constexpr UInt16 kTraitId = 0x0FA8;
constexpr UInt16 kTraitDepth = 0x0FAB;
constexpr UInt16 kTraitY = 0x0FAC;
constexpr UInt16 kTraitX = 0x0FAD;
constexpr UInt16 kTraitHeight = 0x0FCA;
constexpr UInt16 kTraitWidth = 0x0FCB;
constexpr UInt16 kTraitZoom = 0x0FD2;
constexpr UInt16 kTraitString = 0x0FDE;
constexpr UInt16 kTraitFilename = 0x0FE6;

constexpr UInt32 kMaxTiles = 1500;
constexpr UInt32 kMaxDepth = 24;
constexpr UInt32 kMaxListItems = 512;

bool LooksLikeObject(const void* p) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p));
}

template <typename T>
T Read(const UInt8* base, UInt32 offset) {
	return *reinterpret_cast<const T*>(base + offset);
}

// A printable, bounded string, or null. Characters need no alignment.
const char* ReadableText(const char* text, UInt32 limit) {
	const UInt32 address = reinterpret_cast<UInt32>(text);
	if (address < mem::kLowestObjectAddress || address > mem::kHighestObjectAddress) {
		return nullptr;
	}
	for (UInt32 at = 0; at < limit; ++at) {
		if (text[at] == '\0') {
			return text;
		}
		if (static_cast<unsigned char>(text[at]) < 0x20 || static_cast<unsigned char>(text[at]) > 0x7E) {
			return nullptr;
		}
	}
	return nullptr;
}

struct Traits {
	bool have[12] = {};
	float num[12] = {};
	const char* text[12] = {};
};

int TraitSlot(UInt16 id) {
	switch (id) {
	case kTraitX: return 0;
	case kTraitY: return 1;
	case kTraitWidth: return 2;
	case kTraitHeight: return 3;
	case kTraitVisible: return 4;
	case kTraitAlpha: return 5;
	case kTraitDepth: return 6;
	case kTraitLocus: return 7;
	case kTraitId: return 8;
	case kTraitZoom: return 9;
	case kTraitString: return 10;
	case kTraitFilename: return 11;
	default: return -1;
	}
}

Traits ReadTraits(const UInt8* tile) {
	Traits traits;
	const UInt8* node = Read<const UInt8*>(tile, kTileValueListStartOffset);
	for (UInt32 count = 0; LooksLikeObject(node) && count < kMaxListItems; ++count) {
		const UInt8* const value = Read<const UInt8*>(node, kListNodeDataOffset);
		if (LooksLikeObject(value)) {
			const int slot = TraitSlot(Read<UInt16>(value, kValueIdOffset));
			if (slot >= 0) {
				traits.have[slot] = true;
				traits.num[slot] = Read<float>(value, kValueNumberOffset);
				if (Read<UInt8>(value, kValueIsNumberOffset) == 0) {
					traits.text[slot] = ReadableText(Read<const char*>(value, kValueStringOffset), 96);
				}
			}
		}
		node = Read<const UInt8*>(node, kListNodeNextOffset);
	}
	return traits;
}

// One trait as text: its number, its string, or "-" when the tile has none.
void Trait(char* out, size_t size, const Traits& t, int slot) {
	if (!t.have[slot]) {
		std::snprintf(out, size, "-");
	} else if (t.text[slot] != nullptr) {
		std::snprintf(out, size, "\"%s\"", t.text[slot]);
	} else {
		std::snprintf(out, size, "%.1f", static_cast<double>(t.num[slot]));
	}
}

struct Walk {
	const char* label;
	UInt32 logged = 0;
	bool cut = false;
};

void LogTile(Walk& walk, const UInt8* tile, UInt32 depth) {
	if (walk.logged >= kMaxTiles) {
		walk.cut = true;
		return;
	}
	++walk.logged;
	const char* name = ReadableText(Read<const char*>(tile, addr::kTileNameOffset), 64);
	const Traits t = ReadTraits(tile);
	char x[32], y[32], w[32], h[32], vis[32], alpha[32], dep[32], locus[32], id[32], zoom[32];
	char str[112], file[112];
	Trait(x, sizeof(x), t, 0);
	Trait(y, sizeof(y), t, 1);
	Trait(w, sizeof(w), t, 2);
	Trait(h, sizeof(h), t, 3);
	Trait(vis, sizeof(vis), t, 4);
	Trait(alpha, sizeof(alpha), t, 5);
	Trait(dep, sizeof(dep), t, 6);
	Trait(locus, sizeof(locus), t, 7);
	Trait(id, sizeof(id), t, 8);
	Trait(zoom, sizeof(zoom), t, 9);
	Trait(str, sizeof(str), t, 10);
	Trait(file, sizeof(file), t, 11);

	// Where the engine placed its render node: local and world translation.
	const UInt8* const node = Read<const UInt8*>(tile, addr::kTileRenderNodeOffset);
	char placed[96] = "no node";
	if (LooksLikeObject(node)) {
		const float* const local = reinterpret_cast<const float*>(node + addr::kNiTranslateOffset);
		const float* const world = reinterpret_cast<const float*>(node + addr::kNodeWorldTranslateOffset);
		std::snprintf(placed, sizeof(placed), "node local (%.1f %.1f %.1f) world (%.1f %.1f %.1f)",
		              static_cast<double>(local[0]), static_cast<double>(local[1]),
		              static_cast<double>(local[2]), static_cast<double>(world[0]),
		              static_cast<double>(world[1]), static_cast<double>(world[2]));
	}
	OBVR_LOG("HudTiles: %s %u %*s%s [vt %08X] x %s y %s w %s h %s visible %s alpha %s depth %s locus %s "
	         "id %s zoom %s string %s file %s - %s",
	         walk.label, depth, static_cast<int>(depth * 2), "", name != nullptr ? name : "(no name)",
	         Read<UInt32>(tile, 0), x, y, w, h, vis, alpha, dep, locus, id, zoom, str, file, placed);

	if (depth >= kMaxDepth) {
		walk.cut = true;
		return;
	}
	const UInt8* child = Read<const UInt8*>(tile, kTileChildListStartOffset);
	for (UInt32 count = 0; LooksLikeObject(child) && count < kMaxListItems; ++count) {
		const UInt8* const data = Read<const UInt8*>(child, kListNodeDataOffset);
		// Only a real child: one that names this tile as its parent.
		if (LooksLikeObject(data) && Read<const UInt8*>(data, addr::kTileParentOffset) == tile) {
			LogTile(walk, data, depth + 1);
		}
		child = Read<const UInt8*>(child, kListNodeNextOffset);
	}
}

}  // namespace

UInt32 LogHudTileTree(UInt32 menuId, const char* label) {
	const auto* const data = *reinterpret_cast<const UInt8* const* const*>(addr::kTileMenuArrayData);
	if (!LooksLikeObject(data) || menuId < kMenuIdFirst) {
		OBVR_LOG("HudTiles: %s - the tile menu array is not readable", label);
		return 0;
	}
	const UInt32 count = *reinterpret_cast<const UInt16*>(addr::kTileMenuArrayCount);
	const UInt32 index = menuId - kMenuIdFirst;
	if (index >= count || !LooksLikeObject(data[index])) {
		OBVR_LOG("HudTiles: %s - menu %u is not loaded", label, menuId);
		return 0;
	}
	Walk walk;
	walk.label = label;
	LogTile(walk, data[index], 0);
	OBVR_LOG("HudTiles: %s - %u tile(s)%s", label, walk.logged, walk.cut ? ", the walk was cut short" : "");
	return walk.logged;
}

}  // namespace obvr::game

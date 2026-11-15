#pragma once

// The HUD's tile tree as OBVR reads it (game/HudTiles.cpp) and the
// arithmetic on it: a tile's place and rectangle in HUD units, by the
// engine's own rule for the locus trait. Split out of vr/HandHud.h, which
// uses it for the HUD on the hands. Pure; covered by hand_hud_test.

#include "core/Types.h"

namespace obvr::vr {

// Oblivion lays the HUD out in a space 960 units high; the width is 960
// times the aspect (measured, spec section 8).
inline constexpr float kHudUiHeightUnits = 960.0f;

inline constexpr UInt32 kHudTileNameSize = 40;
inline constexpr UInt32 kHudTilesMax = 192;

// One tile as read: its name, its parent's index (-1 for the root), and the
// traits that place it - a missing x or y counts as 0, a missing width or
// height means the tile has no size of its own.
struct HudTile {
	char name[kHudTileNameSize] = {};
	SInt32 parent = -1;
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	bool hasSize = false;
	// Its alpha trait; 255 when it has none.
	float alpha = 255.0f;
	// Its justify trait: 1 left (x is its left edge), 2 centre (x is its
	// middle), 4 right (x is its right edge) - the text tiles'.
	UInt32 justify = 1;
	// Its locus trait set (&true;): its children are placed from it. A tile
	// without one keeps its own x and y in its geometry and its node stays
	// at its parent's place (the probe, 2026-09-29: magic_icons at x 1642.5,
	// its node at the menu's origin), so its children are placed from its
	// parent instead.
	bool locus = true;
};
inline constexpr UInt32 kHudJustifyCentre = 2;
inline constexpr UInt32 kHudJustifyRight = 4;

struct UiRect {
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	bool valid = false;
};

inline bool SameTileName(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	for (;; ++a, ++b) {
		const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + ('a' - 'A')) : *a;
		const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b + ('a' - 'A')) : *b;
		if (ca != cb) {
			return false;
		}
		if (ca == '\0') {
			return true;
		}
	}
}

// The first tile of that name, any case, or -1.
inline SInt32 FindHudTile(const HudTile* tiles, UInt32 count, const char* name) {
	for (UInt32 i = 0; i < count; ++i) {
		if (SameTileName(tiles[i].name, name)) {
			return static_cast<SInt32>(i);
		}
	}
	return -1;
}

// A tile's top left in HUD units: its own x and y plus every parent's that
// has a locus - a parent without one passes on its own parent's place, not
// its offset (vanilla's effect icons under magic_icons). False on a broken
// chain (an index out of range, or deeper than any real menu).
inline bool HudTileOrigin(const HudTile* tiles, UInt32 count, SInt32 index, float& x, float& y) {
	x = 0.0f;
	y = 0.0f;
	for (UInt32 depth = 0; index >= 0; ++depth) {
		if (static_cast<UInt32>(index) >= count || depth > 32) {
			return false;
		}
		if (depth == 0 || tiles[index].locus) {
			x += tiles[index].x;
			y += tiles[index].y;
		}
		index = tiles[index].parent;
	}
	return true;
}

// A tile's rectangle, or invalid when it has no size of its own.
inline UiRect HudTileRect(const HudTile* tiles, UInt32 count, SInt32 index) {
	UiRect r;
	if (index < 0 || static_cast<UInt32>(index) >= count) {
		return r;
	}
	const HudTile& t = tiles[index];
	if (!t.hasSize || !(t.width > 0.0f) || !(t.height > 0.0f)) {
		return r;
	}
	float x = 0.0f;
	float y = 0.0f;
	if (!HudTileOrigin(tiles, count, index, x, y)) {
		return r;
	}
	if (t.justify == kHudJustifyCentre) {
		x -= t.width * 0.5f;
	} else if (t.justify == kHudJustifyRight) {
		x -= t.width;
	}
	r.left = x;
	r.top = y;
	r.right = x + t.width;
	r.bottom = y + t.height;
	r.valid = true;
	return r;
}

inline UiRect UnionRect(const UiRect& a, const UiRect& b) {
	if (!a.valid) {
		return b;
	}
	if (!b.valid) {
		return a;
	}
	UiRect r;
	r.left = a.left < b.left ? a.left : b.left;
	r.top = a.top < b.top ? a.top : b.top;
	r.right = a.right > b.right ? a.right : b.right;
	r.bottom = a.bottom > b.bottom ? a.bottom : b.bottom;
	r.valid = true;
	return r;
}

// Whether tile `index` lies under tile `ancestor`.
inline bool HudTileUnder(const HudTile* tiles, UInt32 count, SInt32 index, SInt32 ancestor) {
	for (UInt32 depth = 0; index >= 0 && static_cast<UInt32>(index) < count && depth <= 32; ++depth) {
		index = tiles[index].parent;
		if (index == ancestor) {
			return true;
		}
	}
	return false;
}

}  // namespace obvr::vr

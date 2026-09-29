#pragma once

// The HUD's tile tree, read (docs/hud-on-hands-spec.md, build step 1).
//
// Oblivion builds every menu, the HUD included, from XML into a tree of
// tiles. Each tile carries named traits - x, y, width, height, visible,
// alpha - and its children. The HUD on the hands cuts single elements out of
// the captured interface picture, and the tree is where their rectangles
// come from, whatever UI mod laid them out.
//
// This is the probe: it logs the tree so the element names and the units
// their rectangles are in are known before anything relies on them.

#include "core/Types.h"
#include "vr/HandHud.h"

namespace obvr::game {

// Logs the tile tree of one menu from the tile menu array, as "HudTiles:"
// lines, one per tile: its depth, name, and the traits that place it. Reads
// only. Returns the number of tiles logged, 0 when the menu is not loaded.
UInt32 LogHudTileTree(UInt32 menuId, const char* label);

// Reads one menu's tile tree into `out`, depth first, each tile with its
// parent's index - the snapshot vr::HudElementRect works on. A tile hidden
// by its own visible trait counts as having no size. Returns the number of
// tiles read, 0 when the menu is not loaded; stops at `capacity`.
UInt32 ReadHudTiles(UInt32 menuId, vr::HudTile* out, UInt32 capacity);

}  // namespace obvr::game

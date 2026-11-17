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
#include "vr/TabletLogic.h"

namespace obvr::game {

// Logs the tile tree of one menu from the tile menu array, as "HudTiles:"
// lines, one per tile: its depth, name, and the traits that place it. Reads
// only. Returns the number of tiles logged, 0 when the menu is not loaded.
UInt32 LogHudTileTree(UInt32 menuId, const char* label);

// Logs the names of the menu roots under InterfaceManager::menuRoot, and the
// whole tree of HUDSubtitleMenu when it hangs there - the subtitles and the
// top-left notices live in that menu (hud_subtitle_menu.xml:
// hudsubtitle_text, hudsubtitle_notice, hudsubtitle_icon), which the tile
// menu array does not carry.
UInt32 LogMenuRootTiles();

// Reads the tree of the menu root with this name under
// InterfaceManager::menuRoot into `out`, as ReadHudTiles does. 0 when no
// such menu hangs there.
UInt32 ReadMenuRootTiles(const char* menuName, vr::HudTile* out, UInt32 capacity);

// Reads one menu's tile tree into `out`, depth first, each tile with its
// parent's index - the snapshot vr::HudElementRect works on. A tile hidden
// by its own visible trait counts as having no size. Returns the number of
// tiles read, 0 when the menu is not loaded; stops at `capacity`.
UInt32 ReadHudTiles(UInt32 menuId, vr::HudTile* out, UInt32 capacity);

// The pictures of one menu for the tablet's rectangle (vr/TabletLogic.h,
// PaperOf): every tile down to the third level under the root that draws a
// file, has a size and is not clear, with no hidden tile above it - a menu
// whose root is hidden (a page of the F1-F4 menus that is not the one shown)
// gives none. Rectangles in HUD units. Returns how many were read, 0 when
// the menu is not loaded; stops at `capacity`.
UInt32 ReadMenuPaper(UInt32 menuId, vr::PaperTile* out, UInt32 capacity);

// The root tile of a loaded menu from the tile menu array; nullptr when it
// is not loaded.
const void* MenuRootTile(UInt32 menuId);

// The first tile of this name under `root`, depth first, `root` itself
// included; nullptr without one.
const void* FindTileNamed(const void* root, const char* name);

// One trait of a tile as a number (xOBSE's GameTiles.h ids: visible 0x0FA1,
// y 0x0FAC, user0 0x0FAE, user1 0x0FAF, height 0x0FCA). False when the tile
// has no such trait or holds a string there.
bool ReadTileNumber(const void* tile, UInt16 trait, float& out);

// The same with a string told apart: false when the tile has no such trait;
// `text` is the string it holds there (valid while the tile lives), nullptr
// for a number, which is `number`.
bool ReadTileTrait(const void* tile, UInt16 trait, float& number, const char*& text);

// The same for the tree under a tile pointer of one's own - the persistent
// HUD roots (GameAddresses.h, kHudAuxRootPointer: the enemy health bar's).
// 0 when the pointer is no object.
UInt32 ReadTilesUnderRoot(const void* root, vr::HudTile* out, UInt32 capacity);

// The sneak eye's alpha, 0..255: the HUDReticle tile "hudreticle_sneak"
// (hud_reticle.xml) - 0 standing, 50 sneaking unseen, up to 255 seen (the
// harness's tile tree, 2026-10-28). False when the tile is not found.
bool ReadSneakEyeAlpha(float& alpha);

}  // namespace obvr::game

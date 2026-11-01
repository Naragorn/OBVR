#pragma once

// The two rings at the hands (docs/controls-spec.md 4.4, 4.5, 4.44):
// - the quick menu on the weapon hand (vr/QuickMenu.h): the ring of the
//   eight hotkeys where the hand is, the hand moved towards one, the
//   trackpad let go - and that hotkey's number key is tapped; on the
//   inventory or the magic menu it sets a hotkey instead;
// - the spell ring on the other hand (vr/SpellRing.h, game/SpellBook.h): the
//   same ring with the player's spellbook on it, opened latched by that
//   stick's short click, the next click making the chosen spell the active
//   one; the other trigger turns its pages of eight.
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.2): the code as it was, its file globals now RingsState's
// members under the same names. Harness: quick-menu*.txt, spell-ring*.txt.

#include "camera/hands/HandStep.h"
#include "game/QuickKeys.h"
#include "ui/CanvasOverlay.h"
#include "ui/QuickMenuPainter.h"
#include "vr/QuickMenu.h"
#include "vr/SpellRing.h"

namespace obvr::camera::hands {

struct RingsState {
	vr::QuickMenuState quickMenu;
	ui::CanvasOverlay quickMenuLayer{"obvr.quickmenu", "OBVR Quick Menu", ui::kQuickMenuCanvas, ui::kQuickMenuCanvas};
	ui::QuickMenuView quickMenuView;
	// Each slot's icon path, the way the view's icon was asked for.
	char quickMenuIconPaths[game::kQuickKeyCount][128] = {};
	// The weapon hand's trigger as read, and whether the ring keeps it.
	bool quickMenuTrigger = false;
	bool quickMenuKeepsTrigger = false;
	UInt32 quickMenuRevision = 1;
	vr::openvr::HmdMatrix34 quickMenuPose{};
	UInt32 quickMenuLinesLeft = 30;
	vr::MenuHapticState ringHaptic;

	// The spell ring: the same state machine, its own state.
	vr::QuickMenuState spellRing;
	ui::CanvasOverlay spellRingLayer{"obvr.spellring", "OBVR Spell Ring", ui::kQuickMenuCanvas, ui::kQuickMenuCanvas};
	ui::QuickMenuView spellRingView;
	char spellRingIconPaths[vr::kSpellRingSlots][128] = {};
	vr::SpellBook spellBook;
	int spellRingPage = 0;
	// The other hand's trigger as read, and whether the ring keeps it.
	bool spellRingTrigger = false;
	bool spellRingKeepsTrigger = false;
	UInt32 spellRingRevision = 1;
	vr::openvr::HmdMatrix34 spellRingPose{};
	UInt32 spellRingLinesLeft = 30;
	vr::MenuHapticState spellRingHaptic;
};

// The quick menu's frame. `allowed`: the mode runs, no settings panel, the
// spell ring shut, and in the world with no menu or on a menu that assigns;
// `assign`: the cursor is on the inventory or the magic menu, where the ring
// sets a hotkey to what the cursor is on (docs/controls-spec.md 4.5).
void UpdateQuickMenu(RingsState& state, const HandStep& step, const HandShared& shared, bool allowed, bool assign,
                     const vr::HandModeFrame& frame);

// The spell ring's frame. `allowed`: the mode runs, in the world, no menu
// up, the quick menu shut.
void UpdateSpellRing(RingsState& state, const HandStep& step, const HandShared& shared, bool allowed,
                     const vr::HandModeFrame& frame);

}  // namespace obvr::camera::hands

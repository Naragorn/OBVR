#pragma once

#include "core/Types.h"

namespace obvr::game {

// Turns Oblivion's static menu background off, so the engine renders the
// world live behind menus instead of blitting one snapshot of it.
//
// This is the engine's own mechanism, not a patch. Oblivion renders the world
// into a texture once when a menu opens and shows that from then on; the byte
// this writes is the copy of its bStaticMenuBackground display setting, and
// clearing it means the snapshot is never taken, so the guard that skips the
// world render falls through every frame. See kStaticMenuBackground for the
// call sites this rests on.
//
// What it does NOT do is unpause the game. Every pause in the update step is
// guarded by its own IsMenuMode call rather than by this byte, so the world
// stays frozen exactly as vanilla freezes it and only the picture comes
// alive. That separation is the reason this is worth doing at all - the
// alternative, an OBVR-driven render, would have to reproduce a great deal of
// what the engine is willing to do here by itself.
//
// The copy is written rather than the setting at 0x00B06DC4, deliberately:
// settings are the things Oblivion writes back to the user's INI, and a value
// the game persists outlives the session - which is the shape that made
// patching iSize dangerous. Nothing persists this copy.
//
// Returns whether the byte now holds the wanted value. Safe to call every
// frame: it reads first and only writes on a change, so a hot-reloaded INI
// can turn it on and off mid-session.
bool SetStaticMenuBackground(bool staticBackground);

// Whether the engine currently holds a valid world snapshot for the menu.
// Read-only, and only interesting as evidence: with the static background
// off this should never become true, which is what makes a run's log show
// that the live path is the one being taken.
bool MenuSnapshotIsValid();

}  // namespace obvr::game

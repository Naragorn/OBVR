#pragma once

#include "core/Types.h"

namespace obvr::game {

// The key names in the game's texts, in Full VR's words (ui/ControlPhrase.h;
// docs/onboarding-spec.md 3.4; the tester, 2026-10-16: the new game's
// tutorial cards are to say the VR controls, without an ESP).
//
// A text the font lays out may carry "&sUActnForward;" and its kin - the
// Tutorial quest's stage entries in Oblivion.esm do, in every language:
// "To move forward, &sUActnForward;, &sUActnBack; to move back ...". The
// layout routine (0x00575B40) finds the name among the 29 control-name
// settings (0xB399D0, in the control map's order) and calls
// addr::kControlKeyName with the control's index and a 1024-byte buffer
// for the words to splice in, which reads the control map and formats
// "press W" - or the unbound string. That entry is detoured: with the mode
// on, OBVR's phrase for the control ("push the left stick forward");
// otherwise, and for a control OBVR has no words for, the engine's own
// answer. Nothing in the esm, the quest or the save is touched, so a mod
// that rewrites the tutorial gets the same words.

// Detours the formatter's entry; logs its outcome. False when the bytes are
// not the ones read (another mod there) - then the texts keep the key names.
bool InstallControlWordsHook();
bool ControlWordsHookInstalled();

// Whether OBVR's words are wanted this frame (the mode on), and which hand
// holds the weapon.
void SetControlWordsWanted(bool wanted, bool leftHanded);

}  // namespace obvr::game

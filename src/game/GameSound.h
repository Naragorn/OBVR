#pragma once

#include "core/Types.h"

namespace obvr::game {

// Playing the game's own sounds (Oblivion.exe 1.2.0.416, read 2026-09-29).
//
// - A sound form, not placed (2D), the way the script command PlaySound
//   does it (its handler around 0x005094DA): the sound system is
//   OSGlobals (0x00B33398) +0x24; 0x006AE0A0 thiscall(system, formID,
//   0x101, 0) makes the playing sound, 0x006B7190 thiscall(sound, 0) starts
//   it, 0x006B73E0 thiscall(sound) hands it over, and the game's free
//   (0x00401F20, cdecl) lets go of the wrapper. Oblivion Reloaded's
//   SoundControl::Play uses the same three calls.
// - The landing after a jump: 0x006B1900 cdecl(actor, material), called by
//   the movement update at 0x005FDA17 with the material the character
//   controller (0x0065A2C0 thiscall(actor)) keeps at +0x214. It picks the
//   ground's FootSound*Land (the table at 0x00B3623C), a splash in water,
//   and the armour's landing.
//
// Both are refused (false) when the first bytes of any function used differ
// from what was read (VerifyGameSoundAddresses).

bool VerifyGameSoundAddresses();

// A SOUN form by its id, as the player's own (not placed). False when it is
// no sound form or the game makes no sound of it.
bool PlaySoundForm(UInt32 formId);

// The landing sound the player makes on the ground under them.
bool PlayPlayerLandingSound();

}  // namespace obvr::game

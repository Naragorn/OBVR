#pragma once

#include "core/Types.h"

namespace obvr::game {

// The player's spell is released by one MagicCaster call, 0x0069BEC0
// (thiscall(caster, 0), ret 4), made by the animation update 0x005FCAB0 at
// two sites, 0x005FCE1D and 0x005FCF6E (read 2026-11-22, OBNG
// knowledge/spell-cast-release.md): it checks the item can be paid for,
// makes the spell (0x0069AF30) and then takes the magicka. For charged spells
// (controls-spec, "spells charged and released") the release is to be held
// back while the cast is held.
//
// Both sites are wrapped. They pass the call on unless OBVR holds the
// player's release back (SetSpellReleaseHold, vr/SpellCharge.h): then the
// release is kept, and made when asked (RequestHeldSpellRelease) at the next
// StepHeldSpellRelease - called in the player's animation update, where the
// engine makes it too. The log says which site released and when.
void InstallSpellRelease();

// Once a frame with whether the cast control is held now (the probe's log).
void StepSpellReleaseProbe(bool castHeld);

// Whether the player's release is held back from now on (true while a spell is charged).
void SetSpellReleaseHold(bool hold);
// Make the held release at the next animation update, with the stage it was let go at and that
// stage's strength and cost (vr::SpellChargeFactor; 1 = as it is).
void RequestHeldSpellRelease(int stage, float factor);
// Drop a held release unmade: no spell, no magicka.
void DropHeldSpellRelease();
// In the player's animation update, after the engine's own: makes a requested held release. Before
// that, the cast's own animation is kept at its release key while the release is held back, and plays
// its second half from there the frame it is made or dropped (vr::StepCastAnimPause).
void StepHeldSpellRelease();

// A release held back or about to be made: the spell is in the hand, so its aim and its start at
// the palm (vr/SpellAim.h) stand as while the cast is under way.
bool SpellChargedInHand();

// What is heard of a charge (vr/SpellCharge.h's stages): the spell's own casting sound (its first
// effect's, MGEF +0x80) as a stage is reached, louder each stage. What is seen is game/SpellOrb.h,
// stepped by StepSpellReleaseProbe with the charged spell's model.
void SpellChargeCue(int stage);

}  // namespace obvr::game

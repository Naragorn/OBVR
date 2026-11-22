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
// For now a probe, no change: both sites are wrapped and pass the call on.
// The log says which site released, the caster's state ([caster+8]) before
// and after, and - from StepSpellReleaseProbe each frame - how the state
// moves through a cast from the press on, so it shows what the state does
// when the animation ends without its release.
void InstallSpellRelease();

// Once a frame with whether the cast control is held now.
void StepSpellReleaseProbe(bool castHeld);

}  // namespace obvr::game

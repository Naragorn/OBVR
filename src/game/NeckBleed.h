#pragma once

// Neck wounds that bleed on, in the game (game/NeckBleedLogic.h decides;
// docs/blade-penetration-spec.md section 12): opened by a blade that went
// into the neck, kept on the neck's bone, spurting the engine's own blood
// (game/Blood.h) with each pulse; said in the log ("Neck wound:").

#include "game/BladeEntry.h"

namespace obvr::game {

// After the entry's verdict: a blade that went into the neck opens a wound
// (or opens anew the one already in that neck).
void NoteNeckEntry(const BladeEntryNow& n, const NeckBleedSettings& settings);

// Once a frame, whether a blade is drawn or not: the wounds pulse and end.
void StepNeckWounds(const NeckBleedSettings& settings, float dtSeconds);

}  // namespace obvr::game

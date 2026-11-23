#pragma once

// The spell from the casting hand ([Hands] SpellFromCastingHand and its two raise limits,
// vr/SpellAim.h) and charged spells ([Hands] SpellCharge and its times, vr/SpellCharge.h).
// Read as one step of the key table (core/Config.cpp), its keys named there for the INI audit.

namespace obvr {

struct Config;

void ReadSpellHandStep(Config& config, const char* path);

}  // namespace obvr

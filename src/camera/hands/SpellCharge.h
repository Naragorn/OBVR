#pragma once

#include "vr/HandInput.h"
#include "vr/SpellCharge.h"

namespace obvr::camera::hands {

// The cast control as the engine gets it, with charged spells (vr/SpellCharge.h): a pulse at the
// press instead of the held trigger, the engine's release held back while the spell is charged
// (game/SpellRelease.h), made when the trigger lets go or dropped when a menu comes up. Steps its
// own state once a frame; the log says each press, charge and release.
vr::HandControlsWanted WithSpellCharge(const vr::HandControlsWanted& controls, float dtSeconds, bool menuUp,
                                       const vr::SpellChargeSettings& settings);

}  // namespace obvr::camera::hands

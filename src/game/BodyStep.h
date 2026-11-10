#pragma once

#include "game/BodyStepLogic.h"

namespace obvr::game {

// Logs each step a foot begins (the first sixty of a game: what the harness
// counts) and hands the verdict on.
const FeetPlantVerdict& SayFootSteps(const FeetPlantVerdict& v, const FeetPlantState& s, const FeetPlantInput& in);

}  // namespace obvr::game

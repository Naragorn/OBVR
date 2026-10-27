#pragma once

// The engine's three screen faders (addr::kScreenFaders), read and kept out
// of the 2D pass for vr/ScreenFade.h: their opacity becomes a fade of the
// whole view instead of a black panel on the HUD.

#include "core/Types.h"

namespace obvr::game {

// Each fader's opacity, 0 for one that is not there; false before the
// table can be read.
bool ReadScreenFaderOpacities(float (&out)[3]);

// Culls every fader shown (its node's app-culled bit) or gives back the ones
// this culled. A node the engine has since detached is forgotten, not
// written: the step clears the record's node when the fade ends.
void CullScreenFaders(bool cull);

}  // namespace obvr::game

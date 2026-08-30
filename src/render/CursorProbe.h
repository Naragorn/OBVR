#pragma once

#include "core/Types.h"

namespace obvr::render {

// Read-only instrument for the mouse offset under a raised screen-size copy:
// clicks land above the visible cursor by roughly frameHeight over
// believedHeight, and the number that feeds that factor into the hit test is
// still unfound (the first suspect, the renderer getter in the cursor
// movement, measured inert - see the dead-end note at kUiScreenWidthCopy).
//
// Each measurement logs the cursor's position triples from the
// InterfaceManager, the cursor tile's node translation (where the sprite is
// actually planted), and the active tile with its node translation (where
// the game believes the mouse is). Holding the cursor still on a known
// button turns one log line into the whole relationship. Nothing is written
// anywhere.
void ProbeCursor(UInt32 frameIndex);

}  // namespace obvr::render

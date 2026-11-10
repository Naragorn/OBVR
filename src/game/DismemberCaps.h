#pragma once

#include "core/Types.h"
#include "game/DismemberLogic.h"

struct NiAVObject;

namespace obvr::game {

// A wound cap (vanilla's zombie neck plug) cloned, its shapes skinned to
// `bone`, hung under `root` and updated: the cap, or 0 with `why` said. The
// engine's loader, clone and updates are checked once (as read, or another
// plugin's jump at their entry, DismemberLogic.h CapEntryOf); otherwise no
// cap is made.
UInt32 AttachCap(UInt32 root, const NiAVObject* bone, char* why, UInt32 whySize);

// Which way a cap faces in its bone's space (its shapes' normals).
CapFacing ReadCapFacing(UInt32 cap);

}  // namespace obvr::game

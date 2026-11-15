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

// A limb's own skin (CutSideLogic.h PartCopyWanted): a body's shape drawn a
// second time for the part. AddPartCopy clones `shape` - before its bones
// are swapped - and points every bone of the clone that is not `part` or
// under it at `partNode`; the clone is not hung yet, the tree is being
// walked. HangPartCopies hangs each beside its original, updates it and
// says what was copied (and what was refused, and why).
struct PartCopies {
	static constexpr UInt32 kMost = 12;
	UInt32 copy[kMost] = {};
	UInt32 parent[kMost] = {};  // each copy's node to hang under: its original's
	UInt32 count = 0;
	char said[240] = {};  // the shapes copied, and the refusals with their reason
};
void AddPartCopy(PartCopies& copies, const NiAVObject* shape, const NiAVObject* part, UInt32 partNode);
void HangPartCopies(PartCopies& copies, UInt32 actor, const char* partName);

// Whether any skinned shape under `root` holds a bone pointer inside
// [first, first + size): a cut's own nodes (CutSideLogic.h CutFateFor).
bool ShapesHoldBoneIn(UInt32 root, UInt32 first, UInt32 size);

// Which way a cap faces in its bone's space (its shapes' normals).
CapFacing ReadCapFacing(UInt32 cap);

}  // namespace obvr::game

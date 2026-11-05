#pragma once

// The player's collapsed head taken out of the world render
// (game/HeadCullLogic.h for why and the decisions).

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// Where ShowPlayerBody collapses the head: with `enabled` ([Body]
// CullHiddenHead) the player's two FaceGen nodes - the skinned face under
// the body's root, the rigid parts under `head` (Bip01 Head) - get the
// engine's hidden bit for this world render, each when it hangs under
// `bodyRoot`. Logs each node, its parts and the verdict once per node.
void CullPlayerHeadForThisRender(NiAVObject* bodyRoot, NiAVObject* head, bool enabled);

// After the world render (afterWorldRender): the bit back off, if this code
// set it. Safe every frame.
void ReleasePlayerHeadCull();

}  // namespace obvr::game

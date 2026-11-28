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

// The mirror's render (game/MirrorHead.h): which of the two FaceGen nodes
// (0 the skinned face, 1 the parts under the head) carry the bit this code set
// this render; that bit off for the render, and on again after it. A bit
// someone else set is never touched.
void PlayerHeadCullOurs(bool (&ours)[2]);
void LiftPlayerHeadCull(const bool (&which)[2]);
void PutBackPlayerHeadCull(const bool (&which)[2]);

}  // namespace obvr::game

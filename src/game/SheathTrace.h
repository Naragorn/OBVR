#pragma once

// A trace of the sheathed weapon around a draw and a sheathe (Nadi, Windows
// 2026-11-14: "sehe ich für paar frames wie die waffe von links nach rechts
// anfliegt und sich dann nach unten an die hüfte ausrichtet" - every weapon,
// the draw and the sheathe; player-body-spec open bug). When the first child
// of "SideWeapon", "BackWeapon" or the body's "Weapon" changes, the next
// kSheathTraceFrames frames log where each of the three and its first child
// stand in the body root's frame and whether the child is hidden - enough
// to tell a weapon shown at the game's place for a few frames from one
// carried there by an animation. A few traces per session.

#include "game/GameTypes.h"

namespace obvr::game {

inline constexpr UInt32 kSheathTraceFrames = 30;

// `sheaths` are SideWeapon, BackWeapon, Quiver as StepBodySheaths found them
// this frame (after its write); `root` the body's root.
void TraceSheaths(NiAVObject* root, NiAVObject* const* sheaths);

}  // namespace obvr::game

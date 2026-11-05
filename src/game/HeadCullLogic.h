#pragma once

// The player's hidden head taken out of the world render, not only collapsed
// ([Body] CullHiddenHead; Nadi, 2026-11-05, option A of OBNG's occlusion
// finding). [Body] HideHead collapses Bip01 Head to a point for the draw
// (game/PlayerBody.cpp), so the face never shows - but the engine still
// draws every FaceGen part skinned to it. Measured by OBNG's occlusion probe
// (OBNG run obng-occlusion/20261105-095111, knowledge/culling-and-draw-
// setup.md): the player's third-person body made 13 draws per eye pass
// outside and 54 inside, 95 % or more of them left no pixel, the head the
// most of it - FaceGenFace alone 0.06 to 0.37 ms per pass, the FaceGen parts
// together 0.08 to 0.45 ms (probe passes: an order of magnitude, not a
// timing). The frustum cull lets them through, their bounds round the camera.
//
// The fix at the place the draw is decided: the engine's own hidden bit
// (bit 0 of NiAVObject +0x18, game/FirstPersonHide.h) on the player's FaceGen
// node for the world render only - set where the head is collapsed, cleared
// after the render (afterWorldRender), so the inventory's figure, third
// person, the race menu and a save's picture see the head as before. The
// picture does not change: the head was a point already, and casts no
// shadow either way. A bit someone else set is never taken off.
//
// Pure, over plain values; game/HeadCull.cpp reads and writes the node.
// head_cull_test covers every flow.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt16 kHeadHiddenBit = 0x0001;

// Whether the head goes this render: it was collapsed this frame (HideHead,
// the body shown), the switch is on, and the face node hangs under the
// body's root - another tree's face is not ours to hide.
inline bool HeadCullWanted(bool headCollapsed, bool enabled, bool faceUnderBody) {
	return headCollapsed && enabled && faceUnderBody;
}

struct HeadCullStep {
	UInt16 flags;
	bool ours;  // this code set the bit and takes it off again
};

// Laying the cull on: a clear bit is set and becomes ours; one already set
// by someone else stays theirs; one this code set earlier in the same
// render stays ours.
inline HeadCullStep CullHead(UInt16 flags, bool alreadyOurs) {
	if (alreadyOurs) {
		return HeadCullStep{static_cast<UInt16>(flags | kHeadHiddenBit), true};
	}
	if ((flags & kHeadHiddenBit) != 0) {
		return HeadCullStep{flags, false};
	}
	return HeadCullStep{static_cast<UInt16>(flags | kHeadHiddenBit), true};
}

// Taking it off after the render: only a bit this code set.
inline UInt16 ReleaseHead(UInt16 flags, bool ours) {
	return ours ? static_cast<UInt16>(flags & ~kHeadHiddenBit) : flags;
}

}  // namespace obvr::game

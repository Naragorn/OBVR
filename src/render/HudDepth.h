#pragma once

#include "core/Types.h"

namespace obvr::render {

// The interface pass's depth while it draws into OBVR's layer.
//
// The inventory's figure of the player (PlayerCharacter+0x5D8, the clone
// "PlayerInventoryPC" hung under the menu root, InterfaceManager+0x60/+0x64)
// is a 3D model drawn by the interface pass (0x0057F170, RenderObject at
// 0x0057F2C3). OBVR points that pass's colour at its own single-sample
// texture but left the depth-stencil the game's - which with antialiasing on
// has 8 samples (the log: "Hud depth: 4028x3380 format=75 multisample=8").
// DXVK leaves a depth-stencil whose sample count differs from the colour
// target's out of the framebuffer altogether (d3d9_device.cpp: `mismatch =
// ... || dsInfo.sampleCount != sampleCount`), so the figure was drawn with no
// depth test: skin over armour, the far side over the near - "die figur ist
// ausgehöhlt" (the tester, 2026-10-05). A depth-stencil of OBVR's own, one
// sample and the layer's size, is bound for the pass when the game's would
// not attach.
//
// Whether the game's depth-stencil would be dropped for this colour target:
// another sample count, or smaller than the target.
inline bool HudNeedsOwnDepth(UInt32 depthSamples, UInt32 targetSamples, UInt32 depthWidth, UInt32 depthHeight,
                             UInt32 targetWidth, UInt32 targetHeight) {
	return depthSamples != targetSamples || depthWidth < targetWidth || depthHeight < targetHeight;
}

}  // namespace obvr::render

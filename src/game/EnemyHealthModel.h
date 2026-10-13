#pragma once

#include "core/Types.h"
#include "render/TestPattern.h"

namespace obvr::game {

// The game's own enemy health bar for the fighters' bars (EnemyHealthLogic.h
// has what the model is; docs/hud-on-hands-spec.md). HUDReticle's enemy
// health tile (GameAddresses.h, kHudAuxRootPointer) holds the loaded model
// under its render node; its geometry - the one with a NiFlipController on
// its texturing property - carries the 40 frames as the controller's
// textures. OBVR reads each frame's pixels back off the texture the game
// loaded (its Direct3D texture, locked read-only, DXT5 decoded -
// ui/DxtDecode.h) the first time a bar wants it, keeps them, and paints
// them on its own overlay above the fighter (ui/EnemyHealthPainter.h): the
// game's picture, frame for frame, with its transparency.
//
// Why not the model itself in the world: it was cloned under each fighter's
// root and placed there (2026-10-13, six harness rounds), and the world's
// renderer drew it - but never with its texture: the world's lighting
// shader property (BSShaderPPLightingProperty, made and set up the engine's
// own way) refuses a texture without mipmaps ("TEXTURE ERROR : texture does
// not contain mipmaps & will not be used", 0x007DA9F5), and the frames
// have none - a black, then a white, then a grey box. The HUD draws them
// through its own path, which does not mind.

struct EnemyHealthFramesInfo {
	UInt32 tile = 0;      // HUDReticle's enemy health tile
	UInt32 tileNode = 0;  // its render node
	UInt32 source = 0;    // the geometry found under it
	UInt32 flip = 0;      // its flip controller
	UInt32 frames = 0;    // the controller's textures
	UInt32 walked = 0;    // objects walked to find it
};

// Whether the game's bar is there to read: found under the HUD's enemy
// health tile (and found again when the HUD was built anew). `why` on
// false.
bool EnemyHealthFramesAvailable(EnemyHealthFramesInfo* info, const char** why);

// The frame's pixels, `width` by `height`, in the canvas's channel order
// (ui/MenuCanvas.h, SwapRedAndBlue) - read off the game's texture the first
// time and kept. Null, and `why`, when the frame cannot be read (no such
// frame, the texture's Direct3D side not found, not lockable, or a format
// OBVR does not decode); the caller paints its own bar then.
const render::Pixel* EnemyHealthFrame(UInt32 index, UInt32* width, UInt32* height, const char** why);

}  // namespace obvr::game

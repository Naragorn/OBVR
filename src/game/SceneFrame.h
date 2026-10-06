#pragma once

#include "core/Types.h"

namespace obvr::game {

// The second eye's picture reaching the back buffer without antialiasing.
//
// With iMultiSample=0 (and no HDR) Oblivion does not draw the world into the
// back buffer: kRenderScene (0x0040C830) renders into a frame-sized texture
// (begun at 0x0040CC2E) and the image-space shaders (0x007B48E0, the manager
// at 0x008037D0) copy it into the back buffer at the end - with no HDR or
// blur shader active, through the plain copy shader 0x00803E40, which begins
// the default render target group (the back buffer, 0x007D71C0) and draws a
// screen quad. With antialiasing the world goes straight into the
// multisampled back buffer, which is why OBVR's dual pass showed stereo with
// iMultiSample=8 and the same picture for both eyes with 0 (measured
// 2026-10-06: the eyes' best horizontal shift -56 px in every band, the crop
// offset alone).
//
// The copy runs once a frame: 0x008040AA tests the renderer's frame state
// ([0x00B3F928]+0x200) and begins the default group only while it is 0. The
// first pass's copy leaves it at 1 - the frame stays open for the 2D pass,
// which ends it (0x007D7210 at 0x0057F429: EndFrame, DisplayFrame, then 0).
// OBVR's second kRenderScene call therefore draws the other eye into the
// texture and the copy shader draws its quad into no group at all: the back
// buffer keeps the first eye.
//
// So between the two passes OBVR closes the frame the first pass's copy left
// open, as the engine's own 0x007D7210 does short of presenting: pop the
// render target group stack (0x007D7150), EndFrame (the renderer's vtable
// +0x134, 0x00762600 - EndScene), and the state back to 0. The second pass
// then runs as the first did: BeginScene, the world into the texture, the
// copy into the back buffer, the state at 1 for the 2D pass. Only on the
// texture path: the image-space effects on (0x00B42F3E, bDoImageSpaceEffects)
// and the effective sample count (0x00B34FC0, 0 without HDR) below 2.
inline constexpr UInt32 kRendererFrameStateOffset = 0x200;
inline constexpr UInt32 kRendererOffscreenStateOffset = 0x204;
inline constexpr UInt32 kRendererEndFrameSlot = 0x134;
inline constexpr UInt32 kRendererEndFrame = 0x00762600;
inline constexpr UInt32 kPopRenderTargetGroups = 0x007D7150;
inline constexpr UInt32 kRendererEndAndDisplayFrame = 0x007D7210;
inline constexpr UInt32 kCopyShaderFrameGate = 0x008040AA;
inline constexpr UInt32 kImageSpaceEffectsByte = 0x00B42F3E;
inline constexpr UInt32 kEffectiveSampleCount = 0x00B34FC0;

// Whether the frame has to be closed between the passes: the texture path
// (image-space effects on, fewer than 2 samples), the frame open after a
// copy (state 1) and no offscreen frame under way.
inline bool SceneFrameCloseWanted(bool imageSpaceEffects, UInt32 effectiveSamples, UInt32 frameState,
                                  UInt32 offscreenState) {
	return imageSpaceEffects && effectiveSamples < 2 && frameState == 1 && offscreenState == 0;
}

// Between the passes of a dual frame. True when the frame was closed (the
// second pass will copy its picture into the back buffer); false when there
// was nothing to close - the antialiased path, or a state not as read.
bool CloseSceneFrameBetweenPasses();

}  // namespace obvr::game

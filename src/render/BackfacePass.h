#pragma once

#include "core/Types.h"

namespace obvr::render {

// Closed hands: the first-person model's inside, drawn as its own surface,
// darkened.
//
// A hand mesh is an open shell - it ends at the wrist, where the arm used to
// continue - and it is drawn with its back faces culled, the default for a
// shape without a NiStencilProperty (hand.nif and the vanilla gauntlets carry
// only a material and a texture, read from Oblivion - Meshes.bsa on
// 2026-09-27). With the arms hidden in Full VR, a hand seen from below and
// behind showed nothing where its inside should be: the room through the
// wrist (the tester, 2026-09-27).
//
// So every draw of the first-person pass is followed by a second one of the
// same geometry with the cull reversed - its back faces. Where the shell is
// closed those faces lie behind its front and the depth test hides them; only
// through an opening do they show. They are drawn by the engine's own
// shaders, textures and skinning, and the blend scales what comes out by a
// constant factor (source times D3DRS_BLENDFACTOR, destination times zero):
// the inside is the hand's own surface - skin, a glove, a gauntlet - only
// darker, so it follows whatever is worn without OBVR knowing what that is.
//
// Before: black (both blend factors zero) showed as black rims under the
// fingernails, where the nails' own shell is open; then one flat gold-brown
// (a pixel shader of OBVR's) looked foreign on a bare hand (the tester,
// 2026-09-28). An average colour of the hand's texture was asked for; its own
// texture, darkened, is that and more - the colour of each point rather than
// of the whole - and needs no texture read back.
//
// An alpha-blended draw - a flame, a spell's glow - has no inside to show and
// is left alone, as is a draw the engine already makes two-sided.
//// The first-person pass is the engine's own: Oblivion.exe 1.2.0.416 swaps a
// second accumulator into the renderer at 0x0040CE1B, sets its byte +0x21E2
// at 0x0040CE34, renders it at 0x0040CE6B and clears the byte at 0x0040CE72
// (the only writes of that byte besides the constructor's zero, 0x007AC05F).

// D3DCULL values (d3d9types.h).
inline constexpr UInt32 kCullNoneValue = 1;
inline constexpr UInt32 kCullClockwise = 2;
inline constexpr UInt32 kCullCounterClockwise = 3;

// The cull the inside draw after this one is made with, or 0 for none.
inline UInt32 BackfaceCullFor(bool enabled, bool firstPersonPass, UInt32 cullMode,
                              bool alphaBlend, UInt32 colorWrite) {
	if (!enabled || !firstPersonPass || alphaBlend || (colorWrite & 0x7u) == 0) {
		return 0;
	}
	if (cullMode == kCullClockwise) {
		return kCullCounterClockwise;
	}
	if (cullMode == kCullCounterClockwise) {
		return kCullClockwise;
	}
	return 0;  // two-sided already, or a value this code does not know
}

// How bright the inside is against the outside, 0..1: [Hands]
// ClosedHandsBrightness. The factor as a D3DCOLOR (0xAARRGGBB), the same in
// the three colour channels; clamped, with a negative or NaN as zero.
inline constexpr float kInsideBrightnessDefault = 0.5f;

inline UInt32 InsideBlendFactor(float brightness) {
	float b = brightness > 0.0f ? brightness : 0.0f;
	if (b > 1.0f) {
		b = 1.0f;
	}
	const UInt32 level = static_cast<UInt32>(b * 255.0f + 0.5f);
	return 0xFF000000u | (level << 16) | (level << 8) | level;
}

// D3DBLEND_BLENDFACTOR and D3DRS_BLENDFACTOR (d3d9types.h).
inline constexpr UInt32 kBlendBlendFactor = 14;
inline constexpr UInt32 kRenderStateBlendFactor = 193;
// The byte the first-person pass sets on the accumulator the renderer holds
// (kRendererPointer +0x08) while it runs.
inline constexpr UInt32 kAccumulatorFirstPersonOffset = 0x21E2;

}  // namespace obvr::render

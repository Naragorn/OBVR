#pragma once

#include "core/Types.h"

namespace obvr::render {

// Closed hands: the first-person model's inside, drawn black.
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
// same geometry with the cull reversed - its back faces - blended to black
// (source and destination factors both zero, colour channels only). Where the
// shell is closed those faces lie behind its front and the depth test hides
// them; only through an opening do they show, as a dark inside. Nothing is
// created: the engine's own shaders and skinning draw the faces, and the
// blend takes away their colour. An alpha-tested draw keeps its holes (the
// shader's alpha is still tested); an alpha-blended one - a flame, a spell's
// glow - has no inside to show and is left alone, as is a draw the engine
// already makes two-sided.
//
// The first-person pass is the engine's own: Oblivion.exe 1.2.0.416 swaps a
// second accumulator into the renderer at 0x0040CE1B, sets its byte +0x21E2
// at 0x0040CE34, renders it at 0x0040CE6B and clears the byte at 0x0040CE72
// (the only writes of that byte besides the constructor's zero, 0x007AC05F).

// D3DCULL values (d3d9types.h).
inline constexpr UInt32 kCullNoneValue = 1;
inline constexpr UInt32 kCullClockwise = 2;
inline constexpr UInt32 kCullCounterClockwise = 3;

// The cull the black draw after this one is made with, or 0 for none.
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

// The byte the first-person pass sets on the accumulator the renderer holds
// (kRendererPointer +0x08) while it runs.
inline constexpr UInt32 kAccumulatorFirstPersonOffset = 0x21E2;

}  // namespace obvr::render

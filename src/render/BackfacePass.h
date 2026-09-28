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
// same geometry with the cull reversed - its back faces - in one flat colour,
// OBVR's Oblivion gold-brown (the stow spot's and the quick menu's,
// 222/190/140). Where the shell is closed those faces lie behind its front
// and the depth test hides them; only through an opening do they show, as a
// closed inside. The engine's own vertex shader and skinning place the
// faces; a pixel shader of OBVR's gives them the colour. Black was the first
// version (a blend with both factors zero), and showed as black rims under
// the fingernails, where the nails' own shell is open (the tester,
// 2026-09-28) - the blend stays as the fallback for a device that refuses the
// shader. An alpha-blended draw - a flame, a spell's glow - has no inside to
// show and is left alone, as is a draw the engine already makes two-sided.
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

// The colour of the inside, 0..1.
inline constexpr float kInsideRed = 222.0f / 255.0f;
inline constexpr float kInsideGreen = 190.0f / 255.0f;
inline constexpr float kInsideBlue = 140.0f / 255.0f;

// The pixel shader that paints it, as the token stream CreatePixelShader
// takes - written out rather than assembled, so it needs no d3dx9 on the
// player's machine:
//
//   ps_2_0 (or ps_3_0)          FFFF0200 / FFFF0300
//   def c0, r, g, b, 1          05000051 A00F0000 r g b 1.0
//   mov oC0, c0                 02000001 800F0800 A0E40000
//   end                         0000FFFF
//
// An instruction token carries its opcode in the low word (def 0x51, mov
// 0x01) and, from shader model 2 on, its parameter count in bits 24-27. A
// register token has bit 31 set, the register type split over bits 28-30 and
// 11-12, and for a destination the write mask in bits 16-19, for a source the
// swizzle in bits 16-23 (0xE4 = xyzw): c0 is type 2 (constant), oC0 type 8
// (colour output) - the D3D9 shader token format (d3d9types.h, D3DSPR_CONST
// and D3DSPR_COLOROUT). The version follows the vertex shader's: Direct3D 9
// draws a 3_0 vertex shader only with a 3_0 pixel shader.
inline constexpr UInt32 kInsideShaderTokens = 11;

inline UInt32 FloatBits(float value) {
	union {
		float f;
		UInt32 u;
	} bits{};
	bits.f = value;
	return bits.u;
}

inline void BuildInsideShader(UInt32 majorVersion, UInt32 (&out)[kInsideShaderTokens]) {
	const UInt32 major = majorVersion >= 3 ? 3u : 2u;
	out[0] = 0xFFFF0000u | (major << 8);
	out[1] = 0x05000051u;
	out[2] = 0xA00F0000u;
	out[3] = FloatBits(kInsideRed);
	out[4] = FloatBits(kInsideGreen);
	out[5] = FloatBits(kInsideBlue);
	out[6] = FloatBits(1.0f);
	out[7] = 0x02000001u;
	out[8] = 0x800F0800u;
	out[9] = 0xA0E40000u;
	out[10] = 0x0000FFFFu;
}

// A shader's major version from its first token (0xFFFE0300 is vs_3_0),
// 0 when the token is not a shader version.
inline UInt32 ShaderMajorVersion(UInt32 versionToken) {
	const UInt32 kind = versionToken >> 16;
	if (kind != 0xFFFEu && kind != 0xFFFFu) {
		return 0;
	}
	return (versionToken >> 8) & 0xFFu;
}

// The byte the first-person pass sets on the accumulator the renderer holds
// (kRendererPointer +0x08) while it runs.
inline constexpr UInt32 kAccumulatorFirstPersonOffset = 0x21E2;

}  // namespace obvr::render

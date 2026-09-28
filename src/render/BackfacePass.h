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

// ------------------------------------------------ the bare hand's lid
//
// A bare hand's inside came out black, fingernails too, and the wrist still
// open to look into (the tester, 2026-09-28): the engine's skin shader lights
// the back faces by the outside's normals, which point away from whoever looks
// into the wrist. The tester chose a lid ("Deckel", the hand cut off at the
// wrist) over the forearm stump to try first. So with bare hands the inside
// is one flat, unlit colour: the average of the texture on stage 0 (read once
// per texture from its smallest mip level), times the brightness. Flat and
// unlit, the faces through the opening show no shape - they read as a lid in
// the plane of the wrist. With a glove or a gauntlet the inside stays its own
// surface darkened, as the tester approved.
//
// The shader, as the token stream CreatePixelShader takes (written out, no
// d3dx9 needed on the player's machine):
//
//   ps_2_0 (or ps_3_0)       FFFF0200 / FFFF0300
//   mov oC0, c31             02000001 800F0800 A0E4001F
//   end                      0000FFFF
//
// mov is opcode 1 with, from shader model 2 on, its parameter count (2) in
// bits 24-27; a register token has bit 31 set, the type split over bits 28-30
// and 11-12 - c (constant) is 2, oC (colour out) 8 - the number in bits 0-10,
// and a write mask (0xF) or swizzle (0xE4, xyzw) in bits 16-23 (d3d9types.h,
// D3DSPR_CONST and D3DSPR_COLOROUT). c31 is the last constant a ps_2_0 has;
// it is saved before and put back after the draw. The version follows the
// vertex shader's: Direct3D 9 draws a 3_0 vertex shader only with a 3_0 pixel
// shader. A ps_3_0 that only writes a constant declares no inputs.
inline constexpr UInt32 kInsideShaderTokens = 5;
inline constexpr UInt32 kInsideColourRegister = 31;

inline void BuildInsideShader(UInt32 majorVersion, UInt32 (&out)[kInsideShaderTokens]) {
	const UInt32 major = majorVersion >= 3 ? 3u : 2u;
	out[0] = 0xFFFF0000u | (major << 8);
	out[1] = 0x02000001u;
	out[2] = 0x800F0800u;
	out[3] = 0xA0E40000u | kInsideColourRegister;
	out[4] = 0x0000FFFFu;
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

// The lid's colour, 0..1 a channel: the texture's average (8 bits a channel)
// times the brightness, clamped as the blend factor is.
inline void InsideColour(UInt8 r, UInt8 g, UInt8 b, float brightness, float (&out)[4]) {
	float k = brightness > 0.0f ? brightness : 0.0f;
	if (k > 1.0f) {
		k = 1.0f;
	}
	out[0] = static_cast<float>(r) / 255.0f * k;
	out[1] = static_cast<float>(g) / 255.0f * k;
	out[2] = static_cast<float>(b) / 255.0f * k;
	out[3] = 1.0f;
}

// Which inside a draw gets: none, the own surface darkened, or the flat lid.
enum class InsideKind : UInt8 { None, Darkened, Flat };

inline InsideKind InsideKindFor(UInt32 reversedCull, bool bareHands, bool haveFlatColour) {
	if (reversedCull == 0) {
		return InsideKind::None;
	}
	return bareHands && haveFlatColour ? InsideKind::Flat : InsideKind::Darkened;
}

// D3DBLEND_BLENDFACTOR and D3DRS_BLENDFACTOR (d3d9types.h).
inline constexpr UInt32 kBlendBlendFactor = 14;
inline constexpr UInt32 kRenderStateBlendFactor = 193;
// The byte the first-person pass sets on the accumulator the renderer holds
// (kRendererPointer +0x08) while it runs.
inline constexpr UInt32 kAccumulatorFirstPersonOffset = 0x21E2;

}  // namespace obvr::render

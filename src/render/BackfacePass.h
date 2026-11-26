#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

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

// Whether a shape is drawn turned over: its world rotation's determinant
// times its scale below 0. Near 0 (a flat or broken matrix): not known.
enum class ShapeTurn : UInt8 { Unknown, TurnedOver, Upright };

inline ShapeTurn ShapeTurnOf(const NiMatrix33& rot, float scale) {
	const float d = Determinant(rot) * scale;
	if (!(d == d) || (d < 1.0e-6f && d > -1.0e-6f)) {
		return ShapeTurn::Unknown;
	}
	return d < 0.0f ? ShapeTurn::TurnedOver : ShapeTurn::Upright;
}

// The turn a draw winds by. A particle draw (NiDX9Renderer slot 92,
// render/ShapeTurnHook.h) builds its quads toward the camera whatever its
// shape's turn, so it winds the engine's way: a spell's glow hangs under the
// mirrored hand (turned over) and went with the cull swapped (spell-mirror-glow,
// 2026-11-26: "PCloud Projectile Local" turned over, the glow gone from the
// palm); the torch's flame is in the world and was upright. A mesh by its turn.
inline ShapeTurn DrawnShapeTurn(bool particles, const NiMatrix33& rot, float scale) {
	return particles ? ShapeTurn::Upright : ShapeTurnOf(rot, scale);
}

// The cull a first-person draw is made with while the skeleton is drawn
// mirrored (left-handed play, game/BonePin.h "The mirror"): a mirrored mesh
// winds its triangles the other way round, so the cull swaps - in the
// first-person pass only, the world is not mirrored. Two-sided stays as it
// is. A shape that is upright under the mirror keeps the engine's cull: the
// torch's flame faces the camera (a billboard sets its own turn) and went out
// with the cull swapped (2026-11-23). A draw whose shape is not known swaps.
inline UInt32 MirroredCullFor(bool mirrored, bool firstPersonPass, UInt32 cullMode,
                              ShapeTurn turn = ShapeTurn::Unknown) {
	if (!mirrored || !firstPersonPass || turn == ShapeTurn::Upright) {
		return cullMode;
	}
	if (cullMode == kCullClockwise) {
		return kCullCounterClockwise;
	}
	if (cullMode == kCullCounterClockwise) {
		return kCullClockwise;
	}
	return cullMode;
}

// How bright the inside is against the outside, 0..1: [Hands]
// ClosedHandsBrightness. The factor as a D3DCOLOR (0xAARRGGBB), the same in
// the three colour channels; clamped, with a negative or NaN as zero.
inline constexpr float kInsideBrightnessDefault = 0.6f;

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

// ------------------------------------------------ the sealed opening
//
// The lid is the hand's own far wall, drawn flat - so whatever lies inside
// the hand, nearer than that wall, shows through the wrist instead: the bow's
// grip in the bow hand's fist (harness 2026-10-01, "wrist": the empty hand's
// wrist one flat lid, the bow hand's showing the bow inside it; the tester:
// "ich will hier wenn keine rüstung dran ist genau dieselbe hand wie ohne
// bogen"). So for a bare, skinned hand the opening is found and sealed:
//   1. parity: every face of the hand, both sides, depth ignored, toggles a
//      stencil bit (kStencilParityBit). A closed surface is crossed an even
//      number of times along a ray, so the bit stays set only where the ray
//      goes in through the opening - the wrist - and hits the far wall;
//   2. the visible skin: the hand's front faces where they are what the depth
//      buffer holds (depth equal) set kStencilSkinBit - where the hand's own
//      faces cross each other the parity is odd too, and the skin there is
//      to stay skin;
//   3. the lid: the back faces, flat, where the parity is set and the skin's
//      bit is not, depth ignored and written as the nearest there is (the
//      viewport's depth range 0 to 0): drawn over what is inside the hand,
//      and anything drawn later inside it fails against it;
//   4. both bits cleared again over the hand's whole silhouette.
// Only with a depth buffer of eight stencil bits (D24S8, D24FS8); without, the
// far-wall lid as before.
inline constexpr UInt32 kStencilParityBit = 0x80;
inline constexpr UInt32 kStencilSkinBit = 0x40;

// D3DFMT_D24S8 and D3DFMT_D24FS8 (d3d9types.h): eight stencil bits.
inline bool HasEightStencilBits(UInt32 depthFormat) { return depthFormat == 75 || depthFormat == 83; }

// D3DRS_* (d3d9types.h) the sealing sets, and their values.
inline constexpr UInt32 kRsZFunc = 23;
inline constexpr UInt32 kRsZWrite = 14;
inline constexpr UInt32 kRsCull = 22;
inline constexpr UInt32 kRsStencilEnable = 52;
inline constexpr UInt32 kRsStencilFail = 53;
inline constexpr UInt32 kRsStencilZFail = 54;
inline constexpr UInt32 kRsStencilPass = 55;
inline constexpr UInt32 kRsStencilFunc = 56;
inline constexpr UInt32 kRsStencilRef = 57;
inline constexpr UInt32 kRsStencilMask = 58;
inline constexpr UInt32 kRsStencilWriteMask = 59;
inline constexpr UInt32 kRsTwoSidedStencil = 185;
inline constexpr UInt32 kRsColorWrite = 168;
inline constexpr UInt32 kRsAlphaTest = 15;
inline constexpr UInt32 kCullNone = 1;
inline constexpr UInt32 kCmpEqual = 3;
inline constexpr UInt32 kCmpAlways = 8;
inline constexpr UInt32 kStencilKeep = 1;
inline constexpr UInt32 kStencilZero = 2;
inline constexpr UInt32 kStencilReplace = 3;
inline constexpr UInt32 kStencilInvert = 6;

// One step of the sealing: the render states it draws with (each a pair of
// state and value), whether it draws the back faces with the flat lid, and
// whether its depth is written as the nearest.
struct SealStep {
	UInt32 states[12][2];
	UInt32 count;
	bool lid;
	bool nearestDepth;
};

// The four steps for a draw whose own cull is `cull`, whose back faces cull
// `reversed`, with the colour channels `colourWrite` the lid may write.
inline UInt32 SealSteps(UInt32 cull, UInt32 reversed, UInt32 colourWrite, SealStep (&out)[4]) {
	const UInt32 both = kStencilParityBit | kStencilSkinBit;
	// 1. parity
	out[0] = SealStep{{{kRsAlphaTest, 0},
	                   {kRsColorWrite, 0},
	                   {kRsCull, kCullNone},
	                   {kRsZFunc, kCmpAlways},
	                   {kRsZWrite, 0},
	                   {kRsStencilEnable, 1},
	                   {kRsTwoSidedStencil, 0},
	                   {kRsStencilFunc, kCmpAlways},
	                   {kRsStencilPass, kStencilInvert},
	                   {kRsStencilFail, kStencilKeep},
	                   {kRsStencilZFail, kStencilKeep},
	                   {kRsStencilWriteMask, kStencilParityBit}},
	                  12, false, false};
	// 2. the visible skin
	out[1] = SealStep{{{kRsAlphaTest, 0},
	                   {kRsColorWrite, 0},
	                   {kRsCull, cull},
	                   {kRsZFunc, kCmpEqual},
	                   {kRsZWrite, 0},
	                   {kRsStencilEnable, 1},
	                   {kRsStencilFunc, kCmpAlways},
	                   {kRsStencilPass, kStencilReplace},
	                   {kRsStencilZFail, kStencilKeep},
	                   {kRsStencilRef, kStencilSkinBit},
	                   {kRsStencilWriteMask, kStencilSkinBit},
	                   {kRsStencilFail, kStencilKeep}},
	                  12, false, false};
	// 3. the lid
	out[2] = SealStep{{{kRsAlphaTest, 0},
	                   {kRsColorWrite, colourWrite},
	                   {kRsCull, reversed},
	                   {kRsZFunc, kCmpAlways},
	                   {kRsZWrite, 1},
	                   {kRsStencilEnable, 1},
	                   {kRsStencilFunc, kCmpEqual},
	                   {kRsStencilRef, kStencilParityBit},
	                   {kRsStencilMask, both},
	                   {kRsStencilWriteMask, 0},
	                   {kRsStencilPass, kStencilKeep},
	                   {kRsStencilZFail, kStencilKeep}},
	                  12, true, true};
	// 4. both bits cleared
	out[3] = SealStep{{{kRsAlphaTest, 0},
	                   {kRsColorWrite, 0},
	                   {kRsCull, kCullNone},
	                   {kRsZFunc, kCmpAlways},
	                   {kRsZWrite, 0},
	                   {kRsStencilEnable, 1},
	                   {kRsStencilFunc, kCmpAlways},
	                   {kRsStencilPass, kStencilZero},
	                   {kRsStencilFail, kStencilZero},
	                   {kRsStencilZFail, kStencilZero},
	                   {kRsStencilWriteMask, both},
	                   {kRsStencilMask, both}},
	                  12, false, false};
	return 4;
}

// The states the sealing touches, saved before and put back after.
inline constexpr UInt32 kSealSavedStates[] = {kRsAlphaTest,     kRsColorWrite,    kRsCull,          kRsZFunc,
                                              kRsZWrite,        kRsStencilEnable, kRsTwoSidedStencil,
                                              kRsStencilFunc,   kRsStencilPass,   kRsStencilFail,
                                              kRsStencilZFail,  kRsStencilRef,    kRsStencilMask,
                                              kRsStencilWriteMask};
inline constexpr UInt32 kSealSavedCount = sizeof(kSealSavedStates) / sizeof(kSealSavedStates[0]);

// D3DBLEND_BLENDFACTOR and D3DRS_BLENDFACTOR (d3d9types.h).
inline constexpr UInt32 kBlendBlendFactor = 14;
inline constexpr UInt32 kRenderStateBlendFactor = 193;
// The byte the first-person pass sets on the accumulator the renderer holds
// (kRendererPointer +0x08) while it runs.
inline constexpr UInt32 kAccumulatorFirstPersonOffset = 0x21E2;

// Whether that byte may be read: only while the engine's scene render runs
// (render::SceneRenderRunning). The byte is set and cleared inside the scene
// render alone - 0x0040CE34 and 0x0040CE72 lie in 0x0040C830, whose one ret
// is at 0x0040D150 - so outside it the answer is no without a read. And
// outside it the renderer's +0x08 need not hold a live accumulator: beta 9's
// crash on a Quest 2 (2026-11-19 03:10, obvr.dll+16F86D, InFirstPersonPass
// from DrawBackfacesAfter) read 131C516A = the pointer at +0x08 plus 0x21E2,
// unmapped, on the frames the new game's intro film owned the screen; a
// Steam Frame's game froze at the same step. The draws there are the
// interface's and the other plugins' (FoliageAA on that stack).
inline bool MayReadFirstPersonByte(bool sceneRenderRunning, bool rendererLooksLikeObject,
                                   bool accumulatorLooksLikeObject) {
	return sceneRenderRunning && rendererLooksLikeObject && accumulatorLooksLikeObject;
}

// The evidence for it: the accumulator the scene render read last, and
// whether an accumulator outside it is worth a log line - a new one, unlike
// the scene render's, at most four times a run.
struct AccumulatorWatch {
	UInt32 scene = 0;
	UInt32 lastLogged = 0;
	int linesLeft = 4;

	void NoteScene(UInt32 accumulator) { scene = accumulator; }
	bool LogOutside(UInt32 accumulator) {
		if (accumulator == scene || accumulator == lastLogged || linesLeft <= 0) {
			return false;
		}
		--linesLeft;
		lastLogged = accumulator;
		return true;
	}
};

}  // namespace obvr::render

// Checks the closed hands' decision (render/BackfacePass.h): which draws are
// followed by their back faces, with which cull, and how bright.

#include <cstdio>

#include "render/BackfacePass.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// The value a step sets for a state, or 0xFFFFFFFF when it sets none.
UInt32 ValueOf(const obvr::render::SealStep& s, UInt32 state) {
	for (UInt32 i = 0; i < s.count; ++i) {
		if (s.states[i][0] == state) {
			return s.states[i][1];
		}
	}
	return 0xFFFFFFFFu;
}

void TestSeal() {
	using namespace obvr::render;
	std::printf("The sealed opening\n");
	Check(HasEightStencilBits(75) && HasEightStencilBits(83) && !HasEightStencilBits(77) &&
	          !HasEightStencilBits(79) && !HasEightStencilBits(73) && !HasEightStencilBits(80),
	      "eight stencil bits: D24S8 and D24FS8, not D24X8, D24X4S4, D15S1 or D16");
	SealStep s[4];
	Check(SealSteps(kCullCounterClockwise, kCullClockwise, 0x7, s) == 4, "four steps");
	for (const SealStep& step : s) {
		Check(ValueOf(step, kRsAlphaTest) == 0 && ValueOf(step, kRsStencilEnable) == 1,
		      "each: alpha test off, the stencil on");
	}
	Check(ValueOf(s[0], kRsCull) == kCullNone && ValueOf(s[0], kRsZFunc) == kCmpAlways &&
	          ValueOf(s[0], kRsColorWrite) == 0 && ValueOf(s[0], kRsStencilPass) == kStencilInvert &&
	          ValueOf(s[0], kRsStencilWriteMask) == kStencilParityBit && !s[0].lid,
	      "1: every face toggles the parity bit, depth ignored, no colour");
	Check(ValueOf(s[1], kRsCull) == kCullCounterClockwise && ValueOf(s[1], kRsZFunc) == kCmpEqual &&
	          ValueOf(s[1], kRsStencilPass) == kStencilReplace && ValueOf(s[1], kRsStencilRef) == kStencilSkinBit &&
	          ValueOf(s[1], kRsStencilWriteMask) == kStencilSkinBit && ValueOf(s[1], kRsColorWrite) == 0,
	      "2: the visible front faces mark the skin");
	Check(s[2].lid && s[2].nearestDepth && ValueOf(s[2], kRsCull) == kCullClockwise &&
	          ValueOf(s[2], kRsZFunc) == kCmpAlways && ValueOf(s[2], kRsZWrite) == 1 &&
	          ValueOf(s[2], kRsStencilFunc) == kCmpEqual && ValueOf(s[2], kRsStencilRef) == kStencilParityBit &&
	          ValueOf(s[2], kRsStencilMask) == (kStencilParityBit | kStencilSkinBit) &&
	          ValueOf(s[2], kRsStencilWriteMask) == 0 && ValueOf(s[2], kRsColorWrite) == 0x7,
	      "3: the lid, flat, only in the opening (parity, no skin), over what is inside, at the nearest depth");
	Check(ValueOf(s[3], kRsCull) == kCullNone && ValueOf(s[3], kRsStencilPass) == kStencilZero &&
	          ValueOf(s[3], kRsStencilFail) == kStencilZero && ValueOf(s[3], kRsStencilZFail) == kStencilZero &&
	          ValueOf(s[3], kRsStencilWriteMask) == (kStencilParityBit | kStencilSkinBit) &&
	          ValueOf(s[3], kRsColorWrite) == 0,
	      "4: both bits cleared over the whole silhouette");
	bool saved = true;
	for (const SealStep& step : s) {
		for (UInt32 i = 0; i < step.count; ++i) {
			bool found = false;
			for (UInt32 k = 0; k < kSealSavedCount; ++k) {
				found = found || kSealSavedStates[k] == step.states[i][0];
			}
			saved = saved && found;
		}
	}
	Check(saved, "every state a step sets is saved and put back");
}

}  // namespace

int main() {
	TestSeal();
	using namespace obvr::render;
	const UInt32 rgba = 0xF;

	Check(BackfaceCullFor(true, true, kCullCounterClockwise, false, rgba) == kCullClockwise,
	      "the engine's usual cull reversed: the back faces");
	Check(BackfaceCullFor(true, true, kCullClockwise, false, rgba) == kCullCounterClockwise,
	      "and the other way round");
	Check(BackfaceCullFor(true, true, kCullNoneValue, false, rgba) == 0,
	      "a two-sided draw has its inside drawn already");
	Check(BackfaceCullFor(true, true, 7, false, rgba) == 0, "an unknown cull value is left alone");
	Check(BackfaceCullFor(true, true, kCullCounterClockwise, true, rgba) == 0,
	      "an alpha-blended draw has no inside to show");
	Check(BackfaceCullFor(true, true, kCullCounterClockwise, false, 0x8) == 0,
	      "a draw that writes no colour is not repeated");
	Check(BackfaceCullFor(true, true, kCullCounterClockwise, false, 0x1) == kCullClockwise,
	      "one colour channel is enough");
	Check(BackfaceCullFor(true, false, kCullCounterClockwise, false, rgba) == 0,
	      "outside the first-person pass: the world is left alone");
	Check(BackfaceCullFor(false, true, kCullCounterClockwise, false, rgba) == 0,
	      "switched off: nothing");

	// The left-handed mirror: the first-person draws cull the other way.
	Check(MirroredCullFor(true, true, kCullCounterClockwise) == kCullClockwise &&
	          MirroredCullFor(true, true, kCullClockwise) == kCullCounterClockwise,
	      "mirrored in the first-person pass: the cull swaps");
	Check(MirroredCullFor(true, true, kCullNoneValue) == kCullNoneValue && MirroredCullFor(true, true, 7) == 7,
	      "two-sided or unknown: left alone");
	Check(MirroredCullFor(true, false, kCullCounterClockwise) == kCullCounterClockwise,
	      "outside the first-person pass: the world is not mirrored");
	Check(MirroredCullFor(false, true, kCullCounterClockwise) == kCullCounterClockwise,
	      "not mirrored: as the engine set it");
	// Only a shape that is turned over winds the other way: the torch's flame
	// faces the camera (a billboard) and is not mirrored - with the cull
	// swapped its front faces went, the flame went out (2026-11-23).
	Check(MirroredCullFor(true, true, kCullCounterClockwise, ShapeTurn::TurnedOver) == kCullClockwise,
	      "a turned-over shape: the cull swaps");
	Check(MirroredCullFor(true, true, kCullCounterClockwise, ShapeTurn::Upright) == kCullCounterClockwise,
	      "an upright shape under the mirror (the flame): as the engine set it");
	Check(MirroredCullFor(true, true, kCullCounterClockwise, ShapeTurn::Unknown) == kCullClockwise,
	      "the shape not known: swapped as before");
	Check(MirroredCullFor(false, true, kCullCounterClockwise, ShapeTurn::TurnedOver) == kCullCounterClockwise,
	      "not mirrored: as the engine set it, whatever the shape");
	{
		obvr::NiMatrix33 m = obvr::NiMatrix33::Identity();
		Check(ShapeTurnOf(m, 1.0f) == ShapeTurn::Upright, "identity: upright");
		m.data[0][0] = -1.0f;
		Check(ShapeTurnOf(m, 1.0f) == ShapeTurn::TurnedOver, "one axis flipped: turned over");
		Check(ShapeTurnOf(m, -1.0f) == ShapeTurn::Upright, "flipped and a negative scale: upright again");
		obvr::NiMatrix33 flat{};
		Check(ShapeTurnOf(flat, 1.0f) == ShapeTurn::Unknown, "a flat matrix: not known");
		Check(ShapeTurnOf(obvr::NiMatrix33::Identity(), 0.0f) == ShapeTurn::Unknown, "scale 0: not known");
	}
	Check(BackfaceCullFor(true, true, MirroredCullFor(true, true, kCullCounterClockwise), false, rgba) ==
	          kCullCounterClockwise,
	      "the inside of a mirrored draw: the engine's own cull again");

	Check(InsideBlendFactor(0.5f) == 0xFF808080u, "half as bright: 128 in each colour");
	Check(InsideBlendFactor(0.0f) == 0xFF000000u, "0: black");
	Check(InsideBlendFactor(1.0f) == 0xFFFFFFFFu, "1: as bright as the outside");
	Check(InsideBlendFactor(-2.0f) == 0xFF000000u && InsideBlendFactor(7.0f) == 0xFFFFFFFFu,
	      "out of range: clamped");
	volatile float zero = 0.0f;
	const float nan = zero / zero;
	Check(InsideBlendFactor(nan) == 0xFF000000u, "not a number: black rather than garbage");
	Check(InsideBlendFactor(kInsideBrightnessDefault) == 0xFF999999u, "the default is 0.6 (the tester, 2026-09-28)");

	// The bare hand's lid.
	UInt32 tokens[kInsideShaderTokens];
	BuildInsideShader(2, tokens);
	Check(tokens[0] == 0xFFFF0200u && tokens[1] == 0x02000001u && tokens[2] == 0x800F0800u &&
	          tokens[3] == 0xA0E4001Fu && tokens[4] == 0x0000FFFFu,
	      "ps_2_0: mov oC0, c31; end");
	BuildInsideShader(3, tokens);
	Check(tokens[0] == 0xFFFF0300u && tokens[3] == 0xA0E4001Fu, "for a vs_3_0: the same as ps_3_0");
	BuildInsideShader(1, tokens);
	Check(tokens[0] == 0xFFFF0200u, "an older vertex shader: ps_2_0");
	Check(ShaderMajorVersion(0xFFFE0300u) == 3 && ShaderMajorVersion(0xFFFE0101u) == 1 &&
	          ShaderMajorVersion(0xFFFF0200u) == 2,
	      "a shader's major version from its first token");
	Check(ShaderMajorVersion(0x12345678u) == 0, "not a version token: 0");
	float colour[4];
	InsideColour(200, 100, 50, 0.5f, colour);
	Check(colour[0] > 0.392f && colour[0] < 0.393f && colour[2] > 0.098f && colour[2] < 0.099f &&
	          colour[3] == 1.0f,
	      "the average times the brightness, opaque");
	InsideColour(255, 255, 255, 3.0f, colour);
	Check(colour[1] == 1.0f, "a brightness past 1: 1");
	InsideColour(255, 255, 255, nan, colour);
	Check(colour[0] == 0.0f && colour[3] == 1.0f, "not a number: black, opaque");
	Check(InsideKindFor(0, true, true) == InsideKind::None, "no inside to draw: none");
	Check(InsideKindFor(kCullClockwise, true, true) == InsideKind::Flat, "bare hands with a colour: the lid");
	Check(InsideKindFor(kCullClockwise, true, false) == InsideKind::Darkened,
	      "bare hands, no average or no shader: darkened, as before");
	Check(InsideKindFor(kCullClockwise, false, true) == InsideKind::Darkened, "a glove or gauntlet: darkened");
	std::printf("When the first-person pass's byte is read\n");
	Check(MayReadFirstPersonByte(true, true, true), "inside the scene render, both pointers plausible: read");
	Check(!MayReadFirstPersonByte(false, true, true),
	      "outside the scene render (a film, the interface pass): not read, the accumulator may be gone");
	Check(!MayReadFirstPersonByte(true, false, true), "no renderer: not read");
	Check(!MayReadFirstPersonByte(true, true, false), "no accumulator: not read");
	Check(!MayReadFirstPersonByte(false, false, false), "nothing: not read");
	AccumulatorWatch watch;
	watch.NoteScene(0x1000u);
	Check(!watch.LogOutside(0x1000u), "outside, the scene render's own accumulator: no line");
	Check(watch.LogOutside(0x2000u), "outside, another one: a line");
	Check(!watch.LogOutside(0x2000u), "the same one again: no second line");
	Check(watch.LogOutside(0x3000u) && watch.LogOutside(0x4000u) && watch.LogOutside(0x5000u),
	      "three more new ones: a line each");
	Check(!watch.LogOutside(0x6000u), "past four lines: quiet");
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

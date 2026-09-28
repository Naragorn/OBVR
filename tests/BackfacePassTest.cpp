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

}  // namespace

int main() {
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
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

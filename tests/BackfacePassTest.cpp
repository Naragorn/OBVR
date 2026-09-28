// Checks the closed hands' decision (render/BackfacePass.h): which draws are
// followed by their back faces in black, and with which cull.

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

	{
		UInt32 tokens[kInsideShaderTokens];
		BuildInsideShader(2, tokens);
		Check(tokens[0] == 0xFFFF0200u, "shader model 2: ps_2_0");
		Check(tokens[1] == 0x05000051u && tokens[2] == 0xA00F0000u,
		      "def c0 with its four values");
		Check(tokens[3] == FloatBits(kInsideRed) && tokens[4] == FloatBits(kInsideGreen) &&
		          tokens[5] == FloatBits(kInsideBlue) && tokens[6] == 0x3F800000u,
		      "the gold-brown, opaque");
		Check(tokens[7] == 0x02000001u && tokens[8] == 0x800F0800u && tokens[9] == 0xA0E40000u,
		      "mov oC0, c0");
		Check(tokens[10] == 0x0000FFFFu, "and the end token");
		BuildInsideShader(3, tokens);
		Check(tokens[0] == 0xFFFF0300u && tokens[10] == 0x0000FFFFu,
		      "shader model 3: ps_3_0, the same body");
		BuildInsideShader(1, tokens);
		Check(tokens[0] == 0xFFFF0200u, "anything below 3 is 2_0");
		BuildInsideShader(4, tokens);
		Check(tokens[0] == 0xFFFF0300u, "anything above is 3_0");
		Check(kInsideRed > kInsideGreen && kInsideGreen > kInsideBlue && kInsideBlue > 0.5f,
		      "the colour is the gold-brown 222/190/140");
	}
	Check(ShaderMajorVersion(0xFFFE0300u) == 3, "vs_3_0 reads as 3");
	Check(ShaderMajorVersion(0xFFFE0200u) == 2, "vs_2_0 reads as 2");
	Check(ShaderMajorVersion(0xFFFE0101u) == 1, "vs_1_1 reads as 1");
	Check(ShaderMajorVersion(0xFFFF0300u) == 3, "a pixel shader's token reads too");
	Check(ShaderMajorVersion(0x12340300u) == 0, "not a version token: 0");

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

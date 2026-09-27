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

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

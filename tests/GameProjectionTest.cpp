// Checks the guard on Oblivion's projection matrix.
//
// The reading itself needs a device and is not tested here. What is tested is
// the decision of whether to believe what comes back, and that decision exists
// because of one specific quiet failure.
//
// Oblivion draws most of its world through shaders. A renderer that never
// calls SetTransform leaves the fixed-function projection at identity, and
// identity is a perfectly well-formed matrix: no null, no NaN, nothing to
// catch. It describes a 90-degree square view that the game is not using.
// Believing it would put the world at the wrong size in a headset, with
// nothing anywhere saying why - and "the world feels the wrong size" is the
// hardest symptom this project has had to attribute, because it looks exactly
// like a correct picture.
//
// No Windows API, so this builds and runs on Linux as well.

#include <cstdio>

#include "render/GameProjection.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestIdentityIsRejected() {
	std::printf("The matrix that is not a projection\n");

	using obvr::render::IsPlausibleProjection;

	// The case this whole function exists for.
	Check(!IsPlausibleProjection(1.0f, 1.0f),
	      "identity is refused, which is what a game using only shaders leaves behind");

	// Any exactly square frustum, for the same reason. Oblivion renders
	// whatever iSize W and H say, and a computed frustum from a non-square
	// frame does not come out equal to the last bit.
	Check(!IsPlausibleProjection(1.3f, 1.3f), "and so is any frustum square to the bit");
	Check(IsPlausibleProjection(1.3f, 2.3f), "while an unequal pair is believed");
}

void TestNonProjections() {
	std::printf("Values that cannot be a frustum\n");

	using obvr::render::IsPlausibleProjection;

	// 1/tan of a half-angle is positive. Zero or negative is not a narrow
	// view, it is not a projection at all - an uninitialised matrix, or a
	// slot the game uses for something else.
	Check(!IsPlausibleProjection(0.0f, 1.3f), "zero is refused");
	Check(!IsPlausibleProjection(1.3f, 0.0f), "in either element");
	Check(!IsPlausibleProjection(-1.3f, 2.0f), "and so is a negative one");

	// Far outside any field of view anyone chose. 1/tan(10 degrees) is 5.67
	// and 1/tan(80 degrees) is 0.176, so the bounds are wide on purpose: they
	// are here to reject nonsense, not to second-guess a choice.
	Check(!IsPlausibleProjection(50.0f, 2.0f), "an absurdly narrow view is refused");
	Check(!IsPlausibleProjection(0.001f, 2.0f), "and an absurdly wide one");
}

void TestRealValues() {
	std::printf("What Oblivion actually renders\n");

	using obvr::render::IsPlausibleProjection;

	// 2560x1440 at 75 degrees across, read as a horizontal field of view:
	// 1/tan(37.5) is 1.303, and 1/tan(23.33) is 2.317.
	Check(IsPlausibleProjection(1.303f, 2.317f),
	      "75 degrees across a 16:9 frame passes");

	// The same number read as a 4:3 figure: 91.3 across, 60 down, so 1/tan of
	// 45.65 and of 30 - 0.9774 and 1.7321.
	Check(IsPlausibleProjection(0.9774f, 1.7321f), "and so does the other reading of it");

	// Which of those two the game is actually using is exactly the question
	// this file exists to stop guessing at. Both are plausible; only one is
	// true; and the matrix says which.
	Check(IsPlausibleProjection(1.303f, 2.317f) && IsPlausibleProjection(0.9774f, 1.7321f),
	      "both readings are plausible, which is why the matrix is asked instead");
}

}  // namespace

int main() {
	std::printf("OBVR game projection test\n\n");

	TestIdentityIsRejected();
	std::printf("\n");
	TestNonProjections();
	std::printf("\n");
	TestRealValues();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}

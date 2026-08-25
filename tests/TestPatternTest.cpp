// Checks the picture OBVR puts in the headset before it can put Oblivion
// there.
//
// A test pattern that is itself wrong is worse than no test pattern: it would
// be read as evidence about the compositor, the projection or the eye order,
// and every one of those investigations would start from a false premise. So
// each feature of the pattern is checked for the property it is supposed to
// demonstrate - the border reaches all four edges, the ramp really does run
// dark to light downwards, the two eyes really are distinguishable.
//
// Pure arithmetic, so this runs on Linux as well.

#include <cstdio>

#include "render/TestPattern.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::Eye;
using obvr::render::Pixel;

constexpr UInt32 kWidth = 320;
constexpr UInt32 kHeight = 240;

Pixel At(UInt32 x, UInt32 y, Eye eye = Eye::Left) {
	return obvr::render::PatternPixel(x, y, kWidth, kHeight, eye);
}

bool SameColour(const Pixel& a, const Pixel& b) {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool IsGrey(const Pixel& p) { return p.r == p.g && p.g == p.b; }

void TestBorderReachesEveryEdge() {
	std::printf("The border, which says the whole texture arrived\n");

	const Pixel white{255, 255, 255, 255};

	// All four edges, because a wrong texture bound cuts one side and the
	// question the border answers is which.
	Check(SameColour(At(0, kHeight / 2), white), "the left edge is border");
	Check(SameColour(At(kWidth - 1, kHeight / 2), white), "the right edge is border");
	Check(SameColour(At(kWidth / 2, 0), white), "the top edge is border");
	Check(SameColour(At(kWidth / 2, kHeight - 1), white), "the bottom edge is border");

	// And it has to stop, or the whole picture is white and proves nothing.
	const UInt32 thickness = obvr::render::BorderThickness(kWidth, kHeight);
	Check(thickness >= 2, "the border is at least two pixels thick");
	Check(thickness <= kHeight / 4, "and does not swallow the picture");
	Check(!SameColour(At(kWidth / 2, thickness + 1), white),
	      "just inside the border is not border any more");

	// A size small enough that a proportional thickness rounds to zero. A
	// border of nothing would test nothing while looking like it passed.
	Check(obvr::render::BorderThickness(8, 8) >= 2,
	      "a tiny texture still gets a visible border");
	Check(obvr::render::BorderThickness(8, 8) <= 2,
	      "and not one that fills it entirely");
}

void TestInsetFrameIsWhereItCanBeSeen() {
	std::printf("The inset frame, which exists because the outer one could not be seen\n");

	// Found by wearing the thing. The border at the extreme edge is
	// arithmetically perfect and every check in this file passed on it, but a
	// real headset shows neither the corners nor much of the edges: the optics
	// do not reach them and the face gasket covers what is left. So there is a
	// second frame, far enough in to be visible.
	const UInt32 offset = obvr::render::InsetFrameOffset(kWidth, kHeight);
	const UInt32 thickness = obvr::render::BorderThickness(kWidth, kHeight);
	const UInt32 shorter = kWidth < kHeight ? kWidth : kHeight;

	Check(offset > thickness * 2 - 1,
	      "the inset frame is clear of the border, not merged into one thick edge");

	// The two numbers that make it useful: far enough in to be inside the
	// lens view, far enough out to still say something about the picture.
	Check(offset >= shorter / 16, "it is far enough from the edge to be visible");
	Check(offset <= shorter / 4, "and not so far in that it says nothing about the edges");

	// All four sides, because the question it answers is which one is missing.
	const Pixel onTop = At(kWidth / 2, offset);
	const Pixel onBottom = At(kWidth / 2, kHeight - offset - 1);
	const Pixel onLeft = At(offset, kHeight / 2);
	const Pixel onRight = At(kWidth - offset - 1, kHeight / 2);

	Check(!IsGrey(onTop), "the top of the inset frame is coloured, not ramp");
	Check(!IsGrey(onBottom), "and the bottom");
	Check(!IsGrey(onLeft), "and the left");
	Check(!IsGrey(onRight), "and the right");

	// Cyan, so it cannot be confused with the white border at a glance -
	// telling which of the two is visible is the entire reason for having both.
	Check(onTop.b > onTop.r && onTop.g > onTop.r, "it is cyan rather than white");
	Check(!SameColour(onTop, Pixel{255, 255, 255, 255}), "and definitely not white");

	// It has to be a frame rather than a filled block, or it covers the ramp
	// and the markers.
	Check(IsGrey(At(kWidth / 2, offset + thickness + 2)),
	      "just inside the inset frame is ramp again");
}

void TestCentreCross() {
	std::printf("The centre cross, for the projection work after this\n");

	// Not needed to get a picture into the headset. Needed to judge where the
	// two eyes' pictures sit relative to each other once the eye offsets and
	// projection go in, which is the next step - and a thin cross is far
	// easier to judge that against than a ramp or a coloured square.
	const Pixel centre = At(kWidth / 2, kHeight / 2);
	Check(!IsGrey(centre) || centre.r > 200, "the exact centre is marked");
	Check(centre.r == 255 && centre.g == 255 && centre.b == 255, "the cross is white");

	// Arms in both directions, so it reads as a cross rather than a dot.
	const UInt32 arm = (kWidth < kHeight ? kWidth : kHeight) / 12;
	Check(At(kWidth / 2, kHeight / 2 - arm + 1).r == 255, "it has a vertical arm");
	Check(At(kWidth / 2 - arm + 1, kHeight / 2).r == 255, "and a horizontal one");

	// And it stops. A cross reaching across the picture would be a grid.
	Check(IsGrey(At(kWidth / 2 + arm * 3, kHeight / 2)),
	      "the arms end well before the edge");
}

void TestRampRunsDownwards() {
	std::printf("The ramp, which says which way up it is\n");

	const UInt32 thickness = obvr::render::BorderThickness(kWidth, kHeight);

	// Sampled at the far left of the picture, inside the border but away from
	// the marker, so only the ramp is under test.
	const UInt32 x = thickness + 1;
	const UInt32 high = kHeight / 2;
	const UInt32 low = kHeight - thickness - 1;

	const Pixel above = At(x, high);
	const Pixel below = At(x, low);

	Check(IsGrey(above) && IsGrey(below), "the ramp is neutral grey, not tinted");
	Check(below.r > above.r, "lower down is brighter, so upside down is obvious");

	// Monotonic, not merely different at two points: a ramp that brightened
	// and then darkened again would pass a two-point check and still be
	// useless for telling up from down.
	bool monotonic = true;
	UInt8 previous = 0;
	for (UInt32 y = thickness; y < kHeight - thickness; ++y) {
		const Pixel pixel = At(x, y);
		if (pixel.r < previous) {
			monotonic = false;
			break;
		}
		previous = pixel.r;
	}
	Check(monotonic, "and it never gets darker on the way down");

	// The ramp is scaled across the full height, so the value at a row is
	// arithmetic the test can reproduce without knowing the border.
	Check(At(x, kHeight - 1 - 0).a == 255, "every pixel is opaque");
	Check(obvr::render::PatternPixel(x, kHeight - 1, kWidth, kHeight, Eye::Left).a == 255,
	      "including at the very bottom");
}

void TestTheEyesAreTellableApart() {
	std::printf("The markers, which say left from right\n");

	// The left eye's marker sits in the left half, the right eye's in the
	// right half. Somewhere inside each.
	const UInt32 y = kHeight / 4;
	const UInt32 leftX = kWidth / 4;
	const UInt32 rightX = (kWidth * 3) / 4;

	const Pixel leftEyeAtLeft = At(leftX, y, Eye::Left);
	const Pixel rightEyeAtRight = At(rightX, y, Eye::Right);

	Check(leftEyeAtLeft.r > leftEyeAtLeft.g && leftEyeAtLeft.r > leftEyeAtLeft.b,
	      "the left eye's marker is red");
	Check(rightEyeAtRight.g > rightEyeAtRight.r && rightEyeAtRight.g > rightEyeAtRight.b,
	      "the right eye's marker is green");

	// The decisive property: at the same place the two eyes differ. Two eyes
	// showing an identical picture would hide a swap, and a swap is the
	// fault this exists to catch.
	Check(!SameColour(At(leftX, y, Eye::Left), At(leftX, y, Eye::Right)),
	      "the same spot looks different to each eye");

	// Off centre horizontally, so a mirrored image moves the marker across.
	Check(SameColour(At(rightX, y, Eye::Left), At(rightX, y, Eye::Left)),
	      "the left eye's marker is not on the right as well");
	const Pixel leftEyeAtRight = At(rightX, y, Eye::Left);
	Check(IsGrey(leftEyeAtRight), "the left eye's right side is plain ramp");

	// Above centre vertically, so a vertical flip shows without a reference.
	Check(!IsGrey(At(leftX, kHeight / 4, Eye::Left)), "the marker sits in the upper part");
	Check(IsGrey(At(leftX, (kHeight * 3) / 4, Eye::Left)), "and not in the lower part");
}

void TestOutOfRangeIsClamped() {
	std::printf("Sizes and coordinates that should not crash anything\n");

	// A generator is not the place to discover a bad size. Clamping keeps a
	// fencepost error looking like a fencepost error rather than a crash.
	Check(SameColour(At(kWidth, kHeight / 2), At(kWidth - 1, kHeight / 2)),
	      "an x past the right edge reads as the right edge");
	Check(SameColour(At(kWidth / 2, kHeight * 4), At(kWidth / 2, kHeight - 1)),
	      "a y far past the bottom reads as the bottom");

	const Pixel degenerate = obvr::render::PatternPixel(0, 0, 0, 0, Eye::Left);
	Check(degenerate.a == 255, "a zero-sized image still returns an opaque pixel");
}

void TestBufferSize() {
	std::printf("How big a buffer the pattern needs\n");

	Check(obvr::render::PatternBufferBytes(100, 50) == 100u * 50u * 4u,
	      "an ordinary size is width times height times four");
	Check(obvr::render::PatternBufferBytes(0, 100) == 0, "a zero width is refused");
	Check(obvr::render::PatternBufferBytes(100, 0) == 0, "a zero height is refused");

	// The one that matters. Without the dimension cap this multiplication
	// wraps, the allocation comes out far too small, and the fill writes
	// straight past the end of it - in a 32-bit process, which is what OBVR
	// always is.
	Check(obvr::render::PatternBufferBytes(50000, 50000) == 0,
	      "a size that would overflow the multiplication is refused, not wrapped");
	Check(obvr::render::PatternBufferBytes(16384, 16384) == 0,
	      "and so is one that fits the arithmetic but not a sane budget");

	// A real headset size has to survive all of that.
	Check(obvr::render::PatternBufferBytes(2160, 2160) > 0,
	      "a plausible per-eye resolution is accepted");
}

void TestFillRespectsRowPitch() {
	std::printf("Filling a buffer whose rows are padded\n");

	// Direct3D is free to make a texture row wider than the pixels in it.
	// Writing rows back to back regardless is the classic way to get a
	// picture that shears diagonally - and it looks like a projection fault
	// rather than a stride fault, which is why it costs a day.
	constexpr UInt32 width = 16;
	constexpr UInt32 height = 8;
	constexpr UInt32 padding = 12;
	constexpr UInt32 pitch = width * 4 + padding;

	UInt8 buffer[pitch * height];
	for (UInt32 i = 0; i < sizeof(buffer); ++i) {
		buffer[i] = 0xAB;
	}

	obvr::render::FillPattern(buffer, width, height, pitch, Eye::Left);

	bool rowsMatch = true;
	bool paddingUntouched = true;
	for (UInt32 y = 0; y < height && rowsMatch; ++y) {
		for (UInt32 x = 0; x < width; ++x) {
			const Pixel expected =
				obvr::render::PatternPixel(x, y, width, height, Eye::Left);
			const UInt8* pixel = buffer + y * pitch + x * 4;
			if (pixel[0] != expected.r || pixel[1] != expected.g || pixel[2] != expected.b ||
			    pixel[3] != expected.a) {
				rowsMatch = false;
				break;
			}
		}
		for (UInt32 i = width * 4; i < pitch; ++i) {
			if (buffer[y * pitch + i] != 0xAB) {
				paddingUntouched = false;
			}
		}
	}

	Check(rowsMatch, "every row lands at its own offset, not packed together");
	Check(paddingUntouched, "and the padding between rows is left alone");

	// A pitch too small to hold a row would mean writing each row over the
	// previous one. Treating it as tightly packed is the only reading that
	// cannot corrupt memory.
	UInt8 packed[width * 4 * height];
	for (UInt32 i = 0; i < sizeof(packed); ++i) {
		packed[i] = 0;
	}
	obvr::render::FillPattern(packed, width, height, 0, Eye::Left);
	const Pixel corner = obvr::render::PatternPixel(0, 0, width, height, Eye::Left);
	Check(packed[0] == corner.r && packed[3] == corner.a,
	      "a pitch of zero is read as tightly packed rather than trusted");

	// A null buffer has to be survivable: the allocation above it can fail.
	obvr::render::FillPattern(nullptr, width, height, pitch, Eye::Left);
	Check(true, "filling a null buffer does nothing rather than crashing");
}

}  // namespace

int main() {
	std::printf("OBVR test pattern test\n\n");

	TestBorderReachesEveryEdge();
	std::printf("\n");
	TestInsetFrameIsWhereItCanBeSeen();
	std::printf("\n");
	TestCentreCross();
	std::printf("\n");
	TestRampRunsDownwards();
	std::printf("\n");
	TestTheEyesAreTellableApart();
	std::printf("\n");
	TestOutOfRangeIsClamped();
	std::printf("\n");
	TestBufferSize();
	std::printf("\n");
	TestFillRespectsRowPitch();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}

#include "render/TestPattern.h"

namespace obvr::render {
namespace {

constexpr UInt8 kOpaque = 255;

// White, so a cut edge is obvious against the ramp behind it at either end.
constexpr Pixel kBorder{255, 255, 255, kOpaque};

// Cyan for the inset frame, so it cannot be mistaken for the outer border at
// a glance - the two answer the same question from different distances, and
// telling which one is visible is the whole point of having both.
constexpr Pixel kInsetFrame{0, 200, 230, kOpaque};

// The centre cross, white and thin. Bright against the middle of the ramp,
// which sits at mid grey.
constexpr Pixel kCross{255, 255, 255, kOpaque};

// One colour per eye, far enough apart to name without thinking. Not pure red
// and green: a fully saturated primary makes it hard to tell a dim panel from
// a dark image, whereas a colour with some of the other channels in it still
// looks like itself when the brightness is off.
constexpr Pixel kLeftMarker{230, 60, 60, kOpaque};
constexpr Pixel kRightMarker{60, 220, 90, kOpaque};

UInt32 Clamp(UInt32 value, UInt32 max) { return value > max ? max : value; }

// Distance between two unsigned values without going below zero on the way.
UInt32 Distance(UInt32 a, UInt32 b) { return a > b ? a - b : b - a; }

// Well above any headset in existence and well below where the arithmetic
// stops being safe. 16384 squared is 268 million pixels, which times four
// bytes still fits in 32 bits with room to spare - so once both dimensions
// are past this check the multiplication cannot overflow.
constexpr UInt32 kMaxDimension = 16384;

// A quarter of a gigabyte for one eye. Nothing legitimate comes near it, and
// a 32-bit process that tried would fail somewhere far less informative.
constexpr UInt32 kMaxBufferBytes = 256u * 1024u * 1024u;

}  // namespace

UInt32 PatternBufferBytes(UInt32 width, UInt32 height) {
	if (width == 0 || height == 0) {
		return 0;
	}
	if (width > kMaxDimension || height > kMaxDimension) {
		return 0;
	}

	// Safe by the check above, and in that order: the cap is what makes the
	// multiplication provable rather than hopeful.
	const UInt32 bytes = width * height * 4u;
	if (bytes > kMaxBufferBytes) {
		return 0;
	}
	return bytes;
}

UInt32 BorderThickness(UInt32 width, UInt32 height) {
	const UInt32 shorter = width < height ? width : height;

	// A sixty-fourth of the shorter side, but never thinner than two pixels
	// and never so thick that it swallows the picture. The lower bound
	// matters: at a small size the division rounds to zero, and a border of
	// nothing tests nothing while looking like it passed.
	UInt32 thickness = shorter / 64u;
	if (thickness < 2u) {
		thickness = 2u;
	}

	const UInt32 limit = shorter / 4u;
	if (limit > 0u && thickness > limit) {
		thickness = limit;
	}
	return thickness;
}

UInt32 InsetFrameOffset(UInt32 width, UInt32 height) {
	const UInt32 shorter = width < height ? width : height;
	const UInt32 offset = shorter / 8u;

	// It has to sit outside the border it is inset from, or the two merge
	// into one thick frame and neither says anything the other does not.
	const UInt32 minimum = BorderThickness(width, height) * 2u;
	return offset > minimum ? offset : minimum;
}

Pixel PatternPixel(UInt32 x, UInt32 y, UInt32 width, UInt32 height, Eye eye, float crossU) {
	if (width == 0u || height == 0u) {
		return Pixel{0, 0, 0, kOpaque};
	}

	x = Clamp(x, width - 1u);
	y = Clamp(y, height - 1u);

	const UInt32 thickness = BorderThickness(width, height);
	const bool onBorder = x < thickness || y < thickness || x >= width - thickness ||
	                      y >= height - thickness;
	if (onBorder) {
		return kBorder;
	}

	// The inset frame, which is the one a wearer can actually see. A rectangle
	// outline: inside the outer rectangle but outside the inner one.
	const UInt32 offset = InsetFrameOffset(width, height);
	if (offset * 2u + thickness * 2u < width && offset * 2u + thickness * 2u < height) {
		const bool insideOuter =
			x >= offset && x < width - offset && y >= offset && y < height - offset;
		const bool insideInner = x >= offset + thickness && x < width - offset - thickness &&
		                         y >= offset + thickness && y < height - offset - thickness;
		if (insideOuter && !insideInner) {
			return kInsetFrame;
		}
	}

	// The cross, placed on the eye's optical axis rather than in the middle of
	// the image. A headset's frustum is asymmetric because the lens points
	// slightly outwards, so the two differ - measured at 0.583 and 0.424 on
	// the headset this was built against. Putting the cross on the axis makes
	// the two fuse into one when the wearer looks straight ahead, which turns
	// a question about eye setup into something answerable by looking.
	//
	// Clamped rather than trusted: a garbled projection would otherwise put
	// the cross outside the texture, where it would look like no cross at all
	// and be read as a rendering failure.
	float clampedU = crossU;
	if (!(clampedU > 0.05f)) {
		clampedU = 0.05f;
	}
	if (clampedU > 0.95f) {
		clampedU = 0.95f;
	}

	// Vertically it stays in the middle. The vertical axis would need the sign
	// convention of GetProjectionRaw's top and bottom, and that is not
	// determinable from what a headset reports: both readings give identical
	// numbers and differ only in which way is up. Left at the middle rather
	// than guessed - and it costs nothing, because 0.1.0 will take a ready
	// made matrix from GetProjectionMatrix and never ask the question.
	const UInt32 centreX = static_cast<UInt32>(clampedU * static_cast<float>(width));
	const UInt32 centreY = height / 2u;
	const UInt32 arm = (width < height ? width : height) / 12u;
	const UInt32 halfThickness = thickness > 1u ? thickness / 2u : 1u;

	const UInt32 fromCentreX = Distance(x, centreX);
	const UInt32 fromCentreY = Distance(y, centreY);
	if ((fromCentreX <= halfThickness && fromCentreY <= arm) ||
	    (fromCentreY <= halfThickness && fromCentreX <= arm)) {
		return kCross;
	}

	// The marker sits in the upper third and on its own side. Upper rather
	// than centred so that a vertical flip shows without anything to compare
	// against; on its own side so that a mirrored image moves it across.
	const UInt32 markerTop = height / 6u;
	const UInt32 markerBottom = height / 3u;
	const UInt32 markerLeft = eye == Eye::Left ? width / 8u : (width * 5u) / 8u;
	const UInt32 markerRight = eye == Eye::Left ? (width * 3u) / 8u : (width * 7u) / 8u;

	if (y >= markerTop && y < markerBottom && x >= markerLeft && x < markerRight) {
		return eye == Eye::Left ? kLeftMarker : kRightMarker;
	}

	// Everything else is the ramp: black at the top, white at the bottom.
	//
	// Scaled across the full height rather than across the area inside the
	// border, so that the value at a given row does not depend on how thick
	// the border happens to be. That keeps the ramp checkable by arithmetic
	// rather than by reproducing the border calculation.
	const UInt32 span = height > 1u ? height - 1u : 1u;
	const UInt8 level = static_cast<UInt8>((y * 255u) / span);
	return Pixel{level, level, level, kOpaque};
}

void FillPattern(UInt8* pixels, UInt32 width, UInt32 height, UInt32 rowPitch, Eye eye,
                 float crossU) {
	if (pixels == nullptr) {
		return;
	}

	// A pitch smaller than the row would mean writing each row over the
	// previous one. Treating it as tightly packed is the only reading that
	// cannot corrupt memory, and the caller passing zero almost certainly
	// meant exactly that.
	if (rowPitch < width * 4u) {
		rowPitch = width * 4u;
	}

	for (UInt32 y = 0; y < height; ++y) {
		UInt8* row = pixels + static_cast<UInt32>(y * rowPitch);
		for (UInt32 x = 0; x < width; ++x) {
			const Pixel pixel = PatternPixel(x, y, width, height, eye, crossU);
			row[x * 4u + 0u] = pixel.r;
			row[x * 4u + 1u] = pixel.g;
			row[x * 4u + 2u] = pixel.b;
			row[x * 4u + 3u] = pixel.a;
		}
	}
}

}  // namespace obvr::render

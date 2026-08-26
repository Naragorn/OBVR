// Checks the arithmetic that turns what the headset says about the eyes into
// figures a person or a renderer can use.
//
// Nothing here is needed to put a picture in the headset - that already
// works. It is needed for the milestone after, which renders Oblivion's world
// twice with these numbers, and the point of checking it now is that a wrong
// frustum does not fail. It produces a picture that is subtly off, and subtly
// off in VR is felt rather than seen.
//
// Pure arithmetic, so this runs on Linux as well.

#include <cmath>
#include <cstdio>

#include "render/EyeGeometry.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void CheckNear(float actual, float expected, float tolerance, const char* what) {
	if (std::fabs(actual - expected) > tolerance) {
		std::printf("        expected %.4f, got %.4f\n", static_cast<double>(expected),
		            static_cast<double>(actual));
	}
	Check(std::fabs(actual - expected) <= tolerance, what);
}

using obvr::render::EyeProjection;

void TestFieldOfView() {
	std::printf("Turning frustum tangents into degrees\n");

	// A tangent of 1 is 45 degrees, so edges at -1 and +1 span 90.
	EyeProjection square;
	square.left = -1.0f;
	square.right = 1.0f;
	square.top = -1.0f;
	square.bottom = 1.0f;

	CheckNear(obvr::render::HorizontalFovDegrees(square), 90.0f, 0.01f,
	          "edges at -1 and +1 span 90 degrees");
	CheckNear(obvr::render::VerticalFovDegrees(square), 90.0f, 0.01f,
	          "and the same vertically");

	// The check that matters, and the one a plausible shortcut gets wrong.
	// Taking atan of the total width instead of adding the two atans gives
	// atan(2) = 63.4 degrees here rather than 90 - the tangent is not linear,
	// and at the angles a headset uses the gap is not a rounding difference.
	Check(obvr::render::HorizontalFovDegrees(square) > 80.0f,
	      "the edges are converted separately, not summed before the atan");

	// An asymmetric frustum, which is what a real headset has: the lens looks
	// slightly outwards, so one edge is further than the other.
	EyeProjection lopsided;
	lopsided.left = -1.2f;
	lopsided.right = 1.0f;
	lopsided.top = -1.1f;
	lopsided.bottom = 1.1f;

	const float wide = obvr::render::HorizontalFovDegrees(lopsided);
	CheckNear(wide, 45.0f + 50.194f, 0.05f, "an asymmetric frustum adds its two halves");
	Check(wide > obvr::render::HorizontalFovDegrees(square),
	      "and covers more than the symmetric one it extends");

	// The convention-free property. Whatever the signs mean, a frustum of a
	// given width spans a given angle - which is what makes this figure
	// readable in a log before the convention has been settled.
	EyeProjection shifted;
	shifted.left = 0.0f;
	shifted.right = 2.0f;
	Check(obvr::render::HorizontalFovDegrees(shifted) > 0.0f,
	      "a frustum with both edges on one side still reports an angle");
}

void TestAsymmetry() {
	std::printf("How lopsided the frustum is\n");

	EyeProjection symmetric;
	symmetric.left = -1.0f;
	symmetric.right = 1.0f;
	symmetric.top = -1.0f;
	symmetric.bottom = 1.0f;

	CheckNear(obvr::render::HorizontalAsymmetry(symmetric), 0.0f, 0.0001f,
	          "a symmetric frustum reports no asymmetry");
	CheckNear(obvr::render::VerticalAsymmetry(symmetric), 0.0f, 0.0001f,
	          "in either direction");

	// More frustum on the left than the right, which is what an eye looking
	// slightly outwards produces.
	EyeProjection outward;
	outward.left = -1.3f;
	outward.right = 1.0f;
	CheckNear(obvr::render::HorizontalAsymmetry(outward), -0.3f, 0.0001f,
	          "an eye with more frustum to the left reports it, and with a sign");
}

void TestOpticalCentre() {
	std::printf("Where the eye actually looks in the texture\n");

	EyeProjection symmetric;
	symmetric.left = -1.0f;
	symmetric.right = 1.0f;
	symmetric.top = -1.0f;
	symmetric.bottom = 1.0f;

	CheckNear(obvr::render::OpticalCentreU(symmetric), 0.5f, 0.0001f,
	          "a symmetric frustum looks at the middle of the image");

	// The reason this function exists. A headset frustum is not symmetric, so
	// the middle of the texture is not where the eye looks - and a centring
	// mark placed at the texture's middle is therefore in the wrong place,
	// which matters because that mark is what the eye offsets get judged
	// against.
	EyeProjection outward;
	outward.left = -1.5f;
	outward.right = 1.0f;
	const float u = obvr::render::OpticalCentreU(outward);
	CheckNear(u, 0.6f, 0.0001f, "more frustum to the left moves the view axis right of centre");
	Check(u > 0.5f, "which is not the middle of the texture");

	// Both readings of the vertical convention, because it is not settled.
	EyeProjection lopsidedV;
	lopsidedV.top = -1.5f;
	lopsidedV.bottom = 1.0f;

	const float vAsNamed = obvr::render::OpticalCentreV(lopsidedV, false);
	const float vIfBackwards = obvr::render::OpticalCentreV(lopsidedV, true);
	CheckNear(vAsNamed, 0.6f, 0.0001f, "read as named, the centre sits below the middle");
	CheckNear(vIfBackwards, 0.4f, 0.0001f, "read as inverted, it sits above it");
	Check(vAsNamed != vIfBackwards,
	      "the two readings genuinely differ, which is why it is a flag and not a guess");

	// A degenerate frustum must not divide by zero. It should not happen -
	// the backend refuses one - but a mark placed at a NaN is far worse than
	// one placed at the middle.
	EyeProjection flat;
	flat.left = 1.0f;
	flat.right = 1.0f;
	CheckNear(obvr::render::OpticalCentreU(flat), 0.5f, 0.0001f,
	          "a frustum with no width falls back to the middle rather than dividing by zero");
}

void TestMonoBounds() {
	std::printf("Sharing one picture between two eyes\n");

	// The numbers are the measured ones: a real headset put the optical axes
	// at 0.583 across the left eye's view and 0.424 across the right. Handing
	// both eyes the whole texture put Oblivion's picture at the middle of
	// each view, which is where neither eye is looking - and the wearer saw
	// one image displaced against the other.
	constexpr float kLeftAxis = 0.583f;
	constexpr float kRightAxis = 0.424f;
	constexpr float kWidth = 0.8f;

	const auto left = obvr::render::MonoBounds(kLeftAxis, kWidth);
	const auto right = obvr::render::MonoBounds(kRightAxis, kWidth);

	// The property the whole thing exists for: the centre of the source image
	// lands on the eye's axis. Everything else is a consequence.
	CheckNear(left.uMin + kLeftAxis * (left.uMax - left.uMin), 0.5f, 0.001f,
	          "the picture's centre lands on the left eye's axis");
	CheckNear(right.uMin + kRightAxis * (right.uMax - right.uMin), 0.5f, 0.001f,
	          "and on the right eye's");

	// Both eyes must see the same amount of picture. Different widths would
	// mean different magnifications, and nothing fuses two images at
	// different sizes - that would be worse than the misalignment it set out
	// to fix.
	CheckNear(left.uMax - left.uMin, right.uMax - right.uMin, 0.001f,
	          "both eyes get the same width, so the two match in scale");

	// The eyes are given different parts, and which way round matters: the
	// left eye's axis sits further right across its view, so its share of the
	// texture has to start further left to compensate.
	Check(left.uMin < right.uMin, "the left eye is given the left part of the picture");
	Check(left.uMax < right.uMax, "and the right eye the right part");

	// Inside the texture, or the driver samples past the edge and smears the
	// last pixel across the view.
	Check(left.uMin >= 0.0f && left.uMax <= 1.0f, "the left bounds stay inside the texture");
	Check(right.uMin >= 0.0f && right.uMax <= 1.0f, "and the right bounds too");

	// Vertical is untouched. The two eyes' vertical asymmetries were measured
	// at -0.406 and -0.384, near enough identical that there is nothing
	// between them to correct - and cropping it would only throw picture away.
	CheckNear(left.vMin, 0.0f, 0.0001f, "the full height is used");
	CheckNear(left.vMax, 1.0f, 0.0001f, "top to bottom");

	// An axis at the centre needs no correction at all, which is the case
	// that says the arithmetic is not doing something arbitrary.
	const auto centred = obvr::render::MonoBounds(0.5f, 1.0f);
	CheckNear(centred.uMin, 0.0f, 0.001f, "a centred axis at full width uses the whole texture");
	CheckNear(centred.uMax, 1.0f, 0.001f, "with nothing cropped");

	// A width of 1 with an off-centre axis would need bounds outside the
	// texture. Sliding rather than shrinking keeps both eyes the same size,
	// at the cost of not quite reaching the centre - the lesser of the two
	// faults.
	const auto impossible = obvr::render::MonoBounds(0.583f, 1.0f);
	Check(impossible.uMin >= 0.0f, "an impossible correction is slid into range");
	CheckNear(impossible.uMax - impossible.uMin, 1.0f, 0.001f, "rather than shrunk");

	// Nonsense widths are clamped rather than obeyed. A width of zero would
	// leave no picture, and one above 1 would sample off the texture.
	const auto tooWide = obvr::render::MonoBounds(0.5f, 5.0f);
	Check(tooWide.uMax - tooWide.uMin <= 1.0f, "a width above 1 is clamped");
	const auto tooNarrow = obvr::render::MonoBounds(0.5f, 0.0f);
	Check(tooNarrow.uMax - tooNarrow.uMin >= 0.2f, "and a width of zero does not blank the view");
}

void TestInterpupillaryDistance() {
	std::printf("The distance between the eyes\n");

	// A typical headset: the eyes sit symmetrically either side of the head
	// origin, so the offsets are half the IPD each.
	const obvr::NiPoint3 left{-0.0325f, 0.0f, 0.0f};
	const obvr::NiPoint3 right{0.0325f, 0.0f, 0.0f};

	CheckNear(obvr::render::InterpupillaryDistance(left, right), 0.065f, 0.0001f,
	          "two offsets of 32.5 mm give an IPD of 65 mm");
	Check(obvr::render::IsPlausibleIpd(0.065f), "65 mm is a plausible human IPD");

	// Order must not matter: it is a distance, not a displacement.
	CheckNear(obvr::render::InterpupillaryDistance(right, left), 0.065f, 0.0001f,
	          "and swapping the eyes gives the same distance");

	// The check is there to catch a misread matrix, and a misread matrix is
	// out by a factor rather than by millimetres.
	Check(!obvr::render::IsPlausibleIpd(0.65f),
	      "an IPD ten times too large is called out rather than used");
	Check(!obvr::render::IsPlausibleIpd(0.0f),
	      "and so is one of zero, which is what a matrix of zeroes would give");
	Check(obvr::render::IsPlausibleIpd(0.052f) && obvr::render::IsPlausibleIpd(0.078f),
	      "the range is wide enough for the adults it has to cover");

	// The eyes are not always exactly level or exactly in line, so the
	// distance has to be the real one rather than the horizontal component.
	const obvr::NiPoint3 tilted{0.0325f, 0.002f, 0.001f};
	Check(obvr::render::InterpupillaryDistance(left, tilted) > 0.065f,
	      "an offset that is not purely sideways still measures the full distance");
}

// The headset this was built against, as it reported itself. Kept as
// measurements rather than as round numbers because the arithmetic below is
// checked against what a person actually saw through these lenses.
obvr::render::EyeProjection MeasuredLeftEye() {
	obvr::render::EyeProjection eye;
	eye.left = -1.1518f;
	eye.right = 0.8248f;
	eye.top = -1.1991f;
	eye.bottom = 0.7927f;
	return eye;
}

void TestPlacePicture() {
	std::printf("Placing the game's picture in an eye's view\n");

	using obvr::render::PicturePlacement;
	using obvr::render::PlacePicture;

	// Oblivion as it is actually configured on this machine: 2560x1440 at
	// fDefaultFOV 75.
	const PicturePlacement measured = PlacePicture(MeasuredLeftEye(), 75.0f, 2560, 1440);

	// tan(37.5) = 0.76733 across, and 0.5625 of that down for a 16:9 frame,
	// against an eye frustum 1.9766 wide and 1.9918 tall.
	CheckNear(measured.uMax - measured.uMin, 0.7764f, 0.001f,
	          "the picture covers 78% of the eye's width");
	CheckNear(measured.vMax - measured.vMin, 0.4334f, 0.001f,
	          "and 43% of its height");

	// Centred on the view axis, not on the texture. The horizontal figure is
	// the same 0.583 that placed the test pattern's cross, and that cross was
	// seen centred in a headset - so this half of the arithmetic has been
	// checked against a person's eyes.
	CheckNear((measured.uMin + measured.uMax) * 0.5f, 0.5827f, 0.001f,
	          "the picture is centred on the eye's view axis, not the texture's middle");
	CheckNear((measured.vMin + measured.vMax) * 0.5f, 0.6020f, 0.001f,
	          "vertically too, on the assumption that the top edge is the one at v=0");

	Check(!measured.cropped, "nothing is cut away, because 75 degrees fits inside 89");
	CheckNear(measured.sourceUMin, 0.0f, 0.0001f, "so the whole frame is used across");
	CheckNear(measured.sourceUMax, 1.0f, 0.0001f, "all of it");
	CheckNear(measured.sourceVMin, 0.0f, 0.0001f, "and all of it down");
	CheckNear(measured.sourceVMax, 1.0f, 0.0001f, "as well");

	// Everything inside the texture, or the destination rectangle is not a
	// picture that overhangs but an invalid call.
	Check(measured.uMin >= 0.0f && measured.uMax <= 1.0f, "and it stays inside the texture");
	Check(measured.vMin >= 0.0f && measured.vMax <= 1.0f, "in both directions");
}

void TestPlacePictureScale() {
	std::printf("The scale that made the world too big\n");

	using obvr::render::PlacePicture;

	const obvr::render::EyeProjection eye = MeasuredLeftEye();
	const obvr::render::PicturePlacement placement = PlacePicture(eye, 75.0f, 2560, 1440);

	// In tangents, not in degrees. An eye's view is linear in the tangent of
	// the angle and not in the angle itself, so comparing degrees per fraction
	// of the view answers a slightly different question and answers it wrong -
	// by about 10% at these angles, which is enough to make a correct
	// placement look broken.
	const float tanHalfWidth = std::tan(37.5f * 3.14159265f / 180.0f);
	const float tanHalfHeight = tanHalfWidth * 1440.0f / 2560.0f;
	const float eyeWidth = eye.right - eye.left;
	const float eyeHeight = eye.bottom - eye.top;

	// Magnification: how much wider the world appears than it should. One
	// means life-sized, and it has to be one in both axes - a picture that is
	// merely large can be got used to, one that is stretched cannot.
	const float across = (2.0f * tanHalfWidth / (placement.uMax - placement.uMin)) / eyeWidth;
	const float down = (2.0f * tanHalfHeight / (placement.vMax - placement.vMin)) / eyeHeight;

	CheckNear(across, 1.0f, 0.005f, "the world is life-sized across");
	CheckNear(down, 1.0f, 0.005f, "and life-sized down");
	CheckNear(across / down, 1.0f, 0.005f, "so it is not stretched in either direction");

	// What it was before, as numbers rather than as a memory. The previous
	// code gave each eye a fixed 80% of the frame's width and all of its
	// height, which is where the report of a world that was too big and a HUD
	// that had left the view came from.
	const float oldAcross = eyeWidth / (0.8f * 2.0f * tanHalfWidth);
	const float oldDown = eyeHeight / (1.0f * 2.0f * tanHalfHeight);

	CheckNear(oldAcross, 1.61f, 0.02f, "the fixed bounds magnified the world 1.61 times across");
	CheckNear(oldDown, 2.31f, 0.02f, "and 2.31 times down");
	CheckNear(oldDown / oldAcross, 1.43f, 0.02f,
	          "which stretched it by 1.43 - the fault this replaces");
}
void TestPlacePictureCrop() {
	std::printf("A game wider than the headset can show\n");

	using obvr::render::PicturePlacement;
	using obvr::render::PlacePicture;

	// A square, symmetric eye two units wide, so a field of view whose tangent
	// exceeds 1 in an axis cannot fit in that axis.
	obvr::render::EyeProjection eye;
	eye.left = -1.0f;
	eye.right = 1.0f;
	eye.top = -1.0f;
	eye.bottom = 1.0f;

	// 140 degrees across a square frame: tan(70) is 2.75, well past the
	// frustum, so both axes overflow.
	const PicturePlacement wide = PlacePicture(eye, 140.0f, 1000, 1000);

	Check(wide.cropped, "a field of view wider than the eye's is reported as cropped");
	Check(wide.uMin >= 0.0f && wide.uMax <= 1.0f, "and the destination is pulled inside");
	Check(wide.vMin >= 0.0f && wide.vMax <= 1.0f, "in both axes");

	// Cut out of the source in the same proportion. Trimming only the
	// destination would squeeze the picture instead of cropping it, which
	// looks like a lens fault rather than like a missing edge.
	Check(wide.sourceUMin > 0.0f && wide.sourceUMax < 1.0f,
	      "the source is trimmed to match, rather than the picture being squeezed");
	CheckNear(wide.sourceUMin, 1.0f - wide.sourceUMax, 0.001f,
	          "and symmetrically, for a symmetric frustum");

	// A field of view that exactly fills the eye leaves nothing over and cuts
	// nothing off. tan(45) is 1, which is the frustum's own edge.
	const PicturePlacement exact = PlacePicture(eye, 90.0f, 1000, 1000);
	Check(!exact.cropped, "a field of view matching the eye's fits exactly");
	CheckNear(exact.uMin, 0.0f, 0.001f, "filling the texture from the left edge");
	CheckNear(exact.uMax, 1.0f, 0.001f, "to the right");
}

void TestPlacePictureRefusals() {
	std::printf("Placements that cannot be computed\n");

	using obvr::render::PicturePlacement;
	using obvr::render::PlacePicture;

	obvr::render::EyeProjection eye;
	eye.left = -1.0f;
	eye.right = 1.0f;
	eye.top = -1.0f;
	eye.bottom = 1.0f;

	// Nothing sensible can be said about these, and the fallback fills the
	// texture - wrong, but wrong in a way that still shows a picture rather
	// than handing Direct3D a rectangle of no area.
	const PicturePlacement noFrame = PlacePicture(eye, 75.0f, 0, 1440);
	CheckNear(noFrame.uMax - noFrame.uMin, 1.0f, 0.0001f,
	          "a frame with no width falls back to the whole texture");

	const PicturePlacement noAngle = PlacePicture(eye, 0.0f, 2560, 1440);
	CheckNear(noAngle.uMax - noAngle.uMin, 1.0f, 0.0001f,
	          "and so does a field of view of zero");

	const PicturePlacement straightUp = PlacePicture(eye, 180.0f, 2560, 1440);
	CheckNear(straightUp.uMax - straightUp.uMin, 1.0f, 0.0001f,
	          "and one of 180 degrees, whose tangent is not a number");

	// A frustum with no extent, which would divide by zero.
	obvr::render::EyeProjection flat;
	flat.left = 0.0f;
	flat.right = 0.0f;
	flat.top = -1.0f;
	flat.bottom = 1.0f;
	const PicturePlacement noEye = PlacePicture(flat, 75.0f, 2560, 1440);
	CheckNear(noEye.uMax - noEye.uMin, 1.0f, 0.0001f, "and an eye that sees nothing wide");
}

}  // namespace

int main() {
	std::printf("OBVR eye geometry test\n\n");

	TestFieldOfView();
	std::printf("\n");
	TestAsymmetry();
	std::printf("\n");
	TestOpticalCentre();
	std::printf("\n");
	TestMonoBounds();
	std::printf("\n");
	TestInterpupillaryDistance();
	std::printf("\n");
	TestPlacePicture();
	std::printf("\n");
	TestPlacePictureScale();
	std::printf("\n");
	TestPlacePictureCrop();
	std::printf("\n");
	TestPlacePictureRefusals();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}

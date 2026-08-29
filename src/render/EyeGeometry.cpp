#include "render/EyeGeometry.h"

#include "core/MathFns.h"

namespace obvr::render {
namespace {

float Abs(float value) { return value < 0.0f ? -value : value; }

// The angle a frustum of the given half-extents covers, in degrees.
//
// atan of each edge separately and added, rather than atan of the sum: the
// tangent is not linear, so adding the tangents and taking one atan gives a
// smaller answer than the truth, and the error grows with the angle. At the
// field of view a headset uses it is not a rounding difference.
float SpanDegrees(float lowEdge, float highEdge) {
	const float low = math::Atan(Abs(lowEdge));
	const float high = math::Atan(Abs(highEdge));
	return (low + high) * math::kRadiansToDegrees;
}

}  // namespace

float HorizontalFovDegrees(const EyeProjection& projection) {
	return SpanDegrees(projection.left, projection.right);
}

float VerticalFovDegrees(const EyeProjection& projection) {
	return SpanDegrees(projection.top, projection.bottom);
}

float HorizontalAsymmetry(const EyeProjection& projection) {
	return projection.right + projection.left;
}

float VerticalAsymmetry(const EyeProjection& projection) {
	return projection.bottom + projection.top;
}

float OpticalCentreU(const EyeProjection& projection) {
	const float width = projection.right - projection.left;
	if (width == 0.0f) {
		return 0.5f;
	}

	// Where zero - the view axis - falls between the two edges.
	return -projection.left / width;
}

float OpticalCentreV(const EyeProjection& projection, bool topIsNegative) {
	const float height = projection.bottom - projection.top;
	if (height == 0.0f) {
		return 0.5f;
	}

	const float fromTopEdge = -projection.top / height;

	// A texture's v runs downwards from the top edge. If the value named
	// "top" is in fact the negative, lower edge - which is what Valve's wiki
	// claims - then the fraction has to be measured from the other end.
	//
	// This is a flag rather than a decision because the convention is not
	// documented, and a centring mark placed on a guess is worse than one
	// placed on nothing: it looks authoritative and is off by however much
	// the frustum is asymmetric.
	return topIsNegative ? 1.0f - fromTopEdge : fromTopEdge;
}

TextureBounds MonoBounds(float opticalCentreU, float width) {
	TextureBounds bounds;

	// A width outside this range is not a crop, it is a mistake. Below a
	// quarter there would be almost no picture left; above 1 the bounds would
	// reach past the texture, which samples whatever the driver decides -
	// usually the edge pixel smeared across the view.
	if (!(width > 0.25f)) {
		width = 0.25f;
	}
	if (width > 1.0f) {
		width = 1.0f;
	}

	bounds.uMin = 0.5f - opticalCentreU * width;
	bounds.uMax = bounds.uMin + width;

	// Slide rather than shrink if that ran off an edge. Shrinking would
	// change the scale between the eyes, which is worse than an imperfect
	// centre: the two images would no longer be the same size, and nothing
	// fuses two pictures at different magnifications.
	if (bounds.uMin < 0.0f) {
		bounds.uMax -= bounds.uMin;
		bounds.uMin = 0.0f;
	}
	if (bounds.uMax > 1.0f) {
		bounds.uMin -= bounds.uMax - 1.0f;
		bounds.uMax = 1.0f;
	}

	// Vertical is left alone. The vertical asymmetry is nearly identical for
	// both eyes - measured at -0.406 and -0.384 - so it displaces both by the
	// same amount and there is nothing between them to correct. Cropping it
	// would only throw away picture.
	bounds.vMin = 0.0f;
	bounds.vMax = 1.0f;
	return bounds;
}

TextureBounds ContentBounds(UInt32 believedWidth, UInt32 believedHeight,
                            UInt32 frameWidth, UInt32 frameHeight) {
	TextureBounds bounds;
	bounds.uMin = 0.0f;
	bounds.vMin = 0.0f;
	bounds.uMax = 1.0f;
	bounds.vMax = 1.0f;

	// Each axis on its own: a frame can be wider than believed without being
	// taller, and the axis that matches stays whole.
	if (believedWidth != 0 && frameWidth != 0 && believedWidth < frameWidth) {
		bounds.uMax = static_cast<float>(believedWidth) / static_cast<float>(frameWidth);
	}
	if (believedHeight != 0 && frameHeight != 0 && believedHeight < frameHeight) {
		bounds.vMax = static_cast<float>(believedHeight) / static_cast<float>(frameHeight);
	}
	return bounds;
}

float InterpupillaryDistance(const NiPoint3& leftEye, const NiPoint3& rightEye) {
	const NiPoint3 between = rightEye - leftEye;
	return math::Sqrt(between.LengthSquared());
}

bool IsPlausibleIpd(float metres) {
	// Human interpupillary distance runs roughly 52 to 78 mm across adults,
	// and headsets allow a little beyond that. The bounds are deliberately
	// generous: this exists to catch a misread matrix, which would be out by
	// a factor rather than by a few millimetres.
	return metres > 0.045f && metres < 0.085f;
}


PicturePlacement PlacePictureFromTangents(const EyeProjection& eye, float tanHalfWidth,
                                          float tanHalfHeight) {
	PicturePlacement placement;

	// Nothing sensible can be said about a frustum with no extent. The default
	// placement fills the texture, which is what OBVR did before this function
	// existed - wrong, but wrong in a way that still shows a picture.
	if (!(tanHalfWidth > 0.0f) || !(tanHalfHeight > 0.0f)) {
		return placement;
	}

	const float eyeWidth = eye.right - eye.left;
	const float eyeHeight = eye.bottom - eye.top;
	if (!(eyeWidth > 0.0f) || !(eyeHeight > 0.0f)) {
		return placement;
	}

	// What share of the eye's view the game's picture covers, at a scale of
	// one to one. Under 1 in both axes for any ordinary field of view, and the
	// remainder is the black margin.
	const float shareU = 2.0f * tanHalfWidth / eyeWidth;
	const float shareV = 2.0f * tanHalfHeight / eyeHeight;

	// Where the eye's view axis lands in the texture.
	//
	// The horizontal figure is the same one that placed the test pattern's
	// cross, and that cross was seen centred in a headset - so u is checked
	// against a person's eyes.
	//
	// The vertical figure is now checked the same way, and it came out the
	// other way round from the obvious reading. Taking -top/height put the
	// axis at 0.602, and the picture was reported sitting too low by about
	// that much; bottom/height puts it at 0.398 instead. So the texture's v
	// grows opposite to the frustum's own vertical axis - which is exactly
	// what Valve's wiki means when it says the two edges are named backwards,
	// and it is measured here rather than deduced.
	//
	// A consistency check that agrees: |top| exceeds |bottom| on both eyes of
	// this headset, so under this reading the eye sees further down than up.
	// That is the usual shape of a headset frustum, the brow being closer to
	// the lens than the cheek.
	const float centreU = -eye.left / eyeWidth;
	const float centreV = eye.bottom / eyeHeight;

	placement.uMin = centreU - shareU * 0.5f;
	placement.uMax = centreU + shareU * 0.5f;
	placement.vMin = centreV - shareV * 0.5f;
	placement.vMax = centreV + shareV * 0.5f;

	// A destination rectangle reaching past its texture is not a picture that
	// hangs over the edge, it is an invalid call - so anything outside is cut
	// off both here and, in the same proportion, out of the source. Cutting
	// only one of the two would slide the picture instead of trimming it.
	const float widthBefore = placement.uMax - placement.uMin;
	const float heightBefore = placement.vMax - placement.vMin;

	if (placement.uMin < 0.0f) {
		placement.sourceUMin = -placement.uMin / widthBefore;
		placement.uMin = 0.0f;
		placement.cropped = true;
	}
	if (placement.uMax > 1.0f) {
		placement.sourceUMax = 1.0f - (placement.uMax - 1.0f) / widthBefore;
		placement.uMax = 1.0f;
		placement.cropped = true;
	}
	if (placement.vMin < 0.0f) {
		placement.sourceVMin = -placement.vMin / heightBefore;
		placement.vMin = 0.0f;
		placement.cropped = true;
	}
	if (placement.vMax > 1.0f) {
		placement.sourceVMax = 1.0f - (placement.vMax - 1.0f) / heightBefore;
		placement.vMax = 1.0f;
		placement.cropped = true;
	}

	return placement;
}

PicturePlacement PlacePicture(const EyeProjection& eye, float fovDegrees, UInt32 frameWidth,
                              UInt32 frameHeight, bool fovIsFor4x3) {
	PicturePlacement placement;

	// Nothing sensible can be said about a frame with no size or a field of
	// view that is not an angle. The default placement fills the texture,
	// which is what OBVR did before this function existed - wrong, but
	// wrong in a way that still shows a picture.
	if (frameWidth == 0 || frameHeight == 0 || !(fovDegrees > 1.0f) ||
	    !(fovDegrees < 179.0f)) {
		return placement;
	}


	// The game's frustum, as tangents of the half-angles.
	//
	// Which way round this is worked out is an open question, and it changes
	// the answer by a third. Oblivion's fDefaultFOV is 75, and the Construction
	// Set wiki says SetCameraFOV "sets the camera's horizontal field of view",
	// with the vertical following at a fixed 5:4 in degrees - 75 across, 60
	// down. That ratio is exactly what a 4:3 image gives: tan(30)/tan(37.5) is
	// 0.7525, which is 3/4 to within rounding. So the documented figure
	// describes a 4:3 screen, and says nothing certain about what a 16:9 one
	// does with it.
	//
	// The two possibilities differ in which axis is held fixed as the picture
	// widens:
	//
	//   fovIsFor4x3 = false   the number is the horizontal field of view at
	//                         whatever the current aspect ratio is, and the
	//                         vertical shrinks to suit. 75 across, 46.7 down
	//                         at 16:9.
	//   fovIsFor4x3 = true    the number describes a 4:3 screen, the vertical
	//                         stays at 60, and the horizontal widens. 91.3
	//                         across at 16:9.
	//
	// At 4:3 the two agree exactly, which is the check below and the reason
	// the ambiguity exists at all: nobody had to choose while screens were
	// 4:3.
	//
	// This is NOT settled. The Widescreen Gaming Forum calls Oblivion's
	// widescreen support a "perfect implementation ... whose example should be
	// studied by all developers", and by their own terms a picture that loses
	// vertical field of view on a wide screen is a fault rather than a perfect
	// implementation - which points at the second reading. But the page never
	// says so, and nothing found says it outright. So both are built, the
	// setting chooses, and the log prints what was used. Getting it wrong
	// makes the world a third too large or a third too small, evenly in both
	// axes, with no stretching to give it away.
	float tanHalfWidth = 0.0f;
	float tanHalfHeight = 0.0f;
	const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);

	if (fovIsFor4x3) {
		tanHalfHeight = math::Tan(fovDegrees * 0.5f * math::kDegreesToRadians) * 0.75f;
		tanHalfWidth = tanHalfHeight * aspect;
	} else {
		tanHalfWidth = math::Tan(fovDegrees * 0.5f * math::kDegreesToRadians);
		tanHalfHeight = tanHalfWidth / aspect;
	}

	return PlacePictureFromTangents(eye, tanHalfWidth, tanHalfHeight);
}

}  // namespace obvr::render

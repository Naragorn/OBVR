#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::render {

// What the headset says about where the eyes are and what they can see.
//
// Nothing here draws anything. It turns the four numbers OpenVR reports for
// an eye's frustum, plus the two eye positions, into figures a person can
// read and a renderer can use - and it is separate from the calls that fetch
// them for the usual reason: the fetching needs a headset, the arithmetic
// does not.
//
// Not needed to get a picture into the headset, which already works. Needed
// for what comes after: rendering Oblivion's world twice, once per eye, with
// the right frustum and the right offset. Reading and logging the numbers now
// is what stops that step starting from an assumption.

// One eye's frustum, as the tangents of the angles from the view axis to each
// clipping plane.
//
// Deliberately stored exactly as OpenVR reports it, signs and all. The header
// does not document the convention, and Valve's wiki says "top" and "bottom"
// are named backwards - so this struct carries the raw answer and the
// functions below make no assumption that a reader cannot check against the
// log.
struct EyeProjection {
	float left = -1.0f;
	float right = 1.0f;
	float top = -1.0f;
	float bottom = 1.0f;
};

// The total angle the eye covers, in degrees, horizontally and vertically.
//
// Computed from the width of the frustum rather than from either edge alone,
// so the answer does not depend on which sign convention is in force - a
// frustum two units wide spans the same angle whether its edges are -1 and +1
// or 0 and +2. That is the point: this figure is readable in the log without
// the convention having been settled first.
float HorizontalFovDegrees(const EyeProjection& projection);
float VerticalFovDegrees(const EyeProjection& projection);

// Whether the frustum is symmetric about the view axis.
//
// Headset frustums usually are not - the lenses look slightly outwards - and
// that asymmetry is exactly why the centre of the texture is not the centre
// of the view. A value near 0 means symmetric; positive means more frustum on
// the right or the bottom, depending on which one is asked.
float HorizontalAsymmetry(const EyeProjection& projection);
float VerticalAsymmetry(const EyeProjection& projection);

// Where the view axis lands in the texture, as a fraction from the left edge
// and from the top edge.
//
// 0.5 would mean the eye looks straight at the middle of the image. It
// generally does not, and putting a centring mark at the texture's middle
// therefore puts it in the wrong place - which matters, because that mark is
// what the eye offsets will be judged against.
//
// The horizontal answer is convention-free: it only needs left to be the
// lower edge and right the higher. The vertical answer is not, which is why
// it takes a flag rather than deciding for itself.
float OpticalCentreU(const EyeProjection& projection);
float OpticalCentreV(const EyeProjection& projection, bool topIsNegative);

// Which part of a submitted texture belongs to one eye, in texture
// coordinates. Matches OpenVR's VRTextureBounds_t.
struct TextureBounds {
	float uMin = 0.0f;
	float vMin = 0.0f;
	float uMax = 1.0f;
	float vMax = 1.0f;
};

// The bounds that put a single mono image in front of both eyes without them
// disagreeing about where it is.
//
// The problem this solves showed up the moment Oblivion's own frame reached a
// headset. A headset's frustums are asymmetric and mirrored - the measured
// optical axes sit at 0.583 across the left eye's view and 0.424 across the
// right - so handing both eyes the whole texture puts the picture's centre at
// the middle of each frustum, which is *not* where either eye is looking. The
// two eyes then see different parts of the same image and the wearer sees one
// picture displaced against the other.
//
// The fix is to give each eye a different sub-rectangle, chosen so that the
// image's centre lands on that eye's axis. With a width w, the offset follows
// from wanting texture 0.5 to sit at frustum position c:
//
//     uMin + c * w = 0.5,  so  uMin = 0.5 - c * w
//
// w below 1 is what makes that possible at all: at w = 1 the left eye would
// need a uMin of -0.083, which is off the texture. So this trades field of
// view for alignment, and the amount traded is the caller's choice.
//
// It is a palliative rather than a fix. A mono image shown to two eyes has no
// depth in it whatever the bounds are; aligning the centres only stops it
// being actively unpleasant to look at. The real answer is rendering the world
// twice, and these bounds are groundwork for that rather than a substitute.
TextureBounds MonoBounds(float opticalCentreU, float width);

// The distance between the two eyes, in metres.
//
// The interpupillary distance, and the number every stereo pair is built on.
// Worth logging on its own: a plausible figure is between about 0.055 and
// 0.075, so a value outside that says the transform was misread long before
// anything is rendered with it.
float InterpupillaryDistance(const NiPoint3& leftEye, const NiPoint3& rightEye);

// Whether an interpupillary distance is within the range human beings come
// in. Not a hard rule - it is a sanity check for the log, and it is better to
// say "that looks wrong" than to render a world at the wrong scale and let
// somebody work out why it feels like a doll's house.
bool IsPlausibleIpd(float metres);

}  // namespace obvr::render

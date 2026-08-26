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
//
// The flag is no longer an open question, though the function keeps it. A
// headset settled it: pass true. Taking the field names at face value put the
// picture a fifth of the view too low, and inverting it put it where it
// belongs - see the comment on centreV in PlacePicture, which is where the
// answer is actually used.
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


// Where Oblivion's picture belongs inside an eye texture, and how much of
// that texture it is entitled to.
//
// This is the arithmetic that decides whether the world looks its real size.
// An eye texture covers the eye's whole frustum: u runs from the frustum's
// left tangent to its right, v from top to bottom. Oblivion's frame covers a
// different, narrower frustum - 75 degrees across a 16:9 image, against a
// headset asking for about 89 degrees in both directions. Laying one over the
// other without doing this sum stretches the picture by whatever ratio
// happens to fall out, and stretches it by a different ratio in each axis.
//
// Measured, on the headset this was built against: the first attempt gave the
// eye 80% of the frame's width and all of its height, which magnified the
// world 1.61 times across and 2.31 times down - so it was both far too large
// and 1.43 times taller than wide. The HUD, which lives at the edges of a
// 16:9 frame, was pushed out of sight entirely.
//
// Fractions of the texture rather than pixels, because the same answer then
// serves a destination rectangle at any texture size.
struct PicturePlacement {
	// Where the picture goes in the eye texture.
	float uMin = 0.0f;
	float vMin = 0.0f;
	float uMax = 1.0f;
	float vMax = 1.0f;

	// Which part of Oblivion's frame is used. Normally all of it; less only
	// when the game's field of view is wider than the eye's and the picture
	// would otherwise reach past the texture.
	float sourceUMin = 0.0f;
	float sourceVMin = 0.0f;
	float sourceUMax = 1.0f;
	float sourceVMax = 1.0f;

	// Whether anything had to be cut away to make it fit.
	bool cropped = false;
};

// Turns a horizontal field of view and a frame size into the placement.
//
// fovDegrees is Oblivion's fDefaultFOV, 75 by default. fovIsFor4x3 says how
// to read it when the frame is not 4:3, and the two readings differ by a third
// - see the comment in the implementation, where the evidence and the gap in
// it are written out. At 4:3 they agree exactly.
//
// The old wording, kept because it is the reading fovIsFor4x3 = false takes:
// fovDegrees is the game's own horizontal field of view - Oblivion's
// fDefaultFOV, 75 by default - and the frame's aspect ratio gives the
// vertical half of it. Both are needed: a field of view alone says nothing
// about how tall the picture is.
//
// Both axes are centred on the eye's view axis, and both of those centrings
// have now been checked against a person's eyes rather than derived: the
// horizontal one placed the test pattern's cross, and the vertical one was
// corrected after the picture was reported sitting too low. See the comment
// on centreV in the implementation.
//
// The result normally leaves a black margin, because a 16:9 frame at 75
// degrees simply does not fill a headset's field of view. That margin is the
// honest outcome. Filling the view instead would mean either magnifying the
// world or asking Oblivion to render at something like 120 degrees across,
// and the second of those pushes the HUD off the edge just as surely.
// The same placement from tangents that were measured rather than computed -
// what ReadGameProjection returns. Preferred when it is available, because it
// asks the game what it is rendering instead of reasoning about what a
// documented number might mean.
PicturePlacement PlacePictureFromTangents(const EyeProjection& eye, float tanHalfWidth,
                                          float tanHalfHeight);

PicturePlacement PlacePicture(const EyeProjection& eye, float fovDegrees, UInt32 frameWidth,
                              UInt32 frameHeight, bool fovIsFor4x3);

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

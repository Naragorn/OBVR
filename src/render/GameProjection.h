#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {

// Oblivion's own frustum, measured off the projection matrix it hands Direct3D
// rather than worked out from a documented field-of-view number.
//
// This exists because the documented number turned out not to settle anything.
// Oblivion's fDefaultFOV is 75, and the Construction Set wiki calls that the
// horizontal field of view with the vertical following at 5:4 in degrees - but
// 5:4 in degrees is exactly what a 4:3 image gives at 75 degrees, so the
// sentence describes a 4:3 screen and says nothing certain about a wide one.
// The two readings differ by a third in the size of the world, and by nothing
// at all in its shape, so there is no distortion to notice and no way to tell
// by looking.
//
// A projection matrix has no such ambiguity. For a standard perspective
// projection m[0][0] is 1/tan(horizontal half-angle) and m[1][1] is
// 1/tan(vertical half-angle), whatever anyone meant by "field of view", and
// whatever the game does with aspect ratios on the way there.
struct GameProjection {
	// Tangents of the half-angles - the same quantity PlacePicture works in,
	// so the answer needs no conversion to be used.
	float tanHalfWidth = 0.0f;
	float tanHalfHeight = 0.0f;

	// Whether these came from the game or are a fallback. False means the
	// matrix could not be read or did not look like a perspective projection,
	// and the configured field of view is what will be used instead.
	bool measured = false;
};

// Whether a matrix element pair looks like a perspective projection at all.
//
// Separated so it can be checked without a game. The failure this guards
// against is specific and quiet: Oblivion draws most of its world through
// shaders, and a renderer that never calls SetTransform leaves the
// fixed-function projection at identity. Identity has m[0][0] = m[1][1] = 1,
// which is a perfectly well-formed matrix describing a 90-degree square view
// that the game is not using. Believing it would put the world at the wrong
// size with nothing anywhere saying why.
//
// The bounds are wide on purpose. They are here to reject identity and
// nonsense, not to second-guess a field of view someone chose: anything from
// about 20 to about 160 degrees passes.
bool IsPlausibleProjection(float m00, float m11);

// Reads it off the device. False if the matrix cannot be had or does not pass
// the check above, in which case out is left alone.
bool ReadGameProjection(void* gameDevice, GameProjection& out);

// The rectangle of the frame Oblivion is actually drawing into.
//
// This became a question the moment OBVR started creating the frame at a size
// the game did not ask for. The back buffer is 3200x3200 because that is what
// a headset wants; whether Oblivion lays its picture over all of it, or over
// the 2560x1440 it still believes in, is not something to be reasoned about
// from the outside. The viewport is the game's own answer.
//
// False when the device or its table cannot be read.
bool ReadViewport(void* gameDevice, d3d9::Viewport& viewport);

}  // namespace obvr::render

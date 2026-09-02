#pragma once

#include "core/Smoothing.h"
#include "game/NiMath.h"

namespace obvr {

// Rotation matrix from Euler angles in degrees, composed from the three
// elementary axis rotations in the order Z * Y * X.
//
// The axis assignment has been confirmed in the running game:
//   X = pitch (looking up and down)
//   Y = roll  (tilting the head sideways)
//   Z = yaw   (looking left and right)
//
// Deliberately free of Windows dependencies, so that the tests can use it as
// the reference for the quaternion route.
NiMatrix33 EulerToMatrix(float degreesX, float degreesY, float degreesZ);

// Expresses the same physical correction in another coordinate space.
//
// fromWorld and toWorld map vectors from their respective local spaces into
// world space. A skeleton bone's parent axes need not agree with the actor's
// right/forward/up axes; applying an actor-space X/Z rotation directly there
// swaps pitch and yaw on Oblivion's Bip01 hierarchy. This basis change is the
// exact bridge between those spaces.
NiMatrix33 RebaseRotation(const NiMatrix33& correction,
                          const NiMatrix33& fromWorld,
                          const NiMatrix33& toWorld);

// Reads which way a camera faces, ignoring how far it is tilted.
//
// This is how OBVR takes the vertical look away from the stick and the
// keyboard without touching Oblivion's input at all: the hook runs after the
// camera has been computed, so the tilt is already sitting in the matrix and
// can be read back out and dropped. UEVR discards a rotation's tilt the same
// way in utility::math::flatten, though it applies it to the HMD rotation
// rather than the game's.
//
// No trigonometry is involved. Column 0 of the matrix is where the camera's
// own right axis points, and for a camera that is not rolled - which
// Oblivion's never is - that axis stays horizontal however far the camera
// tilts. Its first two components are therefore the cosine and sine of the
// heading, up to a common factor that normalising removes.
//
// Returns false for a camera rolled onto its side, which makes that column
// vertical and the heading unreadable. The caller should then leave the game's
// own rotation in place rather than replace it with a guess.
bool HeadingOf(const NiMatrix33& rotation, Heading& out);

// The rotation that faces a heading and is not tilted at all: the inverse of
// HeadingOf, up to the tilt it deliberately drops.
NiMatrix33 RotationFromHeading(const Heading& heading);

// Which way the camera is looking, as a unit vector in the space the matrix
// is expressed in.
//
// Column 1, because that is where Oblivion keeps a camera's forward axis -
// established here rather than assumed: SinPitchOf has read element [2][1] as
// the vertical component of the forward axis since the vertical look was first
// taken off the mouse, and that has been right in the headset for weeks. Column
// 0 is the right axis, which HeadingOf reads for the same reason. So this
// function is not a new claim about the convention; it is the claim those two
// already rest on, given a name and one place to be wrong in.
//
// Not renormalised. The matrix comes out of the engine orthonormal and OBVR's
// own products of orthonormal matrices stay so; a caller that needs the length
// exactly one for a comparison should say so at the comparison.
NiPoint3 ForwardOf(const NiMatrix33& rotation);

// How far the camera is tilted, as the sine of the angle: +1 looking straight
// up, 0 level, -1 straight down.
//
// The sine rather than the angle because that is what the matrix holds
// directly - element [2][1], the vertical component of where the camera's
// forward axis points - and because the caller only scales it. Turning it into
// degrees first and back into a proportion afterwards would add a call to asin
// for nothing.
float SinPitchOf(const NiMatrix33& rotation);

}  // namespace obvr

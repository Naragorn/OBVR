#pragma once

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

}  // namespace obvr

#pragma once

#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// Where the laser's pieces hang in the controller's own frame (x right,
// y up, -z forward), as overlay transforms relative to the controller. One
// direction for all of them - LaserDirectionLocal, the same the hit tests
// use - so the beam, the dot at its end and the in-game crosshair on it all
// lie on the ray that decides what is pointed at.

// The beam: a quad whose up is the beam's direction and whose right is the
// controller's x turned by the same yaw; its front is right x up, which keeps
// the frame right-handed - with no angles the controller's up. It starts
// originMetres along the direction and its centre sits half its length on.
inline openvr::HmdMatrix34 LaserBeamMatrix(float lengthMetres, float pitchDegrees,
                                           float yawDegrees, float originMetres) {
	const NiPoint3 up = LaserDirectionLocal(pitchDegrees, yawDegrees);
	const NiPoint3 right = LaserRightLocal(yawDegrees);
	const NiPoint3 front{right.y * up.z - right.z * up.y, right.z * up.x - right.x * up.z,
	                     right.x * up.y - right.y * up.x};
	const float along = originMetres + 0.5f * lengthMetres;
	openvr::HmdMatrix34 m{};
	m.m[0][0] = right.x;
	m.m[1][0] = right.y;
	m.m[2][0] = right.z;
	m.m[0][1] = up.x;
	m.m[1][1] = up.y;
	m.m[2][1] = up.z;
	m.m[0][2] = front.x;
	m.m[1][2] = front.y;
	m.m[2][2] = front.z;
	m.m[0][3] = up.x * along;
	m.m[1][3] = up.y * along;
	m.m[2][3] = up.z * along;
	return m;
}

// A quad standing on the ray at a distance from the beam's start, facing
// back along it: its front (the overlay's +z) is the reverse of the beam's
// direction, its right the beam's right, its up front x right. With no
// angles and no offset this is the plain "distance ahead" placement the
// crosshair has always used.
inline openvr::HmdMatrix34 LaserPointMatrix(float distanceMetres, float pitchDegrees,
                                            float yawDegrees, float originMetres) {
	const NiPoint3 direction = LaserDirectionLocal(pitchDegrees, yawDegrees);
	const NiPoint3 right = LaserRightLocal(yawDegrees);
	const NiPoint3 front{-direction.x, -direction.y, -direction.z};
	const NiPoint3 up{front.y * right.z - front.z * right.y, front.z * right.x - front.x * right.z,
	                  front.x * right.y - front.y * right.x};
	const float along = originMetres + distanceMetres;
	openvr::HmdMatrix34 m{};
	m.m[0][0] = right.x;
	m.m[1][0] = right.y;
	m.m[2][0] = right.z;
	m.m[0][1] = up.x;
	m.m[1][1] = up.y;
	m.m[2][1] = up.z;
	m.m[0][2] = front.x;
	m.m[1][2] = front.y;
	m.m[2][2] = front.z;
	m.m[0][3] = direction.x * along;
	m.m[1][3] = direction.y * along;
	m.m[2][3] = direction.z * along;
	return m;
}

// The laser as a ray in the game's world, for the engine's own pick - the
// one ray behind the crosshair's tooltip, Activate and the grab. The hand's
// rotation and offset are relative to the head in the game's axes (x right,
// y forward, z up) and units, the way HandMode answers them; the head's
// world rotation and position carry them into the world, as they carry the
// hand bones. The controller's local direction is taken into the game's
// axes the way the poses are: (x, y, z) in OpenVR is (x, -z, y) here.
struct LaserWorldRay {
	NiPoint3 origin{0.0f, 0.0f, 0.0f};
	NiPoint3 direction{0.0f, 1.0f, 0.0f};
};

inline LaserWorldRay HandLaserWorldRay(const NiMatrix33& headRot, const NiPoint3& headPos,
                                       const NiMatrix33& handRelativeRot,
                                       const NiPoint3& handOffsetUnits, float pitchDegrees,
                                       float yawDegrees, float originMetres,
                                       float unitsPerMetre) {
	const NiPoint3 local = LaserDirectionLocal(pitchDegrees, yawDegrees);
	const NiPoint3 inGameAxes{local.x, -local.z, local.y};
	const NiPoint3 headRelative = handRelativeRot * inGameAxes;
	LaserWorldRay ray;
	ray.direction = headRot * headRelative;
	ray.origin = headPos + headRot * (handOffsetUnits + headRelative * (originMetres * unitsPerMetre));
	return ray;
}

// How wide the dot at the beam's end is: the same apparent size near and
// far, about 0.9 degrees, and never smaller than 8 mm.
inline float LaserDotWidth(float distanceMetres) {
	const float width = distanceMetres * 0.016f;
	return width > 0.008f ? width : 0.008f;
}

}  // namespace obvr::vr

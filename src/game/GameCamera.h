#pragma once

#include "core/Types.h"

namespace obvr::game {

// Oblivion's own render camera, and the frustum it draws the world with.
//
// This is the lever that everything left needs, and it is the one FEAR2VR
// names explicitly: change the call that builds the matrices, not the matrices.
// Gamebryo composes the projection, the view-projection and the world-to-screen
// matrix from this one frustum; patching the Direct3D matrix afterwards would
// leave the other two describing a different camera, and shaders that were
// handed constants from the old one.
//
// What it unlocks, all from the same place:
//
//   * the black margin, because a frustum written per eye is the eye's own
//     view rather than a rectangle laid inside it
//   * the asymmetry, which PlacePicture currently compensates for
//     geometrically and which a real frustum simply has
//   * alternate eyes itself, because the way to draw twice is to set this
//     twice
//
// Nothing here writes yet. This reads, so that the addresses can be checked
// against something already known before anything is changed - see
// FrustumLooksRight.

// The global scene graph, and the camera hanging off it.
//
// 0x00B333CC sits eight bytes past kPlayerPointer, which is the kind of
// neighbourhood two adjacent globals share and a weak confirmation on its own.
// The real check is at runtime and is exact: the frustum read through these
// has to describe the same angles the projection matrix already reported.
inline constexpr UInt32 kWorldSceneGraph = 0x00B333CC;
inline constexpr UInt32 kSceneGraphCameraOffset = 0x0DC;

// NiCamera::m_kViewFrustum. Confirmed twice over: xOBSE places the frustum at
// 0x0EC and the viewport at 0x110, and CommonLibSSE's NiCamera - a different
// game, the same engine lineage - lays out the same block as frustum at 0,
// minNearPlaneDist at 0x1C, maxFarNearRatio at 0x20, port at 0x24. Those agree
// exactly: 0xEC + 0x24 is 0x110.
inline constexpr UInt32 kNiCameraFrustumOffset = 0x0EC;

// NiFrustum, as NiTypes.h declares it: six planes and an orthographic flag.
//
// The near plane is what makes the other four readable. l, r, t and b are
// distances on the near plane rather than angles, so a field of view only
// falls out of them once divided by n - which is also what makes the check
// against the projection matrix possible.
struct NiFrustum {
	float l;
	float r;
	float t;
	float b;
	float n;
	float f;
	bool o;
};

// 0x1C rather than 0x1B: the bool is padded out to the alignment of the floats
// in front of it. If this were wrong the viewport would not land at 0x110, and
// that coincidence is what makes the assertion worth writing down.
static_assert(sizeof(NiFrustum) == 0x1C, "NiFrustum is six floats, a flag, and padding");

// Whether a frustum read out of memory describes the same view the projection
// matrix reported.
//
// This is the whole point of reading before writing. The addresses above came
// from documentation of somebody else's reverse engineering, and a wrong one
// does not announce itself - it hands back four floats that are some other
// object's contents and look like numbers. Comparing the angles they imply
// against angles already measured a different way turns that into a question
// with an answer.
//
// tanHalfWidth and tanHalfHeight are what ReadGameProjection measured off the
// device. The tolerance is loose because the two paths round differently, not
// because the claim is soft: a wrong address is out by orders of magnitude,
// not by a percent.
bool FrustumLooksRight(const NiFrustum& frustum, float tanHalfWidth, float tanHalfHeight);

// Reads it. False when the scene graph or the camera is null, which is normal
// before the game has built a world.
bool ReadGameCameraFrustum(NiFrustum& out);

}  // namespace obvr::game

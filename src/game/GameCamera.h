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
// l, r, t and b are the tangents of the half-angles, not distances on the near
// plane. Read on this machine as l=-1.1188 r=1.1188 t=0.6293 b=-0.6293 n=10:
// dividing by n would give a 12.8 degree view, while r/t is 1.7778 to four
// figures - exactly the 16:9 shape of the frame, which settles the reading.
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


// Notices when the frustum changes, so a few lines of log can say whether it
// is one view or many.
//
// The question this answers is specific. The first reading found a frustum
// 1.458 times wider in tangents than the projection matrix reported - the same
// factor in both axes, so the same view at a different size rather than a
// different view. Two explanations fit:
//
//   * the scene graph's camera is a different camera from the one the frame
//     was drawn with - a culling frustum deliberately widened so objects at
//     the edge do not pop, or a pass for shadows or water
//   * it is the right camera read at the wrong moment, and the value changes
//     during a frame
//
// A frustum that never changes points at the first; one that moves points at
// the second. Either way the answer is to take the camera in phase - as an
// argument to the render pass rather than out of a global afterwards - which
// is what FEAR2VR's source says in as many words about its own engine. This
// watcher is how that gets decided on evidence rather than by preference.
class FrustumWatcher {
public:
	// True when this frustum is worth a log line: the first one, or one that
	// differs from the last reported by more than a little. Stops reporting
	// after a handful, because the point is to see whether it varies and not
	// to fill the log with proof that it does.
	bool Observe(const NiFrustum& frustum);

	UInt32 Reported() const { return m_reported; }

private:
	static constexpr UInt32 kMaxReports = 8;

	NiFrustum m_last{};
	bool m_seen = false;
	UInt32 m_reported = 0;
};

// Reads it. False when the scene graph or the camera is null, which is normal
// before the game has built a world.
bool ReadGameCameraFrustum(NiFrustum& out);

}  // namespace obvr::game

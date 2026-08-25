#pragma once

#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// The arithmetic that turns a tracked head position into a camera offset.
//
// Kept apart from HeadTracker for the same reason camera/FrameLogic is kept
// apart from the camera hook: HeadTracker owns a connection to SteamVR and can
// only be exercised in the state where that connection is missing, while every
// decision made on the way here is arithmetic over plain values.

// Expresses a head position as an offset from a reference, in Oblivion units
// and in the recentered frame.
//
// Three steps, and each of them is a place a sign can go wrong:
//
//   1. the difference against the reference position, still in metres and
//      still in the tracking universe
//   2. rotated by the conjugate of the reference orientation, which turns
//      "forward in the room" into "forward as the user was facing when they
//      recentered"
//   3. the change of basis into Oblivion axes, and metres into units
//
// Step 2 is the one that is easy to leave out and hard to notice: without it
// leaning forward moves the camera along whatever direction SteamVR's seated
// zero happens to point, which is right only if the user set that zero facing
// exactly the same way.
NiPoint3 OffsetFromPose(const Quaternion& reference, const NiPoint3& rawPosition,
                        const NiPoint3& referencePosition, float unitsPerMetre);

// Keeps the camera within a sphere around where the game put it. 0 removes the
// limit.
//
// Direction is preserved and only the length is cut, so a lean that runs into
// the limit stops rather than veering sideways. Without this a tracking glitch
// - or someone simply standing up and walking off - would drag the camera
// through the nearest wall, and Oblivion's occlusion does not survive that.
NiPoint3 ClampOffset(const NiPoint3& offset, float maxUnits);

// Eases the camera towards where the head actually is: a fixed share of the
// remaining distance per second, the share being speedPerSecond * deltaSeconds.
//
// Per second rather than per frame, which is how UEVR computes its camera lerp
// as well ("t = m_lerp_camera_speed->value() * delta" in
// VR::on_pre_calculate_stereo_view_offset). A per-frame share would make the
// same setting feel twice as sluggish at 30 fps as at 60.
//
// It never quite arrives, which is exactly what makes it smooth, and the
// residue falls far below what a pixel can show. A missing frame time or a
// speed of zero means no smoothing rather than a frozen camera - of the ways
// to handle a caller with no timing, that is the only one that still tracks
// the head.
NiPoint3 Approach(const NiPoint3& current, const NiPoint3& target, float speedPerSecond,
                  float deltaSeconds);

}  // namespace obvr::vr

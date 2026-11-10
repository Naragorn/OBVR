#pragma once

// The body follows the head through the room (vr/RoomFollowLogic.h,
// game/RoomFollow.h), the camera hook's part of it: taken out of
// CameraHook.cpp on 2026-11-10, when the room follow went to the neck's pivot.

#include "core/Config.h"
#include "game/NiMath.h"
#include "vr/HeadTracker.h"

namespace obvr::camera {

// What the last request's move achieved joins what is followed - the camera
// offset is the rest - and the neck's new lead, in the world, is asked of the
// player's next move. `baseRotation` the camera's base rotation this frame,
// `controlsActive` Full VR's hands running.
void StepRoomFollowFrame(vr::HeadTracker& tracker, const Config& config, const NiMatrix33& baseRotation,
                         bool isThirdPerson, bool controlsActive);

// Whether a walk after the head is under way (the torso follows at once
// then, game/BodyTorsoLogic.h).
bool RoomFollowWalking();

}  // namespace obvr::camera

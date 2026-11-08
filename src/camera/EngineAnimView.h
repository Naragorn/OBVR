#pragma once

struct NiAVObject;

namespace obvr::camera {

// The eyes at the body's head while the engine plays one of its own
// third-person animations (vr/EngineAnimViewLogic.h, [Look]
// FirstPersonAnimations): the camera node put at the face's centre, the head
// hidden from the draw. True on the frames it holds the camera; the headset's
// offset goes on top as in first person.
bool StepEngineAnimViewFrame(NiAVObject* cameraNode, bool thirdPerson, bool headset, float dtSeconds);

}  // namespace obvr::camera

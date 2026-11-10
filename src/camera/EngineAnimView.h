#pragma once

struct NiAVObject;

namespace obvr::render {
class VignetteLayer;
}
namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::camera {

// The eyes at the body's head while the engine plays one of its own
// third-person animations (vr/EngineAnimViewLogic.h, [Look]
// FirstPersonAnimations): the camera node put at the face's centre, the head
// hidden from the draw. True on the frames it holds the camera; the headset's
// offset goes on top as in first person.
bool StepEngineAnimViewFrame(NiAVObject* cameraNode, bool thirdPerson, bool headset, float dtSeconds);

// The vignette over those animations ([Look] AnimationVignette), in first
// or third person: its alpha set by StepEngineAnimViewFrame; its layer (for
// the overlays' warm-up) and its submit with the overlays, black edges at the
// snap vignette's radius.
float EngineAnimVignetteAlpha();
render::VignetteLayer& EngineAnimVignetteLayer();
void SubmitEngineAnimVignette(vr::OpenVRBackend& backend, void* gameDevice, bool worldFrame);

}  // namespace obvr::camera

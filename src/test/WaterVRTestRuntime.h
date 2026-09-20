#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"
#include "vr/OpenVRTypes.h"

namespace obvr::test {

// Developer-only second-save water sweep. It becomes active only after the
// external runner has loaded a world, then replaces just the HMD pose while
// leaving the real renderer, water pass, shaders and eye capture in use.
void InstallWaterVRTest();
bool WaterVRReplayActive();
bool SuppressWaterVRFirstEyeReflection(bool firstEye);
void ObserveWaterVRReflectionRendered();
void GetWaterVRReplayPose(vr::Quaternion& orientation, NiPoint3& position);
void GetWaterVRReplayPoseMatrix(vr::openvr::HmdMatrix34& matrix);
void ObserveWaterVREye(bool left, void* device);
void AdvanceWaterVRTest(bool playerInWorld);

}  // namespace obvr::test

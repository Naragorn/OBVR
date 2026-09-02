#pragma once

namespace obvr::game {

// Adds a visual yaw and pitch to the third-person player's Bip01 Spine2 on top
// of the pose Oblivion animated this frame. Both angles use OBVR's scene-graph
// convention: radians, yaw in the same sense as the head turn and positive
// pitch looking up.
//
// This changes no actor field, movement input, projectile or hit test. Those
// remain the jobs of Oblivion and AimAtSource respectively.
bool TurnThirdPersonAimVisual(float yawRadians, float pitchRadians);

// Removes the correction if it is still present. Safe to call on every frame
// where the visual is not wanted, including a POV switch, menu and hot reload.
void ReleaseThirdPersonAimVisual();

// Turns Bip01 Head to the complete HMD direction. bodyYaw/bodyPitch are the
// correction already inherited from Spine2; only the exact remaining rotation
// is added to the head, so body and head compose to the requested gaze.
bool TurnThirdPersonHeadVisual(float fullYawRadians, float fullPitchRadians,
	                           float bodyYawRadians, float bodyPitchRadians);

void ReleaseThirdPersonHeadVisual();

}  // namespace obvr::game

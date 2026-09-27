#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::test {

// The hand script in the game (test/HandScript.h, [Debug] HandScript): read
// from the file beside OBVR.dll the first time the player stands in the world
// with no menu up, then walked once a frame from UpdateHandMode. While it
// runs, the controllers are the script's and - when the script says so - the
// head is too. Developer-only; with the key empty nothing here does anything.
void StepHandScriptFrame(float dt, bool inWorld, bool menuUp, const vr::OpenVRBackend& backend);

// Whether the script has the controllers this frame.
bool HandScriptDrivesHands();

// Whether this frame's step passed a mark: the frame the caller writes down
// the state a picture is taken of.
bool HandScriptMarkedThisFrame();

// The last mark's name ("" before the first), for pictures saved at it.
const char* HandScriptMarkName();

// Whether the script asked for this OBVR action ("action <name>") since the
// last time it was taken; taking it clears it.
bool TakeHandScriptAction(const char* name);

// The script's controllers, placed from the head the frame is drawn with.
void ScriptedHands(const vr::Quaternion& headOrientation, const NiPoint3& headPosition,
                   vr::HandPose& right, vr::HandPose& left);

// The synthetic head, for OpenVRBackend's pose reads.
bool HandScriptHeadActive();
void GetHandScriptHeadPose(vr::Quaternion& orientation, NiPoint3& position);
void GetHandScriptHeadPoseMatrix(vr::openvr::HmdMatrix34& matrix);

}  // namespace obvr::test

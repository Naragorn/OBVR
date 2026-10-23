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
// `mainMenuUp`: the main menu is the menu on top - where [Debug]
// HandScriptLoad loads the run's save (test/HandScriptConsole.h).
void StepHandScriptFrame(float dt, bool inWorld, bool menuUp, bool mainMenuUp, const vr::OpenVRBackend& backend);

// The form ID of the item nearest the eyes, as the mark's "HandScript: items"
// line gives it: what {near} becomes in the console-at lines OBVR runs itself.
void NoteHandScriptNearestForm(UInt32 formId);
// The form ID of the actor nearest the player, as the mark's "HandScript:
// actors" line gives it (game/ActorRosterLogic.h): what {actor} becomes.
void NoteHandScriptNearestActor(UInt32 formId);

// Whether the script has the controllers this frame.
bool HandScriptDrivesHands();

// Whether this frame's step passed a mark: the frame the caller writes down
// the state a picture is taken of.
bool HandScriptMarkedThisFrame();

// `head face 1`: whether the synthetic head's yaw is to be turned each
// frame to face the nearest fighter, and the turn itself (degrees, positive
// to the left - the script's own yaw), applied by the camera hook.
bool HandScriptHeadFacesFighter();
void TurnHandScriptHead(float degreesLeft);

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

#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/Quaternion.h"

namespace obvr::test {

// The pose recording in the game (test/PoseRecording.h, [Debug]
// PoseRecordSeconds): once a frame, after the controllers are read (and
// after a hand script's take over, so a scripted run can record itself),
// the frame goes into the ring while `live` - in the world, no menu up.
// A save asked for (RequestPoseRecordingSave, the settings row "Save the
// last minute", or a hand script's `action pose_save`) writes the ring as
// OBVR-Recording-<date>-<time>.txt beside OBVR.dll: a hand script that plays
// it back.
void RecordPoseFrame(float dt, bool live, bool headValid, const vr::Quaternion& head, const NiPoint3& headPosition,
                     const vr::HandPose& right, const vr::HandPose& left);

// The camera hook's one call: the hand script's hands over the read ones while
// it drives them (test::ScriptedHands, placed from this head - level and 1.6 m
// up when the head is unread), then the frame recorded.
void ScriptOrRecordHands(float dt, bool live, bool headValid, const vr::Quaternion& head, const NiPoint3& headPosition,
                         vr::HandPose& right, vr::HandPose& left);

void RequestPoseRecordingSave();

}  // namespace obvr::test

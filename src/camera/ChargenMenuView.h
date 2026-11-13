#pragma once

#include "core/Config.h"
#include "game/GameTypes.h"
#include "vr/HeadTracker.h"

namespace obvr::camera {

// The race and sex menu's frame base (camera/ChargenView.h), out of
// CameraHook.cpp's PlaceMenuCamera: while that menu is the top menu, the
// base is the eyes in front of the character's face, facing it, laid before
// the head as it stood when the menu opened (ChargenAlign), and the head's
// offset is counted from where it stood then. Otherwise all four are left
// as they came in. `headOnBase` is the menu frame's own composition of the
// head on a base (base * head, or the free-aim view), used for the log's
// angle to the face.
void ChargenMenuBaseFor(const Config& config, const NiMatrix33& menuBaseRot, const NiPoint3& menuBasePos,
                        bool menuBaseThirdPerson, const vr::HeadTracker& tracker,
                        NiMatrix33 (*headOnBase)(const NiMatrix33&), NiMatrix33& baseRotation, NiPoint3& basePos,
                        float& verticalOffset, NiPoint3& headOffset);

}  // namespace obvr::camera

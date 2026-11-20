#pragma once

// The weapons kept at the hips shown on the body ([Hands] ShowKeptWeapons;
// game/KeptOnBodyLogic.h decides, game/KeptOnBody.cpp hangs them): what the
// places keep and the weapon slot read for the frame, whether the pack still
// holds each hip's weapon counted when they change and about once a second,
// and each hip's verdict logged when it changes ("Kept on body: the left hip -
// ..."). At a hand script's mark the copies' places are logged.

#include "core/Config.h"
#include "vr/Holster.h"

namespace obvr::camera::hands {

// Each frame before the scene is drawn (CameraHook BeforeFirstScenePass),
// whether or not the body is shown in first person: the third-person view
// shows the same body.
void StepKeptOnBodyFrame(const Config& config, const UInt8* equippedWeaponForm, vr::EquippedKind equippedForBody);

}  // namespace obvr::camera::hands

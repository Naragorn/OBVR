#pragma once

// The game's menus on the big quad in front of the body, gliding after it
// (vr/MenuFacing.h): the camera hook's part - the frame read, the anchor
// handed to the layer, the log. Called once a frame from the overlays'
// submit, after the dialogue's and the container's placements.

#include "core/Config.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "render/HudLayer.h"
#include "vr/OpenVRTypes.h"

namespace obvr::camera {

struct MenuFacingFrame {
	bool haveHead = false;
	vr::openvr::HmdMatrix34 head{};  // the render pose, tracking space
	bool menuUp = false;
	// The menu has a place of its own: the dialogue's panel, the container's,
	// the tablet or the menu wrist.
	bool ownPlace = false;
	bool laserOn = false;  // a laser on the menu
	UInt32 recenterSerial = 0;
};

void StepMenuFacingFrame(const Config& config, render::HudLayer& layer, const MenuFacingFrame& frame);

// The player's shapes near the eyes as a menu opens ([Debug] MenuNearShapes;
// Nadi, 2026-10-27: an armour piece in the view in the inventory), the
// first 20 a session.
void LogMenuNearShapes(const Config& config, bool eyeValid, const NiPoint3& eye);

}  // namespace obvr::camera

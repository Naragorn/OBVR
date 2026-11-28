#include "camera/MenuFacingStep.h"

#include "camera/RoomFollowFrame.h"
#include "core/Log.h"
#include "game/BodyIkLogic.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "game/PlayerBody.h"
#include "platform/Win32Min.h"
#include "vr/MenuFacing.h"

namespace obvr::camera {

namespace {

vr::MenuFacingState& State() {
	static vr::MenuFacingState s;
	return s;
}

// The frame's time by the counter, held to a quarter second: the camera
// hook's delta is not taken on the menu frames the world is not drawn on.
float FrameSeconds() {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	static long long last = 0;
	const long long now = ReadPerformanceCounter();
	float dt = 0.0f;
	if (last != 0 && ticksPerSecond > 0) {
		dt = static_cast<float>(static_cast<double>(now - last) / static_cast<double>(ticksPerSecond));
		dt = dt > 0.25f ? 0.25f : (dt > 0.0f ? dt : 0.0f);
	}
	last = now;
	return dt;
}

// Whole degrees for the log, a -0.3 read as 0, not -0 (the scenario's expect).
double Degrees(float d) {
	const int whole = static_cast<int>(d >= 0.0f ? d + 0.5f : d - 0.5f);
	return static_cast<double>(whole);
}

void LogVerdict(const vr::MenuFacingVerdict& v, const vr::MenuFacingInput& in) {
	static UInt32 s_placedLines = 24;
	static UInt32 s_glideLines = 24;
	if (v.placed && s_placedLines > 0) {
		--s_placedLines;
		const UInt32 top = game::TopVisibleMenu();
		OBVR_LOG("Menu facing: %s (0x%03X) placed in front of the body at %.0f degrees, the head at %.0f, "
		         "%.2f m up",
		         game::MenuIdName(top), top, Degrees(v.bodyYawDegrees), Degrees(v.headYawDegrees),
		         static_cast<double>(in.head.m[1][3]));
	}
	if ((v.glideBegan || v.glideEnded) && s_glideLines > 0) {
		--s_glideLines;
		OBVR_LOG("Menu facing: %s - the body at %.0f degrees, the head at %.0f",
		         v.glideBegan && v.glideEnded ? "taken to the body"
		         : v.glideBegan               ? "the body turned away, the menu glides after it"
		                                      : "the menu settled in front of the body",
		         Degrees(v.bodyYawDegrees), Degrees(v.headYawDegrees));
	}
}

}  // namespace

void StepMenuFacingFrame(const Config& config, render::HudLayer& layer, const MenuFacingFrame& frame) {
	vr::MenuFacingSettings set;
	set.enabled = config.tracker.menusFaceBody;
	set.followDegrees = config.tracker.menuFollowDegrees;
	set.followSeconds = config.tracker.menuFollowSeconds;
	set.torso = game::TorsoYawFrom(config.body.torsoFollowDegrees, config.body.torsoFollowSeconds);
	vr::MenuFacingInput in;
	in.haveHead = frame.haveHead;
	in.head = frame.head;
	in.dtSeconds = FrameSeconds();
	// Walking turns the torso at once (game/BodyTorsoLogic.h); behind a menu
	// nobody walks, and the player's process is not asked.
	in.moving = !frame.menuUp &&
	            (!game::FeetStand(game::ReadPlayerMovementFlags()) || RoomFollowWalking());
	in.menusInRoom = config.tracker.menusInWorld && config.tracker.hudAnchorWorld;
	in.menuUp = frame.menuUp;
	in.ownPlace = frame.ownPlace;
	in.laserOn = frame.laserOn;
	in.recenterSerial = frame.recenterSerial;
	// The stack only while it can matter: ten dwords, read in menus alone.
	if (set.enabled && in.menusInRoom && frame.menuUp) {
		UInt32 stack[game::kMenuStackEntries];
		const UInt32 count = game::MenuStackEntries(stack, game::kMenuStackEntries);
		in.kind = vr::MenuFacingFor(stack, count);
	}
	const vr::MenuFacingVerdict v = vr::StepMenuFacing(State(), in, set);
	if (v.anchor) {
		layer.AnchorAt(v.pose);
	}
	LogVerdict(v, in);
}

void LogMenuNearShapes(const Config& config, bool eyeValid, const NiPoint3& eye) {
	static UInt32 s_nearMenus = 20;
	const float within = config.hands.menuNearShapesUnits;
	if (!(within > 0.0f) || s_nearMenus == 0 || !eyeValid) {
		return;
	}
	--s_nearMenus;
	const UInt32 first = game::LogShapesNearEye(game::FirstPersonArmsNode(), eye, within, "first person", 12);
	const UInt32 body = game::LogShapesNearEye(game::PlayerBodyRoot(), eye, within, "the body", 12);
	OBVR_LOG("Menu near shapes: %s opened - %u first-person and %u body shape(s) within %.0f units of the eyes",
	         game::MenuIdName(game::TopVisibleMenu()), first, body, static_cast<double>(within));
}

}  // namespace obvr::camera

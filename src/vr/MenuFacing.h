#pragma once

// The game's menus on the big quad in front of the body, not the head (Nadi,
// 2026-11-28: "alle Messageboxen und größeren Messageboxen ... sich
// automatisch immer vor dem Spielerkörper ausrichten, auch wenn der
// ausgeblendet ist. Ähnlich zum Hauptmenü, dasselbe für das Escape-Menü";
// then: glide, every menu on the big quad when the tablet is off, the
// height as before and the glide sideways only).
//
// Before, a menu opening in the room took the head's heading at that moment
// (HudLayer::ResetAnchor, the next submit levelling the head's pose) and
// stood there until it closed: a box that came up while the player looked
// aside hung aside, and turning round left it behind.
//
// Now:
// - The body's heading is the torso's (game::StepTorsoYaw, the drawn body's
//   own rule and settings, [Body] TorsoFollowDegrees and TorsoFollowSeconds):
//   a glance stays a glance, a turn held turns the body. It is reckoned from
//   the headset alone, so it is there with the body hidden or not drawn. It
//   is stepped every frame, menu or not, so it is the body's as a menu opens.
// - As a menu opens (or comes onto the big quad from a place of its own),
//   the quad is placed level ahead of the eyes along the body's heading, at
//   the eyes' height then - the height it always had.
// - While it is up and the body has turned more than followDegrees from it,
//   it glides after the body (vr::StepViewAnchor, the HUD rows' glide):
//   round the player on the level, its height kept, until within
//   kViewSettledDegrees. A laser on it holds it, as the main menu's
//   (vr/FlatFollow.h): it is never taken from under the hand.
// - Menus with a place of their own are left alone: a conversation and what
//   opens from it (the panel at the speaker), a container's panel over the
//   container, the tablet and the menu wrist - those the caller says
//   (`ownPlace`) - and the main menu, a loading screen, the credits and the
//   race menu, which are not the game in the world (MenuFacingFor).
//
// Pure: plain values in, plain values out; covered by menu_facing_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BodyTorsoLogic.h"
#include "game/MenuType.h"
#include "vr/BodyFrame.h"
#include "vr/HandHud.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// What the menus up ask of the big quad.
enum class MenuFacingKind : UInt8 {
	None,      // no menu of the game's in the stack
	Body,      // in front of the body, gliding after it
	OwnPlace,  // a conversation's: the dialogue panel places itself
	NotWorld,  // the main menu, a load, the credits, the race menu: as before
};

// A conversation and what opens from it, which keep the dialogue's panel.
inline bool MenuOfConversation(UInt32 menuId) {
	return menuId == game::kMenuIdDialog || menuId == game::kMenuIdPersuasion ||
	       menuId == game::kMenuIdTraining || menuId == game::kMenuIdNegotiate;
}

// The menus that are not the game in the world: the cinema screen's, and the
// race menu with its own view of the face (camera/ChargenMenuView.cpp).
inline bool MenuOutsideWorld(UInt32 menuId) {
	return menuId == game::kMenuIdMain || menuId == game::kMenuIdLoading || menuId == game::kMenuIdCredits ||
	       menuId == game::kMenuIdRaceSex;
}

// The stack as the interface manager holds it, the HUD's own entries
// skipped. One menu outside the world anywhere in it decides, then one of a
// conversation: a box over the main menu, or the Esc menu over a talk, stays
// where those are.
inline MenuFacingKind MenuFacingFor(const UInt32* stack, UInt32 count) {
	bool any = false;
	bool talk = false;
	for (UInt32 i = 0; stack != nullptr && i < count; ++i) {
		const UInt32 id = stack[i];
		if (id == game::kMenuIdNone || id == game::kMenuIdHudMain || id == game::kMenuIdHudInfo ||
		    id == game::kMenuIdHudReticle || id == game::kMenuIdHudSubtitle) {
			continue;
		}
		if (MenuOutsideWorld(id)) {
			return MenuFacingKind::NotWorld;
		}
		talk = talk || MenuOfConversation(id);
		any = true;
	}
	if (talk) {
		return MenuFacingKind::OwnPlace;
	}
	return any ? MenuFacingKind::Body : MenuFacingKind::None;
}

struct MenuFacingSettings {
	bool enabled = true;          // [Render] MenusFaceBody
	float followDegrees = 30.0f;  // [Render] MenuFollowDegrees: 0 stands where it opened
	float followSeconds = 0.4f;   // [Render] MenuFollowSeconds: 0 jumps
	game::TorsoYawSettings torso;
};

struct MenuFacingState {
	game::TorsoYawState torso;
	bool bodyValid = false;
	float bodyX = 0.0f;  // the body's forward on the level, tracking space
	float bodyZ = -1.0f;
	HudViewAnchor anchor;  // the quad's, while a menu faces the body
	UInt32 recenterSerial = 0;
	bool recenterSeen = false;
	bool menuWasUp = false;
};

struct MenuFacingInput {
	bool haveHead = false;
	openvr::HmdMatrix34 head{};  // the render pose, tracking space
	float dtSeconds = 0.0f;
	bool moving = false;    // walking or stepping through the room: the torso follows at once
	bool menusInRoom = false;  // Menus=world and HudAnchor=world
	bool menuUp = false;
	bool opened = false;    // a menu opened this frame besides the menu mode's own start (counted here)
	MenuFacingKind kind = MenuFacingKind::None;
	bool ownPlace = false;  // the dialogue's panel, the container's, the tablet or the wrist
	bool laserOn = false;   // a laser on the quad
	UInt32 recenterSerial = 0;
};

struct MenuFacingVerdict {
	bool anchor = false;  // hand `pose` to HudLayer::AnchorAt this frame
	openvr::HmdMatrix34 pose{};
	bool placed = false;       // placed afresh (a menu opened, the recenter)
	bool glideBegan = false;   // the body turned away: the glide starts
	bool glideEnded = false;   // settled in front of the body again
	float bodyYawDegrees = 0.0f;  // left positive, tracking space, for the log
	float headYawDegrees = 0.0f;
};

// The body's heading, stepped from the head every frame.
inline void StepMenuFacingBody(MenuFacingState& s, const MenuFacingInput& in, const MenuFacingSettings& set,
                               MenuFacingVerdict& v) {
	if (!in.haveHead) {
		return;
	}
	const openvr::HmdMatrix34& m = in.head;
	float hx = 0.0f;
	float hz = 0.0f;
	LevelHeadingOf(NiPoint3{-m.m[0][2], -m.m[1][2], -m.m[2][2]}, NiPoint3{m.m[0][1], m.m[1][1], m.m[2][1]}, hx, hz);
	const float headYaw = math::Atan2(-hx, -hz);
	if (!(headYaw == headYaw)) {
		return;
	}
	const float bodyYaw =
		math::WrapAngle(headYaw + game::StepTorsoYaw(s.torso, headYaw, in.dtSeconds, set.torso, in.moving));
	s.bodyX = -math::Sin(bodyYaw);
	s.bodyZ = -math::Cos(bodyYaw);
	s.bodyValid = true;
	v.headYawDegrees = headYaw * math::kRadiansToDegrees;
	v.bodyYawDegrees = bodyYaw * math::kRadiansToDegrees;
}

// One frame. The anchor is handed over only on a frame it changed: placed,
// or a step of the glide.
inline MenuFacingVerdict StepMenuFacing(MenuFacingState& s, const MenuFacingInput& in,
                                        const MenuFacingSettings& set) {
	MenuFacingVerdict v;
	StepMenuFacingBody(s, in, set, v);
	const bool recentred = s.recenterSeen && in.recenterSerial != s.recenterSerial;
	s.recenterSerial = in.recenterSerial;
	s.recenterSeen = true;
	const bool opened = in.opened || (in.menuUp && !s.menuWasUp);
	s.menuWasUp = in.menuUp;
	const bool faces = set.enabled && in.menusInRoom && in.menuUp && in.kind == MenuFacingKind::Body && !in.ownPlace;
	if (!faces || opened || recentred) {
		s.anchor = HudViewAnchor{};  // the next menu to face the body is placed afresh
	}
	if (!faces || !in.haveHead || !s.bodyValid) {
		return v;
	}
	const openvr::HmdMatrix34& m = in.head;
	const bool fresh = !s.anchor.valid;
	// The height the quad was placed at; the eyes' while it is placed.
	const float y = fresh ? m.m[1][3] : s.anchor.head.m[1][3];
	const openvr::HmdMatrix34 body = LevelHeadingPose(s.bodyX, s.bodyZ, m.m[0][3], y, m.m[2][3]);
	if (in.laserOn && !fresh) {
		s.anchor.following = false;  // never from under the hand; the turn is asked again once it leaves
		return v;
	}
	const bool wasFollowing = s.anchor.following;
	const openvr::HmdMatrix34 before = s.anchor.head;
	StepViewAnchor(s.anchor, true, true, body, set.followDegrees, false, set.followSeconds, in.dtSeconds);
	bool moved = fresh;
	for (int r = 0; r < 3 && !moved; ++r) {
		for (int c = 0; c < 4 && !moved; ++c) {
			moved = s.anchor.head.m[r][c] != before.m[r][c];
		}
	}
	v.placed = fresh;
	// No glide time: begun and ended in the one frame it jumps.
	const bool jumped = !fresh && moved && !wasFollowing && !s.anchor.following;
	v.glideBegan = jumped || (!fresh && !wasFollowing && s.anchor.following);
	v.glideEnded = jumped || (!fresh && wasFollowing && !s.anchor.following);
	v.anchor = moved;
	v.pose = s.anchor.head;
	return v;
}

}  // namespace obvr::vr

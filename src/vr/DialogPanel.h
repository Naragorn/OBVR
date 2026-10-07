#pragma once

// The dialogue panel in the room (docs/hud-on-hands-spec.md, section 4): when
// a conversation opens, the panel is placed once facing the NPC - or beside
// them, so they stay visible - and it hangs at its own size (the tester,
// 2026-09-29: "die dialog optione einmal auf den npc recentern ... in größe
// einstellbar sein. default 80%"; "das Panel neben den NPC statt vor sein
// Gesicht, als Einstellung. Ok machen wir auch!").
//
// Pure: covered by dialog_panel_test.

#include "core/ChoiceWord.h"
#include "core/MathFns.h"
#include "core/Types.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// Where the panel goes against the NPC: in front of them, or turned to their
// right or left as the player sees them.
enum class DialogPanelSide : UInt8 {
	Centre = 0,
	Right = 1,
	Left = 2,
};
inline constexpr UInt32 kDialogPanelSideCount = 3;
inline constexpr const char* kDialogPanelSideNames[kDialogPanelSideCount] = {"centre", "right", "left"};

inline bool ParseDialogPanelSide(const char* text, DialogPanelSide& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kDialogPanelSideNames, kDialogPanelSideCount, index)) {
		return false;
	}
	out = static_cast<DialogPanelSide>(index);
	return true;
}

inline DialogPanelSide DialogPanelSideFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kDialogPanelSideCount) - 0.5f)) {
		return DialogPanelSide::Centre;
	}
	return static_cast<DialogPanelSide>(static_cast<int>(value + 0.5f));
}

// [Look], hot reloaded.
struct DialogPanelSettings {
	// Placed on the NPC once when a conversation opens.
	bool recentre = true;
	DialogPanelSide side = DialogPanelSide::Centre;  // the tester, 2026-09-29: right was "zu weit rechts"
	// How far to the side, degrees of the player's view.
	float sideDegrees = 25.0f;
	// The panel's width while talking, times the menus' own.
	float scale = 0.8f;
};

// Whether this frame places the panel on the NPC: the conversation has just
// opened and has not been placed yet, the setting is on, menus hang in the
// room, and the actual speaker is known. Retry while tracking is unavailable.
inline bool DialogRecentreDue(bool recentre, bool placementPending, bool menusInRoom, bool haveTarget) {
	return recentre && placementPending && menusInRoom && haveTarget;
}

struct DialogPanelPlacement {
	bool placed = false;
	bool Pending(bool talking) {
		if (!talking) placed = false;
		return talking && !placed;
	}
};

// The panel's width while talking; a scale that is not positive is ignored.
inline float DialogPanelWidth(float menuWidthMetres, bool talking, float scale) {
	if (!talking || !(scale > 0.0f)) {
		return menuWidthMetres;
	}
	return menuWidthMetres * scale;
}

// The room anchor that puts the panel on the NPC: at the head, levelled,
// its heading from the head to the NPC (tracking space, metres) and turned
// by the side. The panel then hangs straight ahead of it at the menus'
// distance. False, anchor untouched, when the NPC is straight above or
// below the head (no heading to take).
inline bool DialogAnchor(const openvr::HmdMatrix34& head, float npcX, float npcY, float npcZ, DialogPanelSide side,
                         float sideDegrees, openvr::HmdMatrix34& anchor) {
	(void)npcY;
	float fx = npcX - head.m[0][3];
	float fz = npcZ - head.m[2][3];
	const float len = math::Sqrt(fx * fx + fz * fz);
	if (!(len >= 1e-3f) || !(len < 1.0e7f)) {
		return false;
	}
	fx /= len;
	fz /= len;
	float a = 0.0f;
	if (side == DialogPanelSide::Right) {
		a = sideDegrees * (math::kPi / 180.0f);
	} else if (side == DialogPanelSide::Left) {
		a = -sideDegrees * (math::kPi / 180.0f);
	}
	// Turned towards the right by a: forward*cos + right*sin, right = (-fz, fx).
	const float c = math::Cos(a);
	const float s = math::Sin(a);
	const float hx = fx * c - fz * s;
	const float hz = fz * c + fx * s;
	anchor = openvr::HmdMatrix34{};
	// x right, y up, z back (the heading reversed).
	anchor.m[0][0] = -hz;
	anchor.m[2][0] = hx;
	anchor.m[1][1] = 1.0f;
	anchor.m[0][2] = -hx;
	anchor.m[2][2] = -hz;
	anchor.m[0][3] = head.m[0][3];
	anchor.m[1][3] = head.m[1][3];
	anchor.m[2][3] = head.m[2][3];
	return true;
}

}  // namespace obvr::vr

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

// A container's menu over the container, the world running (the tester,
// 2026-10-08: "wenn ich einen container markiere und A drücke das
// inventory menü des containers geöffnet wird (und zwar als overlay über
// dem container und nicht als cinema screen) aber mit dem game unpaused
// (neue setting mit default on). ähnlich wie die fallout mods"). [Look]
// ContainerInWorld, ContainerPanelScale, ContainerPanelRaiseMetres.
struct ContainerPanelSettings {
	bool inWorld = true;        // the panel over the container, the world running behind it
	float scale = 0.6f;         // the panel's width while looting, times the menus' own
	float raiseMetres = 0.15f;  // the panel's middle this far over the container's middle (its bound's centre)
};

// The right edge of a container's panel is level across the line of sight
// while the line is shallow, and the head's own right once the line is
// steeper than this (the sine of 60 degrees). Over a body at the feet the
// line's level part is a residual of where the head stands over the body -
// behind it as often as ahead - and an edge taken from it turned the panel
// round, upside down to the viewer (a tester, 2026-10-17: "ui of dead
// bodies is oftend flipped the wrong way"; his panels sat 41 to 84 degrees
// steep). What the viewer reads by is the head's right.
inline constexpr float kContainerSteepSine = 0.866f;
inline bool ContainerEdgeFromHead(float sinElevation) {
	return sinElevation > kContainerSteepSine || sinElevation < -kContainerSteepSine;
}

// Which edge a container's anchor took (ContainerAnchor).
enum class ContainerEdge : UInt8 {
	AcrossLine = 0,        // level, across the line of sight
	AcrossLineTurned = 1,  // the same, turned round: it pointed against the head's right
	FromHead = 2,          // the head's own right, squared to a steep line
};
inline const char* ContainerEdgeName(ContainerEdge edge) {
	switch (edge) {
	case ContainerEdge::AcrossLineTurned:
		return "level across the line of sight, turned round to the head's right";
	case ContainerEdge::FromHead:
		return "the head's own right (a steep line)";
	default:
		return "level across the line of sight";
	}
}

// The room anchor that puts the panel on a container: the panel hangs the
// menus' distance straight ahead of its anchor (OverlayPoseAhead), so the
// anchor stands that far back from the target along the line from the
// head to it, heading at it AND tilted to it - the panel then lands on the
// target and faces the eyes, leaning back over a chest below them instead
// of standing upright in the air (the tester, 2026-10-09: "achte noch
// darauf dass die overlays so geneigt sind dass sie direkt zum headset
// schauen also nicht in der luft schweben senkrecht"). Its right edge is
// level, across the line of sight: the tilt is a pitch, never a roll. An
// edge that points against the head's own right - the target's level
// direction behind the head's facing, a body reached beside or behind
// oneself - is turned round, since the panel would hang upside down to the
// viewer otherwise; over a steep line (ContainerEdgeFromHead) the edge is
// the head's own right, levelled and squared to the line, so the panel
// reads the way the head looks. `edge` says which was taken.
// HudLayer::AnchorAt keeps the tilt when asked to (keepTilt). False,
// anchor untouched, for a target at the head itself, a line straight up or
// down with no head right to take an edge from, or a place or distance
// that is no number.
inline bool ContainerAnchor(const openvr::HmdMatrix34& head, float targetX, float targetY, float targetZ,
                            float distanceMetres, openvr::HmdMatrix34& anchor, ContainerEdge* edge = nullptr) {
	float fx = targetX - head.m[0][3];
	float fy = targetY - head.m[1][3];
	float fz = targetZ - head.m[2][3];
	const float len = math::Sqrt(fx * fx + fy * fy + fz * fz);
	if (!(len >= 1e-3f) || !(len < 1.0e7f) || !(distanceMetres >= 0.0f) || !(distanceMetres < 1.0e7f)) {
		return false;
	}
	fx /= len;
	fy /= len;
	fz /= len;
	// The head's right, levelled: what the edge is judged by.
	float hx = head.m[0][0];
	float hz = head.m[2][0];
	const float hl = math::Sqrt(hx * hx + hz * hz);
	const bool haveHeadRight = hl >= 1e-3f && hl < 1.0e7f;
	if (haveHeadRight) {
		hx /= hl;
		hz /= hl;
	}
	// x right. Steep: the head's right less its part along the line - what
	// is left is across the line and nearly level. Else: level, across the
	// line of sight (forward x up), turned round if it points against the
	// head's right.
	float rx = 0.0f;
	float ry = 0.0f;
	float rz = 0.0f;
	ContainerEdge taken = ContainerEdge::AcrossLine;
	bool haveRight = false;
	if (haveHeadRight && ContainerEdgeFromHead(fy)) {
		const float along = hx * fx + hz * fz;
		rx = hx - fx * along;
		ry = -fy * along;
		rz = hz - fz * along;
		const float rl = math::Sqrt(rx * rx + ry * ry + rz * rz);
		if (rl >= 1e-3f) {
			rx /= rl;
			ry /= rl;
			rz /= rl;
			taken = ContainerEdge::FromHead;
			haveRight = true;
		}
	}
	if (!haveRight) {
		const float level = math::Sqrt(fx * fx + fz * fz);
		if (!(level >= 1e-3f)) {
			return false;
		}
		rx = -fz / level;
		ry = 0.0f;
		rz = fx / level;
		if (haveHeadRight && rx * hx + rz * hz < 0.0f) {
			rx = -rx;
			rz = -rz;
			taken = ContainerEdge::AcrossLineTurned;
		}
	}
	if (edge != nullptr) {
		*edge = taken;
	}
	anchor = openvr::HmdMatrix34{};
	anchor.m[0][0] = rx;
	anchor.m[1][0] = ry;
	anchor.m[2][0] = rz;
	// y up: right x forward, so a target below the eyes leans the top away.
	anchor.m[0][1] = ry * fz - rz * fy;
	anchor.m[1][1] = rz * fx - rx * fz;
	anchor.m[2][1] = rx * fy - ry * fx;
	// z back: towards the head.
	anchor.m[0][2] = -fx;
	anchor.m[1][2] = -fy;
	anchor.m[2][2] = -fz;
	anchor.m[0][3] = targetX - fx * distanceMetres;
	anchor.m[1][3] = targetY - fy * distanceMetres;
	anchor.m[2][3] = targetZ - fz * distanceMetres;
	return true;
}

}  // namespace obvr::vr

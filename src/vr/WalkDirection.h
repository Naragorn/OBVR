#pragma once

// Which way the left stick walks in Full VR ([Hands] WalkDirection; the
// tester, 2026-09-28: "gerade laufen mit linken stick [läuft] in eine andere
// richtung ... weil das recentering wo anders liegt").
//
// The stick becomes the W/A/S/D keys (vr::StickToDirections,
// game::HandControls), and the game walks those along the player's heading,
// rotZ. The head turns only the camera, measured from the recenter
// reference. So forward was the way the player faced at the last recenter,
// plus turns: stand turned away from that in the room, and the stick walks
// sideways to where one looks.
//
// Now, while the stick walks, the body is turned to the chosen direction:
// through the same hand-over the aim uses (g_aimBodyOffset in CameraHook),
// which takes the body's turn back out of the camera, so the picture does
// not move. The menus and the room-anchored HUD keep the recenter's
// direction: they are anchored in the room, not on the body.
//
// All yaws here are turns from the recenter reference, in the game's
// convention (HeadingOf of the head's camera rotation, and of each
// controller's rotation taken from the same reference).
//
// Pure, covered by walk_direction_test.

#include "core/MathFns.h"
#include "core/Types.h"

namespace obvr::vr {

enum class WalkDirection : UInt8 {
	Head = 0,         // where the headset faces
	RightHand = 1,    // where the right controller points
	LeftHand = 2,     // where the left controller points
	HeadAndHand = 3,  // halfway between the head and the stick's hand
};

inline constexpr UInt32 kWalkDirectionCount = 4;
inline constexpr WalkDirection kWalkDirectionDefault = WalkDirection::HeadAndHand;

// The INI's words, in the order of the values, and the settings row's.
inline constexpr const char* kWalkDirectionNames[kWalkDirectionCount] = {"head", "right", "left", "blend"};

// A word from the INI, any case. False, and `out` left alone, for anything
// else.
inline bool ParseWalkDirection(const char* text, WalkDirection& out) {
	if (text == nullptr) {
		return false;
	}
	for (UInt32 i = 0; i < kWalkDirectionCount; ++i) {
		const char* a = text;
		const char* b = kWalkDirectionNames[i];
		while (*a != '\0' && *b != '\0' && (*a == *b || *a + ('a' - 'A') == *b)) {
			++a;
			++b;
		}
		if (*a == '\0' && *b == '\0') {
			out = static_cast<WalkDirection>(i);
			return true;
		}
	}
	return false;
}

// The settings row's value (0..3) as a direction; out of range is the
// default.
inline WalkDirection WalkDirectionFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kWalkDirectionCount) - 0.5f)) {
		return kWalkDirectionDefault;
	}
	return static_cast<WalkDirection>(static_cast<int>(value + 0.5f));
}

// What the frame knows: each yaw a turn from the recenter reference.
struct WalkYaws {
	bool headValid = false;
	float head = 0.0f;
	bool rightValid = false;  // the right controller, physically
	float right = 0.0f;
	bool leftValid = false;
	float left = 0.0f;
	bool stickHandRight = false;  // left-handed: the stick is on the right
};

// Two yaws' halfway direction, along the shorter way round. Two opposite
// ones have no halfway: the first is kept.
inline float HalfwayYaw(float a, float b) {
	const float x = math::Cos(a) + math::Cos(b);
	const float y = math::Sin(a) + math::Sin(b);
	if (x * x + y * y < 1e-6f) {
		return a;
	}
	return math::Atan2(y, x);
}

// The direction to walk along, as a turn from the recenter reference. A hand
// that is not tracked falls back to the head; false when the head is not
// known either (the body is then left where it is).
inline bool WalkTargetYaw(WalkDirection direction, const WalkYaws& yaws, float& out) {
	const bool stickValid = yaws.stickHandRight ? yaws.rightValid : yaws.leftValid;
	const float stick = yaws.stickHandRight ? yaws.right : yaws.left;
	switch (direction) {
	case WalkDirection::RightHand:
		if (yaws.rightValid) {
			out = yaws.right;
			return true;
		}
		break;
	case WalkDirection::LeftHand:
		if (yaws.leftValid) {
			out = yaws.left;
			return true;
		}
		break;
	case WalkDirection::HeadAndHand:
		if (yaws.headValid && stickValid) {
			out = HalfwayYaw(yaws.head, stick);
			return true;
		}
		break;
	case WalkDirection::Head:
		break;
	}
	if (yaws.headValid) {
		out = yaws.head;
		return true;
	}
	return false;
}

// A turn of the body smaller than this is left: a tracked head or hand never
// holds exactly still, and writing the heading every frame for a hundredth
// of a degree buys nothing. About a quarter of a degree.
inline constexpr float kWalkStepMinimum = 0.004f;

inline bool WalkStepWorthWriting(float step) { return step > kWalkStepMinimum || step < -kWalkStepMinimum; }

// Whether the body is steered this frame: Full VR in first person with the
// headset, in the world with no menu, the stick walking, and the aim not
// turning the body itself this frame (it owns the turn while it does).
inline bool WalkSteerWanted(bool fullVr, bool headset, bool firstPerson, bool menuMode, bool walking,
                            bool aimTurning) {
	return fullVr && headset && firstPerson && !menuMode && walking && !aimTurning;
}

}  // namespace obvr::vr

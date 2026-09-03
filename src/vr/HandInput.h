#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// One hand as OBVR sees it: the controller's pose in the same tracking space
// the head is read in (OpenVR convention, unconverted - the caller changes
// basis the way it does for the head), and what it is pressing. Pure data,
// so the decisions below can be checked without a headset.
struct HandPose {
	bool valid = false;
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	UInt64 buttonsPressed = 0;
	float trigger = 0.0f;  // 0 released, 1 pulled through
	float thumbX = 0.0f;
	float thumbY = 0.0f;
};

// Whether a button bit is down in a pressed mask. Bit positions are
// openvr's EVRButtonId values.
inline bool ButtonDown(UInt64 pressedMask, UInt32 button) {
	return (pressedMask >> button) & 1ull;
}

// The turn from one heading to another, as the shortest signed angle in
// radians. Both headings are angles the way HeadingOf and Atan2 give them;
// the difference is wrapped so a hand held just left of a head looking just
// right of the seam comes out as a small turn and not most of a circle.
inline float ShortestTurn(float fromRadians, float toRadians) {
	float turn = toRadians - fromRadians;
	while (turn > math::kPi) {
		turn -= 2.0f * math::kPi;
	}
	while (turn < -math::kPi) {
		turn += 2.0f * math::kPi;
	}
	return turn;
}

// ------------------------------------------------------------------ Gestures
//
// Every gesture below reads a hand's position RELATIVE TO THE HEAD, in
// metres, in the game's own axes after the change of basis: x right, y
// forward, z up. The head is the eyes, so "chest height" is a negative z.
// The numbers are starting points for a seated or standing adult and are
// hot-reloadable from [Hands]; the first headset session tunes them.

struct GestureThresholds {
	// Block: the left hand held up and out, roughly in front of the chest.
	float blockMinUp = -0.30f;       // no lower than 30 cm below the eyes
	float blockMinForward = 0.12f;   // and at least 12 cm in front of them
	// Reach back: the right hand behind the head plane and up by the shoulder.
	float reachBackMaxForward = -0.05f;
	float reachBackMinUp = -0.30f;
	// Swing: hand speed in metres per second that counts as a swing, and the
	// speed above which it counts as a heavy one.
	float swingLight = 1.6f;
	float swingHeavy = 3.2f;
	// How long the attack control is held for a heavy swing, in seconds -
	// the engine's power attack wants the control held, a tap is a light one.
	float heavyHoldSeconds = 0.6f;
	// Whether the bow's draw waits for a reach back over the shoulder first:
	// the trigger draws only after the right hand has been behind the head
	// since the last release. Off by default until the gesture is tuned.
	bool bowNeedsReachBack = false;
};

inline bool IsBlockGesture(const NiPoint3& leftHandRelative, const GestureThresholds& t) {
	return leftHandRelative.z >= t.blockMinUp && leftHandRelative.y >= t.blockMinForward;
}

inline bool IsReachBackGesture(const NiPoint3& rightHandRelative, const GestureThresholds& t) {
	return rightHandRelative.y <= t.reachBackMaxForward && rightHandRelative.z >= t.reachBackMinUp;
}

// The hand's speed from two positions a frame apart, in metres per second.
// Zero for a frame of no time, which a paused clock produces.
inline float HandSpeed(const NiPoint3& previous, const NiPoint3& current, float dtSeconds) {
	if (dtSeconds <= 0.0f) {
		return 0.0f;
	}
	const NiPoint3 delta = current - previous;
	return math::Sqrt(delta.LengthSquared()) / dtSeconds;
}

// The swing detector: idle until the hand exceeds the light speed, then one
// attack per swing - heavy if the peak speed of the swing crossed the heavy
// threshold before the hand slowed down again. The decision is taken when
// the hand slows, so a swing that starts light and ends fast is heavy; a
// swing is over when the speed falls under half the light threshold.
enum class SwingVerdict { None, Light, Heavy };

struct SwingDetector {
	bool swinging = false;
	float peakSpeed = 0.0f;
};

inline SwingVerdict StepSwing(SwingDetector& d, float speed, const GestureThresholds& t) {
	if (!d.swinging) {
		if (speed >= t.swingLight) {
			d.swinging = true;
			d.peakSpeed = speed;
		}
		return SwingVerdict::None;
	}
	if (speed > d.peakSpeed) {
		d.peakSpeed = speed;
	}
	if (speed < 0.5f * t.swingLight) {
		d.swinging = false;
		const SwingVerdict verdict =
			d.peakSpeed >= t.swingHeavy ? SwingVerdict::Heavy : SwingVerdict::Light;
		d.peakSpeed = 0.0f;
		return verdict;
	}
	return SwingVerdict::None;
}

// A control held for a while - the heavy attack. Started with a duration,
// counted down with the frame time, reports whether the control is still
// to be held this frame.
struct HeldControl {
	float secondsLeft = 0.0f;
};

inline void HoldFor(HeldControl& h, float seconds) {
	if (seconds > h.secondsLeft) {
		h.secondsLeft = seconds;
	}
}

inline bool StepHeld(HeldControl& h, float dtSeconds) {
	if (h.secondsLeft <= 0.0f) {
		return false;
	}
	h.secondsLeft -= dtSeconds;
	return true;
}

// A press-and-release edge on an analogue trigger, with hysteresis so a
// finger resting on the trigger does not chatter.
struct TriggerEdge {
	bool down = false;
};

inline bool StepTrigger(TriggerEdge& e, float value) {
	if (!e.down && value >= 0.55f) {
		e.down = true;
	} else if (e.down && value <= 0.35f) {
		e.down = false;
	}
	return e.down;
}

// A button's rising edge: true on the frame it goes down, never while held.
struct ButtonEdge {
	bool wasDown = false;
};

inline bool StepRisingEdge(ButtonEdge& e, bool down) {
	const bool rose = down && !e.wasDown;
	e.wasDown = down;
	return rose;
}

// A thumbstick as four digital directions with a dead zone, for the WASD
// keys the engine reads.
struct StickDirections {
	bool forward = false;
	bool back = false;
	bool left = false;
	bool right = false;
};

inline StickDirections StickToDirections(float x, float y, float deadZone) {
	StickDirections d;
	d.forward = y >= deadZone;
	d.back = y <= -deadZone;
	d.right = x >= deadZone;
	d.left = x <= -deadZone;
	return d;
}

// ---------------------------------------------------------------- Controls
//
// What the game's controls should be doing this frame, decided from the
// hands. The engine's own key map is honoured by injecting the keys the
// player has bound (the defaults are vanilla's, see [Hands] in the INI), so
// nothing here reaches into the engine's input state.

struct HandControlsWanted {
	bool attack = false;    // the attack control held
	bool block = false;     // the block control held
	bool cast = false;      // the cast control held
	bool activate = false;  // activate held
	bool grab = false;      // grab held
	bool jump = false;
	bool sneak = false;
	bool readyWeapon = false;
	bool menu = false;       // the menu-mode key (Tab)
	bool escape = false;
	bool quickMenu = false;  // F1
	StickDirections move;
	float turn = 0.0f;  // -1..1, the right stick's x, for the mouse-driven turn
	bool menuClick = false;  // the left mouse button, for the laser cursor
};

// The pieces of hand state the planner needs, already stepped this frame.
struct HandFrameInput {
	bool rightValid = false;
	bool leftValid = false;
	bool rightTrigger = false;
	bool leftTrigger = false;
	bool rightGrip = false;
	bool leftGrip = false;
	bool rightA = false;
	bool leftA = false;
	bool rightMenuButton = false;  // rising edge
	bool leftMenuButton = false;   // rising edge
	bool rightStickClick = false;  // rising edge
	bool leftStickClick = false;   // rising edge
	float leftThumbX = 0.0f;
	float leftThumbY = 0.0f;
	float rightThumbX = 0.0f;
	bool blockGesture = false;
	bool swingAttackHeld = false;  // a swing's attack still being held
	bool drawBlocked = false;      // the bow wants a reach-back first and has not had one
	bool menuMode = false;
};

// The mapping. In a menu the hands drive the cursor and nothing else: the
// right trigger is the click, the left menu button closes the menu, the
// right one is escape. In the world: right trigger attacks (the bow draws
// while it is held and looses when it is released, a spell hand casts on
// the left trigger), swings attack by themselves, the raised left hand
// blocks, grips grab and activate, A jumps and sneaks, the sticks move and
// turn, stick clicks ready the weapon and open the quick menu.
inline HandControlsWanted PlanHandControls(const HandFrameInput& in, float stickDeadZone) {
	HandControlsWanted out;
	if (in.menuMode) {
		out.menuClick = in.rightValid && in.rightTrigger;
		out.menu = in.leftMenuButton;
		out.escape = in.rightMenuButton;
		return out;
	}
	if (in.rightValid) {
		out.attack = (in.rightTrigger && !in.drawBlocked) || in.swingAttackHeld;
		out.grab = in.rightGrip;
		out.jump = in.rightA;
		out.escape = in.rightMenuButton;
		out.readyWeapon = in.rightStickClick;
		out.turn = in.rightThumbX;
	}
	if (in.leftValid) {
		out.cast = in.leftTrigger;
		out.activate = in.leftGrip;
		out.sneak = in.leftA;
		out.menu = in.leftMenuButton;
		out.quickMenu = in.leftStickClick;
		out.move = StickToDirections(in.leftThumbX, in.leftThumbY, stickDeadZone);
		out.block = in.blockGesture;
	}
	return out;
}

// ------------------------------------------------------------ Laser cursor
//
// Where the right hand's ray meets the quad the left wrist carries, as a
// pixel of the layer texture. Everything in one consistent space; the quad
// is its centre, its unit right and up axes, and its size in metres, the
// texture rectangle the quad shows is (0,0)-(pixelWidth,pixelHeight).

struct LaserHit {
	bool hit = false;
	float pixelX = 0.0f;
	float pixelY = 0.0f;
};

inline float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline NiPoint3 Cross(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline LaserHit LaserOnQuad(const NiPoint3& rayOrigin, const NiPoint3& rayDirection,
                            const NiPoint3& quadCentre, const NiPoint3& quadRight,
                            const NiPoint3& quadUp, float quadWidthMetres,
                            float quadHeightMetres, float pixelWidth, float pixelHeight) {
	LaserHit hit;
	const NiPoint3 normal = Cross(quadRight, quadUp);
	const float denominator = Dot(rayDirection, normal);
	if (denominator > -0.0001f && denominator < 0.0001f) {
		return hit;  // the ray runs along the quad
	}
	const float t = Dot(quadCentre - rayOrigin, normal) / denominator;
	if (t <= 0.0f) {
		return hit;  // the quad is behind the hand
	}
	const NiPoint3 point = rayOrigin + rayDirection * t;
	const NiPoint3 local = point - quadCentre;
	const float u = Dot(local, quadRight) / quadWidthMetres + 0.5f;
	const float v = 0.5f - Dot(local, quadUp) / quadHeightMetres;
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
		return hit;
	}
	hit.hit = true;
	hit.pixelX = u * pixelWidth;
	hit.pixelY = v * pixelHeight;
	return hit;
}

// The mouse step that walks the game's cursor towards the laser's pixel:
// a share of the remaining distance, capped, so an unknown cursor speed
// converges instead of overshooting.
inline int CursorStep(float current, float wanted, float gain, float maxStep) {
	float step = (wanted - current) * gain;
	if (step > maxStep) {
		step = maxStep;
	} else if (step < -maxStep) {
		step = -maxStep;
	}
	return static_cast<int>(step);
}

}  // namespace obvr::vr

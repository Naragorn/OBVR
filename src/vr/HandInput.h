#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/OpenVRTypes.h"
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
	bool thumbFromJoystickAxis = false;  // the stick came from rAxis[3], for the log
	float gripForce = 0.0f;              // rAxis[2].x on an Index, 0 elsewhere
	// Action provenance for recording/replay. A failed or inactive action is
	// represented by neutral controls; these fields retain why it was neutral.
	bool actionInput = false;
	UInt32 actionActiveMask = 0;
	SInt32 actionError = 0;
};

// Whether a button bit is down in a pressed mask. Bit positions are
// openvr's EVRButtonId values.
inline bool ButtonDown(UInt64 pressedMask, UInt32 button) {
	return (pressedMask >> button) & 1ull;
}

// The buttons by their role on an Index controller (see OpenVRTypes.h for
// the numbering, from openvr.h). A is the grip bit there and the A bit on
// a controller that has one; the grip is the Axis2 button; the stick's
// click is Axis3 on an Index and Axis0 on a wand's touchpad.
inline bool ButtonADown(UInt64 mask) {
	return ButtonDown(mask, openvr::kButtonA) || ButtonDown(mask, openvr::kButtonIndexA);
}
inline bool ButtonBDown(UInt64 mask) { return ButtonDown(mask, openvr::kButtonIndexB); }
inline bool GripDown(UInt64 mask) { return ButtonDown(mask, openvr::kButtonIndexGrip); }
inline bool StickClickDown(UInt64 mask) { return ButtonDown(mask, openvr::kButtonIndexJoystick); }
// The laser's direction in the controller's own frame (x right, y up, -z
// forward): forward turned down by pitchDegrees about x, then turned left
// by yawDegrees about the controller's y (negative turns it right). At no
// angles it is -z.
inline NiPoint3 LaserDirectionLocal(float pitchDegrees, float yawDegrees = 0.0f) {
	const float pitch = pitchDegrees * math::kDegreesToRadians;
	const float yaw = yawDegrees * math::kDegreesToRadians;
	const float level = math::Cos(pitch);
	return NiPoint3{-level * math::Sin(yaw), -math::Sin(pitch), -level * math::Cos(yaw)};
}

// The beam's right in the same frame: the controller's x turned by the same
// yaw, so it stays square to the direction whatever the pitch.
inline NiPoint3 LaserRightLocal(float yawDegrees) {
	const float yaw = yawDegrees * math::kDegreesToRadians;
	return NiPoint3{math::Cos(yaw), 0.0f, -math::Sin(yaw)};
}

inline bool TrackpadClickDown(UInt64 mask) {
	return ButtonDown(mask, openvr::kButtonIndexTrackpad);
}

// The legacy controller state, brought to the bits the action path reports.
// There the stick click is its own action (Axis3, 35) and the trackpad click
// another (Axis0, 32). The legacy state reports the Index stick's click on
// Axis0 - measured on an Index, the stick deflected in rAxis[0] and clicked on
// bit 32 - and a wand's touchpad click there too, so on this path bit 32 is
// the stick click and is moved to 35. A real trackpad click cannot be told
// from it here; with the action manifest installed it can.
inline UInt64 NormalizeLegacyButtons(UInt64 pressed) {
	const UInt64 trackpad = 1ull << openvr::kButtonIndexTrackpad;
	if ((pressed & trackpad) != 0) {
		pressed = (pressed & ~trackpad) | (1ull << openvr::kButtonIndexJoystick);
	}
	return pressed;
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

// The weapon's guard: the weapon hand up and out in front, the blade held
// across the body - more sideways than along the view, nearly level - the
// way a sword is raised against a blow. Blocks like the raised left hand,
// with or without a shield (2026-09-25: "Waffe ... gegen die Gegner-Attacke
// heben soll auch blocken"). The blade is the controller's forward axis, the
// same one the strike by motion runs along. Not while the hand is swinging:
// a swing passes through the same place on its way.
constexpr float kGuardMinUp = -0.20f;       // no lower than 20 cm below the eyes
constexpr float kGuardMinAcross = 0.70f;    // the blade at least this far sideways
constexpr float kGuardMaxTilt = 0.50f;      // and at most this far up or down

inline bool IsWeaponGuard(const NiPoint3& weaponHandRelative, const NiPoint3& bladeDirection,
                          bool swinging, const GestureThresholds& t) {
	if (swinging) {
		return false;
	}
	const float across = bladeDirection.x < 0.0f ? -bladeDirection.x : bladeDirection.x;
	const float tilt = bladeDirection.z < 0.0f ? -bladeDirection.z : bladeDirection.z;
	return weaponHandRelative.z >= kGuardMinUp && weaponHandRelative.y >= t.blockMinForward &&
	       across >= kGuardMinAcross && tilt <= kGuardMaxTilt;
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
	bool run = false;        // the run control held
	bool readyWeapon = false;
	bool menu = false;       // the menu-mode key (Tab)
	bool escape = false;
	bool quickMenu = false;  // F1
	bool togglePov = false;  // the view switch (R)
	StickDirections move;
	float turn = 0.0f;  // -1..1, the right stick's x, for the mouse-driven turn
	bool menuClick = false;  // the left mouse button, for the laser cursor
};

// ---------------------------------------------------------- Gamepad layout
//
// The controllers as a gamepad, for playing seated with the head as the aim
// and the mode off: the layout a 360 pad has in Oblivion, laid onto an
// Index. From NorthernUI's "Dutiful" scheme (NorthernUI.ctrl.txt, read
// 2026-09-07) and the game's own [Controls]: the triggers attack and block,
// the grips cast and grab, A jumps and the other A activates, the B buttons
// are the menus, the stick clicks sneak and switch the view, the trackpad
// clicks ready the weapon and open the quick menu, the sticks move and
// turn. Both stick clicks together stay OBVR's own menu (the chord).
//
// Held controls are held; the ones the game treats as a toggle - sneak,
// the view switch, ready weapon, the menus - are given as a press on the
// frame the button went down, so the key is tapped once.
struct GamepadInput {
	bool rightValid = false;
	bool leftValid = false;
	bool rightTrigger = false;   // held
	bool leftTrigger = false;    // held
	bool rightGrip = false;      // held
	bool leftGrip = false;       // held
	bool rightA = false;         // held
	bool leftA = false;          // held
	bool rightB = false;         // rising edge
	bool leftB = false;          // rising edge
	bool rightStickClick = false;     // released alone (the chord's verdict)
	bool leftStickClick = false;      // released alone
	bool rightTrackpadClick = false;  // rising edge
	bool leftTrackpadClick = false;   // rising edge
	float leftThumbX = 0.0f;
	float leftThumbY = 0.0f;
	float rightThumbX = 0.0f;
};

inline HandControlsWanted PlanGamepadControls(const GamepadInput& in, float stickDeadZone) {
	HandControlsWanted out;
	if (in.rightValid) {
		out.attack = in.rightTrigger;
		out.cast = in.rightGrip;
		out.jump = in.rightA;
		out.escape = in.rightB;
		out.togglePov = in.rightStickClick;
		out.readyWeapon = in.rightTrackpadClick;
		out.turn = in.rightThumbX;
	}
	if (in.leftValid) {
		out.block = in.leftTrigger;
		out.grab = in.leftGrip;
		out.activate = in.leftA;
		out.menu = in.leftB;
		out.sneak = in.leftStickClick;
		out.quickMenu = in.leftTrackpadClick;
		out.move = StickToDirections(in.leftThumbX, in.leftThumbY, stickDeadZone);
	}
	return out;
}

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
	bool leftStickHeld = false;    // level: the left stick pressed in
	bool rightStickUp = false;     // rising edge of a flick up
	bool rightStickDown = false;   // rising edge of a flick down
	bool leftTrackpadClick = false;  // rising edge
	float leftThumbX = 0.0f;
	float leftThumbY = 0.0f;
	float rightThumbX = 0.0f;
	bool blockGesture = false;
	bool swingAttackHeld = false;  // a swing's attack still being held
	bool drawBlocked = false;      // the bow wants a reach-back first and has not had one
	bool meleeByMotion = false;    // a swung weapon strikes by motion: the trigger does not attack
	bool menuMode = false;
	bool pointRight = true;        // in a menu: which hand holds the pointer, and so the click
	bool leftHanded = false;       // activate on the left A rather than the right
};

// The mapping. In a menu the hands drive the cursor and nothing else: the
// pointing hand's trigger is the click, the left menu button closes the
// menu, the right one is escape. In the world: right trigger attacks (the
// bow draws while it is held and looses when it is released, a spell hand
// casts on the left trigger), swings attack by themselves, the raised left
// hand blocks, either grip grabs, right A activates, the left stick walks
// and runs while it is pressed in, the right stick turns, jumps on a flick
// up and sneaks on a flick down, its click readies the weapon, the left
// trackpad click opens the quick menu. The left grip only grabs: one that
// grabbed and activated at once would take the object it was meant to hold.
inline HandControlsWanted PlanHandControls(const HandFrameInput& in, float stickDeadZone) {
	HandControlsWanted out;
	if (in.menuMode) {
		out.menuClick = in.pointRight ? (in.rightValid && in.rightTrigger)
		                              : (in.leftValid && in.leftTrigger);
		out.menu = in.leftMenuButton;
		out.escape = in.rightMenuButton;
		return out;
	}
	if (in.rightValid) {
		out.attack = (in.rightTrigger && !in.drawBlocked && !in.meleeByMotion) || in.swingAttackHeld;
		out.jump = in.rightStickUp;
		out.sneak = in.rightStickDown;
		out.escape = in.rightMenuButton;
		out.activate = !in.leftHanded && in.rightA;  // the pointing hand's A
		out.readyWeapon = in.rightStickClick;
		out.turn = in.rightThumbX;
	}
	if (in.leftValid) {
		out.cast = in.leftTrigger;
		if (in.leftHanded) {
			out.activate = in.leftA;  // left-handed: the left A activates instead
		}
		out.run = in.leftStickHeld;
		out.menu = in.leftMenuButton;
		out.quickMenu = in.leftTrackpadClick;
		out.move = StickToDirections(in.leftThumbX, in.leftThumbY, stickDeadZone);
		out.block = in.blockGesture;
	}
	// Either grip can grab - both hands share the same Havok grab (Z key)
	out.grab = (in.rightValid && in.rightGrip) || (in.leftValid && in.leftGrip);
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

// ------------------------------------------------------------- Menu quads
//
// A quad a menu hangs on, wherever it hangs - a wrist, the head, the room -
// as the laser and the finger want it: centre, unit right and up axes, and
// its size, all in tracking space.

struct MenuQuad {
	bool valid = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 right{1.0f, 0.0f, 0.0f};
	NiPoint3 up{0.0f, 1.0f, 0.0f};
	float width = 0.0f;
	float height = 0.0f;
};

// Two OpenVR poses composed, a then b - the product a * b of the 4x4
// matrices they stand for, rows of three columns of axes and a fourth of
// position. What puts a head-relative overlay transform into tracking
// space: the head's pose composed with the overlay's offset from it.
inline openvr::HmdMatrix34 ComposePose(const openvr::HmdMatrix34& a,
                                       const openvr::HmdMatrix34& b) {
	openvr::HmdMatrix34 out{};
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 4; ++col) {
			float sum = a.m[row][0] * b.m[0][col] + a.m[row][1] * b.m[1][col] +
			            a.m[row][2] * b.m[2][col];
			if (col == 3) {
				sum += a.m[row][3];
			}
			out.m[row][col] = sum;
		}
	}
	return out;
}

// The quad an overlay pose describes: its position is the centre, its first
// two columns the right and up axes, its width is given and its height
// follows the picture's aspect. Invalid without a width or a picture.
inline MenuQuad QuadFromPose(const openvr::HmdMatrix34& pose, float widthMetres,
                             float pixelWidth, float pixelHeight) {
	MenuQuad quad;
	if (widthMetres <= 0.0f || pixelWidth <= 0.0f || pixelHeight <= 0.0f) {
		return quad;
	}
	quad.valid = true;
	quad.centre = NiPoint3{pose.m[0][3], pose.m[1][3], pose.m[2][3]};
	quad.right = NiPoint3{pose.m[0][0], pose.m[1][0], pose.m[2][0]};
	quad.up = NiPoint3{pose.m[0][1], pose.m[1][1], pose.m[2][1]};
	quad.width = widthMetres;
	quad.height = widthMetres * (pixelHeight / pixelWidth);
	return quad;
}

// The flat picture - the cinema screen the main menu, the loading screens
// and the films are shown on - as a thing a laser can point at.
//
// It is not a quad in the room. Both eyes are shown the same picture centred
// on their own optical axis, which puts it at infinity: it has a direction
// from the head and an angular size, and no distance. So a hit is worked
// out in two steps: the hand's ray is met with a stand-in plane some way
// ahead of the anchor pose, which is where the beam is drawn to end, and the
// pixel under that point is the one the HEAD sees it against - the direction
// from the head to the point, expressed as tangents in the anchor's axes
// against the picture's own angular half-extents.
struct FlatPicture {
	bool valid = false;
	// The anchor pose the picture is held at: where the head was when it
	// appeared, levelled. The picture is centred on forward.
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 right{1.0f, 0.0f, 0.0f};
	NiPoint3 up{0.0f, 1.0f, 0.0f};
	NiPoint3 forward{0.0f, 0.0f, -1.0f};
	// Half the picture's angular extent, as tangents.
	float tanHalfWidth = 0.0f;
	float tanHalfHeight = 0.0f;
	// The frame pixels the picture shows, in the space the game's cursor
	// lives in: a window into the frame (the flat source crop).
	float pixelLeft = 0.0f;
	float pixelTop = 0.0f;
	float pixelWidth = 0.0f;
	float pixelHeight = 0.0f;
};

struct FlatLaserHit {
	bool hit = false;
	float pixelX = 0.0f;
	float pixelY = 0.0f;
	float lengthMetres = 0.0f;
};

inline FlatLaserHit LaserOnFlatPicture(const NiPoint3& rayOrigin, const NiPoint3& rayDirection,
                                       const NiPoint3& headPosition, const FlatPicture& flat,
                                       float planeDistanceMetres) {
	FlatLaserHit hit;
	if (!flat.valid || flat.tanHalfWidth <= 0.0f || flat.tanHalfHeight <= 0.0f ||
	    flat.pixelWidth <= 0.0f || flat.pixelHeight <= 0.0f || planeDistanceMetres <= 0.0f) {
		return hit;
	}
	// The stand-in plane, ahead of the anchor and facing it.
	const NiPoint3 planeCentre = flat.centre + flat.forward * planeDistanceMetres;
	const float along = Dot(rayDirection, flat.forward);
	if (along <= 0.0001f) {
		return hit;  // pointing away from the picture, or along it
	}
	const float t = Dot(planeCentre - rayOrigin, flat.forward) / along;
	if (t <= 0.0f) {
		return hit;  // the plane is behind the hand
	}
	const NiPoint3 point = rayOrigin + rayDirection * t;

	// What the head sees the point against.
	const NiPoint3 fromHead = point - headPosition;
	const float depth = Dot(fromHead, flat.forward);
	if (depth <= 0.0001f) {
		return hit;
	}
	const float tx = Dot(fromHead, flat.right) / depth;
	const float ty = Dot(fromHead, flat.up) / depth;
	const float u = (tx / flat.tanHalfWidth + 1.0f) * 0.5f;
	const float v = (1.0f - ty / flat.tanHalfHeight) * 0.5f;
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
		return hit;  // past the picture's edge
	}
	hit.hit = true;
	hit.pixelX = flat.pixelLeft + u * flat.pixelWidth;
	hit.pixelY = flat.pixelTop + v * flat.pixelHeight;
	hit.lengthMetres = t;
	return hit;
}

// A held direction that repeats: once when it goes down, then again after
// the first delay and every interval after that while it stays down - the
// mouse wheel a stick becomes in a menu list.
struct RepeatState {
	bool active = false;
	float secondsToNext = 0.0f;
};

inline bool StepRepeat(RepeatState& s, bool held, float dtSeconds, float firstDelay,
                       float interval) {
	if (!held) {
		s.active = false;
		return false;
	}
	if (!s.active) {
		s.active = true;
		s.secondsToNext = firstDelay;
		return true;
	}
	s.secondsToNext -= dtSeconds;
	if (s.secondsToNext <= 0.0f) {
		s.secondsToNext += interval > 0.0f ? interval : 0.1f;
		return true;
	}
	return false;
}

// ------------------------------------------------------------- Poke press
//
// The pointing hand's tip against the quad the other wrist carries: a press
// by touching, the way a finger presses a button, in the same space and
// with the same quad description LaserOnQuad takes. The tip is the
// controller's origin carried forward along its pointing axis by however
// far the index finger reaches past it.

struct PokeSample {
	bool inside = false;   // the tip's foot on the plane is within the quad
	float depth = 0.0f;    // metres in front of the quad (negative: pushed through)
	float pixelX = 0.0f;
	float pixelY = 0.0f;
};

inline PokeSample PokeOnQuad(const NiPoint3& tip, const NiPoint3& quadCentre,
                             const NiPoint3& quadRight, const NiPoint3& quadUp,
                             float quadWidthMetres, float quadHeightMetres, float pixelWidth,
                             float pixelHeight) {
	PokeSample sample;
	if (quadWidthMetres <= 0.0f || quadHeightMetres <= 0.0f) {
		return sample;
	}
	const NiPoint3 normal = Cross(quadRight, quadUp);
	const NiPoint3 local = tip - quadCentre;
	sample.depth = Dot(local, normal);
	const float u = Dot(local, quadRight) / quadWidthMetres + 0.5f;
	const float v = 0.5f - Dot(local, quadUp) / quadHeightMetres;
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
		return sample;
	}
	sample.inside = true;
	sample.pixelX = u * pixelWidth;
	sample.pixelY = v * pixelHeight;
	return sample;
}

// How close counts. Hovering within `hover` metres of the quad puts the
// cursor under the tip; touching within `press` fires the click once;
// the tip has to come back out past `release` before it can press again,
// so a finger resting on the surface does not chatter. A tip pushed
// through the quad by more than `through` is a hand behind the panel, not
// a press.
struct PokeThresholds {
	float hover = 0.10f;
	float press = 0.015f;
	float release = 0.04f;
	float through = 0.06f;
};

struct PokeState {
	bool pressed = false;
};

struct PokeVerdict {
	bool hover = false;  // the cursor follows the tip
	bool press = false;  // the click's rising edge
	bool held = false;   // the click still held down
};

inline PokeVerdict StepPoke(PokeState& state, const PokeSample& sample,
                            const PokeThresholds& t) {
	PokeVerdict v;
	if (!sample.inside || sample.depth < -t.through) {
		state.pressed = false;
		return v;
	}
	if (sample.depth <= t.press) {
		v.hover = true;
		v.press = !state.pressed;
		v.held = true;
		state.pressed = true;
		return v;
	}
	if (state.pressed && sample.depth <= t.release) {
		v.hover = true;
		v.held = true;  // still down, in the hysteresis band
		return v;
	}
	state.pressed = false;
	v.hover = sample.depth <= t.hover;
	return v;
}

// ------------------------------------------------------------ Stick chord
//
// Both sticks clicked together is one gesture (OBVR's own menu), so a single
// stick's click can no longer fire on the way down - the other stick's
// click may be about to join it. A single click fires on the release
// instead, and only when the other stick stayed up for the whole press.

struct StickChordState {
	bool rightDown = false;
	bool leftDown = false;
	bool chorded = false;  // both were down at some point in this press
	float sinceFirst = 0.0f;  // seconds since the first of the two went down
};

struct StickChordVerdict {
	bool rightClick = false;  // the right stick, released alone
	bool leftClick = false;   // the left stick, released alone
	bool both = false;        // the frame both came to be down
};

// Both sticks count as the chord only when the second goes down within this
// of the first. The left stick held in is running: a right click while
// running readies the weapon, and must not open OBVR's menu instead.
constexpr float kStickChordWindowSeconds = 0.25f;

inline StickChordVerdict StepStickChord(StickChordState& s, bool rightDown, bool leftDown,
                                        float dtSeconds = 0.0f) {
	StickChordVerdict v;
	const bool bothDown = rightDown && leftDown;
	if (!s.rightDown && !s.leftDown) {
		s.sinceFirst = 0.0f;
	} else if (dtSeconds > 0.0f) {
		s.sinceFirst += dtSeconds;
	}
	if (bothDown && !s.chorded && !(s.rightDown && s.leftDown)) {
		// Both are down for the first time in this press: a chord if the
		// second came quickly, otherwise a click held under a running stick.
		if (s.sinceFirst <= kStickChordWindowSeconds) {
			v.both = true;
			s.chorded = true;
		}
	}
	if (s.rightDown && !rightDown && !s.chorded) {
		v.rightClick = true;
	}
	if (s.leftDown && !leftDown && !s.chorded) {
		v.leftClick = true;
	}
	if (!rightDown && !leftDown) {
		s.chorded = false;
	}
	s.rightDown = rightDown;
	s.leftDown = leftDown;
	return v;
}

// ------------------------------------------------------ Laser drag-scroll
//
// The laser's trigger the way a finger works a touch screen: pulled and let
// go without moving, it is a click, sent on the release; pulled and dragged
// up or down, the list scrolls with the beam as mouse wheel notches and no
// click is sent - and let go while still moving, it coasts on, further the
// faster the flick, slowing to a stop; dragged sideways, the button is held
// down from there on so a slider can be pulled. Pulled on a scroll bar, the
// button is held at once, so the bar's marker is dragged the game's own way.
// Distances are fractions of the layer's height, so the feel does not depend
// on the resolution.

enum class LaserPressPhase { Idle, Pressed, Scrolling, Holding };

struct LaserPressState {
	LaserPressPhase phase = LaserPressPhase::Idle;
	float startX = 0.0f;
	float startY = 0.0f;
	int notchesSent = 0;
	bool clickPending = false;  // the release's click, down now and up next
	float lastY = 0.0f;
	float velocity = 0.0f;      // layer pixels a second, smoothed, while scrolling
	float coastVelocity = 0.0f; // after a flick's release
	float coastPixels = 0.0f;   // travelled while coasting, not yet a notch
};

struct LaserPressVerdict {
	bool mouseDown = false;  // the left button, this frame
	int wheel = 0;           // notches, positive up
};

constexpr float kLaserDragStart = 0.02f;       // of the layer height: a drag, not a wobble
constexpr float kLaserPixelsPerNotch = 0.04f;  // of the layer height, per wheel notch
constexpr float kLaserCoastMinSpeed = 0.5f;    // layer heights a second: slower stops dead
constexpr float kLaserCoastFriction = 3.0f;    // a second: the speed falls by e every third
constexpr float kLaserVelocitySmoothing = 0.5f;  // of each new frame's speed in the average

inline LaserPressVerdict StepLaserPress(LaserPressState& s, bool triggerDown, bool hit,
                                        float x, float y, float layerHeight,
                                        float dtSeconds = 0.0f, bool onScrollBar = false) {
	LaserPressVerdict v;
	const float height = layerHeight > 1.0f ? layerHeight : 1.0f;
	const float notchPixels = kLaserPixelsPerNotch * height;
	const bool dtValid = dtSeconds > 0.0f && dtSeconds <= 0.1f;
	if (s.clickPending) {
		// The release's click went down last frame; this frame it comes up.
		s.clickPending = false;
		s.phase = LaserPressPhase::Idle;
		return v;
	}
	if (!triggerDown) {
		if (s.phase == LaserPressPhase::Pressed && hit) {
			v.mouseDown = true;
			s.clickPending = true;
			s.coastVelocity = 0.0f;
			return v;
		}
		if (s.phase == LaserPressPhase::Scrolling) {
			// Let go while moving: the list coasts on in the same direction.
			const float speed = s.velocity < 0.0f ? -s.velocity : s.velocity;
			s.coastVelocity = speed >= kLaserCoastMinSpeed * height ? s.velocity : 0.0f;
			s.coastPixels = 0.0f;
		}
		s.phase = LaserPressPhase::Idle;
		s.notchesSent = 0;
		s.velocity = 0.0f;
		if (s.coastVelocity != 0.0f && dtValid) {
			s.coastPixels += s.coastVelocity * dtSeconds;
			const int notches = static_cast<int>(s.coastPixels / notchPixels);
			v.wheel = notches;
			s.coastPixels -= static_cast<float>(notches) * notchPixels;
			float decay = 1.0f - kLaserCoastFriction * dtSeconds;
			decay = decay < 0.0f ? 0.0f : decay;
			s.coastVelocity *= decay;
			const float speed = s.coastVelocity < 0.0f ? -s.coastVelocity : s.coastVelocity;
			if (speed < kLaserCoastMinSpeed * 0.5f * height) {
				s.coastVelocity = 0.0f;
			}
		}
		return v;
	}
	switch (s.phase) {
	case LaserPressPhase::Idle:
		s.coastVelocity = 0.0f;  // a new touch stops a coasting list
		if (hit) {
			s.startX = x;
			s.startY = y;
			s.lastY = y;
			s.notchesSent = 0;
			s.velocity = 0.0f;
			if (onScrollBar) {
				s.phase = LaserPressPhase::Holding;
				v.mouseDown = true;
			} else {
				s.phase = LaserPressPhase::Pressed;
			}
		}
		break;
	case LaserPressPhase::Pressed: {
		if (!hit) {
			break;
		}
		const float dx = x - s.startX;
		const float dy = y - s.startY;
		const float ax = dx < 0.0f ? -dx : dx;
		const float ay = dy < 0.0f ? -dy : dy;
		if (ay >= kLaserDragStart * height && ay >= ax) {
			s.phase = LaserPressPhase::Scrolling;
		} else if (ax >= kLaserDragStart * height) {
			s.phase = LaserPressPhase::Holding;
			v.mouseDown = true;
		}
		break;
	}
	case LaserPressPhase::Holding:
		v.mouseDown = true;
		break;
	case LaserPressPhase::Scrolling:
		break;
	}
	if (s.phase == LaserPressPhase::Scrolling && hit) {
		// The list follows the beam: dragged down, the earlier entries come
		// into view - a wheel notch up - as on a touch screen.
		const int wanted = static_cast<int>((y - s.startY) / notchPixels);
		v.wheel = wanted - s.notchesSent;
		s.notchesSent = wanted;
		if (dtValid) {
			const float now = (y - s.lastY) / dtSeconds;
			s.velocity += (now - s.velocity) * kLaserVelocitySmoothing;
		}
	}
	if (hit) {
		s.lastY = y;
	}
	return v;
}

// ------------------------------------------------------------ Tap holds
//
// A control the game toggles on a press - ready weapon, the view switch, the
// quick menu - held down for a short while rather than a single frame: the
// ready weapon tap did not always take in the 2026-09-25 run, and a key
// held for a tenth of a second is what a finger on a keyboard gives anyway.

constexpr float kTapHoldSeconds = 0.12f;

struct TapHoldState {
	float left = 0.0f;
};

inline bool StepTapHold(TapHoldState& s, bool tap, float dtSeconds,
                        float seconds = kTapHoldSeconds) {
	if (tap) {
		s.left = seconds;
	}
	const bool held = s.left > 0.0f;
	if (dtSeconds > 0.0f) {
		s.left -= dtSeconds;
	} else if (!tap) {
		s.left = 0.0f;  // no time to count: one frame, as before
	}
	return held;
}

// ---------------------------------------------------------- Stick flicks
//
// The right stick pushed well up or down, more up or down than sideways: a
// jump or a sneak, once per push. Held back to the middle before the next,
// with some slack, so a thumb resting at the edge does not fire again.

// Near the rim and near straight: 0.7 fired on turns and resting thumbs in
// the first headset run with it.
constexpr float kStickFlick = 0.9f;

struct StickFlickState {
	bool up = false;
	bool down = false;
};

struct StickFlickVerdict {
	bool up = false;
	bool down = false;
};

inline StickFlickVerdict StepStickFlick(StickFlickState& s, float x, float y) {
	StickFlickVerdict v;
	if (!(x == x) || !(y == y)) {
		x = 0.0f;
		y = 0.0f;
	}
	const float ax = x < 0.0f ? -x : x;
	const float ay = y < 0.0f ? -y : y;
	const bool up = y >= kStickFlick && ay >= 2.0f * ax;
	const bool down = y <= -kStickFlick && ay >= 2.0f * ax;
	v.up = up && !s.up;
	v.down = down && !s.down;
	s.up = up || (s.up && y > kStickFlick * 0.5f);
	s.down = down || (s.down && y < -kStickFlick * 0.5f);
	return v;
}

// ------------------------------------------------------------ Ready weapon
//
// The ready-weapon click as a wish for the other state, followed until the
// game shows it. A plain key tap was not enough: the game does not ready or
// sheathe while the player blocks, and the block is a gesture - a left hand
// up in front of the chest - that is easily up while the right thumb clicks.
// Every tap of the 2026-09-25 evening run fell inside one; letting go of
// block for the tap's own frames did not help either (the late run: eight
// taps, each "the weapon was drawn", all inside a block from 1090 to 1248),
// because the player is still in the block action until its animation has
// lowered the shield.
//
// So: the click decides which state is wanted. Block is kept off while the
// wish stands; the key goes down once the player is in neither the block nor
// an equip/unequip action; and if the state has not changed kReadyRetrySeconds
// after that, the key goes down again - up to kReadyGiveUpSeconds in all.
// The actions are HighProcess's kAction_ values (xOBSE obse/GameProcess.h:
// EquipWeapon 0, UnequipWeapon 1, Block 6; IsBlocking() is action == Block).
// With the weapon's state unreadable the click is a plain tap, as before.
constexpr SInt32 kPlayerActionEquipWeapon = 0;
constexpr SInt32 kPlayerActionUnequipWeapon = 1;
constexpr SInt32 kPlayerActionBlock = 6;
constexpr float kReadyRetrySeconds = 0.6f;
constexpr float kReadyGiveUpSeconds = 2.5f;
// For a frame without a time: the headset's 90 Hz.
constexpr float kReadyNominalFrameSeconds = 1.0f / 90.0f;

enum class WeaponSeen { Unknown, Sheathed, Drawn };

struct ReadyWeaponState {
	bool pending = false;
	bool wantDrawn = false;
	float hold = 0.0f;   // the key still down for this long
	float wait = 0.0f;   // until the next press may go
	float left = 0.0f;   // until the wish is given up
};

struct ReadyWeaponVerdict {
	bool key = false;        // the ready-weapon key down this frame
	bool dropBlock = false;  // block kept off this frame
	bool gaveUp = false;     // the wish was given up this frame, for the log
	bool reached = false;    // the game showed the wanted state this frame
};

inline ReadyWeaponVerdict StepReadyWeapon(ReadyWeaponState& s, bool click, WeaponSeen seen,
                                          SInt32 action, float dtSeconds) {
	ReadyWeaponVerdict v;
	const float dt = dtSeconds > 0.0f ? dtSeconds : kReadyNominalFrameSeconds;
	if (click) {
		if (seen == WeaponSeen::Unknown) {
			s = ReadyWeaponState{};
			s.hold = kTapHoldSeconds;  // nothing to follow: a plain tap
		} else if (s.pending) {
			s.wantDrawn = !s.wantDrawn;  // a second click takes the wish back
			s.left = kReadyGiveUpSeconds;
		} else {
			s = ReadyWeaponState{};
			s.pending = true;
			s.wantDrawn = seen == WeaponSeen::Sheathed;
			s.left = kReadyGiveUpSeconds;
		}
	}
	if (!s.pending) {
		v.key = s.hold > 0.0f;
		s.hold = v.key ? s.hold - dt : 0.0f;
		return v;
	}
	const bool animating =
		action == kPlayerActionEquipWeapon || action == kPlayerActionUnequipWeapon;
	const bool there = seen == (s.wantDrawn ? WeaponSeen::Drawn : WeaponSeen::Sheathed);
	if (there && !animating && s.hold <= 0.0f) {
		s = ReadyWeaponState{};
		v.reached = true;
		return v;
	}
	v.dropBlock = true;
	if (s.hold > 0.0f) {
		v.key = true;
		s.hold -= dt;
	} else if (animating) {
		s.wait = kReadyRetrySeconds;  // the game is on it: the retry counts from its end
	} else if (s.wait > 0.0f) {
		s.wait -= dt;
	} else if (action != kPlayerActionBlock && !there) {
		v.key = true;
		s.hold = kTapHoldSeconds - dt;
		s.wait = kReadyRetrySeconds;
	}
	s.left -= dt;
	if (s.left <= 0.0f) {
		s = ReadyWeaponState{};
		v.gaveUp = true;
	}
	return v;
}

// Left-handed in Full VR: the two controllers swap roles as a whole - pose
// and every button, trigger and stick. The game's weapon hand (its right,
// "Bip01 R Hand", where the weapon hangs) follows the left controller, the
// shield and torch hand the right one; the left trigger attacks, the swing
// and the blade are the left controller's, the right one walks and blocks
// (2026-09-25: "für Linkshänder auch an den linken Controller wandern").
// Everything downstream is written for roles; only what is hung on a
// physical device - the beam, the wrist quads - asks HandDeviceForRole.
inline bool AssignHandRoles(HandPose& right, HandPose& left, bool leftHanded) {
	if (leftHanded) {
		const HandPose physicalRight = right;
		right = left;
		left = physicalRight;
	}
	return leftHanded;
}

// Which physical controller plays a role: the right role is the left
// controller when the roles were swapped.
inline bool HandDeviceForRole(bool rightRole, bool rolesSwapped) {
	return rightRole != rolesSwapped;
}

// Run, held or toggled. Held (the default): the run control is down while
// the left stick is pressed in. Toggled: a click of the left stick - one
// released alone, so both sticks together still open OBVR's menu - switches
// running on, the next one off; OBVR holds the run control in between.
inline bool StepRunToggle(bool& latched, bool toggleMode, bool click, bool held) {
	if (!toggleMode) {
		latched = false;
		return held;
	}
	if (click) {
		latched = !latched;
	}
	return latched;
}

// Whether a swing presses the attack control. Not when the swung weapon
// strikes by motion (the blade itself hits), and not with the weapon away:
// the attack control readies a sheathed weapon (a player on the gamesas
// forum, 2011: "if my weapon was sheathed, one left-click would draw
// (ready) my weapon"), so a hand moved quickly to reach for or throw
// something drew the fists (2026-09-25). Not while a grip is closed either:
// that hand is holding or reaching, not striking.
inline bool SwingPressesAttack(bool strikeByMotion, bool weaponDrawn, bool gripHeld) {
	return !strikeByMotion && weaponDrawn && !gripHeld;
}

// ------------------------------------------------------------ Grab by reach
//
// The grab is the game's own (Z): it takes the reference the activation pick
// found, holds it on a Havok spring and, let go, leaves it with the spring's
// speed - which is the throw. What OBVR adds is how it starts: the hand goes
// to the object and the grip closes. While a grip is held, the pick runs
// along that hand's laser, started a reach behind the hand (reachPick); once
// the pick has had kGrabReachSettleFrames to run that way and the point it
// touched lies within reach of the hand, the key goes down and stays down until the
// grip opens. A grip closed on nothing within reach grabs nothing.
constexpr int kGrabReachSettleFrames = 2;

struct GrabReachState {
	bool grabbing = false;
	int armedFrames = 0;
};

struct GrabReachVerdict {
	bool reachPick = false;  // the pick runs through the grabbing hand
	bool key = false;        // the grab key is down
};

inline GrabReachVerdict StepGrabReach(GrabReachState& s, bool gripHeld, bool targetInReach) {
	GrabReachVerdict v;
	if (!gripHeld) {
		s = GrabReachState{};
		return v;
	}
	// The pick stays through the hand while the key is down. The key is sent
	// at Present and read by the game's input on a later frame; the pick went
	// back to the laser the moment the key went down, so by the time the game
	// looked for what to take, the target had moved off the object: the
	// 2026-09-25 late run sent the key twenty times ("took ... key 5A down")
	// and the grab update never ran once.
	if (s.grabbing) {
		v.key = true;
		v.reachPick = true;
		return v;
	}
	if (s.armedFrames >= kGrabReachSettleFrames && targetInReach) {
		s.grabbing = true;
		v.key = true;
		v.reachPick = true;
		return v;
	}
	if (s.armedFrames < kGrabReachSettleFrames) {
		++s.armedFrames;
	}
	v.reachPick = true;
	return v;
}

// Which hand's laser the world pick runs along while no grip is closed. The
// engine picks along one ray a frame, and it was always the right hand's:
// the left hand's reach marker and tooltip came only once its grip had
// closed (2026-09-26). The pick now follows the hand that has been moving:
// each hand's recent motion (metres, fading over kPickHandMemorySeconds),
// and the pick goes over when the other hand leads by kPickHandMarginMetres
// - so a hand reaching for something takes the pick, and one held still
// does not take it back. An untracked hand never has it.
//
// The left hand keeps it only while what its laser finds is within its
// reach: kPickHandLetGoSeconds without, and the pick goes back to the right
// hand - the weapon hand - and the left may not take it again for
// kPickHandCooldownSeconds, or a left hand still moving away would take it
// straight back (2026-09-26: the laser stayed on the left hand after it
// had moved away from the object).
constexpr float kPickHandMemorySeconds = 0.5f;
constexpr float kPickHandMarginMetres = 0.05f;
constexpr float kPickHandLetGoSeconds = 0.4f;
constexpr float kPickHandCooldownSeconds = 1.0f;

struct PickHandState {
	bool left = false;
	float rightMotion = 0.0f;
	float leftMotion = 0.0f;
	float leftMiss = 0.0f;  // seconds the left hand's pick has found nothing in reach
	float cooldown = 0.0f;  // seconds before the left hand may take the pick again
};

inline bool StepPickHand(PickHandState& s, bool rightValid, bool leftValid, float rightSpeed,
                         float leftSpeed, float dtSeconds, bool leftInReach = true) {
	float keep = dtSeconds > 0.0f ? 1.0f - dtSeconds / kPickHandMemorySeconds : 1.0f;
	if (keep < 0.0f) {
		keep = 0.0f;
	}
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	s.rightMotion = rightValid ? s.rightMotion * keep + rightSpeed * dt : 0.0f;
	s.leftMotion = leftValid ? s.leftMotion * keep + leftSpeed * dt : 0.0f;
	s.cooldown = s.cooldown > dt ? s.cooldown - dt : 0.0f;
	if (!leftValid) {
		s.left = false;
	} else if (!rightValid) {
		s.left = true;
	} else if (s.left) {
		s.leftMiss = leftInReach ? 0.0f : s.leftMiss + dt;
		if (s.rightMotion > s.leftMotion + kPickHandMarginMetres) {
			s.left = false;
		} else if (s.leftMiss >= kPickHandLetGoSeconds) {
			s.left = false;
			s.leftMotion = 0.0f;
			s.cooldown = kPickHandCooldownSeconds;
		}
	} else if (s.cooldown <= 0.0f && s.leftMotion > s.rightMotion + kPickHandMarginMetres) {
		s.left = true;
		s.leftMiss = 0.0f;
	}
	return s.left;
}

// Where the held object goes: along the line from the head to the hand, as
// far as the hand is. headRelative is the hand's offset in the head's frame
// (game axes: x right, y forward, z up); trackingDelta is the same offset in
// the room (OpenVR axes, y up), for the pitch against the true vertical. The
// turn is counter-clockwise positive, the way HeadingOf reads the hand's
// orientation; the sine is up positive, the way SinPitchOf reads a forward
// axis. False for a hand at the eyes, whose direction is not a direction.
inline bool ReachDirection(const NiPoint3& headRelative, const NiPoint3& trackingDelta,
                           float& yawTurn, float& sinPitch) {
	const float flat = headRelative.x * headRelative.x + headRelative.y * headRelative.y;
	const float lengthSquared = trackingDelta.LengthSquared();
	if (!(flat > 1.0e-6f) || !(lengthSquared > 1.0e-6f)) {
		return false;
	}
	yawTurn = math::Atan2(-headRelative.x, headRelative.y);
	sinPitch = trackingDelta.y / math::Sqrt(lengthSquared);
	if (sinPitch > 1.0f) {
		sinPitch = 1.0f;
	} else if (sinPitch < -1.0f) {
		sinPitch = -1.0f;
	}
	return true;
}

// ------------------------------------------------------ Sneak, held or toggled
//
// Oblivion's sneak key toggles. In toggle mode a flick down is that key, as
// before. In hold mode the player sneaks while the right stick is held down:
// the key is tapped whenever what the stick says differs from what the game
// says, and then not again until the game has followed or kSneakTapRetrySeconds
// have passed - a tap takes a few frames to show in the movement flags, and
// tapping on every one of them would toggle straight back.
constexpr float kSneakTapRetrySeconds = 0.5f;

struct SneakHoldState {
	float wait = 0.0f;
};

inline bool StepSneakTap(SneakHoldState& s, bool holdMode, bool flickDown, bool stickHeldDown,
                         bool sneaking, float dtSeconds) {
	if (!holdMode) {
		s.wait = 0.0f;
		return flickDown;
	}
	if (s.wait > 0.0f) {
		if (sneaking == stickHeldDown) {
			s.wait = 0.0f;  // the game followed
		} else if (dtSeconds > 0.0f) {
			s.wait -= dtSeconds;
		}
		return false;
	}
	if (stickHeldDown != sneaking) {
		s.wait = kSneakTapRetrySeconds;
		return true;
	}
	return false;
}

// ----------------------------------------------------------- Stick as keys
//
// A stick pushed past the dead zone fires a direction once and again only
// after it has come back - the arrow keys of OBVR's own menu.

struct StickNavState {
	StickDirections was;
};

struct StickNavVerdict {
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
};

inline StickNavVerdict StepStickNav(StickNavState& s, float x, float y, float deadZone) {
	const StickDirections now = StickToDirections(x, y, deadZone);
	StickNavVerdict v;
	v.up = now.forward && !s.was.forward;
	v.down = now.back && !s.was.back;
	v.left = now.left && !s.was.left;
	v.right = now.right && !s.was.right;
	s.was = now;
	return v;
}

}  // namespace obvr::vr

#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// The hand-tracked mode's settings, all hot reloaded from [Hands].
struct HandSettings {
	bool enabled = false;
	GestureThresholds gestures;

	// The arms. Where the right hand rests when the animation's own pose is
	// right - metres relative to the eyes, x right, y forward, z up - so the
	// arms are moved by how far the controller is from there rather than by
	// where it is; and whether pitch and position follow at all.
	float restHandRight = 0.20f;
	float restHandForward = 0.35f;
	float restHandUp = -0.30f;
	bool armsFollowPitch = true;
	bool armsFollowPosition = true;
	float armOffsetScale = 1.0f;

	// The wrists: where the quad sits relative to the controller, in metres
	// along the controller's up and back axes, and how far it is tilted
	// towards the face.
	float wristUp = 0.06f;
	float wristBack = 0.12f;
	float wristTiltDegrees = 35.0f;
	float wristHudWidth = 0.16f;   // the HUD on the right wrist
	float wristMenuWidth = 0.45f;  // the Tab menu on the left wrist
	bool wristHud = true;
	bool wristMenu = true;

	// The laser cursor: how much of the remaining distance the game's cursor
	// is walked per frame, and the largest step.
	float laserGain = 0.5f;
	float laserMaxStep = 60.0f;

	// The sticks: dead zone, and mouse pixels per frame at full deflection
	// for the turn.
	float stickDeadZone = 0.4f;
	float turnSpeed = 12.0f;
};

// Everything one frame of the mode needs to know, gathered by the camera
// hook. Poses are as the backend reads them - OpenVR convention, seated
// space - and converted in here.
struct HandModeFrame {
	float dtSeconds = 0.0f;
	bool menuMode = false;
	bool firstPerson = true;
	bool headValid = false;
	Quaternion head = Quaternion::Identity();
	NiPoint3 headPosition{0.0f, 0.0f, 0.0f};
	HandPose right;
	HandPose left;
	float unitsPerMetre = 70.0f;
	// For the laser: the pixels the left wrist's quad shows, and where the
	// game's cursor currently is in those pixels.
	float layerPixelsWidth = 0.0f;
	float layerPixelsHeight = 0.0f;
	bool cursorValid = false;
	float cursorX = 0.0f;
	float cursorY = 0.0f;
};

struct HandModeResult {
	// The aim, when the right hand is tracked: its heading as a turn from
	// the head's, and the sine of its pitch (positive up), both in the
	// game's convention.
	bool aimValid = false;
	float aimYawTurn = 0.0f;
	float aimSinPitch = 0.0f;

	// The arms: a rotation relative to the head and an offset in game
	// units, both in the game's convention, to apply in the render pass.
	bool armsValid = false;
	NiMatrix33 armsRotation{};
	NiPoint3 armsOffsetUnits{0.0f, 0.0f, 0.0f};

	// The controls to press, and whether any are to be pressed at all
	// (false releases everything).
	bool controlsActive = false;
	HandControlsWanted controls;

	// The wrists: device-to-overlay transforms for the HUD and the menu.
	bool hudOnRightWrist = false;
	bool menuOnLeftWrist = false;
	openvr::HmdMatrix34 hudTransform{};
	openvr::HmdMatrix34 menuTransform{};

	// The laser: the mouse step that walks the cursor towards the hit.
	bool laserHit = false;
	int cursorDx = 0;
	int cursorDy = 0;

	// The grab: whether the right grip holds it, and how far the right hand
	// is from the eyes in metres - the distance the held object is kept at.
	bool grabWanted = false;
	float grabDistanceMetres = 0.0f;

	// For the log.
	bool blocking = false;
	bool reachBack = false;
	SwingVerdict swing = SwingVerdict::None;
};

// The device-to-overlay transform for a quad on a wrist: the controller's
// frame is x right, y up, -z forward along the pointing direction, so the
// quad goes up by `up`, back along +z by `back`, and is turned from facing
// +z (an overlay's front) to facing up and tilted back towards the eyes by
// the given degrees.
openvr::HmdMatrix34 WristOverlayTransform(float up, float back, float tiltDegrees);

// The mode's per-frame decision, with the little state a frame carries into
// the next: trigger and button edges, the swing in progress, the held heavy
// attack, the last hand position.
class HandMode {
public:
	HandModeResult Update(const HandModeFrame& frame, const HandSettings& settings);

	// Forgets the edges and the swing, for when the mode is switched off or a
	// controller is lost mid-swing.
	void Reset();

private:
	TriggerEdge m_rightTrigger;
	TriggerEdge m_leftTrigger;
	ButtonEdge m_rightMenu;
	ButtonEdge m_leftMenu;
	ButtonEdge m_rightStick;
	ButtonEdge m_leftStick;
	SwingDetector m_swing;
	HeldControl m_heavyHold;
	bool m_haveLastRight = false;
	bool m_reachArmed = false;  // the reach back seen since the last release
	bool m_reachSpent = false;  // a draw used the armed reach
	NiPoint3 m_lastRightRelative{0.0f, 0.0f, 0.0f};
};

}  // namespace obvr::vr

#include "vr/HandMode.h"

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "vr/HeadOffset.h"

namespace obvr::vr {

openvr::HmdMatrix34 WristOverlayTransform(float up, float back, float tiltDegrees) {
	// Rotation about the controller's x axis by -(90 - tilt) degrees: the
	// overlay's front (+z) turns towards +y (up) and leans back by the tilt
	// so it faces the eyes of someone looking down at their wrist.
	const float radians = -(90.0f - tiltDegrees) * (math::kPi / 180.0f);
	const float c = math::Cos(radians);
	const float s = math::Sin(radians);
	openvr::HmdMatrix34 m{};
	m.m[0][0] = 1.0f;
	m.m[1][1] = c;
	m.m[1][2] = -s;
	m.m[2][1] = s;
	m.m[2][2] = c;
	m.m[0][3] = 0.0f;
	m.m[1][3] = up;
	m.m[2][3] = back;
	return m;
}

namespace {

// Rotates a vector by a quaternion's matrix, staying in the OpenVR
// convention - for the laser, where every point is in tracking space.
NiPoint3 TrackingRotate(const Quaternion& q, const NiPoint3& v) { return Rotate(q, v); }

}  // namespace

void HandMode::Reset() {
	m_rightTrigger = TriggerEdge{};
	m_leftTrigger = TriggerEdge{};
	m_rightMenu = ButtonEdge{};
	m_leftMenu = ButtonEdge{};
	m_rightStick = ButtonEdge{};
	m_leftStick = ButtonEdge{};
	m_swing = SwingDetector{};
	m_heavyHold = HeldControl{};
	m_haveLastRight = false;
	m_reachArmed = false;
	m_reachSpent = false;
}

HandModeResult HandMode::Update(const HandModeFrame& f, const HandSettings& s) {
	HandModeResult r;
	if (!s.enabled || !f.headValid) {
		Reset();
		return r;
	}

	// The hands relative to the head, in metres, in the game's axes: what
	// every gesture reads.
	NiPoint3 rightRelative{0.0f, 0.0f, 0.0f};
	NiPoint3 leftRelative{0.0f, 0.0f, 0.0f};
	if (f.right.valid) {
		rightRelative = OffsetFromPose(f.head, f.right.position, f.headPosition, 1.0f);
	}
	if (f.left.valid) {
		leftRelative = OffsetFromPose(f.head, f.left.position, f.headPosition, 1.0f);
	}

	// The aim and the arms from the right hand's orientation. The relative
	// rotation is head-conjugate then hand - hand first, head undone after -
	// changed into the game's basis the way the head itself is.
	if (f.right.valid) {
		const Quaternion relative = (f.head.Conjugate() * f.right.orientation).Normalized();
		const NiMatrix33 relativeMatrix = ToMatrix(FromOpenXR(relative));
		const NiMatrix33 absoluteMatrix = ToMatrix(FromOpenXR(f.right.orientation));
		Heading heading{};
		if (HeadingOf(relativeMatrix, heading)) {
			r.aimValid = true;
			r.aimYawTurn = math::Atan2(heading.sine, heading.cosine);
			r.aimSinPitch = SinPitchOf(absoluteMatrix);
		}
		if (f.firstPerson && !f.menuMode) {
			r.armsValid = true;
			if (s.armsFollowPitch) {
				r.armsRotation = relativeMatrix;
			} else {
				r.armsRotation = RotationFromHeading(heading);
			}
			if (s.armsFollowPosition) {
				const NiPoint3 rest{s.restHandRight, s.restHandForward, s.restHandUp};
				r.armsOffsetUnits = (rightRelative - rest) * (f.unitsPerMetre * s.armOffsetScale);
			}
		}
	}

	// Gestures.
	r.blocking = f.left.valid && !f.menuMode && IsBlockGesture(leftRelative, s.gestures);
	r.reachBack = f.right.valid && IsReachBackGesture(rightRelative, s.gestures);

	// The swing, from the right hand's speed across the head-relative frame
	// (so walking does not swing the sword).
	if (f.right.valid && !f.menuMode) {
		if (m_haveLastRight) {
			const float speed = HandSpeed(m_lastRightRelative, rightRelative, f.dtSeconds);
			r.swing = StepSwing(m_swing, speed, s.gestures);
			if (r.swing == SwingVerdict::Heavy) {
				HoldFor(m_heavyHold, s.gestures.heavyHoldSeconds);
			} else if (r.swing == SwingVerdict::Light) {
				HoldFor(m_heavyHold, 0.05f);  // a tap: down this frame, up soon after
			}
		}
		m_lastRightRelative = rightRelative;
		m_haveLastRight = true;
	} else {
		m_haveLastRight = false;
		m_swing = SwingDetector{};
	}
	const bool swingHeld = StepHeld(m_heavyHold, f.dtSeconds);

	// The controls.
	HandFrameInput in;
	in.rightValid = f.right.valid;
	in.leftValid = f.left.valid;
	in.rightTrigger = f.right.valid && StepTrigger(m_rightTrigger, f.right.trigger);
	in.leftTrigger = f.left.valid && StepTrigger(m_leftTrigger, f.left.trigger);
	in.rightGrip = f.right.valid && ButtonDown(f.right.buttonsPressed, openvr::kButtonGrip);
	in.leftGrip = f.left.valid && ButtonDown(f.left.buttonsPressed, openvr::kButtonGrip);
	in.rightA = f.right.valid && ButtonDown(f.right.buttonsPressed, openvr::kButtonA);
	in.leftA = f.left.valid && ButtonDown(f.left.buttonsPressed, openvr::kButtonA);
	in.rightMenuButton = StepRisingEdge(
		m_rightMenu, f.right.valid && ButtonDown(f.right.buttonsPressed, openvr::kButtonApplicationMenu));
	in.leftMenuButton = StepRisingEdge(
		m_leftMenu, f.left.valid && ButtonDown(f.left.buttonsPressed, openvr::kButtonApplicationMenu));
	in.rightStickClick = StepRisingEdge(
		m_rightStick, f.right.valid && ButtonDown(f.right.buttonsPressed, openvr::kButtonAxis0));
	in.leftStickClick = StepRisingEdge(
		m_leftStick, f.left.valid && ButtonDown(f.left.buttonsPressed, openvr::kButtonAxis0));
	in.leftThumbX = f.left.thumbX;
	in.leftThumbY = f.left.thumbY;
	in.rightThumbX = f.right.thumbX;
	in.blockGesture = r.blocking;
	in.swingAttackHeld = swingHeld;
	// The reach-back gate: armed by the gesture, spent when the trigger comes
	// up after a draw, so every arrow wants a fresh reach over the shoulder.
	if (r.reachBack) {
		m_reachArmed = true;
	}
	if (!in.rightTrigger) {
		m_reachSpent = false;
	} else if (m_reachArmed) {
		m_reachSpent = true;
	}
	in.drawBlocked = s.gestures.bowNeedsReachBack && !m_reachArmed;
	if (!in.rightTrigger && m_reachSpent) {
		m_reachArmed = false;
	}
	in.menuMode = f.menuMode;
	r.controls = PlanHandControls(in, s.stickDeadZone);
	r.controlsActive = f.right.valid || f.left.valid;
	r.grabWanted = r.controls.grab;
	if (f.right.valid) {
		r.grabDistanceMetres = math::Sqrt(rightRelative.LengthSquared());
	}

	// The wrists.
	if (s.wristHud && f.right.valid && !f.menuMode) {
		r.hudOnRightWrist = true;
		r.hudTransform = WristOverlayTransform(s.wristUp, s.wristBack, s.wristTiltDegrees);
	}
	if (s.wristMenu && f.left.valid && f.menuMode) {
		r.menuOnLeftWrist = true;
		r.menuTransform = WristOverlayTransform(s.wristUp, s.wristBack, s.wristTiltDegrees);
	}

	// The laser, in tracking space: the right hand's ray against the quad the
	// left wrist carries, then a mouse step towards the pixel it hits.
	if (r.menuOnLeftWrist && f.right.valid && f.cursorValid && f.layerPixelsWidth > 0.0f &&
	    f.layerPixelsHeight > 0.0f) {
		const openvr::HmdMatrix34& t = r.menuTransform;
		const NiPoint3 localCentre{t.m[0][3], t.m[1][3], t.m[2][3]};
		const NiPoint3 localRight{t.m[0][0], t.m[1][0], t.m[2][0]};
		const NiPoint3 localUp{t.m[0][1], t.m[1][1], t.m[2][1]};
		const NiPoint3 centre = f.left.position + TrackingRotate(f.left.orientation, localCentre);
		const NiPoint3 right = TrackingRotate(f.left.orientation, localRight);
		const NiPoint3 up = TrackingRotate(f.left.orientation, localUp);
		const NiPoint3 rayDirection =
			TrackingRotate(f.right.orientation, NiPoint3{0.0f, 0.0f, -1.0f});
		const float quadHeight = s.wristMenuWidth * (f.layerPixelsHeight / f.layerPixelsWidth);
		const LaserHit hit = LaserOnQuad(f.right.position, rayDirection, centre, right, up,
		                                 s.wristMenuWidth, quadHeight, f.layerPixelsWidth,
		                                 f.layerPixelsHeight);
		if (hit.hit) {
			r.laserHit = true;
			r.cursorDx = CursorStep(f.cursorX, hit.pixelX, s.laserGain, s.laserMaxStep);
			r.cursorDy = CursorStep(f.cursorY, hit.pixelY, s.laserGain, s.laserMaxStep);
		}
	}

	return r;
}

}  // namespace obvr::vr

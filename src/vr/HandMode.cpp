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

// How far ahead of the flat picture's anchor the beam is drawn to end. The
// picture itself is at infinity, so this is only where the beam stops; the
// pixel it lands on is worked out from the head's view of that point and
// does not depend on the figure.
constexpr float kFlatLaserPlaneMetres = 2.0f;

// The share of OBVR's own panel, top and bottom, that scrolls the list
// while the beam rests on it: the title band and the help band, where no
// row is drawn (two lines of nine font pixels at scale three, plus the
// margin, of 768 - a little under a tenth).
constexpr float kPanelScrollBand = 0.09f;

}  // namespace

void HandMode::StepPointerHand(const HandModeFrame& f, bool rightTrigger, bool leftTrigger) {
	// A pull, not a hold: the hand that pulls its trigger takes the pointer
	// with it, and the pull that took it is also the click, so the first
	// press after a switch lands where the new hand points a frame later.
	const bool rightPulled = StepRisingEdge(m_rightPointEdge, f.right.valid && rightTrigger);
	const bool leftPulled = StepRisingEdge(m_leftPointEdge, f.left.valid && leftTrigger);
	if (rightPulled && !leftPulled) {
		m_pointRight = true;
	} else if (leftPulled && !rightPulled) {
		m_pointRight = false;
	}
	// A hand that is not tracked cannot hold the pointer.
	if (m_pointRight && !f.right.valid && f.left.valid) {
		m_pointRight = false;
	} else if (!m_pointRight && !f.left.valid && f.right.valid) {
		m_pointRight = true;
	}
}

void HandMode::Reset() {
	m_pointRight = true;
	m_clickBlocked = false;
	m_rightPointEdge = ButtonEdge{};
	m_leftPointEdge = ButtonEdge{};
	m_rightTrigger = TriggerEdge{};
	m_leftTrigger = TriggerEdge{};
	m_rightTriggerEdge = ButtonEdge{};
	m_leftTriggerEdge = ButtonEdge{};
	m_rightGripEdge = ButtonEdge{};
	m_leftGripEdge = ButtonEdge{};
	m_rightAEdge = ButtonEdge{};
	m_leftAEdge = ButtonEdge{};
	m_rightMenu = ButtonEdge{};
	m_leftMenu = ButtonEdge{};
	m_scrollUp = RepeatState{};
	m_scrollDown = RepeatState{};
	m_sticks = StickChordState{};
	m_navRight = StickNavState{};
	m_navLeft = StickNavState{};
	m_poke = PokeState{};
	m_swing = SwingDetector{};
	m_heavyHold = HeldControl{};
	m_haveLastRight = false;
	m_reachArmed = false;
	m_reachSpent = false;
}

HandModeResult HandMode::Update(const HandModeFrame& f, const HandSettings& s) {
	HandModeResult r;
	if (!f.headValid || (!s.enabled && !f.menusOnly)) {
		Reset();
		return r;
	}
	if (!s.enabled) {
		return UpdateMenusOnly(f, s);
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
		if (f.firstPerson) {
			r.rightHandValid = true;
			r.rightHandRotation = relativeMatrix;
			r.rightHandOffsetUnits = rightRelative * f.unitsPerMetre;
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

	if (f.left.valid && f.firstPerson) {
		const Quaternion relative = (f.head.Conjugate() * f.left.orientation).Normalized();
		r.leftHandValid = true;
		r.leftHandRotation = ToMatrix(FromOpenXR(relative));
		r.leftHandOffsetUnits = leftRelative * f.unitsPerMetre;
	}

	// Gestures.
	r.blocking = f.left.valid && !f.menuMode && IsBlockGesture(leftRelative, s.gestures);
	r.reachBack = f.right.valid && IsReachBackGesture(rightRelative, s.gestures);

	// The swing, from the right hand's speed across the head-relative frame
	// (so walking does not swing the sword).
	//
	// With a swung weapon in hand and strikes by motion on, the swing presses
	// nothing: the blade itself strikes what it passes through (MeleeHits),
	// heavy when the swing has been fast enough by then. Otherwise a swing
	// taps or holds the attack control and the engine's animation decides.
	r.strikeByMotion = s.motionHits && f.meleeInHand;
	if (f.right.valid && !f.menuMode) {
		if (m_haveLastRight) {
			const float speed = HandSpeed(m_lastRightRelative, rightRelative, f.dtSeconds);
			const bool wasSwinging = m_swing.swinging;
			r.swing = StepSwing(m_swing, speed, s.gestures);
			if (m_swing.swinging && !wasSwinging) {
				++m_swingSerial;
			}
			if (!r.strikeByMotion) {
				if (r.swing == SwingVerdict::Heavy) {
					HoldFor(m_heavyHold, s.gestures.heavyHoldSeconds);
				} else if (r.swing == SwingVerdict::Light) {
					HoldFor(m_heavyHold, 0.05f);  // a tap: down this frame, up soon after
				}
			}
		}
		m_lastRightRelative = rightRelative;
		m_haveLastRight = true;
	} else {
		m_haveLastRight = false;
		m_swing = SwingDetector{};
	}
	r.swingActive = m_swing.swinging;
	r.swingHeavy = m_swing.swinging && m_swing.peakSpeed >= s.gestures.swingHeavy;
	r.swingSerial = m_swingSerial;
	const bool swingHeld = StepHeld(m_heavyHold, f.dtSeconds);

	// The sticks' clicks: both together is OBVR's own menu, one alone fires on
	// its release.
	const StickChordVerdict sticks = StepChord(f, r);

	// OBVR's own menu open: the sticks are its arrow keys, either hand's, and
	// nothing reaches the game - a stick that scrolls the menu must not walk
	// the player at the same time.
	if (f.settingsMenuOpen) {
		SteerSettingsMenu(f, s, r);
		return r;
	}
	m_navRight = StickNavState{};
	m_navLeft = StickNavState{};

	// The controls.
	HandFrameInput in;
	in.rightValid = f.right.valid;
	in.leftValid = f.left.valid;
	in.rightTrigger = f.right.valid && StepTrigger(m_rightTrigger, f.right.trigger);
	in.leftTrigger = f.left.valid && StepTrigger(m_leftTrigger, f.left.trigger);
	in.rightGrip = f.right.valid && GripDown(f.right.buttonsPressed);
	in.leftGrip = f.left.valid && GripDown(f.left.buttonsPressed);
	in.rightA = f.right.valid && ButtonADown(f.right.buttonsPressed);
	in.leftA = f.left.valid && ButtonADown(f.left.buttonsPressed);
	in.rightMenuButton = StepRisingEdge(
		m_rightMenu, f.right.valid && ButtonBDown(f.right.buttonsPressed));
	in.leftMenuButton = StepRisingEdge(
		m_leftMenu, f.left.valid && ButtonBDown(f.left.buttonsPressed));
	in.rightStickClick = sticks.rightClick;
	in.leftStickClick = sticks.leftClick;
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
	in.meleeByMotion = r.strikeByMotion;
	if (!in.rightTrigger && m_reachSpent) {
		m_reachArmed = false;
	}
	in.menuMode = f.menuMode;
	if (f.menuMode) {
		const bool wasRight = m_pointRight;
		StepPointerHand(f, in.rightTrigger, in.leftTrigger);
		if (m_pointRight != wasRight) {
			m_clickBlocked = true;
		}
	} else {
		m_rightPointEdge = ButtonEdge{};
		m_leftPointEdge = ButtonEdge{};
		m_clickBlocked = false;
	}
	in.pointRight = m_pointRight;
	r.controls = PlanHandControls(in, s.stickDeadZone);
	// The pull that moved the pointer over is not a click: the cursor is
	// still where the other hand left it. The trigger has to come up first.
	if (m_clickBlocked) {
		if (!(m_pointRight ? in.rightTrigger : in.leftTrigger)) {
			m_clickBlocked = false;
		}
		r.controls.menuClick = false;
	}
	r.controlsActive = f.right.valid || f.left.valid;
	r.grabWanted = r.controls.grab;
	if (f.right.valid) {
		r.grabDistanceMetres = math::Sqrt(rightRelative.LengthSquared());
	}

	// The wrists, and the hands on the menu.
	if (s.wristHud && f.right.valid && !f.menuMode) {
		r.hudOnRightWrist = true;
		r.hudTransform = WristOverlayTransform(s.wristUp, s.wristBack, s.wristTiltDegrees);
	}
	PointAtMenu(f, s, r);

	return r;
}

StickChordVerdict HandMode::StepChord(const HandModeFrame& f, HandModeResult& r) {
	const StickChordVerdict sticks = StepStickChord(
		m_sticks, f.right.valid && StickClickDown(f.right.buttonsPressed),
		f.left.valid && StickClickDown(f.left.buttonsPressed));
	r.settingsMenuToggle = sticks.both;
	return sticks;
}

void HandMode::SteerSettingsMenu(const HandModeFrame& f, const HandSettings& s,
                                 HandModeResult& r) {
	// The sticks are the arrow keys, either hand's.
	const StickNavVerdict right =
		StepStickNav(m_navRight, f.right.thumbX, f.right.thumbY, s.stickDeadZone);
	const StickNavVerdict left =
		StepStickNav(m_navLeft, f.left.thumbX, f.left.thumbY, s.stickDeadZone);
	r.settingsNav.up = right.up || left.up;
	r.settingsNav.down = right.down || left.down;
	r.settingsNav.left = right.left || left.left;
	r.settingsNav.right = right.right || left.right;

	// The buttons: a trigger or an A is Right - the row's next value, the
	// walkthrough's next page - a grip is Left, and a menu button closes the
	// menu. Edges, so a held trigger is one step.
	const bool rightTrigger = f.right.valid && StepTrigger(m_rightTrigger, f.right.trigger);
	const bool leftTrigger = f.left.valid && StepTrigger(m_leftTrigger, f.left.trigger);
	const bool rightPull = StepRisingEdge(m_rightTriggerEdge, rightTrigger);
	const bool leftPull = StepRisingEdge(m_leftTriggerEdge, leftTrigger);
	const bool wasRight = m_pointRight ? f.right.valid : !f.left.valid;
	StepPointerHand(f, rightTrigger, leftTrigger);

	// The laser on the panel: the pointing hand's ray meets the panel's
	// quad, and the pixel it lands on goes back so the caller can put the
	// highlight on that row. While it lands, the pointing hand's trigger is
	// a click on that row rather than the stick's Right.
	const bool pointRight = m_pointRight ? f.right.valid : !f.left.valid;
	const HandPose* const pointHand = pointRight ? &f.right : &f.left;
	// The pull that moved the pointer to the other hand is spent on the
	// move: the row it lands on a frame later is not the row it was pulled
	// on, and the row under the old cursor is not what was meant either.
	const bool handMoved = pointRight != wasRight;
	if (f.settingsQuad.valid && pointHand->valid && f.settingsPixelsWidth > 0.0f &&
	    f.settingsPixelsHeight > 0.0f) {
		const NiPoint3 pointing =
			TrackingRotate(pointHand->orientation, NiPoint3{0.0f, 0.0f, -1.0f});
		const LaserHit hit = LaserOnQuad(pointHand->position, pointing, f.settingsQuad.centre,
		                                 f.settingsQuad.right, f.settingsQuad.up,
		                                 f.settingsQuad.width, f.settingsQuad.height,
		                                 f.settingsPixelsWidth, f.settingsPixelsHeight);
		if (hit.hit) {
			r.settingsPointerValid = true;
			r.settingsPointerX = hit.pixelX;
			r.settingsPointerY = hit.pixelY;
			// The panel's top and bottom bands - the title and the help line,
			// where no row is - scroll the list while the beam rests on them:
			// a notch, then repeats, the way the stick's wheel does.
			const bool onTop = hit.pixelY < f.settingsPixelsHeight * kPanelScrollBand;
			const bool onBottom = hit.pixelY > f.settingsPixelsHeight * (1.0f - kPanelScrollBand);
			if (StepRepeat(m_scrollUp, onTop, f.dtSeconds, s.scrollFirstDelaySeconds,
			               s.scrollIntervalSeconds)) {
				r.settingsNav.up = true;
			}
			if (StepRepeat(m_scrollDown, onBottom, f.dtSeconds, s.scrollFirstDelaySeconds,
			               s.scrollIntervalSeconds)) {
				r.settingsNav.down = true;
			}
			const NiPoint3 normal = Cross(f.settingsQuad.right, f.settingsQuad.up);
			const float along = Dot(pointing, normal);
			if (along < -0.0001f || along > 0.0001f) {
				const float t = Dot(f.settingsQuad.centre - pointHand->position, normal) / along;
				if (t > 0.0f) {
					r.laserLengthMetres = t;
				}
			}
		}
	}

	const bool pointingPull = pointRight ? rightPull : leftPull;
	if (r.settingsPointerValid && pointingPull && !handMoved) {
		r.settingsClick = true;
	}
	const bool accept =
		(!r.settingsPointerValid && (rightPull || leftPull) && !handMoved) ||
		StepRisingEdge(m_rightAEdge,
		               f.right.valid && ButtonADown(f.right.buttonsPressed)) ||
		StepRisingEdge(m_leftAEdge,
		               f.left.valid && ButtonADown(f.left.buttonsPressed));
	const bool back =
		StepRisingEdge(m_rightGripEdge,
		               f.right.valid && GripDown(f.right.buttonsPressed)) ||
		StepRisingEdge(m_leftGripEdge,
		               f.left.valid && GripDown(f.left.buttonsPressed));
	const bool close =
		StepRisingEdge(m_rightMenu, f.right.valid && ButtonBDown(f.right.buttonsPressed)) ||
		StepRisingEdge(m_leftMenu, f.left.valid && ButtonBDown(f.left.buttonsPressed));
	r.settingsNav.right = r.settingsNav.right || accept;
	r.settingsNav.left = r.settingsNav.left || back;
	r.settingsMenuToggle = r.settingsMenuToggle || close;

	// The beam stays on, from the pointing hand, as long as the way to the
	// panel when it meets it and a metre otherwise; nothing else reaches
	// the game.
	if (pointHand->valid) {
		r.laserVisible = true;
		r.laserRight = pointRight;
		if (!r.settingsPointerValid) {
			r.laserLengthMetres = 1.0f;
		}
	}
	r.controlsActive = f.right.valid || f.left.valid;
	m_poke = PokeState{};
	// The scroll bands keep their repeat state only while the beam rests on
	// the panel; off it, a fresh notch waits for the next visit.
	if (!r.settingsPointerValid) {
		m_scrollUp = RepeatState{};
		m_scrollDown = RepeatState{};
	}
}

void HandMode::PointAtMenu(const HandModeFrame& f, const HandSettings& s, HandModeResult& r) {
	// Where the menu is: on the menu wrist while a game is loaded and the
	// setting says so, otherwise on its big quad. The pointing hand is the
	// other one for a wrist, and for the big quad the right hand, or the
	// left when only that is tracked.
	const HandPose& menuHand = s.menuOnRight ? f.right : f.left;
	MenuQuad quad;
	const HandPose* pointHand = nullptr;
	bool pointRight = false;
	if (s.enabled && s.wristMenu && menuHand.valid && f.menuMode && f.inWorld) {
		r.menuOnWrist = true;
		r.menuWristRight = s.menuOnRight;
		r.menuTransform = WristOverlayTransform(s.wristUp, s.wristBack, s.wristTiltDegrees);
		const openvr::HmdMatrix34& t = r.menuTransform;
		const NiPoint3 localCentre{t.m[0][3], t.m[1][3], t.m[2][3]};
		const NiPoint3 localRight{t.m[0][0], t.m[1][0], t.m[2][0]};
		const NiPoint3 localUp{t.m[0][1], t.m[1][1], t.m[2][1]};
		quad.valid = f.layerPixelsWidth > 0.0f && f.layerPixelsHeight > 0.0f;
		quad.centre = menuHand.position + TrackingRotate(menuHand.orientation, localCentre);
		quad.right = TrackingRotate(menuHand.orientation, localRight);
		quad.up = TrackingRotate(menuHand.orientation, localUp);
		quad.width = s.wristMenuWidth;
		quad.height = quad.valid ? s.wristMenuWidth * (f.layerPixelsHeight / f.layerPixelsWidth)
		                         : 0.0f;
		pointHand = s.menuOnRight ? &f.left : &f.right;
		pointRight = !s.menuOnRight;
	} else if (f.menuMode) {
		// The big quad, or the cinema screen behind it when there is none:
		// the hand that last pulled its trigger points, the other one when
		// that hand is not tracked.
		quad = f.menuQuad;
		pointRight = m_pointRight ? f.right.valid : !f.left.valid;
		pointHand = pointRight ? &f.right : &f.left;
	}

	if (!f.menuMode) {
		m_poke = PokeState{};
		m_scrollUp = RepeatState{};
		m_scrollDown = RepeatState{};
		return;
	}

	// The beam from the pointing hand, as long as the way to the quad, or a
	// default length when the hand points past it.
	if (pointHand != nullptr && pointHand->valid) {
		r.laserVisible = true;
		r.laserRight = pointRight;
		r.laserLengthMetres = 1.5f;
	}

	// The cursor on the quad, in tracking space. The pointing hand's finger
	// tip pressing the quad comes first - it is the click, and while it
	// hovers the cursor sits under it; otherwise the hand's ray is a laser
	// and the cursor walks towards where it hits.
	if (quad.valid && pointHand != nullptr && pointHand->valid && f.cursorValid &&
	    f.layerPixelsWidth > 0.0f && f.layerPixelsHeight > 0.0f) {
		const NiPoint3 pointing =
			TrackingRotate(pointHand->orientation, NiPoint3{0.0f, 0.0f, -1.0f});
		const NiPoint3 tip = pointHand->position + pointing * s.pokeTipForward;
		const PokeSample sample = PokeOnQuad(tip, quad.centre, quad.right, quad.up, quad.width,
		                                     quad.height, f.layerPixelsWidth, f.layerPixelsHeight);
		const PokeVerdict poke = StepPoke(m_poke, sample, s.poke);
		if (poke.hover) {
			r.pokeHover = true;
			r.pokePress = poke.press;
			r.controls.menuClick = r.controls.menuClick || poke.held;
			// Straight under the tip, no easing: a finger on a button must not
			// find the cursor still on its way there.
			r.cursorDx = CursorStep(f.cursorX, sample.pixelX, 1.0f, 4096.0f);
			r.cursorDy = CursorStep(f.cursorY, sample.pixelY, 1.0f, 4096.0f);
			r.laserLengthMetres = s.pokeTipForward;
		} else {
			const LaserHit hit =
				LaserOnQuad(pointHand->position, pointing, quad.centre, quad.right, quad.up,
				            quad.width, quad.height, f.layerPixelsWidth, f.layerPixelsHeight);
			if (hit.hit) {
				r.laserHit = true;
				r.laserPixelX = hit.pixelX;
				r.laserPixelY = hit.pixelY;
				r.cursorDx = CursorStep(f.cursorX, hit.pixelX, s.laserGain, s.laserMaxStep);
				r.cursorDy = CursorStep(f.cursorY, hit.pixelY, s.laserGain, s.laserMaxStep);
				// The way to the quad along the ray: the plane's distance over
				// the ray's share of the normal.
				const NiPoint3 normal = Cross(quad.right, quad.up);
				const float along = Dot(pointing, normal);
				if (along < -0.0001f || along > 0.0001f) {
					const float t = Dot(quad.centre - pointHand->position, normal) / along;
					if (t > 0.0f) {
						r.laserLengthMetres = t;
					}
				}
			}
		}
	} else if (f.flat.valid && pointHand != nullptr && pointHand->valid && f.cursorValid &&
	           f.headValid) {
		// No quad: the frame is a flat one and the picture hangs at infinity.
		// The laser alone, no finger - there is nothing at arm's length to
		// press. The pixel is the one the head sees the beam's end against.
		m_poke = PokeState{};
		const NiPoint3 pointing =
			TrackingRotate(pointHand->orientation, NiPoint3{0.0f, 0.0f, -1.0f});
		const FlatLaserHit hit = LaserOnFlatPicture(pointHand->position, pointing, f.headPosition,
		                                            f.flat, kFlatLaserPlaneMetres);
		if (hit.hit) {
			r.laserHit = true;
			r.laserPixelX = hit.pixelX;
			r.laserPixelY = hit.pixelY;
			r.cursorDx = CursorStep(f.cursorX, hit.pixelX, s.laserGain, s.laserMaxStep);
			r.cursorDy = CursorStep(f.cursorY, hit.pixelY, s.laserGain, s.laserMaxStep);
			r.laserLengthMetres = hit.lengthMetres;
		}
	} else {
		m_poke = PokeState{};
	}

	// The left stick as the mouse wheel: a notch on the flick, then repeats
	// while it is held.
	const bool up = f.left.valid && f.left.thumbY >= s.stickDeadZone;
	const bool down = f.left.valid && f.left.thumbY <= -s.stickDeadZone;
	if (StepRepeat(m_scrollUp, up, f.dtSeconds, s.scrollFirstDelaySeconds, s.scrollIntervalSeconds)) {
		r.menuScroll = 1;
	}
	if (StepRepeat(m_scrollDown, down, f.dtSeconds, s.scrollFirstDelaySeconds,
	               s.scrollIntervalSeconds)) {
		r.menuScroll = -1;
	}
}

HandModeResult HandMode::UpdateMenusOnly(const HandModeFrame& f, const HandSettings& s) {
	// The mode off: no aim, no arms, no gestures, nothing pressed in the
	// world. The controllers reach only the menus - OBVR's own through the
	// sticks and buttons, the game's through the laser and the trigger.
	HandModeResult r;
	const StickChordVerdict sticks = StepChord(f, r);
	(void)sticks;
	if (f.settingsMenuOpen) {
		SteerSettingsMenu(f, s, r);
		return r;
	}
	m_navRight = StickNavState{};
	m_navLeft = StickNavState{};

	if (f.menuMode) {
		HandFrameInput in;
		in.rightValid = f.right.valid;
		in.leftValid = f.left.valid;
		in.rightTrigger = f.right.valid && StepTrigger(m_rightTrigger, f.right.trigger);
		in.leftTrigger = f.left.valid && StepTrigger(m_leftTrigger, f.left.trigger);
		in.rightMenuButton = StepRisingEdge(
			m_rightMenu,
			f.right.valid && ButtonBDown(f.right.buttonsPressed));
		in.leftMenuButton = StepRisingEdge(
			m_leftMenu,
			f.left.valid && ButtonBDown(f.left.buttonsPressed));
		in.menuMode = true;
		const bool wasRight = m_pointRight;
		StepPointerHand(f, in.rightTrigger, in.leftTrigger);
		if (m_pointRight != wasRight) {
			m_clickBlocked = true;
		}
		in.pointRight = m_pointRight;
		r.controls = PlanHandControls(in, s.stickDeadZone);
		if (m_clickBlocked) {
			if (!(m_pointRight ? in.rightTrigger : in.leftTrigger)) {
				m_clickBlocked = false;
			}
			r.controls.menuClick = false;
		}
		r.controlsActive = f.right.valid || f.left.valid;
	} else {
		// Kept stepped so a trigger held across the menu's closing does not
		// fire as it opens again.
		const bool rightTrigger = f.right.valid && StepTrigger(m_rightTrigger, f.right.trigger);
		const bool leftTrigger = f.left.valid && StepTrigger(m_leftTrigger, f.left.trigger);
		m_rightMenu = ButtonEdge{};
		m_leftMenu = ButtonEdge{};
		m_rightPointEdge = ButtonEdge{};
		m_leftPointEdge = ButtonEdge{};
		m_clickBlocked = false;

		// In the world, as a gamepad: the head aims, the controllers are the
		// pad. Nothing of the hand-tracked mode - no gestures, no arms.
		if (s.gamepadLayout) {
			GamepadInput in;
			in.rightValid = f.right.valid;
			in.leftValid = f.left.valid;
			in.rightTrigger = rightTrigger;
			in.leftTrigger = leftTrigger;
			in.rightGrip = f.right.valid && GripDown(f.right.buttonsPressed);
			in.leftGrip = f.left.valid && GripDown(f.left.buttonsPressed);
			in.rightA = f.right.valid && ButtonADown(f.right.buttonsPressed);
			in.leftA = f.left.valid && ButtonADown(f.left.buttonsPressed);
			in.rightB = StepRisingEdge(m_gpRightB, f.right.valid && ButtonBDown(f.right.buttonsPressed));
			in.leftB = StepRisingEdge(m_gpLeftB, f.left.valid && ButtonBDown(f.left.buttonsPressed));
			in.rightStickClick = sticks.rightClick;
			in.leftStickClick = sticks.leftClick;
			in.rightTrackpadClick = StepRisingEdge(
				m_gpRightTrackpad, f.right.valid && TrackpadClickDown(f.right.buttonsPressed));
			in.leftTrackpadClick = StepRisingEdge(
				m_gpLeftTrackpad, f.left.valid && TrackpadClickDown(f.left.buttonsPressed));
			in.leftThumbX = f.left.thumbX;
			in.leftThumbY = f.left.thumbY;
			in.rightThumbX = f.right.thumbX;
			r.controls = PlanGamepadControls(in, s.stickDeadZone);
			r.controlsActive = f.right.valid || f.left.valid;
		}
	}
	PointAtMenu(f, s, r);
	return r;
}

}  // namespace obvr::vr

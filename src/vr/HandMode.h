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
	float wristHudWidth = 0.35f;   // the HUD on the right wrist
	float wristMenuWidth = 0.70f;  // the Tab menu on the menu wrist
	bool wristHud = false;  // off: the 2026-09-25 headset run found the wrists awkward
	bool wristMenu = false;
	bool menuOnRight = true;  // the menu wrist; the other hand points and presses

	// The poke: how far the index finger's tip reaches past the controller's
	// origin, in metres along its pointing axis, and the distances that
	// count as hovering, pressing and releasing (see PokeThresholds).
	float pokeTipForward = 0.08f;
	PokeThresholds poke;

	// The body: keep the player in first person while the mode is on (the
	// hands are only drawn there), and hide the named parts of the
	// first-person model - the arms, whose animation the controllers do not
	// follow - by the comma-separated list of node names.
	bool forceFirstPerson = true;
	bool hideArms = true;
	// Hide the sheaths and what hangs in them (the side-weapon, back-weapon and
	// quiver bones, and any "Scb" scabbard): they rode with the right hand.
	bool hideSheaths = true;
	char hideNodes[128] = "Arms";

	// The laser cursor: how much of the remaining distance the game's cursor
	// is walked per frame, and the largest step.
	float laserGain = 0.5f;
	float laserMaxStep = 60.0f;

	// The sticks: dead zone, and mouse pixels per frame at full deflection
	// for the turn.
	float stickDeadZone = 0.4f;
	float turnSpeed = 12.0f;

	// The hand bones, written each frame to where the controllers are, so
	// the hands and the weapon stay with the controllers while the animation
	// swings the hidden arms. The names are the Bip01 skeleton's; the three
	// angles per hand are the calibration between controller and bone axes
	// (roll about the bone first, then pitch, then yaw - see BonePin.h).
	bool pinHands = true;
	char rightHandBone[64] = "Bip01 R Hand";
	char leftHandBone[64] = "Bip01 L Hand";
	float rightHandRoll = 0.0f;
	float rightHandPitch = 0.0f;
	float rightHandYaw = 90.0f;
	float leftHandRoll = 180.0f;  // the left bone's axes are the right's turned about x
	float leftHandPitch = 0.0f;
	float leftHandYaw = 90.0f;
	// Each hand's own grip, in metres along its controller's right, forward
	// and up axes, on top of the shared HandGripForwardMetres/UpMetres -
	// what adjusting the hands writes.
	float rightHandGripX = 0.0f;
	float rightHandGripY = 0.0f;
	float rightHandGripZ = 0.0f;
	float leftHandGripX = 0.0f;
	float leftHandGripY = 0.0f;
	float leftHandGripZ = 0.0f;
	// Adjusting the hands with the controllers: while on, closing a grip
	// holds that hand still in the world; move the controller to where the
	// hand should sit on it and open the grip - the fit is the new
	// calibration (angles and grip, saved to the INI). The grab is off
	// meanwhile.
	bool adjustHands = false;
	// A fit has been kept (or the defaults chosen) in the guided window: the
	// first-fit guide is not offered again. Written by the window.
	bool handsAdjusted = false;
	// Where the hand sits from the controller's tracked origin, in metres
	// along the controller's own up and forward axes. The origin is on the
	// tracking head; a hand placed there floated above the handle it holds
	// (2026-09-25). -0.04 is a first guess to tune in the headset.
	float handGripUpMetres = -0.04f;
	float handGripForwardMetres = 0.0f;
	// How close the hand has to be to an object for the grip to take it,
	// metres from the hand to the object's origin.
	float grabReachMetres = 0.3f;
	// Where a held object sits: this many metres from the palm along the
	// fingers; 0 is the palm, negative pulls it back towards the wrist.
	float heldObjectMetres = 0.0f;
	// Small things (potions, keys, ingredients ...) fixed in the palm and
	// turned with the wrist instead of hanging on the spring (game::HeldObject).
	bool attachSmallObjects = true;
	// How a grabbed object is held. Off (the default): in the hand - every
	// object sits fixed in the palm and turns with the wrist, like a sword or a
	// torch. On: levitated - the game's own spring pulls it towards the palm and
	// only small things (AttachSmallObjects) sit fixed. Either way, letting go
	// hands it back to Havok with the hand's speed.
	bool levitateObjects = false;
	// Letting go mid-swing throws with the palm's speed times this; 0 leaves
	// the engine's soft drop.
	float throwStrength = 0.6f;  // 1.0 threw "zu stark" (2026-09-26)
	// How far each finger link bends while the hand holds something, degrees
	// (game::HandGrip); negative bends the other way, 0 leaves the hand open.
	float gripCurlDegrees = 45.0f;
	// What a closed grip would take, while it is within ReachMarkerMetres of a
	// hand: the crosshair's tooltip icon moved onto it, large, and a light-brown
	// ring around it (render::ReachMarker). Each on its own switch.
	bool reachTooltip = true;
	bool reachRing = true;
	// How close a hand has to be for the ring to show, metres, and how
	// opaque it is (0 to 1).
	float reachMarkerMetres = 0.3f;
	float reachMarkerOpacity = 0.85f;

	// Strikes by motion: with a swung weapon in hand the swing itself is the
	// attack - no attack control, no animation - and the blade strikes the
	// bodies it passes through, heavy when the swing was fast enough. How
	// close it has to pass is this fraction of a body's bound radius plus
	// this many game units (see MeleeHit.h). Off, swings press the attack
	// control and the engine's animation decides the hit.
	bool motionHits = true;
	float hitBoundFactor = 0.7f;
	float hitPadUnits = 8.0f;

	// Menus with the controllers even while the mode is off: a laser from a
	// hand onto whichever quad shows the game's menus, the trigger clicks,
	// the sticks scroll, and OBVR's own menu takes the sticks and buttons.
	// The beam is the drawn laser. The scroll repeats while the stick is
	// held: the first repeat after firstDelay, then every interval. Off by
	// default until it has been seen to work in a headset: under
	// construction, like the rest of the mode.
	bool controllerMenus = false;
	bool laserBeam = true;
	// How the laser leaves the controller: turned down from its tracked
	// forward (-z) by the pitch, about the controller's own x; turned inwards
	// by the yaw - the right hand's to the left, the left hand's mirrored to
	// the right; and starting this far along the beam from the tracked
	// origin, negative back towards the controller. Tuned in the headset on
	// 2026-09-25: 40 degrees down, 5 in, 4 cm back (2 was still too far out).
	float laserPitchDegrees = 40.0f;
	float laserYawDegrees = 5.0f;
	float laserOriginMetres = -0.04f;
	// The laser's trigger as a finger on a touch screen: click on release,
	// drag to scroll - see StepLaserPress. Off, it clicks on the pull.
	bool laserDragScroll = true;
	// A dot at the end of the laser, wherever the laser is.
	bool laserDot = true;
	// Whether the right hand also aims: attacks, spells and the player's own
	// rotation follow it. Off, they follow the head - the NPCs looked at the
	// hand with it on (2026-09-25). The pick, the crosshair and the tooltip
	// follow the laser either way.
	bool aimWithHand = false;
	// Activate on the left A instead of the right, for a left-handed player.
	bool leftHanded = false;
	// Sneak only while the right stick is held down, instead of a flick down
	// switching it on and the next one off.
	bool sneakHold = false;
	// Run switched on and off by a click of the left stick, instead of held
	// while the stick is pressed in.
	bool runToggle = false;
	// With ControllerMenus on and the mode off, the controllers in the
	// WORLD as well, as a gamepad: see PlanGamepadControls for the layout.
	bool gamepadLayout = true;
	float scrollFirstDelaySeconds = 0.35f;
	float scrollIntervalSeconds = 0.12f;
};

// Everything one frame of the mode needs to know, gathered by the camera
// hook. Poses are as the backend reads them - OpenVR convention, seated
// space - and converted in here.
struct HandModeFrame {
	float dtSeconds = 0.0f;
	bool menuMode = false;
	bool settingsMenuOpen = false;  // OBVR's own menu: the sticks steer it, nothing else fires
	bool firstPerson = true;
	bool meleeInHand = false;  // a drawn blade, blunt weapon or bare fists: swung, not shot
	// The mode is off but the controllers still steer menus (ControllerMenus).
	bool menusOnly = false;
	// A game is loaded - the player stands in a cell. Before that (the main
	// menu, the intro) there is no wrist to hang a menu on, so it stays on
	// its big quad and the laser points at that.
	bool inWorld = true;
	// The player is sneaking, as the game has it. What hold-to-sneak compares
	// the stick with.
	bool sneaking = false;
	// The player's weapon, drawn or not, and the action the player's process
	// is in (HighProcess kAction_, -1 none): what the ready-weapon click
	// follows, and whether a swing may press attack.
	WeaponSeen weaponSeen = WeaponSeen::Unknown;
	SInt32 playerAction = -1;
	// The hands are being adjusted (the INI switch or the guided window): a
	// closed grip holds a hand, it does not grab.
	bool adjustingHands = false;
	// The quad the game's menus hang on when they are not on a wrist - on
	// the head or in the room - in tracking space, for the laser.
	MenuQuad menuQuad;
	// The cinema screen, when the frame is a flat one - the main menu, a
	// loading screen, a film - for the laser when there is no quad.
	FlatPicture flat;
	// OBVR's own panel - the settings menu or the walkthrough - while it is
	// open, in tracking space, with the pixels it is painted on: for the
	// laser to put its highlight on the row it points at.
	MenuQuad settingsQuad;
	float settingsPixelsWidth = 0.0f;
	float settingsPixelsHeight = 0.0f;
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
	// Whether the tile under the game's cursor is a scroll bar's: pulled there,
	// the laser holds the button at once so the marker can be dragged.
	bool cursorOnScrollBar = false;
	// The menu is one surface dragged with the button held - the map: a pull
	// holds the button at once, as on a scroll bar, so the map pans with the
	// beam in every direction. Before, an up-and-down drag turned into wheel
	// notches (the map's zoom) and only a sideways one held the button, so the
	// map moved only now and then (2026-09-26).
	bool menuIsDragSurface = false;
	float cursorX = 0.0f;
	float cursorY = 0.0f;
};

struct HandModeResult {
	// The aim, when the right hand is tracked: its heading as a turn from
	// the head's, and the sine of its pitch (positive up), both in the
	// game's convention. Used for attacks, spells, and grab direction when
	// the right grip holds it.
	bool aimValid = false;
	float aimYawTurn = 0.0f;
	float aimSinPitch = 0.0f;

	// Left hand aim: same structure as above, used for grab direction when
	// the left grip holds an object - allows each hand to independently
	// control grabbed objects through the shared Havok grab system.
	bool leftAimValid = false;
	float leftAimYawTurn = 0.0f;
	float leftAimSinPitch = 0.0f;

	// The arms: a rotation relative to the head and an offset in game
	// units, both in the game's convention, to apply in the render pass.
	bool armsValid = false;
	NiMatrix33 armsRotation{};
	NiPoint3 armsOffsetUnits{0.0f, 0.0f, 0.0f};

	// The hands themselves, for the bone pin: each controller's rotation
	// relative to the head and its offset from the eyes in game units, in
	// the game's convention, whenever it is tracked and the player is in
	// first person.
	bool rightHandValid = false;
	bool leftHandValid = false;
	NiMatrix33 rightHandRotation{};
	NiMatrix33 leftHandRotation{};
	NiPoint3 rightHandOffsetUnits{0.0f, 0.0f, 0.0f};
	NiPoint3 leftHandOffsetUnits{0.0f, 0.0f, 0.0f};

	// The controls to press, and whether any are to be pressed at all
	// (false releases everything).
	bool controlsActive = false;
	HandControlsWanted controls;

	// The wrists: device-to-overlay transforms for the HUD and the menu, and
	// which hand carries the menu.
	bool hudOnRightWrist = false;
	bool menuOnWrist = false;
	bool menuWristRight = true;
	openvr::HmdMatrix34 hudTransform{};
	openvr::HmdMatrix34 menuTransform{};

	// The cursor: the mouse step that walks it towards the laser's hit, or
	// puts it under the pointing finger's tip; and the poke's click.
	bool laserHit = false;
	bool pokeHover = false;
	bool pokePress = false;  // rising edge: one click
	int cursorDx = 0;
	int cursorDy = 0;

	// The drawn beam: from which hand, and how long - to where it meets the
	// quad, or a default length when it points past it.
	bool laserVisible = false;
	bool laserRight = true;
	float laserLengthMetres = 0.0f;
	// The pixel the laser's hit asked the cursor to go to, for the log.
	float laserPixelX = 0.0f;
	float laserPixelY = 0.0f;

	// The mouse wheel in a menu, in notches this frame: up positive.
	int menuScroll = 0;

	// OBVR's own menu: both sticks clicked together toggle it, and while it
	// is open the sticks are its arrow keys.
	bool settingsMenuToggle = false;
	StickNavVerdict settingsNav;
	// The laser on OBVR's own panel: the canvas pixel it points at, and
	// the pointing hand's trigger pulled on it - a click on that row, which
	// the caller turns into the row's own action. While the pointer is on
	// the panel the trigger is a click and not the stick's Right.
	bool settingsPointerValid = false;
	float settingsPointerX = 0.0f;
	float settingsPointerY = 0.0f;
	bool settingsClick = false;

	// The grab: whether either grip holds it, and how far the grabbing hand
	// is from the eyes in metres - the distance the held object is kept at.
	// Both grips share the same Havok grab (Z key); whichever grip is down
	// determines which hand's position drives the grabbed object.
	bool grabWanted = false;
	float grabDistanceMetres = 0.0f;
	bool grabWithLeftHand = false;  // true when left grip holds it, false for right
	// The hand the world pick runs from with no grip closed: the one an item
	// is near (set by the caller from game::FindNearestItem), else the right.
	bool pickWithLeftHand = false;
	// Each physical grip, whatever the handedness: adjusting the hands holds
	// the hand whose grip is closed.
	bool rightGripDown = false;
	bool leftGripDown = false;
	// The line from the head to the grabbing hand (ReachDirection): where the
	// held object is carried, so it moves with the hand and flies with it.
	bool grabDirectionValid = false;
	float grabYawTurn = 0.0f;
	float grabSinPitch = 0.0f;

	// The swing in progress, for the strikes by motion: whether the right
	// hand is swinging now, whether it has been fast enough for a heavy
	// attack so far, and which swing this is - counted up as each starts, so
	// a body is struck once per swing.
	bool swingActive = false;
	bool swingHeavy = false;
	UInt32 swingSerial = 0;
	bool strikeByMotion = false;  // this frame's swing strikes by motion rather than by control

	// The ready-weapon click's progress this frame.
	ReadyWeaponVerdict ready;

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
	// The mode off, the controllers on the menus alone.
	HandModeResult UpdateMenusOnly(const HandModeFrame& frame, const HandSettings& settings);
	// Both sticks clicked: OBVR's menu. Answers its verdict for the clicks.
	StickChordVerdict StepChord(const HandModeFrame& frame, HandModeResult& r);
	// The toggled controls held for kTapHoldSeconds rather than one frame.
	// The ready-weapon click followed until the game shows it (StepReadyWeapon).
	void HoldTaps(HandControlsWanted& controls, const HandModeFrame& frame, HandModeResult& r);
	ReadyWeaponState m_ready;
	bool m_runLatched = false;
	TapHoldState m_povHold;
	TapHoldState m_quickHold;
	SneakHoldState m_sneak;
	// OBVR's own menu open: the sticks and buttons steer it, nothing else.
	void SteerSettingsMenu(const HandModeFrame& frame, const HandSettings& settings,
	                       HandModeResult& r);
	// The hands on the game's menu: the wrist quad or the big one, the
	// finger and the laser, the cursor, the beam, the scroll.
	void PointAtMenu(const HandModeFrame& frame, const HandSettings& settings, HandModeResult& r);

	// Which hand holds the pointer on the game's menus: the last one whose
	// trigger was pulled. Starts on the right. The wrist menu is the
	// exception - there the hand without the menu points.
	void StepPointerHand(const HandModeFrame& frame, bool rightTrigger, bool leftTrigger);
	bool m_pointRight = true;
	bool m_clickBlocked = false;  // the pull that moved the pointer is not a click
	// The gamepad layout's tapped buttons, edge by edge.
	ButtonEdge m_gpRightB;
	ButtonEdge m_gpLeftB;
	ButtonEdge m_gpRightTrackpad;
	ButtonEdge m_gpLeftTrackpad;
	ButtonEdge m_rightPointEdge;
	ButtonEdge m_leftPointEdge;

	TriggerEdge m_rightTrigger;
	TriggerEdge m_leftTrigger;
	ButtonEdge m_rightTriggerEdge;
	ButtonEdge m_leftTriggerEdge;
	ButtonEdge m_rightGripEdge;
	ButtonEdge m_leftGripEdge;
	ButtonEdge m_rightAEdge;
	ButtonEdge m_leftAEdge;
	ButtonEdge m_rightMenu;
	ButtonEdge m_leftMenu;
	ButtonEdge m_leftTrackpad;
	StickFlickState m_rightFlick;
	LaserPressState m_press;
	RepeatState m_scrollUp;
	RepeatState m_scrollDown;
	StickChordState m_sticks;
	StickNavState m_navRight;
	StickNavState m_navLeft;
	PokeState m_poke;
	SwingDetector m_swing;
	HeldControl m_heavyHold;
	UInt32 m_swingSerial = 0;
	bool m_haveLastRight = false;
	bool m_reachArmed = false;  // the reach back seen since the last release
	bool m_reachSpent = false;  // a draw used the armed reach
	NiPoint3 m_lastRightRelative{0.0f, 0.0f, 0.0f};
};

}  // namespace obvr::vr

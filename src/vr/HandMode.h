#pragma once

#include "vr/WalkersShake.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "game/WeaponGrab.h"
#include "game/TorchRelease.h"
#include "game/WeaponRelease.h"
#include "game/BlockAngleLogic.h"
#include "game/EnemyHealthLogic.h"
#include "game/GrappleLogic.h"
#include "game/LeadLogic.h"
#include "game/ThrowLogic.h"
#include "game/ShoveLogic.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"
#include "vr/HandHud.h"
#include "vr/TwoHandLogic.h"
#include "vr/OneHandTwoHander.h"
#include "vr/Archery.h"
#include "vr/Fist.h"
#include "vr/GripHold.h"
#include "vr/Holster.h"
#include "vr/HolsterFit.h"
#include "vr/QuickMenu.h"
#include "vr/Stow.h"
#include "vr/Teleport.h"
#include "vr/WalkDirection.h"
#include "vr/WeaponWeight.h"
#include "vr/ReachOpen.h"
#include "vr/SneakIndicator.h"
#include "vr/SpellAim.h"
#include "vr/ArmSwing.h"

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

	// The body: hide the named parts of the first-person model - the arms,
	// whose animation the controllers do not follow - by the comma-separated
	// list of node names. (The mode no longer forces first person: the
	// engine undoes every switch of its own, measured 2026-10-20, and a
	// third person the player or a mod chose is theirs.)
	bool hideArms = true;
	// Hide the sheaths and what hangs in them (the side-weapon, back-weapon and
	// quiver bones, and any "Scb" scabbard): they rode with the right hand.
	bool hideSheaths = true;
	// Hide every shape of the worn cuirass in the first-person model but its
	// hands and arm meshes (game::HiddenUnderCuirass): a torso named other
	// than "UpperBody" hung before the eyes. With hideArms only.
	bool hideWornCuirass = true;
	char hideNodes[128] = "Arms";
	// Closed hands: the inside of the first-person model drawn as its own
	// surface, darkened to closedHandsBrightness, so a hand ending at the
	// wrist is not seen through (render/BackfacePass.h).
	bool closedHands = true;
	float closedHandsBrightness = 0.6f;
	// A bare hand's wrist drawn closed: its forearm shrunk into the cuff
	// (game/BonePin.h, "Bare wrists").
	bool closeBareWrists = true;

	// The laser cursor: how much of the remaining distance the game's cursor
	// is walked per frame, and the largest step - the way it is steered by
	// mouse steps when the cursor is not placed directly.
	float laserGain = 0.5f;
	float laserMaxStep = 60.0f;
	// The laser places the game's cursor itself, before the engine's tile
	// search reads it (game/MenuCursor.h), instead of steering it by mouse
	// steps ([Hands] LaserCursorDirect).
	bool laserCursorDirect = true;

	// The sticks: dead zone, and mouse pixels per frame at full deflection
	// for the turn.
	float stickDeadZone = 0.4f;
	// How long the right stick's button is held before the hold counts
	// (vr::StepStickHold): the hold is the Rest menu (ready weapon with the
	// holsters off), a shorter press the quick menu's click. The left stick's
	// hold stays the run's, its short click the spell ring's (spellRing).
	// Longer than the stick chord's window
	// (kStickChordWindowSeconds), so a chord that forms late is not a hold.
	float stickHoldSeconds = 0.4f;
	float turnSpeed = 12.0f;
	// Which way the left stick walks (vr/WalkDirection.h).
	WalkDirection walkDirection = kWalkDirectionDefault;
	// Where the crosshair quad with its context icon hangs (vr/HandHud.h,
	// CrosshairPlace): on the laser, over the thing pointed at, or ahead.
	CrosshairPlace crosshairPlace = kCrosshairPlaceDefault;
	// SteamVR's keyboard for the game's text fields (vr/VrKeyboard.h).
	bool vrKeyboard = true;
	// A save made in third person is put into first person once after it
	// loads, through the game's own switch, and never again until the next
	// load (camera/FrameLogic.h, StepFirstPersonAfterLoad).
	bool firstPersonAfterLoad = true;
	// A knockdown begun in first person ends in first person: once up, if
	// the engine has not switched back itself, the game's own switch
	// (camera/FrameLogic.h, StepFirstPersonAfterKnockdown).
	bool firstPersonAfterKnockdown = true;
	// A tick in the controller at every button hovered or clicked
	// (vr/MenuHaptics.h).
	bool menuHaptics = true;
	// The controllers while another window is in front (game/InputRoute.h):
	// on, they go straight into the game's input, so the player keeps
	// walking with the game in the background; off, they do nothing there.
	// Either way nothing reaches the window in front.
	bool backgroundInput = true;
	// Whether the laser picks items at all (game/NearbyItems.h,
	// g_laserPicksItems): off, items are the hands' alone.
	bool laserPicksItems = false;

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
	// How far a closed grip reaches for an item that then floats to the hand
	// (game::GripTakes), metres from the hand; beyond the grab's reach only
	// while the hand points at it. 0 is off.
	float pullReachMetres = 2.0f;
	// The laser drawn in the world as well as at menus, and how long it is
	// when the pick hits nothing (HandMode: the weapon hand's).
	bool laserInWorld = false;
	// What a closed grip would take, marked on itself: an outline (default),
	// a soft glow (game/TargetMarker.h).
	// Both off by default (the tester, 2026-10-07: things were marked
	// "although the setting is off" - the outline was the one on).
	bool targetOutline = false;
	bool targetGlow = false;
	float worldLaserMetres = 2.0f;
	// How fast a pulled or grabbed item floats into the hand, metres a second.
	float pullSpeedMetres = 4.0f;
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
	float throwStrength = 1.6f;
	// Held in the hand, an object's physics body is driven to where it is
	// shown, so it pushes what it meets (game/GrabPhysics.h).
	bool heldObjectsPush = true;
	// The weapon and the hands push the objects they meet (game/WorldPush.h).
	bool pushWorld = true;
	// The hands and the drawn melee weapon as keyframed Havok bodies
	// (game/HandBodies.h); PushWorld's rays then only cover what has no body.
	bool bodyCollision = true;
	// The physics step in Hz, 0 the game's own 60, below 0 the headset's own
	// display rate (game/HandBodyLogic.h, ResolvePhysicsRate).
	float physicsRate = 0.0f;
	// The player's own capsule radius, times the game's (game/PlayerCapsule.h):
	// smaller, the body pushes people and bumps into things less. 0.5 the
	// tester's (2026-09-29: "body radius geht nun gut ... als default").
	float bodyRadiusScale = 0.5f;
	// The hands and the weapon push people (character controllers); off, they
	// pass through the living (game::HandBodyPushesActors).
	bool pushPeople = false;
	// The shove: an open hand driven fast into someone, weapons away
	// (game/ShoveLogic.h).
	game::ShoveSettings shove;
	// The middle finger at an NPC (game/Insult.h): their disposition drops
	// by this much.
	bool middleFinger = true;
	float middleFingerDisposition = 30.0f;  // the tester, 2026-10-07: 30 by default
	// Taking someone by the hand (game/LeadLogic.h).
	game::LeadSettings lead;
	// Thrown things that hit people (game/ThrowLogic.h).
	game::ThrowHitSettings throwHit;
	// Holding a two-hander with both hands (vr/TwoHandLogic.h).
	TwoHandSettings twoHand;
	// The bow by hand: an arrow from the quiver, nocked, drawn, loosed
	// (vr/Archery.h).
	ArcherySettings archery;
	// The HUD on the hands and the compass in the sky ([HandHud], vr/HandHud.h).
	HandHudSettings handHud;
	// The enemy health bars over the fighters ([HandHud] EnemyHealthBars,
	// EnemyHealthMetres, EnemyHealthWidthMetres, EnemyHealthAboveMetres,
	// EnemyHealthShowSeconds; game/EnemyHealthLogic.h).
	game::EnemyHealthSettings enemyHealth;
	// How far each finger link bends while the hand holds something, degrees
	// (game::HandGrip); negative bends the other way, 0 leaves the hand open.
	float gripCurlDegrees = 45.0f;
	// Each finger of an empty hand follows the controller's finger (Index,
	// Touch: SteamVR's finger curls), from the game's open hand to its fist
	// (game::FingerPoseFor). A hand holding a weapon or a torch keeps the
	// animation's grip.
	bool fingerTracking = true;
	// What a closed grip would take, while it is within ReachMarkerMetres of a
	// hand: the crosshair's tooltip icon moved onto it, large, and a light-brown
	// ring around it (render::ReachMarker). Each on its own switch.
	bool reachTooltip = true;
	bool reachRing = true;
	// How much larger the crosshair and its tooltip icon are when they hang
	// ahead of the controller, and the icon on a reachable item, than the
	// seated crosshair ([Hands] TooltipScale; the tester, 2026-09-27: "die
	// tooltips am controller crosshair sind zu klein"). 1 is the seated size.
	float tooltipScale = 2.0f;
	// How close a hand has to be for the ring to show, metres, and how
	// opaque it is (0 to 1). Beyond GrabReachMetres an item counts only while
	// the hand's laser points roughly at it (game::ReachingFor), so an item
	// on a table does not take the pick from a door being pointed at.
	float reachMarkerMetres = 2.0f;
	// From how near the hand the ring sits all the way on the item's side
	// nearest it, metres; from the marker's distance it moves there evenly
	// with the hand: very near, very near the edge.
	float reachNearSideMetres = 0.05f;
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
	// The hit by touch ([Hands] MotionHitsByTouch): the blade or the fist has
	// to come within a few units of one of the actor's capsules (the
	// contact steps' living) rather than the bound's ball - the ball read
	// hits a metre off a big fighter (the tester, 2026-10-15). With no
	// capsules collected (the contact settings off) the ball as before.
	bool hitsByTouch = true;
	// A body is struck the moment the blade or the fist meets it ([Hands]
	// PowerStrikeSpeed, m/s; game::StrikeAtContact): a power attack when the
	// swing is one already or the hand meets it this fast, light otherwise.
	// 0: only the swing's length makes power.
	float powerStrikeSpeed = 4.5f;

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
	// And beside the controller: metres to the right and up in its own frame,
	// the right mirrored on the left hand (vr::LaserOffsetLocal).
	float laserOffsetRightMetres = -0.01f;
	float laserOffsetUpMetres = -0.02f;
	// The laser's trigger as a finger on a touch screen: click on release,
	// drag to scroll - see StepLaserPress. Off, it clicks on the pull.
	bool laserDragScroll = true;
	// On a menu of buttons alone (the main menu, a message box) the button
	// under the beam sticks: the cursor keeps the last pixel that lay on a
	// tile while the beam wobbles less than this many pixels of the picture
	// from it (vr::StickyCursor; the main menu's row is 95 pixels tall of
	// 1444 and the beam moves 27 a degree, 2026-10-21). 0: off.
	float laserStickyPixels = 60.0f;
	// A dot at the end of the laser, wherever the laser is.
	bool laserDot = true;
	// Whether the right hand also aims: attacks, spells and the player's own
	// rotation follow it. Off, they follow the head - the NPCs looked at the
	// hand with it on (2026-09-25). The pick, the crosshair and the tooltip
	// follow the laser either way.
	bool aimWithHand = false;
	// A spell from the casting hand (vr/SpellAim.h): with a target spell
	// ready, the crosshair on that hand's ray while it is raised, the spell
	// along that ray and the projectile from its palm. Ahead of AimWithHand
	// for a spell; the bow and the grab keep their own aims.
	SpellAimSettings spellAim;
	// Sneaking in Full VR (vr/SneakIndicator.h): the vignette in the eye's gold
	// by default, or the eye above the view; never the crosshair.
	SneakIndicator sneakIndicator = kSneakIndicatorDefault;
	// The vignette's strength when seen (the eye at 255); unseen it is the
	// eye's share of that.
	float sneakVignetteStrength = 0.8f;
	// The eye's size above the view, times a crosshair at its distance (Nadi,
	// 2026-10-28: "das sneak icon muss noch wesentlich größer sein"; 1.5 at
	// first, 5.6 cm wide at 1.5 m).
	float sneakEyeSize = 4.5f;
	// Swaps the hands for a left-handed player (vr::AssignHandRoles): the left
	// controller becomes the weapon hand.
	bool leftHanded = false;
	// Sneak only while the right stick is held down, instead of a flick down
	// switching it on and the next one off.
	bool sneakHold = false;
	// Crouching in the room sneaks, standing up ends it (HandInput.h,
	// StepCrouchSneak); the head this far under its standing height is a
	// crouch.
	bool crouchSneak = true;
	float crouchDropMetres = 0.30f;
	// The game's sneak no longer lowers the eye on a real crouch: the
	// engine's eye is held at its standing height above the feet while a
	// crouch in the room sneaks, the real head gives the height; the stick's
	// sneak while standing keeps the game's drop (since 2026-10-26;
	// camera/SneakEyeHold.h).
	bool keepEyeHeightSneaking = true;
	// The weapon's weight as a lag of the drawn hand behind the controller
	// ([Hands] WeaponWeight, 1-100 %, vr/WeaponWeight.h): 1 is on the
	// controller, 100 the heaviest feel. With the swing-through the weapon
	// has momentum: it overruns a hand that stops and swings back ([Hands]
	// WeaponSwingThrough); off, it only trails.
	float weaponWeightPercent = 40.0f;
	bool weaponSwingThrough = true;
	// A two-hander held in one hand (vr/OneHandTwoHander.h): heavier, every
	// fatigue cost dearer, and holding it up over the hip burns fatigue
	// ([Hands] OneHandTwoHanderWeight, OneHandTwoHanderFatigue,
	// OneHandTwoHanderHoldCosts, OneHandTwoHanderHoldFatigue,
	// HipBelowEyesMetres).
	OneHandTwoHanderSettings oneHandTwoHander;
	// The drawn weapon stops at walls, floors and furniture and slides along
	// them; pulled further than this from the hand, or turned further, it lets
	// go and passes through, and strikes nothing until it is free ([Hands]
	// WeaponStopsAtWalls, WeaponLetGoMetres, WeaponLetGoDegrees;
	// game/BladeContactLogic.h).
	bool weaponStopsAtWalls = true;
	float weaponLetGoMetres = 0.60f;  // the tester, 2026-10-10: 0.30 "ist generell zu früh"
	float weaponLetGoDegrees = 45.0f;
	// The cap at people, of its own: how far the hand may go past where a
	// person holds the weapon or the hand before it goes into them ([Hands]
	// BodyLetGoMetres); 0 holds for good, as Blade & Sorcery's "block" does -
	// the hand comes back, the blade comes free (the tester, 2026-10-15:
	// "bau die eigene grenze für personen, default 0 wie B&S block").
	float bodyLetGoMetres = 0.0f;
	// The turn cap at people, the slide's ([Hands] BodyLetGoDegrees, with
	// BladeSlidesOffPeople): a blade a person has turned further off the
	// hand's turn than this slices out of them, as a Blade & Sorcery slash
	// slides out. 0 none. 45 is a first guess, the same as the weapon's cap at
	// walls (WeaponLetGoDegrees) - the harness slash of 2026-10-26 lay up to 63
	// degrees off the hand for 1.5 s without it; the headset tunes it.
	float bodyLetGoDegrees = 45.0f;
	// The living as well: held there slowly the blade rests on a person, a
	// swing goes into them and through ([Hands] WeaponStopsAtBodies); and the
	// hit-stop, the blade slowed for a few frames once a swing goes in
	// ([Hands] WeaponHitStop).
	bool weaponStopsAtBodies = true;
	bool weaponHitStop = true;
	// The free hand and the worn shield the same way: the hand the weapon is
	// not drawn in - both with nothing drawn - stops at walls and rests on
	// people, a fast hand goes through; the shield on the off hand's arm is
	// met as its disc ([Hands] HandsStopAtWalls, HandsStopAtBodies;
	// game/HandContactLogic.h; the tester, 2026-10-11). The weapon's let-go
	// caps are theirs too.
	bool handsStopAtWalls = true;
	bool handsStopAtBodies = true;
	// The living hold a swing and a fast hand as they hold a slow one (the
	// tester, 2026-10-15: "mach es wie in blade and sorcery" - there the
	// body's colliders stop the blade and the fist; weapon-collision-spec
	// section 17): a swing is held at the person it meets and only pressed
	// on past the let-go cap goes in; a punch, a slap or any hand at the
	// shove's speed is held at them while its blow still lands ([Hands]
	// SwingsStopAtBodies, PunchesStopAtBodies; game/BladeContactLogic.h,
	// BladeLiving::hold). Off: a swing or a fast hand goes through them and
	// they are passed, the hit-stop slowing the swing.
	bool swingsStopAtBodies = true;
	bool punchesStopAtBodies = true;
	// A person holding the blade lets the grip go on with the hand and turns
	// the blade off them, on the side it met them, until it comes off at its
	// tip - Blade & Sorcery's blade sliding while it cuts (Nadi, 2026-10-26:
	// "Schwert bleibt oft zu sehr an npcs hängen fast schon kleben. Soll eher
	// wie b&s sein"; [Hands] BladeSlidesOffPeople, game::SlideOffPeople). Off,
	// the whole pose - the grip too - stays where it met them until the hand
	// comes back; a thrust straight at them is held so either way.
	bool bladeSlidesOffPeople = true;
	// The capsules the blade, the fist and the stoppers meet on the living
	// fitted to the meshes each actor wears - body, face, armour, clothing -
	// instead of the guessed radii (docs/mesh-fitted-capsules-spec.md,
	// game/FittedCapsules.h): a steel upper arm 7.6 units round where the
	// guess says 4.5. On since the tester's headset run on Windows of
	// 2026-10-25 ("FittedBodyCapsules=1 standard").
	bool fittedBodyCapsules = true;
	// [Hands] FittedWeaponShapes: the drawn weapon's line, its blade's width
	// and thickness and a mace's or an axe's head read from its model
	// (game/WeaponShapeLogic.h), and a person held the steel's reach short
	// instead of the contact margin (Nadi, 2026-10-27: the blade resting
	// 1.5 units off everyone). Walls keep the margin.
	bool fittedWeaponShapes = true;
	// [Hands] FitLengthSplitRatio: a fitted body group that narrows along its
	// length becomes two capsules when one half is this many times as round
	// as the other (game/CapsuleFitLogic.h, SplitAlongLength); 0 or below
	// never splits along the length.
	float fitLengthSplitRatio = 1.25f;
	// [Hands] FittedHands: an empty hand's length and its palm's width and
	// thickness read from the first-person hand's model as it is drawn, the
	// fingers as they are bent, instead of a palm-long finger and one radius
	// of 2.5; a person held the hand's reach toward them short. Walls keep
	// the radius.
	bool fittedHands = true;
	// [Debug] CapsuleTrace: a "Capsules:" line for every actor fitted, not
	// only the first eight.
	bool capsuleTrace = false;
	// [Debug] MeshGapTrace: this many "Contact: mesh gap" lines - while the
	// blade or a hand rests on someone, how far it lies over the mesh they
	// wear there or sunk into it (game/MeshGapLogic.h). 0 off: each line
	// reads and skins their shapes on the game's thread.
	UInt32 meshGapTraceLines = 0;
	// [Debug] MenuNearShapes: as a menu opens, every shown shape of the
	// player's within this many units of the eyes is logged ("Menu near
	// shapes:"), once a menu, the first 20 menus a session; 0 off. On by
	// default: an opening's walk, no frame's (game/NearShapesLogic.h).
	float menuNearShapesUnits = 40.0f;
	// [Debug] ContactTrace: this many log lines of the weapon's and the hands'
	// contact step, frame by frame, while people are near and the shape
	// swings, is held or goes into someone - where the tip went, what held
	// it, who was passed. 0 off.
	UInt32 contactTraceLines = 0;
	// [Debug] GripTrace: a line per hand with the grip's pull, force and
	// click - at its first tracked frame, every half second, and when the
	// click changes (vr/GripTrace.h) - to measure where a realistic hold
	// lies. Off for play.
	bool gripTrace = false;
	bool weaponNodeTrace = false;  // [Debug] WeaponNodeTrace: what hangs on each hand, at every change
	// The hands stay in the game's menus - pinned and drawn over the
	// inventory, a container, the map, a book, a dialogue - and go only for
	// the Esc menu and what opens from it ([Hands] HandsInMenus;
	// camera/FrameLogic.h, HandsAwayForMenus; the tester, 2026-10-11). Off:
	// every menu takes them, as before.
	bool handsInMenus = true;
	// The parry: the drawn blade - or the torch held in the off hand, with a
	// weapon drawn or without (game/TorchShaftLogic.h) - meeting an
	// attacker's during their attack blocks their blow by the engine's own
	// path, all of it with ParryStopsAll, for ParryFatigue fatigue ([Hands]
	// WeaponParries, ParryStopsAll, ParryFatigue; game/ParryLogic.h).
	bool weaponParries = true;
	bool parryStopsAll = true;
	float parryFatigue = 10.0f;
	// The block's angle: a raised weapon blocks only a blade crossing it near
	// a right angle (within blockBladeDegrees of it), a shield only a blow its
	// face is turned to (within blockShieldDegrees), and either only a blow
	// that comes within blockReachUnits of it at the blow (0 asks no
	// distance), so a guard held high does not block a cut at the belly; the
	// weapon guard is then any raised blade not pointing ahead ([Hands]
	// BlockByAngle, BlockBladeDegrees, BlockShieldDegrees, BlockReachUnits;
	// game/BlockAngleLogic.h).
	bool blockByAngle = true;
	float blockBladeDegrees = 30.0f;
	float blockBodyDegrees = 70.0f;      // [Hands] BlockBodyDegrees (game/BlockAngleLogic.h)
	float blockInterposeUnits = 25.0f;   // [Hands] BlockInterposeUnits
	float blockShieldDegrees = 70.0f;
	float blockReachUnits = 40.0f;
	// How close fighters come before they strike, game units: the game setting
	// fCombatDistance held at this while Full VR runs, so a blow is struck
	// within reach of the player's blade ([Hands] CombatReach, 0 the game's own
	// 128; game/CombatReach.h).
	float combatReach = 85.0f;
	// Fighters run to the player as in vanilla, and walk once within this many
	// metres - a slower fight, as in Blade & Sorcery ([Hands] SlowApproach,
	// SlowApproachMetres; game/SlowApproach.h).
	bool slowApproach = true;
	float slowApproachMetres = 5.0f;  // the tester, 2026-10-10: "muss per default auf 5m"
	// The fighters' ring: those fighting the player wait this far off and walk
	// round, and only so many have a turn to fight at once ([Hands]
	// CombatRing, CombatRingMetres 0 off to 10, MeleeAttackersAtOnce 1-5;
	// game/CombatRingLogic.h; the tester, 2026-10-10).
	bool combatRing = true;
	float combatRingMetres = 5.0f;
	int meleeAttackersAtOnce = 1;
	float combatRingTurnSeconds = 6.0f;  // how long a turn lasts at least, 2-30 s
	// And at most: each turn's length is drawn between the two (the tester,
	// 2026-10-11: "random(5s, 15s)"); and the order of turns by level - the
	// weakest first, the strongest last (CombatRingTurnMaxSeconds,
	// CombatRingByLevel; game/CombatRingLogic.h).
	float combatRingTurnMaxSeconds = 12.0f;
	bool combatRingByLevel = true;
	// The waiting ones do not strike: their attacks are not begun while they
	// have no turn (Nadi, 2026-10-28; [Hands] CombatRingHoldsBlows,
	// game/AttackTempo.cpp, the attack's start 0x00612BD0).
	bool combatRingHoldsBlows = true;
	// The item highlighted in the inventory taken into the hand whose hold
	// closes (Nadi, 2026-10-28; [Hands] GrabFromInventory,
	// game/InventoryGrab.h, docs/holding-objects-spec.md).
	bool grabFromInventory = true;
	// Oblivion Walkers' grab shaken off by the held hand, shown on the body
	// (vr/WalkersShake.h; Nadi, 2026-10-30; [Hands] WalkersShake,
	// ShakeStrokeMetres, ShakeStrokeSpeed); [Debug] TestWalkersHold pretends
	// a holder at that arm for the harness.
	bool walkersShake = true;
	ShakeSettings shake;
	UInt32 testWalkersHold = 0;
	// Those left out of the ring and its turns, and out of the attack tempo:
	// comma-separated, matched in the actor's name or its race's name
	// ([Hands] CombatRingExceptions, AttackTempoExceptions;
	// game/ActorExceptionsLogic.h; Nadi, 2026-10-26: Zombie, Walker).
	char combatRingExceptions[256] = "";
	char attackTempoExceptions[256] = "";
	// Strikes by motion train the weapon's skill as the game's attacks do
	// (game/SkillCreditLogic.h); off, they train none, as before.
	bool strikesTrainSkills = true;
	// Enemy attacks slower and rarer: the attack animations of those fighting
	// the player play at EnemyAttackSpeed per cent of the game's own (20-200),
	// and EnemyAttackFrequency per cent (10-200) scales their chance to
	// attack, with the hold between their melee decisions scaled the other
	// way ([Hands] EnemyAttackSpeed, EnemyAttackFrequency; game/AttackTempo.h;
	// the tester, 2026-10-10). 100 is the game's own.
	float enemyAttackSpeed = 75.0f;
	float enemyAttackFrequency = 100.0f;
	// Your hits and the game's knockback: every landed hit shoves the one hit
	// through their Havok proxy ([Hands] EnemyKnockback, per cent of the
	// game's own push - 0 none, they stay where they stand as in Blade &
	// Sorcery; EnemyKnockbackPowerOnly, light hits pushing nobody;
	// game/HitKnockbackLogic.h; the tester, 2026-10-15: "gegner wegschieben
	// mit schlägen ... wäre ein fehler", "bau mal alle vier optionen").
	float enemyKnockbackPercent = 0.0f;
	bool enemyKnockbackPowerOnly = false;
	// The off hand in a fight: it strikes with a weapon drawn, and its grip
	// holds a fighter ([Hands] OffHandStrikes, OffHandGrabs, GrabHoldSeconds,
	// GrabFatigue; game/GrappleLogic.h).
	game::GrappleSettings grapple;
	// Opening by reaching: an open, empty hand at a container, a body or -
	// sneaking - a person opens it, the hand away closes it ([Hands]
	// ReachOpens, ReachOpenMetres, ReachCloseMetres; vr/ReachOpen.h).
	ReachOpenSettings reachOpen;
	// Run switched on and off by a click of the left stick, instead of held
	// while the stick is pressed in.
	bool runToggle = false;
	// Handing a thing to the other hand (game/HandoverLogic.h): the other
	// hand's hold within this many metres of it when the wearing hand lets go.
	// 0 off. A first guess. [Hands] HandoverReachMetres.
	float handoverReachMetres = 0.12f;
	// A bow, a shield or a torch, worn at the other hand, taken as a ball this
	// many metres round that hand for the hand-over's reach (a first guess).
	// [Hands] HandoverThingMetres.
	float handoverThingMetres = 0.15f;
	// Running by swinging both arms instead of the left stick held ([Hands]
	// ArmSwingRun, ArmSwingSpeed, ArmSwingHoldSeconds; vr/ArmSwing.h).
	ArmSwingSettings armSwing;
	// With ControllerMenus on and the mode off, the controllers in the
	// WORLD as well, as a gamepad: see PlanGamepadControls for the layout.
	bool gamepadLayout = true;
	float scrollFirstDelaySeconds = 0.35f;
	float scrollIntervalSeconds = 0.12f;
	// The quick menu on the right trackpad ([Hands] QuickMenu, vr::QuickMenu).
	QuickMenuSettings quickMenu;
	// The spell ring on the other hand (vr/SpellRing.h; docs/controls-spec.md
	// 4.44): the other stick's short click opens the player's spellbook on
	// the quick menu's ring, the next click makes the chosen spell the active
	// one, the other trigger turns its pages. The stick held past
	// stickHoldSeconds stays the run's.
	bool spellRing = true;

	// Drawing by reaching: the sword from the left hip, the bow from the left
	// shoulder ([Hands] Holsters, vr::Holster).
	HolsterSettings holster;

	// The holsters' grip is the hold: the fingers closed round the grip by
	// its pull, with a hysteresis ([Hands] GripByPull, GripCloseAt,
	// GripOpenAt; vr/GripHold.h) - no squeeze to the binding's click needed.
	// The click stays where no pull reads.
	GripHoldSettings gripHold;

	// A weapon in the world taken at its grip ([Hands] GrabEquipsWeapons,
	// WeaponGripReachMetres; game/WeaponGrab.h).
	game::WeaponGrabSettings weaponGrab;

	// The drawn weapon held while the hold is closed, let go of when it opens
	// ([Hands] WeaponHold, WeaponReleaseSeconds, WeaponThrowStrength;
	// game/WeaponRelease.h).
	game::WeaponReleaseSettings weaponRelease;

	// The other hand's torch let go of, dropped still burning ([Hands]
	// TorchDrop; WeaponReleaseSeconds and WeaponThrowStrength as the
	// weapon's; game/TorchRelease.h, game/TorchFire.h).
	game::TorchReleaseSettings torchRelease;
	// The torch kept in the other hand with a two-handed blade or blunt
	// drawn ([Hands] TorchWithTwoHander; game/TorchKeepLogic.h).
	bool torchWithTwoHander = true;
	// The torch's blow knocks down only with a power swing, a stagger
	// otherwise ([Hands] TorchKnockdownPowerOnly; game/TorchBlowLogic.h).
	bool torchKnockdownPowerOnly = true;

	// Fists by making a fist ([Hands] Fists, vr::Fist).
	FistSettings fist;

	// How much faster the draw and sheathe animations' time runs ([Hands]
	// WeaponDrawSpeed, game::StepWeaponDrawSpeed); 1 is the game's own speed.
	float weaponDrawSpeed = 10.0f;

	// Stowing a held item at the body, and taking loose items only by hand
	// ([Hands] StowAtBody, TakeOnlyByHand, vr::Stow).
	StowSettings stow;

	// The teleport on the right stick pushed forward ([Locomotion], vr::Teleport).
	TeleportSettings teleport;
};

// Where the weapon hand rests from the head when the arms' root follows the
// hands (RestHandRight/Forward/Up, in metres, the game's axes). With the
// first-person skeleton drawn mirrored (a left-hander's roles swapped,
// game/BonePin.h "The mirror") the rest is the mirror image: the weapon hand
// is the left one, at -x. Before that the root slid 40 cm sideways for a
// left-hander.
inline NiPoint3 ArmsRestOffset(const HandSettings& s, bool mirrored) {
	return NiPoint3{mirrored ? -s.restHandRight : s.restHandRight, s.restHandForward, s.restHandUp};
}

// Everything one frame of the mode needs to know, gathered by the camera
// hook. Poses are as the backend reads them - OpenVR convention, seated
// space - and converted in here.
struct HandModeFrame {
	float dtSeconds = 0.0f;
	// The hand a thing OBVR keeps in a hand's grab is held by (an item from
	// the inventory, a hand-over; game/HandoverLogic.h): 0 the right, 1 the
	// left, -1 none - the grab's line goes from that hand whatever grip
	// buttons are down (else the right grip down won, and a thing passed to
	// the left hand stayed on the right hand's line, handover-item.txt
	// 2026-10-28).
	int grabHand = -1;
	bool menuMode = false;
	// The Rest menu (SleepWait) is on top: the right B closes it with the
	// Rest key, as T does, not with Escape, which opened the pause menu over
	// it (2026-09-27).
	bool restMenuUp = false;
	// A container's menu opened by reaching is on top (vr/ReachOpen.h): the
	// left stick walks and the right one turns under it, as in the world.
	bool walkUnderMenu = false;
	bool settingsMenuOpen = false;  // OBVR's own menu: the sticks steer it, nothing else fires
	bool firstPerson = true;
	bool meleeInHand = false;  // a drawn blade, blunt weapon or bare fists: swung, not shot
	bool meleeHeld = false;    // the same, whether or not the hits by motion are on: the trigger never attacks
	// A click of the ready-weapon control from outside the hand mode this
	// frame, followed by StepReadyWeapon like the stick's: the yield raising
	// the fists for the engine's block (vr/Yield.h), and lowering them after.
	bool readyWeaponClick = false;
	// A shield on the left arm: only then does the raised left hand block.
	bool shieldEquipped = false;
	// The mode is off but the controllers still steer menus (ControllerMenus).
	bool menusOnly = false;
	// The roles were swapped this frame (vr::AssignHandRoles, left-handed in
	// Full VR): what is about a controller's own shape goes by the physical
	// side (vr::PhysicalLeftOf), and the arms draw mirrored (ArmsRestOffset).
	bool rolesSwapped = false;
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
	// The weapon slot, and the place on the body that keeps it (the holster
	// gestures, vr::StepHolster).
	EquippedKind equipped = EquippedKind::Nothing;
	HolsterSlot equippedSlot = HolsterSlot::None;
	// The other hand's shield or torch, worn, and what the four places keep -
	// and whether that is worn now (the equipped weapon, a shield or torch
	// equipped): the holster's memory, game/HolsterSlots.h.
	OffHandItem offHand = OffHandItem::Nothing;
	SlotItem slotItem[kHolsterSlotCount] = {};
	bool slotWorn[kHolsterSlotCount] = {};
	bool torchBesideTwoHander = false;  // HolsterInput's
	// The drawn weapon's weight, the item's own (game::WeaponWeightOf), while
	// a swung weapon - a blade or a blunt one - is drawn in the hand; 0 with
	// the fists, a bow, a staff, or nothing drawn. The weapon's lag is set by
	// it (vr/WeaponWeight.h).
	float weaponWeight = 0.0f;
	// That weapon is a two-handed blade or blunt weapon - not a staff
	// (vr::IsSwungTwoHanderType) - and the other hand holds its handle
	// (vr::TwoHandHolds): without the other hand it is heavier
	// (vr/OneHandTwoHander.h), with it stiffer (kWeaponLagTwoHandFactor).
	bool twoHanderSwung = false;
	bool bothHandsOnHandle = false;
	// Exhausted (game/ExhaustionLogic.h): how far in, 0 to 1, and the
	// weapon's lag factor at full - the lag is made heavier by them
	// (vr::WeaponLagExhausted).
	float exhaustionEase = 0.0f;
	float exhaustionFactor = 1.0f;
	// The player holds an object (the grab): a fist round it is not a fist.
	bool holdingObject = false;
	// The left grip holds a weapon's handle, or is about to (vr/TwoHandLogic.h):
	// it is the weapon's, not the grab's - an object it brushed stuck to it
	// (the tester, 2026-09-30: "objekte die ich rum haue an der linken hand
	// kleben bleiben").
	bool leftGripOnHandle = false;
	// The left hand holds a weapon's handle: its trigger slides it along the
	// handle, as in Blade & Sorcery, and casts nothing.
	bool leftTriggerOnHandle = false;
	// Where the drawn bow shoots, in the bow hand's controller frame (x right,
	// y up, -z forward), as the last frame showed the bow (game::BowShotAxis):
	// the arrow's aim. Without it, that hand's laser.
	// Arrows in the quiver, while a bow is drawn (game::QuiverHasArrows): with
	// none no arrow is taken.
	bool haveArrows = true;
	bool bowShotValid = false;
	NiPoint3 bowShotLocal{0.0f, 0.0f, -1.0f};
	// The bow's limbs (its model's y), in the bow hand's controller frame:
	// upright, the bow blocks (vr::IsBowGuard).
	bool bowLimbValid = false;
	NiPoint3 bowLimbLocal{0.0f, 1.0f, 0.0f};
	SInt32 playerAction = -1;
	// The hands are being adjusted (the INI switch or the guided window): a
	// closed grip holds a hand, it does not grab.
	bool adjustingHands = false;
	// The quad the game's menus hang on when they are not on a wrist - on
	// the head or in the room - in tracking space, for the laser.
	MenuQuad menuQuad;
	// The cinema screen, when the frame is a flat one - the main menu, a
	// loading screen, a film - for the laser when there is no quad; and the
	// depth it is shown at ([Render] FlatDepthMetres), where the laser's
	// stand-in plane goes.
	FlatPicture flat;
	float flatDepthMetres = 2.0f;
	// Whether the menu on top takes the trigger as a finger on a touch
	// screen (lists, sliders) or as a button (game::MenuTakesTouchPress).
	bool menuTakesTouchPress = true;
	// OBVR's own panel - the settings menu or the walkthrough - while it is
	// open, in tracking space, with the pixels it is painted on: for the
	// laser to put its highlight on the row it points at.
	MenuQuad settingsQuad;
	float settingsPixelsWidth = 0.0f;
	float settingsPixelsHeight = 0.0f;
	bool headValid = false;
	Quaternion head = Quaternion::Identity();
	NiPoint3 headPosition{0.0f, 0.0f, 0.0f};
	// The head tracker's recenter reference (yaw only): what the walk
	// direction measures each hand's heading from (vr/WalkDirection.h).
	Quaternion reference = Quaternion::Identity();
	// Counts the recenters: a change means one happened since the last
	// frame (the crouch's standing height is taken anew, HandInput.h).
	UInt32 recenterSerial = 0;
	HandPose right;
	HandPose left;
	float unitsPerMetre = 70.0f;
	// For the laser: the pixels the left wrist's quad shows, and where the
	// game's cursor currently is in those pixels.
	float layerPixelsWidth = 0.0f;
	float layerPixelsHeight = 0.0f;
	bool cursorValid = false;
	// The engine's tile search found a tile under the pixel placed last
	// frame (the sticky button, vr::StickyCursor).
	bool cursorOnTile = false;
	// The menu is one surface dragged with the button held - the map: a pull
	// holds the button at once, as on a scroll bar, so the map pans with the
	// beam in every direction. Before, an up-and-down drag turned into wheel
	// notches (the map's zoom) and only a sideways one held the button, so the
	// map moved only now and then (2026-09-26).
	bool menuIsDragSurface = false;
	// The tile under the cursor is a slider's or scroll bar's knob (its name
	// ends in "scroll_marker"): a pull there holds the button at once - the
	// knob is the only part a drag moves.
	bool cursorOnScrollKnob = false;
	// The running press's knob, found round the beam at its start
	// (game::MenuCursorKnobOffset): the press goes there.
	bool knobSnapValid = false;
	float knobSnapDx = 0.0f;
	float knobSnapDy = 0.0f;
	float cursorX = 0.0f;
	float cursorY = 0.0f;
	// Whether a teleport may be aimed now: in the world, no menu, no move
	// running, and in combat only when that is allowed. The camera hook asks
	// the game.
	bool teleportAllowed = false;
	// The inventory is the menu up: the left A drops the item under the cursor.
	bool inventoryOpen = false;
};

struct HandModeResult {
	// The teleport's stick: aiming shows the arc, commit goes this frame.
	bool teleportAiming = false;
	bool teleportCommit = false;
	// Drawing by reaching: the ready click, a weapon to equip first, the grips
	// that are the holster's (vr::StepHolster).
	HolsterVerdict holster;
	// The bow by hand (vr::StepArchery), and the arrow's line while it is
	// nocked or drawn: the shot goes along it, the crosshair hangs on it.
	ArcheryVerdict archery;
	bool arrowAimValid = false;
	float arrowYawTurn = 0.0f;
	float arrowSinPitch = 0.0f;
	NiPoint3 arrowOrigin{0.0f, 0.0f, 0.0f};     // the bow hand, tracking space, metres
	NiPoint3 arrowDirection{0.0f, 0.0f, -1.0f};  // tracking space, unit
	// Fists by making a fist: the ready click, and whether the hand is one
	// (vr::StepFist).
	FistVerdict fist;

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
	// The weapon hand as it is drawn: behind the controller by the weapon's
	// weight (vr/WeaponWeight.h), in the same frame as rightHandRotation and
	// rightHandOffsetUnits, and equal to them while nothing lags. The pin,
	// the strike and the weapon's body take these; the reach, the ring, the
	// laser and the grab stay on the controller's.
	bool weaponLagging = false;
	NiMatrix33 weaponHandRotation{};
	NiPoint3 weaponHandOffsetUnits{0.0f, 0.0f, 0.0f};
	// How far the drawn hand is behind the controller, for the log.
	float weaponGapMetres = 0.0f;
	float weaponGapDegrees = 0.0f;
	// SteamVR's velocity of each controller, relative to the head's frame in
	// the game's axes: metres and radians a second (for the throw).
	NiPoint3 rightVelocity{0.0f, 0.0f, 0.0f};
	NiPoint3 rightAngularVelocity{0.0f, 0.0f, 0.0f};
	NiPoint3 leftVelocity{0.0f, 0.0f, 0.0f};
	bool armSwinging = false;  // both arms swung: the run ([Hands] ArmSwingRun)
	NiPoint3 leftAngularVelocity{0.0f, 0.0f, 0.0f};
	// Each hand's finger curls, thumb to little finger, 0 open to 1 curled,
	// when the device gives them (the finger tracking).
	bool rightCurlValid = false;
	bool leftCurlValid = false;
	float rightCurl[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	float leftCurl[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	// And each thumb joint by joint, base to tip, when the full skeleton gave it.
	bool rightThumbValid = false;
	bool leftThumbValid = false;
	float rightThumb[3] = {0.0f, 0.0f, 0.0f};
	float leftThumb[3] = {0.0f, 0.0f, 0.0f};

	// Each controller's heading as a turn from the recenter reference, the
	// same measure as the head's camera rotation: for the walk direction.
	// In the frame's hand roles (left-handed: "right" is the left controller).
	bool rightWalkYawValid = false;
	float rightWalkYaw = 0.0f;
	bool leftWalkYawValid = false;
	float leftWalkYaw = 0.0f;

	// The controls to press, and whether any are to be pressed at all
	// (false releases everything).
	bool controlsActive = false;
	HandControlsWanted controls;
	// The right stick's button clicked short (vr::StepStickHold): the quick
	// menu's tap - the ring opens latched, the next tap lets go of it.
	bool ringTap = false;
	// The other stick's button clicked short: the spell ring's tap, the same
	// way (HandSettings::spellRing).
	bool spellRingTap = false;
	// The other hand's trackpad held, on an Index with the spell ring on
	// (vr::RingsOnTrackpad): the spell ring's, as the right trackpad held is
	// the quick menu's. Level, as long as it is held.
	bool spellRingPad = false;
	// The sticks move the player under a container's menu opened by reaching
	// (HandModeFrame::walkUnderMenu): the player's controls are to run, and
	// the laser's cursor is left where it is.
	bool walkingUnderMenu = false;

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
	// The pixel the cursor is to be placed at this frame (laserCursorDirect:
	// written before the engine's tile search), or none.
	bool cursorWanted = false;
	float cursorWantedX = 0.0f;
	float cursorWantedY = 0.0f;
	// The laser's touch-screen press is a hold: the button held for a drag
	// (a slider, a scroll bar's marker, the map), the engine's own cursor
	// leading it - nothing placed meanwhile.
	bool menuDragging = false;
	// A laser press runs on a menu (pulled, scrolling or held): the knob
	// search is armed with this ring step, in cursor pixels.
	bool menuPressRunning = false;
	float knobProbeStep = 0.0f;

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

	// OBVR's own menu: both sticks clicked together and let go toggle it, and
	// while it is open the sticks are its arrow keys.
	bool settingsMenuToggle = false;
	// Both sticks held in for kStickRecentreHoldSeconds: the recenter, as the
	// recenter key (the frame the hold is reached).
	bool recentreChord = false;
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
	// Each role's hold (vr/GripHold.h): the fingers closed round the grip by
	// the pull, the click where no pull reads - what the holsters and the
	// world weapon's take go by.
	bool rightHold = false;
	bool leftHold = false;
	// Each grip's force, 0..1 (an Index's rAxis[2].x; 0 elsewhere): a fist
	// squeezes it, so it counts as a fist where the fingers cannot be read
	// ([Hands] FistGripForce; game::HandBodyFist).
	float rightGripForce = 0.0f;
	float leftGripForce = 0.0f;
	// The left trigger as pulled, before a handle takes it
	// (HandModeFrame::leftTriggerOnHandle): what slides the hand on it.
	bool leftTriggerDown = false;
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
	// The fastest the hand went in the swing that ended this frame (m/s,
	// in the room), 0 otherwise: the log's, to set "Swing speed" by.
	// How far the right hand's swing has travelled so far (metres), and
	// whether that is far enough to be one ([Hands] SwingMinMetres): strikes
	// and the swish wait for it.
	float swingTravelledMetres = 0.0f;
	bool swingLongEnough = false;
	// The right hand's speed this frame (m/s, in the room): the touch's, for
	// a power strike by speed (game::StrikeAtContact).
	float swingSpeedNow = 0.0f;
	// A melee weapon just readied: no swing is taken for kDrawGraceSeconds.
	bool drawGrace = false;
	// Crouching in the room (HandInput.h, StepCrouchSneak), and the frame
	// it began or ended.
	bool crouched = false;
	bool crouchChanged = false;
	float swingPeakSpeed = 0.0f;
	// And how far it travelled (m): a power attack from PowerSwingMetres.
	float swingMetres = 0.0f;
	// The running swing's direction so far, from its start, relative to the
	// head: the power attack it makes (PowerDirection).
	PowerDirection powerDirection = PowerDirection::Standing;
	bool strikeByMotion = false;  // this frame's swing strikes by motion rather than by control
	// This frame's swing stabs with the arrow in the hand (vr::ArrowStabs).
	bool arrowStrike = false;
	// The left hand's swing, the same way - it strikes only with the fists up
	// (hand to hand: both hands punch; the tester, 2026-09-29: "ich kann mit
	// den linken controller nicht zuschlagen"). Its serials count from
	// kLeftSwingSerialBase, so they are never the right hand's.
	bool leftStrikeByMotion = false;
	bool leftSwingActive = false;
	bool leftSwingLongEnough = false;  // the left swing far enough to strike and swish
	bool leftSwingHeavy = false;
	float leftSwingTravelledMetres = 0.0f;  // how far the left swing has come so far
	float leftSwingSpeedNow = 0.0f;         // the left hand's speed this frame (m/s)
	UInt32 leftSwingSerial = 0;
	PowerDirection leftPowerDirection = PowerDirection::Standing;
	SwingVerdict leftSwing = SwingVerdict::None;

	// The ready-weapon click's progress this frame.
	ReadyWeaponVerdict ready;

	// For the log, and for the block's angle rule: what the block is.
	bool blocking = false;
	game::BlockWith blockWith = game::BlockWith::None;
	bool bowGuard = false;  // the block is the bow held upright
	bool fistGuard = false;  // the block is both fists raised
	bool reachBack = false;
	SwingVerdict swing = SwingVerdict::None;
};

// Where the left hand's swing serials start, apart from the right's.
inline constexpr UInt32 kLeftSwingSerialBase = 0x80000000u;

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
	ArmSwingState m_armSwing;
	TapHoldState m_povHold;
	TapHoldState m_waitHold;
	TapHoldState m_quickHold;
	SneakHoldState m_sneak;
	CrouchState m_crouch;
	// The drawn weapon hand behind the controller (vr/WeaponWeight.h).
	WeaponLagState m_weaponLag;
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
	TeleportStickState m_teleportStick;
	HolsterState m_holster;
	// The hands' holds by the pull (vr/GripHold.h), the holsters' grips.
	GripHoldState m_rightHold;
	GripHoldState m_leftHold;
	ArcheryState m_archery;
	// The last arrow's line, kept for the loose (vr::StepArrowAimHold).
	struct ArrowAimKept {
		bool valid = false;
		float yawTurn = 0.0f;
		float sinPitch = 0.0f;
		NiPoint3 origin{0.0f, 0.0f, 0.0f};
		NiPoint3 direction{0.0f, 0.0f, -1.0f};
	};
	ArrowAimKept m_lastArrow;
	ArrowAimHold m_arrowHold;
	FistState m_fist;
	// The weapon hand's fingers that have read (vr/FingersRead.h), for the
	// fist; kept across Reset - a dead sensor stays dead.
	FingersRead m_fistFingersRead;
	ButtonEdge m_dropEdge;
	DropPressState m_drop;
	bool m_dropClickNow = false;  // this frame carries the drop's click
	LaserPressState m_press;
	ButtonPressState m_buttonPress;
	CursorHoldState m_cursorHold;
	StickyCursorState m_sticky;
	RepeatState m_scrollUp;
	RepeatState m_scrollDown;
	StickChordState m_sticks;
	StickHoldState m_rightStickHold;
	StickHoldState m_leftStickHold;
	StickNavState m_navRight;
	StickNavState m_navLeft;
	PokeState m_poke;
	SwingDetector m_swing;
	DrawGraceState m_drawGrace;
	HeldControl m_heavyHold;
	UInt32 m_swingSerial = 0;
	bool m_haveLastRight = false;
	bool m_reachArmed = false;  // the reach back seen since the last release
	bool m_reachSpent = false;  // a draw used the armed reach
	NiPoint3 m_lastRightRoom{0.0f, 0.0f, 0.0f};  // tracking space, for the swing speed
	NiPoint3 m_swingStartRoom{0.0f, 0.0f, 0.0f};  // where the running swing began
	PowerDirection m_powerDirection = PowerDirection::Standing;
	SwingDetector m_leftSwing;
	UInt32 m_leftSwingSerial = kLeftSwingSerialBase;
	bool m_haveLastLeft = false;
	NiPoint3 m_lastLeftRoom{0.0f, 0.0f, 0.0f};
	NiPoint3 m_leftSwingStartRoom{0.0f, 0.0f, 0.0f};
	PowerDirection m_leftPowerDirection = PowerDirection::Standing;
};

}  // namespace obvr::vr

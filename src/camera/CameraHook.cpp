#include "camera/CameraHook.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "camera/CameraTrampoline.h"
#include "camera/CastTrampoline.h"
#include "camera/LookControl.h"
#include "camera/SnapTurn.h"
#include "core/Config.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "core/MathFns.h"
#include "game/CrosshairTarget.h"
#include "game/DialogZoom.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonDepth.h"
#include "game/FirstPersonHide.h"
#include "game/BonePin.h"
#include "game/HandAdjust.h"
#include "game/HandBones.h"
#include "game/HandControls.h"
#include "game/MeleeHits.h"
#include "game/Shove.h"
#include "game/ThirdPersonAimVisual.h"
#include "game/BodyPlacement.h"
#include "game/PlayerBody.h"
#include "game/WorldPickHook.h"
#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/MenuBackground.h"
#include "game/MenuPause.h"
#include "core/Rotation.h"
#include "core/Watchdog.h"
#include "game/MenuMode.h"
#include "game/NativeMenuPrototype.h"
#include "game/MenuType.h"
#include "game/AimAtSource.h"
#include "game/PlayerAim.h"
#include "game/PlayerStagger.h"
#include "game/HandGrip.h"
#include "game/DeathBody.h"
#include "game/HeldObject.h"
#include "game/PlayerLookAt.h"
#include "game/PlayerTeleport.h"
#include "game/ControlBindings.h"
#include "game/GrabPhysics.h"
#include "game/WorldPush.h"
#include "game/HandBodies.h"
#include "game/HandBodyLogic.h"
#include "game/PlayerCapsule.h"
#include "game/TakeItem.h"
#include "game/WeaponDrawSpeed.h"
#include "game/GrabNearBody.h"
#include "game/NearbyItems.h"
#include "game/MeleeHit.h"
#include "game/QuickKeys.h"
#include "game/ItemIcons.h"
#include "game/ArmStump.h"
#include "game/QuickKeyPages.h"
#include "game/TeleportNoise.h"
#include "game/GameSound.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"
#include "render/DxvkInterop.h"
#include "render/GameDevice.h"
#include "render/GameProjection.h"
#include "render/HeadsetRenderer.h"
#include "render/WaterReprojection.h"
#include "test/HandScriptRuntime.h"
#include "test/WaterVRTestRuntime.h"
#include "render/CrosshairLayer.h"
#include "render/VignetteLayer.h"
#include "render/HudLayer.h"
#include "render/LaserLayer.h"
#include "render/ReachMarker.h"
#include "render/TeleportArcLayer.h"
#include "vr/LaserGeometry.h"
#include "vr/StowPlace.h"
#include "ui/CanvasOverlay.h"
#include "ui/Onboarding.h"
#include "ui/SettingsMenu.h"
#include "ui/SettingsMenuLayer.h"
#include "ui/QuickMenuPainter.h"
#include "ui/StowSpotPainter.h"
#include "ui/GuidePanel.h"
#include "render/InterfaceRenderHook.h"
#include "render/CursorPickHook.h"
#include "render/CursorProbe.h"
#include "render/SceneGraphProbe.h"
#include "render/LayoutProbe.h"
#include "render/MenuShade.h"
#include "render/PresentHook.h"
#include "render/ResolutionHook.h"
#include "render/SceneRenderHook.h"
#include "perf/Profiler.h"

namespace obvr::camera {
namespace {

State g_state;
vr::HeadTracker g_headTracker;

constexpr UInt32 kTrampolineSize = 64;

KeyEdge g_recenterEdge;
FrameClock g_frameClock;
float g_deltaSeconds = 0.0f;  // last frame's delta, for use by overlay updates
LookControl g_lookControl;

// Snap turning, stepped once per frame with the controls - see SnapTurn.h.
SnapTurnState g_snapTurnState;
bool g_snapTurnReported = false;

render::HeadsetRenderer g_headsetRenderer;

// What the camera hook decided about this frame, kept for Present to act on.
//
// Kept rather than rebuilt at the end, because the eye it names has to be the
// eye the camera was actually moved to. Reading the frame counter twice would
// be two chances to disagree, and disagreeing means each eye showing the
// other's viewpoint - a fault this project has already had once and does not
// need a second route to.
render::HeadsetRenderer::FrameRequest g_pendingRequest;

// Whether hooking the end of the frame was tried and failed. One attempt, not
// one per frame: a device whose table cannot be written this frame will not
// become writable on the next.
bool g_presentHookRefused = false;

// Whether BeginFrame opened a frame this pass. The submit at the end - here or
// from Present - is only owed when it did.
bool g_frameOpen = false;

// Watches Oblivion's own render frustum for a few frames, to say whether it is
// one view or several. See game::FrustumWatcher.
game::FrustumWatcher g_frustumWatcher;

// Counts frames delivered with no camera pass behind them, to answer whether
// Oblivion presents more than once per step while a menu is up.
UInt32 g_flatFramesSinceCamera = 0;

// What the second render pass of a dual-pass frame needs: the node to move,
// how far to move it, and whether this frame's camera pass actually set the
// two up. Armed by the camera hook, consumed by the scene render hook - both
// on the game's thread, in that order within a frame.
//
// The node pointer is only ever used between the camera pass that stored it
// and the render of the same frame, so its lifetime is the frame's own.
NiAVObject* g_dualNode = nullptr;
NiPoint3 g_dualShift{0.0f, 0.0f, 0.0f};
bool g_dualArmed = false;

// The camera as the game last left it, kept for the live menu background.
// See where they are written for why the node's own transform cannot serve.
NiAVObject* g_menuBaseNode = nullptr;
NiPoint3 g_menuBasePos{0.0f, 0.0f, 0.0f};
// The death view held still (StepDeathView), read by the menu camera too.
DeathViewState g_deathView;
NiMatrix33 g_menuBaseRot;
float g_menuBaseVerticalOffset = 0.0f;
bool g_menuBaseThirdPerson = false;

// The live menu background's frame: the request its two captures are made
// against, which eye its first render drew, the step between the eyes for the
// bone lock, and whether it actually produced a pair this frame - which is
// what tells OnFrameEnd to submit a fresh stereo frame instead of a held one,
// and what stops a second entry of the 2D pass starting the whole thing again.
render::HeadsetRenderer::FrameRequest g_menuRequest;
bool g_menuFirstIsLeft = true;
NiPoint3 g_menuEyeShift{0.0f, 0.0f, 0.0f};
bool g_menuLiveThisFrame = false;
UInt32 g_menuLiveReportsLeft = 6;

// The 2D layer's own picture and overlay, fed by the interface render hook
// and paid at Present alongside the eyes.
render::HudLayer g_hudLayer;
render::CrosshairLayer g_crosshairLayer;
render::VignetteLayer g_vignetteLayer;
render::LaserLayer g_laserLayer;
render::ReachMarker g_reachMarker;
// The Full VR teleport (vr::Teleport, game::PlayerTeleport): the arc while it
// aims and the move once it goes.
render::TeleportArcLayer g_teleportArc;
vr::TeleportMove g_teleportMove;
// Whether the ring shows this frame and where: the crosshair's icon hangs in it.
bool g_reachIconShown = false;
// The item nearest a hand within its reach (game::FindNearestItem): the world
// pick is aimed from that hand at it, so a hand brought to an item gets its
// tooltip and marker without pointing at it.
game::NearItem g_nearItem;
vr::openvr::HmdMatrix34 g_reachIconPose{};
// The cyclopean camera, snapshotted in the camera pass (see there); declared
// early because the hand mode measures the grab reach from it.
NiTransform g_cyclopeanCameraWorldTransform{};
bool g_cyclopeanCameraWorldValid = false;
// Left-handed in Full VR this frame: "right" in g_hand is the left controller.
bool g_handRolesSwapped = false;
// The grab by reach - see vr::StepGrabReach.
vr::GrabReachState g_grabReach;
bool g_grabReachPick = false;
bool g_grabKeyDown = false;
// The conversation approach is on (game::StepDialogApproach), as last decided
// at Present - read again before the next world render.
bool g_handsAwayForDialog = false;
// The forearm stump (game/ArmStump.h): in place after the last pins, stepped
// this frame, and the hands away (menus, dialogue) as Present last decided.
bool g_stumpShown = false;
bool g_stumpStepped = false;
bool g_stumpHandsAway = false;
int g_stumpSleevesLogged = -1;  // the last classification logged, -1 none yet
UInt32 g_stumpLinesLeft = 12;

// OBVR's own settings menu: what it is showing, and the quad it shows it on.
//
// Separate objects on purpose. The menu is state and decisions and can be
// driven from a test; the layer is a texture and an overlay and cannot.
ui::SettingsMenu g_settingsMenu;
ui::SettingsMenuLayer g_settingsMenuLayer;

// The first-start walkthrough, on a layer of its own so it can stand in front
// of the world while the settings menu stays closed. Offered once per game
// start, the first time the headset is there to show it.
ui::OnboardingMenu g_onboarding;
ui::SettingsMenuLayer g_onboardingLayer;
bool g_onboardingOffered = false;

// One edge each for the keys that drive it. Edges rather than held states,
// because every one of these means "do this once" - a held arrow key that
// moved the highlight every frame would cross a twenty-row list in a third of
// a second.
KeyEdge g_menuToggleEdge;
KeyEdge g_menuUpEdge;
KeyEdge g_menuDownEdge;
KeyEdge g_menuLeftEdge;
KeyEdge g_menuRightEdge;

// How many of those bursts have been reported.
UInt32 g_flatBurstsReported = 0;

// Reported once for a menu frame and once for a world frame, because the two
// can differ and the difference is the whole question.
bool g_viewportReported[2] = {false, false};

// The menu measurement.
//
// Menus=world put the menu on the monitor instead of in the headset, and the
// standing traces could not say why: every one of them is spent within the
// first second of play, and nobody opens an inventory that fast. So opening or
// closing a menu reopens a short window, and the log gets its numbers from the
// frames the question is actually about.
//
// What the window has to settle, because the first run's log gave two readings
// that cannot both be right: the scene counter advanced once per 2D pass, which
// says the second world render did not run, while every pass reported "already
// captured", which only happens when the run between the two renders did. One
// of those is being misread, and the frame line below prints both side by side
// rather than leaving it to inference.
UInt32 g_menuTraceLeft = 0;

// Frames for which the blocking steps of a frame are written down as they are
// reached, armed by a point-of-view switch.
//
// A HANG is not a crash and does not leave the same evidence. Nothing is
// corrupted and nothing throws; some call simply does not return, and the last
// line in the log is wherever logging happened to stop rather than where the
// fault is. Twice now a session has ended right after "Camera: switched to
// third person", and both times the log said only that much.
//
// OBVR has exactly one kind of call that can block indefinitely: the ones that
// talk to the VR compositor. WaitGetPoses waits for the headset to want the
// next frame, and it waits without a timeout. Oblivion on its own never waits
// for anything of the sort, which is why a hang is a stronger hint that OBVR is
// involved than a crash would be.
//
// So the steps around those calls now leave a mark while this is armed. If the
// next hang's log ends on "waiting for poses", the question is answered; if it
// ends after them, the compositor is cleared and the search moves on.
//
// Thirty frames rather than twelve: the last hang came seven frames after the
// switch, which is well inside the old window but too close to its end to be
// comfortable.
UInt32 g_stepTraceLeft = 0;

// Written down only while armed, so this costs one comparison a frame in
// ordinary play.
void TraceStep(const char* where) {
	watchdog::NoteStep(where);
	if (g_stepTraceLeft > 0) {
		OBVR_LOG("Step: %s", where);
	}
}
bool g_menuTraceWasUp = false;
UInt32 g_menuTraceLastScene = 0;
UInt32 g_menuTraceLastId = 0;

// Lines the aim probe has left. Budgeted rather than endless: a few seconds
// of looking around is the whole measurement, and the log is read by hand.
UInt32 g_aimProbeLeft = 60;

// Lines the third person probe has left. Every fourth frame, so six hundred
// covers the better part of half a minute at the headset's rate - long enough
// for the mouse to tilt the view through its whole range twice.
UInt32 g_thirdPersonProbeLeft = 600;

// Whether the log has already said that the gaze is now steering the player's
// pitch. Once per session: it is the confirmation that the write reached the
// player at all, and repeating it every frame would bury everything else.
bool g_aimPitchReported = false;

// The pitch OBVR put into the player on the last frame it wrote one, and
// whether it has ever written. Kept so the probe can hold the engine's value
// up against it: if the two match on the next frame, a written rotation
// survives, and writing the YAW would feed back into the camera. If they do
// not, the engine sets the field afresh from its own input each frame and
// there is no loop to break.
float g_aimLastWrittenPitch = 0.0f;
bool g_aimEverWrote = false;

// The sideways half's own state. The heading OBVR last put into the player,
// the step it took to get there, and whether that write is still waiting to be
// checked next frame.
float g_aimYawWrote = 0.0f;
float g_aimYawStepTaken = 0.0f;
bool g_aimYawPending = false;

// How much of the head's turn the body has been given, and never taken back.
//
// This is the whole sideways mechanism in one number. A written heading was
// measured to survive the frame, so the camera - which is built on the player's
// heading with the head's turn added on top - would read OBVR's own turn back
// and add the head to it again, and the view would creep round for as long as
// the head stayed turned. That is not a reason to stop writing; it is a reason
// to book the turn in one place instead of two.
//
// So every step handed to the body is subtracted from the head again, at the
// base rotation, before the head is laid on it. The view does not move at all:
// the wearer keeps looking at exactly what they were looking at, and only the
// body comes round underneath. Turning the head is a glance; turning it while
// aiming is a glance that the body follows.
//
// It is never reset, because the turn it accounts for is never undone either -
// the body stays where it was turned to, and the head sits on it wherever it
// happens to be. Its one soft spot is a load or a script that moves the player
// itself: the offset then describes a turn that is no longer in the player's
// heading, and the view sits crooked until the next aim brings the two back
// together. Writes that fail to land are already taken back out below; a load
// is not, and that is the known limit rather than a solved case.
float g_aimBodyOffset = 0.0f;

// THE THIRD PERSON'S OWN BOOKKEEPING, beside the offset above.
//
// The first person camera is built from the heading the frame it is written,
// so the offset above is also what the picture carries. The third person
// camera is not: it eases towards the heading a few percent of what is left
// per physics step, at the physics' own rate rather than the renderer's
// (camera::MeasuredChaseRate has the measurement). So what has to be taken
// back out of that picture is not the offset but the share of it the camera
// has reached, and that share is kept here, stepped by the rate the camera
// itself showed this frame.
float g_aimChasedOffset = 0.0f;

// The chase camera's own angles last frame, so this frame's step can be
// measured against them, and the rates that came out - kept for the trace.
float g_chaseYawBefore = 0.0f;
float g_chasePitchBefore = 0.0f;
bool g_chaseHaveBefore = false;
float g_chaseYawRate = 0.0f;
float g_chasePitchRate = 0.0f;

// The pitch, which in third person is borrowed rather than written outright
// - see camera::AimPitchHold. The offset is how far the field stands from
// the mouse's own tilt after this frame, and the chased value the share of
// it the camera has eased towards, both in the engine's convention.
AimPitchHold g_aimPitchHold{};
float g_aimPitchOffset = 0.0f;
float g_aimChasedPitch = 0.0f;

// Whether this frame gives a held aim back - decided where the body's return
// is, and read again where the pitch is written, which comes later in the
// frame.
bool g_aimReturnDue = false;

// The vertical counterpart of the arc correction, for the trace.
NiPoint3 g_aimTiltApplied{0.0f, 0.0f, 0.0f};

bool g_aimYawReported = false;
bool g_aimYawLostReported = false;
bool g_aimReturnReported = false;
// The walk direction's first turn of the body, said once.
bool g_walkSteerReported = false;
bool g_aimThirdPersonReported = false;

// How far the first person weapon should be turned, and whether it should be
// turned at all. Decided in the camera pass, applied at the top of the render -
// see BeforeFirstScenePass for why those cannot be the same place.
float g_weaponTurnRadians = 0.0f;
bool g_weaponTurnWanted = false;

// The hand-tracked mode: its per-frame decision, taken at Present so it runs
// on menu frames too, and read by the camera pass (the aim) and the render
// pass (the arms). See vr/HandMode.h and docs/hand-tracked-mode.md.
vr::HandMode g_handMode;
vr::HandModeResult g_hand;
bool g_handArmsWanted = false;
bool g_handControlsHeld = false;
long long g_handClockLast = 0;

// For the mode's log lines on change: which controllers are tracked and
// which gestures are on.
bool g_rightHandTracked = false;
bool g_leftHandTracked = false;
bool g_flatLaserTargetReported = false;
UInt32 g_flatLaserLinesLeft = 6;
UInt32 g_handMaskLinesLeft = 24;
UInt64 g_lastRightMask = 0;
UInt64 g_lastLeftMask = 0;
bool g_flatCursorInvalidReported = false;
// The same for a menu in the world, on its quad: a few lines each time a
// menu opens. The first Full VR headset run (2026-09-25) had the beam and
// neither a cursor nor a click on the Tab and pause menus, and nothing in
// the log could say which input to the laser was missing.
UInt32 g_quadLaserLinesLeft = 0;
bool g_quadLaserMenuWasUp = false;
bool g_handBlocking = false;
bool g_handReachBack = false;
UInt32 g_handSwingLinesLeft = 60;
UInt32 g_handPokeLinesLeft = 20;
UInt32 g_handPovLinesLeft = 20;

bool ReadIsThirdPerson();

// The mode's frame, gathered and decided. Runs from Present, before the
// frame's delivery, so a wrist placement is in place for the overlay submit
// and the controls are pressed before the engine's next input read.
// The one list of first-person nodes Full VR hides (ComposeHandsHideList).
const char* HandsHideList(const vr::HandSettings& hands, bool handsAway, bool sheathing,
                          bool armsShownAsStump = false) {
	static char list[256];
	ComposeHandsHideList(list, sizeof(list), hands.hideArms, hands.hideNodes, hands.hideSheaths,
	                     handsAway, sheathing, armsShownAsStump);
	return list;
}

// ------------------------------------------------------------------ Teleport
//
// The Full VR teleport (docs/next-up.md 6, vr::Teleport for the logic,
// game::PlayerTeleport for the engine): the right stick pushed forward aims
// an arc from the right hand, the ring shows where it lands, letting go
// moves the player there.

bool TeleportAllowedNow(const Config& config, bool menuIsUp, bool inWorld) {
	const vr::TeleportSettings& tp = config.hands.teleport;
	if (!tp.enabled || menuIsUp || !inWorld || ReadIsThirdPerson() || game::PlayerIsDead() ||
	    g_teleportMove.phase != vr::TeleportPhase::Idle) {
		return false;
	}
	if (game::PlayerRiding()) {
		return false;
	}
	return tp.inCombat || !game::PlayerInCombat();
}

// Where the arc goes and what it lands on this frame.
struct TeleportAim {
	NiPoint3 points[vr::kArcMaxPoints];
	UInt32 count = 0;
	bool landed = false;
	NiPoint3 landing{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};
	vr::TeleportRefusal refusal = vr::TeleportRefusal::NoGround;
	float cost = 0.0f;
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
};

// Without Blink the glide must not pass through anything: the straight line
// at waist height from here to the landing has to be clear.
constexpr float kTeleportWaistUnits = 60.0f;
// Set down a hair above the ground, so the capsule does not start in it.
constexpr float kTeleportLiftUnits = 2.0f;
// How long a teleport is heard at least, with TeleportMakesNoise: about
// the time of a step.
constexpr float kTeleportNoiseSeconds = 0.5f;
float g_teleportNoiseLeft = 0.0f;
// Where the last teleport landed, and the second after it: then the log
// says how far the player has moved since, with the sticks untouched it
// should be nothing (the noise once walked it a step on, 2026-09-28).
NiPoint3 g_teleportLandedFeet{0.0f, 0.0f, 0.0f};
float g_teleportDriftCheckLeft = 0.0f;
UInt32 g_teleportDriftLines = 4;

TeleportAim AimTeleport(const Config& config, const vr::LaserWorldRay& ray) {
	const vr::TeleportSettings& tp = config.hands.teleport;
	const float perMetre = config.tracker.unitsPerMetre;
	const float rangeUnits = tp.rangeMetres * perMetre;
	const float gravity = vr::kArcGravityMetres * perMetre;
	const float speed = vr::ArcSpeed(rangeUnits, gravity);
	const float step = vr::ArcTimeStep(rangeUnits, gravity);
	TeleportAim aim;
	aim.points[0] = ray.origin;
	aim.count = 1;
	bool worldAsked = true;
	for (UInt32 i = 1; i < vr::kArcMaxPoints && worldAsked; ++i) {
		const NiPoint3 from = aim.points[i - 1];
		const NiPoint3 to =
			vr::ArcPoint(ray.origin, ray.direction, speed, gravity, step * static_cast<float>(i));
		game::WorldPick pick;
		worldAsked = game::PickWorldSegment(from, to, pick);
		if (worldAsked && pick.hit) {
			aim.points[aim.count++] = pick.point;
			aim.landed = true;
			aim.landing = pick.point;
			aim.normal = pick.normal;
			break;
		}
		aim.points[aim.count++] = to;
	}
	vr::LandingQuery q;
	q.hit = aim.landed;
	q.point = aim.landing;
	const float normalLength = math::Sqrt(aim.normal.LengthSquared());
	q.normalUp = normalLength > 1.0e-6f ? aim.normal.z / normalLength : 0.0f;
	q.allowedNow = game::ReadPlayerFeet(aim.feet);
	q.feet = aim.feet;
	q.rangeUnits = rangeUnits;
	q.jumpUnits = game::PlayerJumpUnits();
	q.blink = tp.blink;
	if (aim.landed && !tp.blink) {
		const NiPoint3 up{0.0f, 0.0f, kTeleportWaistUnits};
		game::WorldPick between;
		q.pathClear =
			game::PickWorldSegment(aim.feet + up, aim.landing + up, between) && !between.hit;
	}
	const float rise = (aim.landing.z - aim.feet.z) / (perMetre > 0.0f ? perMetre : 1.0f);
	aim.cost = vr::TeleportFatigueCost(game::PlayerDodgeFatigueCost(), rise, tp);
	float fatigueBase = 0.0f;
	q.fatigueCost = aim.cost;
	if (!game::ReadPlayerFatigue(q.fatigueNow, fatigueBase)) {
		q.fatigueNow = aim.cost;  // unreadable: do not lock the player out
	}
	aim.refusal = vr::JudgeLanding(q);
	return aim;
}

UInt32 g_teleportLinesLeft = 16;

void UpdateTeleport(const Config& config, vr::OpenVRBackend& backend, bool active,
                    bool menuIsUp, float dtSeconds) {
	const vr::TeleportSettings& tp = config.hands.teleport;
	const float perMetre = config.tracker.unitsPerMetre;
	bool arcShown = false;
	if (g_teleportMove.phase != vr::TeleportPhase::Idle && (!active || menuIsUp)) {
		// A menu opened mid-move (a notice, a tutorial message), or the mode
		// stopped: the move ends at once, at its target, the view back from
		// black and the player hurtable again, so nothing of the teleport is
		// left standing while the menu waits for its click. Pausing it there
		// instead was when one message box could not be clicked away
		// (2026-09-27); that the pause was the cause is not proven.
		if (g_teleportMove.fade) {
			backend.FadeToColor(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		}
		const bool placed = active && game::PlacePlayerAt(g_teleportMove.to);
		if (g_teleportLinesLeft > 0) {
			--g_teleportLinesLeft;
			OBVR_LOG("Teleport: %s mid-move - ended at once%s",
			         active ? "a menu opened" : "the mode stopped",
			         placed ? ", placed at the target" : "");
		}
		g_teleportMove = vr::TeleportMove{};
	}
	if (active && !menuIsUp && (g_hand.teleportAiming || g_hand.teleportCommit) &&
	    g_hand.rightHandValid && g_cyclopeanCameraWorldValid) {
		const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
			g_cyclopeanCameraWorldTransform.rot, g_cyclopeanCameraWorldTransform.pos,
			g_hand.rightHandRotation, g_hand.rightHandOffsetUnits, config.hands.laserPitchDegrees,
			config.hands.laserYawDegrees, config.hands.laserOriginMetres, perMetre);
		const TeleportAim aim = AimTeleport(config, ray);
		const bool valid = aim.refusal == vr::TeleportRefusal::None;
		vr::openvr::HmdMatrix34 head{};
		if (g_hand.teleportAiming && backend.GetRenderPoseMatrix(head)) {
			const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
			const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
			NiPoint3 tracked[vr::kArcMaxPoints];
			for (UInt32 i = 0; i < aim.count; ++i) {
				tracked[i] = vr::WorldPointInTracking(head, camRot, camPos, aim.points[i], perMetre);
			}
			const NiPoint3 ringAt =
				vr::WorldPointInTracking(head, camRot, camPos, aim.landing, perMetre);
			const NiPoint3 normalTip = vr::WorldPointInTracking(
				head, camRot, camPos, aim.landing + aim.normal * perMetre, perMetre);
			const NiPoint3 eyes{head.m[0][3], head.m[1][3], head.m[2][3]};
			g_teleportArc.Submit(backend, true, tracked, aim.count, eyes, aim.landed, ringAt,
			                     normalTip - ringAt, valid);
			arcShown = true;
		}
		if (g_hand.teleportCommit) {
			if (valid) {
				// The price first: while untouchable the player cannot lose fatigue.
				game::SpendPlayerFatigue(aim.cost);
				const NiPoint3 target = aim.landing + NiPoint3{0.0f, 0.0f, kTeleportLiftUnits};
				g_teleportMove = vr::StartTeleport(aim.feet, target, tp, perMetre);
				g_teleportNoiseLeft = g_teleportMove.duration > kTeleportNoiseSeconds
				                          ? g_teleportMove.duration
				                          : kTeleportNoiseSeconds;
				game::SetPlayerUntouchable(true);
				if (!tp.instant && tp.vignette) {
					g_vignetteLayer.Trigger();
				}
			}
			if (g_teleportLinesLeft > 0) {
				--g_teleportLinesLeft;
				const NiPoint3 d = aim.landing - aim.feet;
				OBVR_LOG("Teleport: %s - %.2f m away, %.2f m up, cost %.1f fatigue, %s (%u arc "
				         "points)",
				         valid ? "going" : "refused",
				         static_cast<double>(math::Sqrt(d.LengthSquared()) / perMetre),
				         static_cast<double>(d.z / perMetre), static_cast<double>(aim.cost),
				         valid ? (tp.instant ? "instant" : "glide")
				               : vr::TeleportRefusalName(aim.refusal),
				         aim.count);
			}
		}
	}
	if (!arcShown) {
		g_teleportArc.Submit(backend, false, nullptr, 0, NiPoint3{0.0f, 0.0f, 0.0f}, false,
		                     NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, false);
	}

	// The move, once it runs (a menu has ended it above).
	if (g_teleportMove.phase != vr::TeleportPhase::Idle) {
		const vr::TeleportMoveStep step = vr::StepTeleportMove(g_teleportMove, dtSeconds);
		if (step.fadeOut) {
			backend.FadeToColor(tp.fadeSeconds, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (step.place && !game::PlacePlayerAt(step.at)) {
			if (g_teleportLinesLeft > 0) {
				--g_teleportLinesLeft;
				OBVR_LOG("Teleport: the player could not be placed - the move stops");
			}
			if (g_teleportMove.fade) {
				backend.FadeToColor(tp.fadeSeconds, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			g_teleportMove = vr::TeleportMove{};
		}
		if (step.fadeIn) {
			backend.FadeToColor(tp.fadeSeconds, 0.0f, 0.0f, 0.0f, 0.0f);
		}
		if (vr::TeleportSoundDue(tp.makesNoise, step.finished)) {
			// The landing heard ([Locomotion] TeleportSound).
			static UInt32 soundLines = 4;
			const UInt32 form = vr::TeleportSoundForm(tp.sound);
			const bool played = form != 0 ? game::PlaySoundForm(form) : game::PlayPlayerLandingSound();
			if (soundLines > 0) {
				--soundLines;
				OBVR_LOG("Teleport: the landing sound %s (%08X) %s",
				         vr::kTeleportSoundNames[static_cast<UInt32>(tp.sound)], form,
				         played ? "played" : "NOT played");
			}
		}
		if (step.finished) {
			static UInt32 arrivedLines = 4;
			NiPoint3 feet{0.0f, 0.0f, 0.0f};
			if (arrivedLines > 0 && game::ReadPlayerFeet(feet)) {
				--arrivedLines;
				const NiPoint3 miss = feet - g_teleportMove.to;
				OBVR_LOG("Teleport: arrived, %.1f units from the target",
				         static_cast<double>(math::Sqrt(miss.LengthSquared())));
				g_teleportLandedFeet = feet;
				g_teleportDriftCheckLeft = 1.0f;
			}
		}
	}
	// Untouchable exactly while the move runs; also given back when the mode
	// stops mid-move.
	game::SetPlayerUntouchable(active && g_teleportMove.phase != vr::TeleportPhase::Idle);
	// Heard like walking ([Locomotion] TeleportMakesNoise): for as long as
	// the move lasts, and at least kTeleportNoiseSeconds - a glide shorter
	// than a frame (the hand-script runs at a few frames a second) would
	// otherwise be over before the engine ever read the flags.
	if (g_teleportNoiseLeft > 0.0f) {
		g_teleportNoiseLeft -= dtSeconds;
	}
	if (g_teleportDriftCheckLeft > 0.0f) {
		g_teleportDriftCheckLeft -= dtSeconds;
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		if (g_teleportDriftCheckLeft <= 0.0f && g_teleportDriftLines > 0 && game::ReadPlayerFeet(feet)) {
			--g_teleportDriftLines;
			const NiPoint3 moved = feet - g_teleportLandedFeet;
			OBVR_LOG("Teleport: a second after the landing the player stands %.1f units from it "
			         "(%.1f across the ground; the left stick %s)",
			         static_cast<double>(math::Sqrt(moved.LengthSquared())),
			         static_cast<double>(math::Sqrt(moved.x * moved.x + moved.y * moved.y)),
			         g_hand.controls.move.forward || g_hand.controls.move.back || g_hand.controls.move.left ||
			                 g_hand.controls.move.right
			             ? "walking"
			             : "still");
		}
	}
	game::SetTeleportNoise(active && config.hands.teleport.makesNoise &&
	                       (g_teleportMove.phase != vr::TeleportPhase::Idle ||
	                        g_teleportNoiseLeft > 0.0f));
}

// The quick menu on the right trackpad (vr/QuickMenu.h, docs/controls-spec.md
// 4.4): the ring of the eight hotkeys where the hand is, the hand moved
// towards one, the trackpad let go - and that hotkey's number key is tapped.
// The last weapon of each kind seen in the slot this session: base forms.
// A plugin's forms stay for as long as the game runs; a form the game made
// (a custom enchanted weapon) could in principle go, which is not guarded
// (vr::StepHolster).
UInt8* g_lastOneHandForm = nullptr;
UInt8* g_lastTwoHandForm = nullptr;
UInt8* g_lastBowForm = nullptr;

UInt8*& LastFormOf(vr::EquippedKind kind) {
	static UInt8* s_none = nullptr;
	switch (kind) {
	case vr::EquippedKind::OneHand:
		return g_lastOneHandForm;
	case vr::EquippedKind::TwoHand:
		return g_lastTwoHandForm;
	case vr::EquippedKind::Bow:
		return g_lastBowForm;
	default:
		s_none = nullptr;
		return s_none;
	}
}
UInt32 g_holsterLinesLeft = 30;

// The guided fit of the weapon places (vr::StepHolsterFit), asked for by the
// settings row or a hand script, with its instruction panel.
vr::HolsterFitState g_holsterFit;
// Whether each hand is a fist, for its Havok body (game::HandBodyFist).
bool g_rightBodyFist = false;
bool g_leftBodyFist = false;

// The shove (game/ShoveLogic.h, game/Shove.h): each tracked hand, open and
// not gripping, moving fast towards a living actor it is at, with the
// weapons away. The actor is pushed away from a point behind the hand along
// its motion.
game::ShoveCooldown g_shoveCooldown;

void StepShoves(const Config& config, float dt) {
	game::StepShoveCooldown(g_shoveCooldown, dt);
	const game::ShoveSettings& settings = config.hands.shove;
	if (!settings.enabled || !g_cyclopeanCameraWorldValid) {
		return;
	}
	const bool weaponDrawn = game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
	const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
	const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
	for (int side = 0; side < 2; ++side) {
		const bool right = side == 0;
		game::ShoveHand hand;
		hand.valid = right ? g_hand.rightHandValid : g_hand.leftHandValid;
		hand.open = !(right ? g_rightBodyFist : g_leftBodyFist);
		hand.gripHeld = right ? g_hand.rightGripDown : g_hand.leftGripDown;
		if (!hand.valid) {
			continue;
		}
		const NiPoint3 at = camPos + camRot * (right ? g_hand.rightHandOffsetUnits : g_hand.leftHandOffsetUnits);
		NiPoint3 centre{0.0f, 0.0f, 0.0f};
		void* const actor = game::LivingActorAt(at, 0.5f, 4.0f, &centre);
		if (actor == nullptr || !game::ShoveAllowed(g_shoveCooldown, actor)) {
			continue;
		}
		// SteamVR's velocity, relative to the head's frame in the game's axes
		// (m/s): into the world by the camera's rotation.
		const NiPoint3 velocity = camRot * (right ? g_hand.rightVelocity : g_hand.leftVelocity);
		hand.towardsSpeed = game::SpeedTowards(velocity, at, centre);
		const game::ShoveKind kind = game::ShoveFor(settings, weaponDrawn, hand);
		if (kind == game::ShoveKind::None) {
			continue;
		}
		const float across = math::Sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		NiPoint3 from = at;
		if (across > 0.001f) {
			from = at - NiPoint3{velocity.x / across, velocity.y / across, 0.0f} * 30.0f;
		}
		if (game::ShoveActor(actor, kind, from, centre, settings)) {
			game::StartShoveCooldown(g_shoveCooldown, actor, settings.cooldownSeconds);
			OBVR_LOG("Shove: the %s hand at %.1f m/s towards %08X - %s", right ? "right" : "left",
			         static_cast<double>(hand.towardsSpeed), reinterpret_cast<UInt32>(actor),
			         kind == game::ShoveKind::Hard ? "hard" : "light");
		}
	}
}

// Measuring the bodies against what is drawn (the spec's open bugs,
// 2026-09-29): the fingers against the hand capsule's front, the drawn
// blade against the weapon capsule, and a swing's fastest tip per physics
// step against what a thin object lets through.
NiPoint3 g_bladeTipBefore{0.0f, 0.0f, 0.0f};
bool g_bladeTipBeforeValid = false;
float g_bladeTipFastest = 0.0f;
UInt32 g_bladeSwingLinesLeft = 16;
UInt32 g_bodyGeometryLinesLeft = 3;

void LogHandGeometry(const game::HandBodyFrame& bodies, int slot, const char* fingerName) {
	if (!bodies.valid[slot]) {
		return;
	}
	const NiPoint3 axis = bodies.rot[slot] * NiPoint3{0.0f, 1.0f, 0.0f};
	const NiAVObject* const finger = game::FindFirstPersonNode(fingerName);
	if (finger == nullptr) {
		OBVR_LOG("Measure: %s - no node \"%s\"", slot == static_cast<int>(game::HandBodySlot::LeftHand) ? "left hand" : "right hand",
		         fingerName);
		return;
	}
	const game::AlongAxis a = game::MeasureAlongAxis(bodies.pos[slot], axis, finger->worldTransform.pos);
	OBVR_LOG("Measure: %s - \"%s\" (the last joint, the tip beyond it) %.1f units ahead of the grip, %.1f aside; "
	         "the constant capsule would reach %.1f ahead (%.1f + %.1f round); the body is built from the bones",
	         slot == static_cast<int>(game::HandBodySlot::LeftHand) ? "left hand" : "right hand", fingerName,
	         static_cast<double>(a.ahead), static_cast<double>(a.aside),
	         static_cast<double>(game::kHandBodyAheadUnits + game::kHandBodyRadiusUnits),
	         static_cast<double>(game::kHandBodyAheadUnits), static_cast<double>(game::kHandBodyRadiusUnits));
}

void LogBladeGeometry(const game::HandBodyFrame& bodies) {
	const int w = static_cast<int>(game::HandBodySlot::Weapon);
	const NiAVObject* const node = game::FindFirstPersonNode("Weapon");
	if (!bodies.valid[w] || node == nullptr) {
		return;
	}
	const NiPoint3 axis = bodies.rot[w] * NiPoint3{0.0f, 1.0f, 0.0f};
	const NiPoint3 attach = node->worldTransform.pos;
	const NiPoint3 centre = node->worldBound.center;
	const float radius = node->worldBound.radius;
	NiPoint3 along = centre - attach;
	const float alongLength = math::Sqrt(along.LengthSquared());
	if (alongLength > 0.001f) {
		along = along * (1.0f / alongLength);
	}
	// Derived: the far end of the bound along the line from the attach point
	// through its centre - the drawn tip, if the bound is round the blade.
	const NiPoint3 drawnTip = centre + along * radius;
	const game::AlongAxis tip = game::MeasureAlongAxis(bodies.pos[w], axis, drawnTip);
	const game::AlongAxis att = game::MeasureAlongAxis(bodies.pos[w], axis, attach);
	const float cosAngle = axis.x * along.x + axis.y * along.y + axis.z * along.z;
	OBVR_LOG("Measure: blade - the body %.1f units from the grip; the drawn weapon: attach %.1f ahead %.1f aside, "
	         "bound %.1f round, its far end %.1f ahead %.1f aside, %.0f degrees off the body's axis",
	         static_cast<double>(bodies.bladeUnits), static_cast<double>(att.ahead), static_cast<double>(att.aside),
	         static_cast<double>(radius), static_cast<double>(tip.ahead), static_cast<double>(tip.aside),
	         static_cast<double>(std::acos(cosAngle < -1.0f ? -1.0f : cosAngle > 1.0f ? 1.0f : cosAngle) *
	                             math::kRadiansToDegrees));
	// Which of the drawn weapon's own axes the blade runs along: each column
	// of its world rotation against the line to the bound's centre and
	// against the body's axis (cosines).
	const NiMatrix33& r = node->worldTransform.rot;
	for (int c = 0; c < 3; ++c) {
		const NiPoint3 col{r.data[0][c], r.data[1][c], r.data[2][c]};
		OBVR_LOG("Measure: blade - the weapon node's axis %c: %.2f with the line to the bound's centre, %.2f with "
		         "the body's axis",
		         "xyz"[c], static_cast<double>(col.x * along.x + col.y * along.y + col.z * along.z),
		         static_cast<double>(col.x * axis.x + col.y * axis.y + col.z * axis.z));
	}
}

void MeasureBodies(const game::HandBodyFrame& bodies, bool blade, bool marked) {
	const int w = static_cast<int>(game::HandBodySlot::Weapon);
	if (blade && bodies.valid[w]) {
		const NiPoint3 tip = bodies.pos[w] + (bodies.rot[w] * NiPoint3{0.0f, 1.0f, 0.0f}) * bodies.bladeUnits;
		if (g_bladeTipBeforeValid) {
			const float step =
				game::TravelPerStep(math::Sqrt((tip - g_bladeTipBefore).LengthSquared()), game::HandBodyPhysicsSteps());
			if (step > g_bladeTipFastest) {
				g_bladeTipFastest = step;
			}
			// A swing is over when the tip is nearly still again: its fastest step.
			if (step < 1.0f && g_bladeTipFastest >= 2.0f) {
				if (g_bladeSwingLinesLeft > 0) {
					--g_bladeSwingLinesLeft;
					OBVR_LOG("Measure: a swing - the blade's tip moved up to %.1f units in one physics step; a "
					         "1-unit plate is passed at %.1f, a 4-unit cup at %.1f",
					         static_cast<double>(g_bladeTipFastest),
					         static_cast<double>(game::PassThroughTravelUnits(1.0f, game::kBladeBodyRadiusUnits)),
					         static_cast<double>(game::PassThroughTravelUnits(4.0f, game::kBladeBodyRadiusUnits)));
				}
				g_bladeTipFastest = 0.0f;
			}
		}
		g_bladeTipBefore = tip;
		g_bladeTipBeforeValid = true;
	} else {
		g_bladeTipBeforeValid = false;
		g_bladeTipFastest = 0.0f;
	}
	const bool anyValid = bodies.valid[0] || bodies.valid[1] || bodies.valid[2];
	if (marked || (anyValid && g_bodyGeometryLinesLeft > 0)) {
		if (!marked) {
			--g_bodyGeometryLinesLeft;
		}
		LogHandGeometry(bodies, static_cast<int>(game::HandBodySlot::RightHand), "Bip01 R Hand");
		LogHandGeometry(bodies, static_cast<int>(game::HandBodySlot::RightHand), "Bip01 R Finger2");
		LogHandGeometry(bodies, static_cast<int>(game::HandBodySlot::RightHand), "Bip01 R Finger21");
		LogHandGeometry(bodies, static_cast<int>(game::HandBodySlot::RightHand), "Bip01 R Finger22");
		LogHandGeometry(bodies, static_cast<int>(game::HandBodySlot::LeftHand), "Bip01 L Finger22");
		if (blade) {
			LogBladeGeometry(bodies);
		}
		if (marked) {
			const game::PlayerBodyReading body = game::ReadPlayerBody();
			if (body.valid) {
				OBVR_LOG("Measure: the player's body - the game's radius %.1f, asked %.1f, the capsule now %.1f, slot %u",
				         static_cast<double>(body.gameRadiusUnits), static_cast<double>(body.targetRadiusUnits),
				         static_cast<double>(body.capsuleRadiusUnits), body.slot);
			}
		}
		if (marked) {
			NiPoint3 feet{0.0f, 0.0f, 0.0f};
			if (game::ReadPlayerFeet(feet)) {
				game::MeasureNearbyShapes(feet, 600.0f, 16);
			}
		}
	}
}
bool g_holsterFitRequested = false;
ui::CanvasOverlay g_holsterFitLayer("obvr.holsterfit", "OBVR Weapon Places", ui::kGuidePanelWidth,
                                    ui::kGuidePanelHeight);
ui::GuidePanelText g_holsterFitText;
UInt32 g_holsterFitRevision = 1;
vr::openvr::HmdMatrix34 g_holsterFitPose{};
bool g_holsterFitWasLeftB = false;
bool g_holsterFitWasRightB = false;

vr::QuickMenuState g_quickMenu;

// Stowing a held item at the body (vr::StepStow).
vr::StowState g_stow;
// The spot at the chest an item is let go in (ui::PaintStowSpot).
ui::CanvasOverlay g_stowSpotLayer("obvr.stowspot", "OBVR Stow Spot", ui::kStowSpotCanvas,
                                  ui::kStowSpotCanvas);
ui::StowSpotView g_stowSpotView;
UInt32 g_stowSpotRevision = 1;

// The spot's pose: at its point, its face turned to the eyes, upright.
vr::openvr::HmdMatrix34 StowSpotPose(const NiPoint3& at, const NiPoint3& eyes) {
	NiPoint3 z = eyes - at;
	float length = math::Sqrt(z.LengthSquared());
	z = length > 1e-4f ? z * (1.0f / length) : NiPoint3{0.0f, 0.0f, 1.0f};
	// x = up cross z, level; y = z cross x.
	NiPoint3 x{z.z, 0.0f, -z.x};
	length = math::Sqrt(x.LengthSquared());
	x = length > 1e-4f ? x * (1.0f / length) : NiPoint3{1.0f, 0.0f, 0.0f};
	const NiPoint3 y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};
	vr::openvr::HmdMatrix34 pose{};
	const NiPoint3 axes[3] = {x, y, z};
	for (int column = 0; column < 3; ++column) {
		pose.m[0][column] = axes[column].x;
		pose.m[1][column] = axes[column].y;
		pose.m[2][column] = axes[column].z;
	}
	pose.m[0][3] = at.x;
	pose.m[1][3] = at.y;
	pose.m[2][3] = at.z;
	return pose;
}
bool g_stowWasAtBody = false;
UInt32 g_stowLinesLeft = 40;
// The activate button kept from the game over a loose item
// ([Hands] TakeOnlyByHand), said the first several times.
bool g_activateWasWithheld = false;
UInt32 g_activateWithheldLinesLeft = 12;
ui::CanvasOverlay g_quickMenuLayer("obvr.quickmenu", "OBVR Quick Menu", ui::kQuickMenuCanvas,
                                   ui::kQuickMenuCanvas);
ui::QuickMenuView g_quickMenuView;
// Each slot's icon path, the way the view's icon was asked for.
char g_quickMenuIconPaths[game::kQuickKeyCount][128] = {};
static_assert(ui::kQuickMenuIconSide == game::kItemIconSide, "the ring draws the icons one to one");
static_assert(vr::kQuickMenuMaxPages == game::kQuickKeyPagesMax, "one page count for the ring and the store");
// The weapon hand's trigger as read, and whether the ring keeps it.
bool g_quickMenuTrigger = false;
bool g_quickMenuKeepsTrigger = false;
UInt32 g_quickMenuRevision = 1;
vr::openvr::HmdMatrix34 g_quickMenuPose{};
UInt32 g_quickMenuLinesLeft = 30;

// The ring upright where the hand opened it, facing the head: its right the
// ring's right, its up tracking up, its face towards the eyes; a few
// centimetres ahead of the hand so the controller does not hide the middle.
vr::openvr::HmdMatrix34 QuickMenuPose(const vr::QuickMenuState& s) {
	const NiPoint3 toHead{-s.right.z, 0.0f, s.right.x};
	const NiPoint3 at = s.anchor - toHead * 0.03f;
	vr::openvr::HmdMatrix34 pose{};
	const NiPoint3 axes[3] = {s.right, s.up, toHead};
	for (int column = 0; column < 3; ++column) {
		pose.m[0][column] = axes[column].x;
		pose.m[1][column] = axes[column].y;
		pose.m[2][column] = axes[column].z;
	}
	pose.m[0][3] = at.x;
	pose.m[1][3] = at.y;
	pose.m[2][3] = at.z;
	return pose;
}

// `assign`: the cursor is on the inventory or the magic menu, where the ring
// sets a hotkey to what the cursor is on (docs/controls-spec.md 4.5).
void UpdateQuickMenu(const Config& config, vr::OpenVRBackend& backend, bool allowed, bool assign,
                     const vr::HandModeFrame& frame, float dt) {
	vr::QuickMenuInput in;
	in.pad = frame.right.valid && vr::TrackpadClickDown(frame.right.buttonsPressed);
	in.allowed = allowed;
	in.assign = assign;
	in.hand = frame.right.position;
	in.headRight = frame.headValid ? vr::ToMatrix(frame.head) * NiPoint3{1.0f, 0.0f, 0.0f}
	                               : NiPoint3{1.0f, 0.0f, 0.0f};
	in.dt = dt;
	in.trigger = g_quickMenuTrigger;
	// The pages: several with the co-save to keep them in, the game's eight
	// alone without. A ring opening on a page past the count (the count
	// lowered since) turns to the first.
	const int pageCount =
		game::QuickKeyPagesAvailable() ? vr::QuickMenuPageCount(config.hands.quickMenu.pages) : 1;
	if (in.pad && !g_quickMenu.open && allowed && game::CurrentQuickKeyPage() >= pageCount) {
		game::TurnQuickKeyPage(pageCount, true);
	}
	// The hotkeys are read while the trackpad is down or the ring is open:
	// what the ring shows and what a release uses is what they hold now.
	if (in.pad || g_quickMenu.open) {
		game::QuickKeySlot slots[game::kQuickKeyCount];
		game::ReadQuickKeys(slots);
		for (int i = 0; i < game::kQuickKeyCount; ++i) {
			in.filled[i] = slots[i].filled;
			const char* const iconPath = config.hands.quickMenu.icons ? slots[i].iconPath : "";
			if (g_quickMenuView.filled[i] != slots[i].filled ||
			    std::strcmp(g_quickMenuView.names[i], slots[i].name) != 0 ||
			    std::strcmp(g_quickMenuIconPaths[i], iconPath) != 0) {
				g_quickMenuView.filled[i] = slots[i].filled;
				strncpy_s(g_quickMenuView.names[i], slots[i].name, _TRUNCATE);
				strncpy_s(g_quickMenuIconPaths[i], iconPath, _TRUNCATE);
				++g_quickMenuRevision;
			}
		}
		// Asked for every time: the cache keeps what was asked for last, so
		// the eight shown are never the ones it lets go (game/ItemIcons.h).
		for (int i = 0; i < game::kQuickKeyCount; ++i) {
			const render::Pixel* const icon =
				g_quickMenuView.filled[i] ? game::ItemIcon(g_quickMenuIconPaths[i]) : nullptr;
			if (icon != g_quickMenuView.icons[i]) {
				g_quickMenuView.icons[i] = icon;
				++g_quickMenuRevision;
			}
		}
	}
	vr::QuickMenuSettings settings = config.hands.quickMenu;
	settings.pages = pageCount;
	const vr::QuickMenuVerdict v = vr::StepQuickMenu(g_quickMenu, in, settings);
	if (v.turnPage) {
		// The game's eight are written now; the next frame's read shows them.
		game::TurnQuickKeyPage(pageCount);
	}
	const int page = game::CurrentQuickKeyPage();
	if (page != g_quickMenuView.page || pageCount != g_quickMenuView.pageCount) {
		g_quickMenuView.page = page;
		g_quickMenuView.pageCount = pageCount;
		++g_quickMenuRevision;
	}
	g_hand.controls.quickKey = static_cast<UInt8>(v.key);
	g_hand.controls.menuClick = g_hand.controls.menuClick || v.click;
	if (v.holdsCursor) {
		// The click has to land on what the laser was on when the trackpad
		// went down, not where the hand has moved the laser to reach a slot.
		g_hand.cursorDx = 0;
		g_hand.cursorDy = 0;
	}
	if (v.highlighted != g_quickMenuView.highlighted || v.assigning != g_quickMenuView.assigning) {
		g_quickMenuView.highlighted = v.highlighted;
		g_quickMenuView.assigning = v.assigning;
		++g_quickMenuRevision;
	}
	if (v.assigned >= 0) {
		OBVR_LOG("QuickMenu: setting hotkey %d to what the cursor is on in the %s menu (%s) - "
		         "its number key held, then a click",
		         v.assigned + 1, game::MenuIdName(game::ActiveMenuId()),
		         g_quickMenuView.filled[v.assigned] ? g_quickMenuView.names[v.assigned]
		                                            : "empty until now");
	}
	if (v.assignDone || v.assignAborted) {
		game::QuickKeySlot slots[game::kQuickKeyCount];
		game::ReadQuickKeys(slots);
		char list[400] = "";
		for (int i = 0; i < game::kQuickKeyCount; ++i) {
			char one[56];
			std::snprintf(one, sizeof(one), "%s%d %s", i == 0 ? "" : ", ", i + 1,
			              slots[i].filled ? (slots[i].name[0] != '\0' ? slots[i].name : "(no name)")
			                              : "-");
			strncat_s(list, one, _TRUNCATE);
		}
		OBVR_LOG("QuickMenu: setting a hotkey %s - the hotkeys now: %s",
		         v.assignDone ? "done" : "stopped, the menu went", list);
	}
	if (v.opened) {
		g_quickMenuPose = QuickMenuPose(g_quickMenu);
		if (g_quickMenuLinesLeft > 0) {
			--g_quickMenuLinesLeft;
			char list[400] = "";
			for (int i = 0; i < game::kQuickKeyCount; ++i) {
				char one[56];
				std::snprintf(one, sizeof(one), "%s%d %s", i == 0 ? "" : ", ", i + 1,
				              g_quickMenuView.filled[i]
				                  ? (g_quickMenuView.names[i][0] != '\0' ? g_quickMenuView.names[i]
				                                                          : "(no name)")
				                  : "-");
				strncat_s(list, one, _TRUNCATE);
			}
			OBVR_LOG("QuickMenu: opened at the hand (%.2f %.2f %.2f) - %s",
			         static_cast<double>(g_quickMenu.anchor.x),
			         static_cast<double>(g_quickMenu.anchor.y),
			         static_cast<double>(g_quickMenu.anchor.z), list);
			char raw[400];
			game::DescribeQuickKeyLists(raw, sizeof(raw));
			OBVR_LOG("QuickMenu: the lists (start node/count->form) %s", raw);
		}
	}
	if ((v.used >= 0 || v.cancelled) && g_quickMenuLinesLeft > 0) {
		--g_quickMenuLinesLeft;
		if (v.used >= 0) {
			OBVR_LOG("QuickMenu: hotkey %d used (%s) - its number key tapped", v.used + 1,
			         g_quickMenuView.names[v.used]);
		} else {
			OBVR_LOG("QuickMenu: closed without a hotkey");
		}
	}
	if (v.visible) {
		g_quickMenuLayer.Show(backend, render::GetGameDevice(), g_quickMenuPose,
		                      ui::QuickMenuWidthMetres(config.hands.quickMenu.ringMetres),
		                      g_quickMenuRevision, ui::PaintQuickMenuFor, &g_quickMenuView);
	} else {
		g_quickMenuLayer.Hide(backend);
	}
	// For the test runner: the ring as painted, at each mark it is up for.
	if (test::HandScriptMarkedThisFrame() && g_quickMenuLayer.IsVisible()) {
		char name[96];
		char path[512];
		std::snprintf(name, sizeof(name), "OBVR-QuickMenu-%s.bmp", test::HandScriptMarkName());
		if (platform::BuildGamePath(name, path, sizeof(path)) && g_quickMenuLayer.SaveBmp(path)) {
			OBVR_LOG("HandScript: the quick menu's picture saved as %s", name);
		}
	}
}

// Steps the fit of the weapon places. True while it runs: the triggers and
// menu buttons are its, and nothing else of the hands reaches the game.
bool UpdateHolsterFit(Config& config, vr::OpenVRBackend& backend, const vr::HandModeFrame& frame,
                      bool allowed) {
	vr::HolsterFitInput in;
	if (test::TakeHandScriptAction("holster_fit") || game::TakeHolsterFitRequest()) {
		g_holsterFitRequested = true;
	}
	// Asked for from the settings menu: it starts once that has closed and
	// the player stands in the world.
	in.start = g_holsterFitRequested && allowed;
	if (in.start) {
		g_holsterFitRequested = false;
	}
	in.weaponValid = frame.right.valid && frame.headValid;
	in.otherValid = frame.left.valid && frame.headValid;
	if (frame.headValid) {
		in.weaponRelative = vr::BodyRelative(frame.head, frame.headPosition, frame.right.position);
		in.otherRelative = vr::BodyRelative(frame.head, frame.headPosition, frame.left.position);
	}
	in.weaponTrigger = frame.right.valid && frame.right.trigger > 0.5f;
	in.otherTrigger = frame.left.valid && frame.left.trigger > 0.5f;
	const bool leftB = frame.left.valid && vr::ButtonBDown(frame.left.buttonsPressed);
	const bool rightB = frame.right.valid && vr::ButtonBDown(frame.right.buttonsPressed);
	in.cancel = (leftB && !g_holsterFitWasLeftB) || (rightB && !g_holsterFitWasRightB) || !allowed;
	g_holsterFitWasLeftB = leftB;
	g_holsterFitWasRightB = rightB;
	in.leftHanded = config.hands.leftHanded;
	const vr::HolsterFitVerdict v = vr::StepHolsterFit(g_holsterFit, in);

	if (v.stepChanged) {
		++g_holsterFitRevision;
		g_holsterFitText = ui::GuidePanelText{};
		g_holsterFitText.title = "Weapon places";
		if (v.step == vr::HolsterFitStep::OneHand) {
			g_holsterFitText.lines[0] = "1. One-handed weapons: hold the weapon hand";
			g_holsterFitText.lines[1] = "   where they hang, pull its trigger.";
			g_holsterFitText.lines[3] = "A menu button cancels.";
			vr::openvr::HmdMatrix34 head{};
			if (backend.GetRenderPoseMatrix(head)) {
				vr::LevelPose(head);
				g_holsterFitPose = vr::OverlayPoseAhead(head, 0.8f);
			}
			OBVR_LOG("Holster fit: started - the one-handed place first");
		} else if (v.step == vr::HolsterFitStep::TwoHand) {
			g_holsterFitText.lines[0] = "2. Two-handed weapons and staffs: the weapon";
			g_holsterFitText.lines[1] = "   hand where they sit, pull its trigger.";
			g_holsterFitText.lines[3] = "A menu button cancels.";
			OBVR_LOG("Holster fit: the one-handed place taken - now the two-handed one");
		} else if (v.step == vr::HolsterFitStep::Bow) {
			g_holsterFitText.lines[0] = "3. Your bow: hold the other hand";
			g_holsterFitText.lines[1] = "   where it should be, pull its trigger.";
			g_holsterFitText.lines[3] = "A menu button cancels.";
			OBVR_LOG("Holster fit: the two-handed place taken - now the bow's");
		}
	}
	if (v.cancelled) {
		OBVR_LOG("Holster fit: cancelled - the places stay as they were");
	}
	if (v.finished) {
		vr::HolsterSettings& h = config.hands.holster;
		h.oneHandZone = v.oneHand;
		h.twoHandZone = v.twoHand;
		h.bowZone = v.bow;
		struct Entry {
			const char* key;
			float value;
		};
		const Entry entries[] = {
			{"HolsterOneHandX", v.oneHand.x}, {"HolsterOneHandForward", v.oneHand.y},
			{"HolsterOneHandUp", v.oneHand.z}, {"HolsterTwoHandX", v.twoHand.x},
			{"HolsterTwoHandForward", v.twoHand.y}, {"HolsterTwoHandUp", v.twoHand.z},
			{"HolsterBowX", v.bow.x}, {"HolsterBowForward", v.bow.y},
			{"HolsterBowUp", v.bow.z},
		};
		bool saved = true;
		for (const Entry& entry : entries) {
			char value[32];
			std::snprintf(value, sizeof(value), "%.2f", static_cast<double>(entry.value));
			saved = SaveSetting("Hands", entry.key, value) && saved;
		}
		OBVR_LOG("Holster fit: done - one-handed at %.2f %.2f %.2f, two-handed at %.2f %.2f %.2f, "
		         "bow at %.2f %.2f %.2f (right, forward, up from the eyes)%s",
		         static_cast<double>(v.oneHand.x), static_cast<double>(v.oneHand.y),
		         static_cast<double>(v.oneHand.z), static_cast<double>(v.twoHand.x),
		         static_cast<double>(v.twoHand.y), static_cast<double>(v.twoHand.z),
		         static_cast<double>(v.bow.x), static_cast<double>(v.bow.y),
		         static_cast<double>(v.bow.z), saved ? "" : " - COULD NOT SAVE the INI");
	}
	if (v.active) {
		g_holsterFitLayer.Show(backend, render::GetGameDevice(), g_holsterFitPose, 0.6f,
		                       g_holsterFitRevision, ui::PaintGuidePanelFor, &g_holsterFitText);
		if (test::HandScriptMarkedThisFrame()) {
			char name[96];
			char path[512];
			std::snprintf(name, sizeof(name), "OBVR-HolsterFit-%s.bmp", test::HandScriptMarkName());
			if (platform::BuildGamePath(name, path, sizeof(path)) && g_holsterFitLayer.SaveBmp(path)) {
				OBVR_LOG("HandScript: the weapon places panel saved as %s", name);
			}
		}
	} else {
		g_holsterFitLayer.Hide(backend);
	}
	return v.active || v.finished || v.cancelled;
}

// Placing the stow spot (vr/StowPlace.h): stepped every frame of the hand
// mode, the window's commands taken as they come. While it is on - its
// window open, so a menu is up - the ring shows at the spot, lit while a grip
// drags it, and this owns the ring's overlay.
vr::StowPlaceState g_stowPlace;

void UpdateStowPlacing(Config& config, vr::OpenVRBackend& backend, const vr::HandModeFrame& frame) {
	vr::StowPlaceInput in;
	in.command = static_cast<vr::StowPlaceCommand>(game::TakeStowPlaceCommand());
	in.rightValid = frame.right.valid && frame.headValid;
	in.leftValid = frame.left.valid && frame.headValid;
	if (frame.headValid) {
		in.right = vr::BodyRelative(frame.head, frame.headPosition, frame.right.position);
		in.left = vr::BodyRelative(frame.head, frame.headPosition, frame.left.position);
	}
	in.rightGrip = frame.right.valid && vr::GripDown(frame.right.buttonsPressed);
	in.leftGrip = frame.left.valid && vr::GripDown(frame.left.buttonsPressed);
	vr::StowSettings& stow = config.hands.stow;
	const vr::StowPlaceVerdict v = vr::StepStowPlace(g_stowPlace, in, stow);
	game::NoteStowPlaceActive(v.active);
	if (in.command == vr::StowPlaceCommand::Start) {
		OBVR_LOG("Stow place: started - the ring at %.2f right, %.2f forward, %.2f up",
		         static_cast<double>(stow.centreRight), static_cast<double>(stow.centreForward),
		         static_cast<double>(stow.centreUp));
	}
	if (v.grabbed || v.dropped) {
		OBVR_LOG("Stow place: the ring %s at %.2f right, %.2f forward, %.2f up",
		         v.grabbed ? "taken" : "let go", static_cast<double>(v.spot.x),
		         static_cast<double>(v.spot.y), static_cast<double>(v.spot.z));
	}
	if (v.active) {
		// The running settings follow the ring, so it is drawn - and would
		// stow - where it is being put.
		stow.centreRight = v.spot.x;
		stow.centreForward = v.spot.y;
		stow.centreUp = v.spot.z;
	}
	if (v.save || v.restore) {
		stow.centreRight = v.spot.x;
		stow.centreForward = v.spot.y;
		stow.centreUp = v.spot.z;
	}
	if (v.save) {
		const struct {
			const char* key;
			float value;
		} entries[] = {{"StowRight", v.spot.x}, {"StowForward", v.spot.y}, {"StowUp", v.spot.z}};
		bool saved = true;
		for (const auto& entry : entries) {
			char value[32];
			std::snprintf(value, sizeof(value), "%.2f", static_cast<double>(entry.value));
			saved = SaveSetting("Hands", entry.key, value) && saved;
		}
		OBVR_LOG("Stow place: kept at %.2f right, %.2f forward, %.2f up - the ring hidden again%s",
		         static_cast<double>(v.spot.x), static_cast<double>(v.spot.y),
		         static_cast<double>(v.spot.z), saved ? "" : " - COULD NOT SAVE the INI");
	} else if (v.restore) {
		OBVR_LOG("Stow place: cancelled - the spot back at %.2f right, %.2f forward, %.2f up",
		         static_cast<double>(v.spot.x), static_cast<double>(v.spot.y),
		         static_cast<double>(v.spot.z));
	}
	if (v.active && frame.headValid) {
		if (g_stowSpotView.lit != v.dragging) {
			g_stowSpotView.lit = v.dragging;
			++g_stowSpotRevision;
		}
		const NiPoint3 at = vr::StowSpotInTracking(frame.head, frame.headPosition, stow);
		g_stowSpotLayer.Show(backend, render::GetGameDevice(), StowSpotPose(at, frame.headPosition),
		                     2.0f * stow.radius, g_stowSpotRevision, ui::PaintStowSpotFor,
		                     &g_stowSpotView);
	} else if (v.save || v.restore) {
		g_stowSpotLayer.Hide(backend);
	}
}

void UpdateHandMode(const Config& config, bool menuIsUp) {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long now = ReadPerformanceCounter();
	float dt = 0.0f;
	if (g_handClockLast != 0 && ticksPerSecond > 0) {
		dt = static_cast<float>(static_cast<double>(now - g_handClockLast) /
		                        static_cast<double>(ticksPerSecond));
		if (dt > 0.25f) {
			dt = 0.25f;
		}
	}
	g_handClockLast = now;

	// The hand script's clock ([Debug] HandScript, test/HandScript.h), ahead
	// of everything that reads the controllers.
	test::StepHandScriptFrame(dt, game::PlayerInWorld(), menuIsUp,
	                          g_headTracker.GetBackendForFrame());

	// The whole mode, or - with it off - the controllers on the menus alone:
	// the laser at the game's menus and the sticks in OBVR's own, so a seated
	// player and the walkthrough on the very first start can be steered from
	// the controllers before the mode is ever switched on.
	const bool headset = g_headTracker.IsHeadsetConnected();
	const bool active = config.fullVrMode && headset;
	const bool menusOnly = !active && headset && config.hands.controllerMenus;
	if (!active && !menusOnly) {
		if (test::HandScriptMarkedThisFrame()) {
			OBVR_LOG("HandScript: state - the hand mode is not running (Hands.Enabled %d, "
			         "headset %d)",
			         config.fullVrMode ? 1 : 0, headset ? 1 : 0);
		}
		if (g_handControlsHeld) {
			game::ReleaseHandControls(config.handKeys);
			g_handControlsHeld = false;
		}
		g_hudLayer.ClearWristPlacement();
		game::HideFirstPersonNodes(false, "");
		game::RestoreHandBoneScales();
		game::KeepFirstPersonDepth(false);
		render::SetFirstPersonBackfaces(false, 0.0f, false);
		game::AllowGrabNearBody(false);
		g_headsetRenderer.SetControllersWanted(false);
		game::ForgetStrikes();
		game::SetMenuCursorHidden(false);
		game::StepGrabPhysics(false, 0.0f, false, NiPoint3{0.0f, 0.0f, 0.0f}, g_deltaSeconds, false);
		game::StepHandBodies(game::HandBodyFrame{});  // out of the world
		g_hand = vr::HandModeResult{};
		g_reachIconShown = false;
		g_nearItem = game::NearItem{};
		g_handMode.Reset();
		g_quickMenu = vr::QuickMenuState{};
		g_stow = vr::StowState{};
		g_quickMenuLayer.Hide(g_headTracker.GetBackendForFrame());
		g_stowSpotLayer.Hide(g_headTracker.GetBackendForFrame());
		UpdateTeleport(config, g_headTracker.GetBackendForFrame(), false, menuIsUp, dt);
		return;
	}

	if (active) {
		// Once: whether the inventory drop can work with the game's bindings.
		game::CheckDropBinding(config.handKeys.run);
		// The hands are only drawn in first person, so the mode keeps the
		// player there through the game's own switch - the one the dialogue
		// shim uses. Not while a menu is up: the game flips to third person
		// for the race menu and the like on purpose, and would be fought
		// every frame.
		// Nor when the player has died: the game shows the death in third
		// person, the body falling, and forcing first person back showed the
		// character standing (2026-09-25 log: "switched to third person" at the
		// death, "put back into first" thirteen lines later).
		if (HandModeForcesFirstPerson(config.hands.forceFirstPerson, menuIsUp,
		                              ReadIsThirdPerson(), game::PlayerIsDead())) {
			auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
			if (mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(player))) {
				using ToggleCameraFn = void(__fastcall*)(UInt8* self, void* edx, UInt8 firstPerson);
				reinterpret_cast<ToggleCameraFn>(addr::kToggleCamera)(player, nullptr, 1);
				if (g_handPovLinesLeft > 0) {
					--g_handPovLinesLeft;
					OBVR_LOG("Hands: the player was in third person - put back into first");
				}
			}
		}

		if (config.firstPersonTreeProbe) {
			game::ProbeFirstPersonTree();
		}
		// In a menu the hands are not pinned (the arms placement stands down
		// there), and with the game paused nothing animates them either: they
		// stayed where the controllers last put them, two hands hanging in the
		// room through a dialogue (2026-09-25). So in a menu the hand meshes
		// ("Hand", hand.nif's shape) are hidden along with the arms.
		// And from the conversation's first frame, not its menu's: see
		// game::StepDialogApproach.
		static game::DialogApproachState s_approach;
		g_handsAwayForDialog =
			game::StepDialogApproach(s_approach, game::TakeDialogCameraCall(), menuIsUp);
		const bool handsAway = g_handsAwayForDialog || menuIsUp;
		g_stumpHandsAway = handsAway;
		// The sheaths: a weapon's scabbard is its own node, "Scb", which the
		// engine hangs on the skeleton's side-weapon bone (cs.uesp.net,
		// NifSkope Comprehensive Guide, "Scabbards"), and a sheathed weapon
		// hangs there with it. In the first-person skeleton that bone moved
		// with the right hand, so the scabbard floated in the room beside it
		// (2026-09-25). The bones a sheathed weapon, a bow or a quiver hang on
		// are hidden with everything under them; "Scb" is named too, for a
		// scabbard hung anywhere else.
		// The weapon gone from the hand while it is sheathed: the engine's
		// animation carries it away from the controller (ComposeHandsHideList).
		const bool sheathing = game::ReadPlayerAction() == vr::kPlayerActionUnequipWeapon;
		game::HideFirstPersonNodes(!ReadIsThirdPerson(),
		                           HandsHideList(config.hands, handsAway, sheathing, g_stumpShown && !handsAway));
		// The hands in the world rather than on top of it (FirstPersonDepth.h).
		game::KeepFirstPersonDepth(true);
		// And closed: their inside their own surface, darker - not the room
		// (BackfacePass.h).
		render::SetFirstPersonBackfaces(config.hands.closedHands, config.hands.closedHandsBrightness,
		                                config.hands.closedHands && game::FirstPersonHandsBare());
		if (test::HandScriptMarkedThisFrame()) {
			render::TraceFirstPersonBackfaces(16);
		}
		// Held objects may come up to the mouth and the body (GrabNearBody.h).
		game::AllowGrabNearBody(true);
		// The real controllers in the eyes while the hands are being adjusted.
		g_headsetRenderer.SetControllersWanted(
			(config.hands.adjustHands || game::HandAdjustActive()) && !menuIsUp);
	} else {
		game::HideFirstPersonNodes(false, "");
		game::RestoreHandBoneScales();
		game::KeepFirstPersonDepth(false);
		render::SetFirstPersonBackfaces(false, 0.0f, false);
		game::AllowGrabNearBody(false);
		g_headsetRenderer.SetControllersWanted(false);
		game::ForgetStrikes();
	}

	vr::OpenVRBackend& backend = g_headTracker.GetBackendForFrame();
	vr::HandModeFrame frame;
	frame.dtSeconds = dt;
	frame.menuMode = menuIsUp;
	frame.restMenuUp = menuIsUp && game::TopVisibleMenu() == game::kMenuIdSleepWait;
	frame.settingsMenuOpen = g_settingsMenu.IsOpen() || g_onboarding.IsOpen();
	frame.firstPerson = !ReadIsThirdPerson();
	frame.meleeInHand = active && config.hands.motionHits && game::MeleeInHand(nullptr);
	frame.menusOnly = menusOnly;
	frame.inWorld = game::PlayerInWorld();
	frame.adjustingHands = config.hands.adjustHands || game::HandAdjustActive();
	// The weapon and the player's action: what the ready-weapon click follows
	// and whether a swing may attack. World frames only, like the sneak.
	if (!menuIsUp && frame.inWorld) {
		const game::WeaponState weapon = game::ReadPlayerWeaponState();
		frame.weaponSeen = weapon == game::WeaponState::Drawn      ? vr::WeaponSeen::Drawn
		                   : weapon == game::WeaponState::Sheathed ? vr::WeaponSeen::Sheathed
		                                                           : vr::WeaponSeen::Unknown;
		frame.playerAction = game::ReadPlayerAction();
	}
	// The weapon slot, and the last weapon of each kind seen in it: what a
	// reach to the hip or a shoulder draws when another kind is in hand. In
	// menus too, so a weapon picked in the inventory counts; only with the
	// player in the world.
	{
		SInt32 weaponType = 0;
		UInt8* const weaponForm = frame.inWorld ? game::EquippedWeaponForm(&weaponType) : nullptr;
		frame.equipped =
			weaponForm == nullptr ? vr::EquippedKind::Nothing : vr::KindOfWeaponType(weaponType);
		if (weaponForm != nullptr && frame.equipped != vr::EquippedKind::Nothing) {
			LastFormOf(frame.equipped) = weaponForm;
		}
	}
	frame.haveOneHand = g_lastOneHandForm != nullptr;
	frame.haveTwoHand = g_lastTwoHandForm != nullptr;
	frame.haveBow = g_lastBowForm != nullptr;
	frame.holdingObject = active && game::PlayerHoldsGrab();
	frame.sneaking = active && !menuIsUp && frame.inWorld && config.hands.sneakHold &&
	                 game::IsPlayerSneaking();
	frame.headValid = backend.GetRenderPose(frame.head, frame.headPosition) ||
	                  backend.ReadHeadPose(frame.head, frame.headPosition);
	frame.reference = g_headTracker.GetReference();
	backend.ReadHand(true, frame.right);
	backend.ReadHand(false, frame.left);
	// Or the hand script's ([Debug] HandScript): controllers played from a
	// file, placed from this frame's head.
	if (test::HandScriptDrivesHands()) {
		test::ScriptedHands(frame.headValid ? frame.head : vr::Quaternion::Identity(),
		                    frame.headValid ? frame.headPosition : NiPoint3{0.0f, 1.6f, 0.0f},
		                    frame.right, frame.left);
	}
	// Left-handed in Full VR the controllers swap roles as a whole: the
	// weapon hand is the left controller (vr::AssignHandRoles).
	g_handRolesSwapped =
		vr::AssignHandRoles(frame.right, frame.left, active && config.hands.leftHanded);
	// The weapon hand's trigger turns the quick menu's pages while the ring
	// is up, and is nobody else's then (vr::QuickMenuKeepsTrigger).
	g_quickMenuTrigger = frame.right.valid && frame.right.trigger > 0.5f;
	if (vr::QuickMenuKeepsTrigger(g_quickMenuKeepsTrigger, g_quickMenu.open, g_quickMenuTrigger)) {
		frame.right.trigger = 0.0f;
	}
	frame.unitsPerMetre = config.tracker.unitsPerMetre;
	g_hudLayer.ShownPixels(frame.layerPixelsWidth, frame.layerPixelsHeight);
	frame.cursorValid = game::InterfaceCursorPosition(frame.cursorX, frame.cursorY);
	if (menuIsUp) {
		char tileName[48];
		frame.cursorOnScrollBar = game::CursorOverScrollBar(tileName, sizeof(tileName));
		frame.menuIsDragSurface = game::ActiveMenuId() == game::kMenuIdMap ||
		                          game::TopVisibleMenu() == game::kMenuIdMap;
		// Which tile a pull lands on, the first several times: the scroll bar
		// is recognised by its name, and the log is where a name that is
		// missed shows up.
		static UInt32 pullLinesLeft = 10;
		static bool wasPulled = false;
		const bool pulled = (frame.right.valid && frame.right.trigger > 0.5f) ||
		                    (frame.left.valid && frame.left.trigger > 0.5f);
		if (pulled && !wasPulled && pullLinesLeft > 0) {
			--pullLinesLeft;
			OBVR_LOG("Hands: pulled over tile \"%s\" in the %s menu - %s", tileName,
			         game::MenuIdName(game::ActiveMenuId()),
			         frame.cursorOnScrollBar ? "a scroll bar, the button is held"
			                                 : "not a scroll bar");
		}
		wasPulled = pulled;
	}
	// The quad the game's menus hang on when they are not on a wrist, for
	// the laser: where the layer last hung it, in tracking space.
	{
		vr::openvr::HmdMatrix34 quadPose{};
		float quadWidth = 0.0f;
		if (g_hudLayer.QuadInTracking(backend, quadPose, quadWidth)) {
			frame.menuQuad = vr::QuadFromPose(quadPose, quadWidth, frame.layerPixelsWidth,
			                                  frame.layerPixelsHeight);
		}
	}
	// And the cinema screen when the frame is a flat one - the main menu,
	// character creation, a loading screen - which has no quad: the laser
	// meets the picture the head sees at infinity instead.
	g_headsetRenderer.FlatPictureInTracking(frame.flat);
	// OBVR's own panel, whichever is open, so the laser can put the
	// highlight on the row it points at.
	if (frame.settingsMenuOpen) {
		const ui::SettingsMenuLayer& panel =
			g_onboarding.IsOpen() ? g_onboardingLayer : g_settingsMenuLayer;
		vr::openvr::HmdMatrix34 panelPose{};
		float panelWidth = 0.0f;
		UInt32 canvasWidth = 0;
		UInt32 canvasHeight = 0;
		ui::SettingsMenuLayer::CanvasSize(canvasWidth, canvasHeight);
		frame.settingsPixelsWidth = static_cast<float>(canvasWidth);
		frame.settingsPixelsHeight = static_cast<float>(canvasHeight);
		if (panel.QuadInTracking(backend, panelPose, panelWidth)) {
			frame.settingsQuad = vr::QuadFromPose(panelPose, panelWidth, frame.settingsPixelsWidth,
			                                      frame.settingsPixelsHeight);
		}
	}
	if (frame.flat.valid != g_flatLaserTargetReported) {
		g_flatLaserTargetReported = frame.flat.valid;
		if (frame.flat.valid) {
			OBVR_LOG("Hands: the laser has the flat picture to point at (%.0fx%.0f frame pixels "
			         "from %.0f,%.0f; half-extents tan %.3f x %.3f)",
			         static_cast<double>(frame.flat.pixelWidth),
			         static_cast<double>(frame.flat.pixelHeight),
			         static_cast<double>(frame.flat.pixelLeft),
			         static_cast<double>(frame.flat.pixelTop),
			         static_cast<double>(frame.flat.tanHalfWidth),
			         static_cast<double>(frame.flat.tanHalfHeight));
		}
	}

	// What the controllers report, the first few times a button mask changes:
	// which bits an Index puts its buttons on under the legacy path is what
	// the gamepad layout rests on, and a wrong bit shows here as a mask.
	if (g_handMaskLinesLeft > 0) {
		for (int side = 0; side < 2; ++side) {
			const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
			UInt64& last = side == 0 ? g_lastRightMask : g_lastLeftMask;
			if (hand.valid && hand.buttonsPressed != last) {
				last = hand.buttonsPressed;
				--g_handMaskLinesLeft;
				OBVR_LOG("Hands: %s buttons=%08X%08X trigger=%.2f stick=%.2f,%.2f (%s) grip=%.2f",
				         side == 0 ? "right" : "left",
				         static_cast<UInt32>(hand.buttonsPressed >> 32),
				         static_cast<UInt32>(hand.buttonsPressed), static_cast<double>(hand.trigger),
				         static_cast<double>(hand.thumbX), static_cast<double>(hand.thumbY),
				         hand.thumbFromJoystickAxis ? "axis 3" : "axis 0",
				         static_cast<double>(hand.gripForce));
			}
		}
	}

	// The grips and which actions SteamVR has bound, whenever either changes:
	// the 2026-09-25 evening run closed a grip on an object with nothing to
	// show for it, and the mask lines above had run out by then.
	{
		static bool s_grip[2] = {false, false};
		static UInt32 s_active[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
		for (int side = 0; side < 2; ++side) {
			const vr::HandPose& hand = side == 0 ? frame.right : frame.left;
			if (!hand.valid) {
				continue;
			}
			const bool grip = vr::GripDown(hand.buttonsPressed);
			if (grip != s_grip[side] || hand.actionActiveMask != s_active[side]) {
				s_grip[side] = grip;
				s_active[side] = hand.actionActiveMask;
				OBVR_LOG("Hands: %s grip %s, actions %s, bound mask %02X (stick click, A, B, grip, "
				         "trackpad from bit 0)",
				         side == 0 ? "right" : "left", grip ? "CLOSED" : "open",
				         hand.actionInput ? "in use" : "not in use (legacy input)",
				         hand.actionActiveMask);
			}
		}
	}

	if (frame.right.valid != g_rightHandTracked || frame.left.valid != g_leftHandTracked) {
		g_rightHandTracked = frame.right.valid;
		g_leftHandTracked = frame.left.valid;
		OBVR_LOG("Hands: right controller %s, left controller %s",
		         frame.right.valid ? "tracked" : "not tracked",
		         frame.left.valid ? "tracked" : "not tracked");
	}

	frame.teleportAllowed = active && TeleportAllowedNow(config, menuIsUp, frame.inWorld);
	frame.inventoryOpen = active && menuIsUp && game::ActiveMenuId() == game::kMenuIdInventory;
	// The fit of the weapon places: while it runs the triggers and menu
	// buttons are its (the triggers would attack and cast, B would open the
	// menus), and neither reach nor fist changes the weapon.
	const bool fitting = UpdateHolsterFit(GetConfig(), backend, frame,
	                                      active && !menuIsUp && frame.inWorld &&
	                                          !frame.settingsMenuOpen);
	UpdateStowPlacing(GetConfig(), backend, frame);
	g_hand = g_handMode.Update(frame, config.hands);
	// Taking loose items only by hand ([Hands] TakeOnlyByHand, vr::Stow): the
	// activate button is kept from the game while the laser is on one. Read
	// on the press and kept for as long as it is held.
	{
		static bool s_activateWas = false;
		const bool pressed = active && g_hand.controls.activate && !menuIsUp;
		if (pressed && !s_activateWas) {
			const game::CrosshairTarget target = game::ReadCrosshairTarget();
			bool isBook = false;
			const bool isItem = target.haveRef && game::RefIsItem(target.refAddress, &isBook);
			g_activateWasWithheld =
				vr::ActivateWithheld(config.hands.stow, target.haveRef, isItem, isBook);
			if (g_activateWasWithheld && g_activateWithheldLinesLeft > 0) {
				--g_activateWithheldLinesLeft;
				OBVR_LOG("Hands: activate kept from the game - a loose item is taken by hand here "
				         "([Hands] TakeOnlyByHand)");
			}
		} else if (!pressed) {
			g_activateWasWithheld = false;
		}
		s_activateWas = pressed;
		if (g_activateWasWithheld) {
			g_hand.controls.activate = false;
		}
	}
	if (fitting) {
		vr::HandControlsWanted& c = g_hand.controls;
		c.attack = c.cast = c.block = c.menu = c.escape = c.readyWeapon = c.grab = false;
		g_hand.holster = vr::HolsterVerdict{};
		g_hand.fist.readyClick = false;
		g_hand.fist.unequip = false;
	}
	// A reach to the hip or the shoulder with the other kind in hand: the
	// remembered weapon is equipped first (vr::StepHolster draws it once the
	// game shows it). What each reach did goes to the log.
	{
		const vr::HolsterVerdict& h = g_hand.holster;
		if (h.equip != vr::HolsterKind::None) {
			UInt8* const form = LastFormOf(vr::KindFor(h.equip));
			const bool sent = game::EquipWeaponForm(form);
			if (!sent) {
				LastFormOf(vr::KindFor(h.equip)) = nullptr;
			}
		}
		const bool unequipped = g_hand.fist.unequip && game::UnequipWeapon();
		if ((g_hand.fist.changed || g_hand.fist.gaveUp) && g_holsterLinesLeft > 0) {
			--g_holsterLinesLeft;
			if (g_hand.fist.gaveUp) {
				OBVR_LOG("Fist: the weapon never left the slot - fists given up");
			} else {
				OBVR_LOG("Fist: the weapon hand %s%s", g_hand.fist.closed ? "closed" : "opened",
				         g_hand.fist.unequip
				             ? (unequipped ? " - the sheathed weapon taken off for the fists"
				                           : " - the sheathed weapon could NOT be taken off")
				         : g_hand.fist.readyClick
				             ? (g_hand.fist.closed ? " - fists up" : " - fists down")
				             : " - nothing to ready");
			}
		}
		if (g_hand.fist.readyClick && !g_hand.fist.changed && g_holsterLinesLeft > 0) {
			--g_holsterLinesLeft;
			OBVR_LOG("Fist: the slot is empty - fists up");
		}
		if ((h.gesture != vr::HolsterKind::None || h.gaveUp) && g_holsterLinesLeft > 0) {
			--g_holsterLinesLeft;
			const char* const what = h.gesture == vr::HolsterKind::Bow       ? "bow"
			                         : h.gesture == vr::HolsterKind::TwoHand ? "two-handed weapon"
			                                                                 : "one-handed weapon";
			if (h.gaveUp) {
				OBVR_LOG("Holster: the weapon equipped never showed in the hand - draw given up");
			} else if (h.otherDrawn) {
				OBVR_LOG("Holster: reach for the %s - another weapon is drawn, it goes back first",
				         what);
			} else if (h.refused) {
				OBVR_LOG("Holster: reach for the %s - none seen this session, nothing to draw", what);
			} else if (h.equip != vr::HolsterKind::None) {
				OBVR_LOG("Holster: reach for the %s - equipping the last one seen, then drawing", what);
			} else {
				OBVR_LOG("Holster: reach for the %s - %s", what,
				         frame.weaponSeen == vr::WeaponSeen::Drawn ? "sheathing" : "drawing");
			}
		}
	}
	// Drawing and sheathing without the animation's second (game::StepWeaponDrawSpeed):
	// the Equip and Unequip sequences' time runs WeaponDrawSpeed times faster.
	{
		static UInt32 s_drawLinesLeft = 60;
		const game::WeaponDrawSpeedReport draw =
			game::StepWeaponDrawSpeed(config.hands.weaponDrawSpeed, active);
		if ((draw.newlyHastened > 0 || draw.released > 0 || draw.blendsShortened > 0) && s_drawLinesLeft > 0) {
			--s_drawLinesLeft;
			if (draw.newlyHastened > 0) {
				OBVR_LOG("Hands: the %s animation's time runs %.0fx faster (%u sequence(s))",
				         draw.group == 17 ? "draw" : "sheathe",
				         static_cast<double>(config.hands.weaponDrawSpeed), draw.hastened);
			} else if (draw.blendsShortened > 0) {
				OBVR_LOG("Hands: %u draw or sheathe blend(s) shortened %.0fx", draw.blendsShortened,
				         static_cast<double>(config.hands.weaponDrawSpeed));
			} else {
				OBVR_LOG("Hands: %u draw or sheathe sequence(s) done", draw.released);
			}
		}
	}
	{
		// The ring sets hotkeys where the cursor is on the inventory or the
		// magic menu (the menu under the cursor: the click lands there).
		const UInt32 underCursor = menuIsUp ? game::ActiveMenuId() : game::kMenuIdNone;
		const bool assign = underCursor == game::kMenuIdInventory ||
		                    underCursor == game::kMenuIdMagic;
		UpdateQuickMenu(config, backend,
		                active && !frame.settingsMenuOpen &&
		                    ((!menuIsUp && frame.inWorld) || assign),
		                assign, frame, dt);
	}
	// What a hand script's picture is of ([Debug] HandScript): the state on
	// the frame of each mark.
	if (test::HandScriptMarkedThisFrame()) {
		// Where the player stands and faces (the engine's yaw, forward = sin,
		// cos): what a walk between two marks is measured against.
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		game::PlayerRotation turned{};
		if (game::ReadPlayerFeet(feet) && game::ReadPlayerRotation(turned)) {
			OBVR_LOG("HandScript: player at %.1f %.1f %.1f, heading %.1f degrees",
			         static_cast<double>(feet.x), static_cast<double>(feet.y), static_cast<double>(feet.z),
			         static_cast<double>(turned.yaw * math::kRadiansToDegrees));
		}
		game::LogHandBodies();
		OBVR_LOG("HandScript: state - world %d, menu %d, third person %d, head %d at %.2f %.2f "
		         "%.2f, right %d at %.2f %.2f %.2f stick %.2f %.2f, left %d, camera %d, hand "
		         "pinned %d, teleport allowed %d aiming %d commit %d, weapon %d (slot %d), active menu %s, laser hit %d at %.0f,%.0f, cursor %d at %.0f,%.0f, top menu %s",
		         frame.inWorld ? 1 : 0, menuIsUp ? 1 : 0, frame.firstPerson ? 0 : 1,
		         frame.headValid ? 1 : 0, static_cast<double>(frame.headPosition.x),
		         static_cast<double>(frame.headPosition.y),
		         static_cast<double>(frame.headPosition.z), frame.right.valid ? 1 : 0,
		         static_cast<double>(frame.right.position.x),
		         static_cast<double>(frame.right.position.y),
		         static_cast<double>(frame.right.position.z),
		         static_cast<double>(frame.right.thumbX), static_cast<double>(frame.right.thumbY),
		         frame.left.valid ? 1 : 0, g_cyclopeanCameraWorldValid ? 1 : 0,
		         g_hand.rightHandValid ? 1 : 0, frame.teleportAllowed ? 1 : 0,
		         g_hand.teleportAiming ? 1 : 0, g_hand.teleportCommit ? 1 : 0,
		         static_cast<int>(frame.weaponSeen), static_cast<int>(frame.equipped),
		         menuIsUp ? game::MenuIdName(game::ActiveMenuId()) : "none", g_hand.laserHit ? 1 : 0,
		         static_cast<double>(g_hand.laserPixelX), static_cast<double>(g_hand.laserPixelY),
		         frame.cursorValid ? 1 : 0, static_cast<double>(frame.cursorX),
		         static_cast<double>(frame.cursorY), game::MenuIdName(game::TopVisibleMenu()));
		// What the hands could take and what they hold: the grab and the stow
		// scenarios read whether an item was found at all.
		const NiPoint3 camera =
			g_cyclopeanCameraWorldValid ? g_cyclopeanCameraWorldTransform.pos : NiPoint3{0.0f, 0.0f, 0.0f};
		game::SearchHand eyes;
		eyes.valid = g_cyclopeanCameraWorldValid;
		eyes.position = camera;
		const game::NearItem anyItem =
			game::FindNearestItem(eyes, game::SearchHand{}, 2000.0f, 2000.0f, 0);
		const NiPoint3 hand = g_cyclopeanCameraWorldTransform.pos +
		                      g_cyclopeanCameraWorldTransform.rot * g_hand.rightHandOffsetUnits;
		OBVR_LOG("HandScript: items - near a hand %d (ref %08X type %02X, %.0f units from the %s "
		         "hand), nearest to the eyes %08X form %08X type %02X at %.0f %.0f %.0f (%.0f "
		         "units); "
		         "eyes %.0f %.0f %.0f, right hand %.0f %.0f %.0f; held %08X; blocking %d, block "
		         "key %d, attack key %d, player action %d",
		         g_nearItem.valid ? 1 : 0, g_nearItem.ref, game::RefBaseFormType(g_nearItem.ref),
		         static_cast<double>(g_nearItem.distance), g_nearItem.left ? "left" : "right",
		         anyItem.ref,
		         mem::LooksLikeObjectAddress(anyItem.ref)
		             ? *reinterpret_cast<const UInt32*>(anyItem.ref + addr::kFormIdOffset)
		             : 0u,
		         game::RefBaseFormType(anyItem.ref),
		         static_cast<double>(anyItem.centre.x), static_cast<double>(anyItem.centre.y),
		         static_cast<double>(anyItem.centre.z), static_cast<double>(anyItem.distance),
		         static_cast<double>(camera.x), static_cast<double>(camera.y),
		         static_cast<double>(camera.z), static_cast<double>(hand.x),
		         static_cast<double>(hand.y), static_cast<double>(hand.z), game::GrabbedRef(),
		         g_hand.blocking ? 1 : 0, g_hand.controls.block ? 1 : 0,
		         g_hand.controls.attack ? 1 : 0, static_cast<int>(game::ReadPlayerAction()));
	}

	// The item nearest a hand, by distance (game::FindNearestItem): the pick is
	// aimed from that hand at it in the camera pass. While a grip is closed,
	// only that hand looks; while something is held, nothing does.
	g_nearItem = game::NearItem{};
	if (active && !menuIsUp && g_cyclopeanCameraWorldValid && !game::PlayerHoldsGrab()) {
		const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
		const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
		const bool gripping = g_hand.grabWanted;
		float reachMetres = config.hands.grabReachMetres;
		reachMetres = config.hands.reachMarkerMetres > reachMetres ? config.hands.reachMarkerMetres
		                                                            : reachMetres;
		reachMetres = config.hands.pullReachMetres > reachMetres ? config.hands.pullReachMetres
		                                                          : reachMetres;
		const float reach = reachMetres * config.tracker.unitsPerMetre;
		// Each hand with its laser, the beam the settings tilt: beyond the
		// grab's reach an item counts only while that beam points at it.
		const auto hand = [&](bool left, bool valid) {
			game::SearchHand h;
			h.valid = valid;
			h.position =
				camPos + camRot * (left ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits);
			if (valid) {
				h.direction = vr::HandLaserWorldRay(
					camRot, camPos, left ? g_hand.leftHandRotation : g_hand.rightHandRotation,
					left ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits,
					config.hands.laserPitchDegrees,
					left ? -config.hands.laserYawDegrees : config.hands.laserYawDegrees,
					config.hands.laserOriginMetres, config.tracker.unitsPerMetre)
				                  .direction;
				// The palm: the controller's own sideways axis, towards the other
				// hand - a right hand wrapped round the handle faces its palm
				// left (-x), a left hand right (+x). Read off how the controller
				// is held, not measured; a palm turned the wrong way would show
				// as the back of the hand finding items.
				const NiMatrix33& rel = left ? g_hand.leftHandRotation : g_hand.rightHandRotation;
				const float side = left ? 1.0f : -1.0f;
				h.palm = camRot * NiPoint3{rel.data[0][0] * side, rel.data[1][0] * side,
				                           rel.data[2][0] * side};
			}
			return h;
		};
		g_nearItem = game::FindNearestItem(
			hand(false, g_hand.rightHandValid && (!gripping || !g_hand.grabWithLeftHand)),
			hand(true, g_hand.leftHandValid && (!gripping || g_hand.grabWithLeftHand)), reach,
			config.hands.grabReachMetres * config.tracker.unitsPerMetre, 0);
	}
	g_hand.pickWithLeftHand = g_nearItem.valid && g_nearItem.left;
	// The grab follows whichever hand is holding it: direction through the aim
	// pose (set above based on which grip is down), distance from that hand.
	// No floor: a hand brought to the body brings the object with it - the
	// quarter-metre floor made it flicker there (2026-09-26), and a gesture
	// that stows an object at the body is planned.
	const float grabUnits = g_hand.grabDistanceMetres * config.tracker.unitsPerMetre;
	// The grab by reach (vr::StepGrabReach): the grip arms it, the pick runs
	// through the hand, and the key goes down once what it found is within
	// reach of that hand - measured from the camera the hands are pinned to.
	const bool gripHeld = active && !menuIsUp && g_hand.grabWanted;
	bool inReach = false;
	UInt32 reachRef = 0;
	if (gripHeld && g_grabReachPick && g_cyclopeanCameraWorldValid) {
		const game::CrosshairTarget target = game::ReadCrosshairTarget();
		const NiPoint3& offset =
			g_hand.grabWithLeftHand ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits;
		const NiPoint3 handWorld = g_cyclopeanCameraWorldTransform.pos +
		                           g_cyclopeanCameraWorldTransform.rot * offset;
		// The point the laser touched, not the object's origin: the hand has
		// to be at the object itself.
		NiPoint3 hit = target.position;
		game::ReadPickHit(hit);
		// Within the grab's reach, or an item within the pull's, which then
		// floats to the hand (game::GripTakes, game::HeldObject).
		const float perMetre = config.tracker.unitsPerMetre;
		inReach = target.haveRef &&
		          game::GripTakes(
		              math::Sqrt((hit - handWorld).LengthSquared()),
		              game::IsHandItemType(game::RefBaseFormType(target.refAddress)),
		              config.hands.grabReachMetres * perMetre, config.hands.pullReachMetres * perMetre);
		reachRef = target.haveRef ? target.refAddress : 0;
	}
	const vr::GrabReachVerdict reach = vr::StepGrabReach(g_grabReach, gripHeld, inReach);
	g_grabReachPick = reach.reachPick;
	g_grabKeyDown = reach.key;
	g_hand.controls.grab = reach.key;
	// Where the held object goes: the palm of the grabbing hand's pinned bone
	// (game::HeldObject - the spring pulls the touched point there), moved
	// along the fingers by [Hands] HeldObjectMetres; without a pinned bone,
	// the controller moved along its laser by as much. Handed to the engine's
	// update as a point, which it looks at from its own camera origin.
	bool haveHoldPoint = false;
	NiPoint3 holdPoint{0.0f, 0.0f, 0.0f};
	NiMatrix33 palmRot;
	NiPoint3 palmBone;
	if (reach.key && game::ReadHandBoneWorld(!g_hand.grabWithLeftHand, palmRot, palmBone)) {
		holdPoint = game::PalmPoint(palmRot, palmBone,
		                            (game::kPalmAlongMetres + config.hands.heldObjectMetres) *
		                                config.tracker.unitsPerMetre);
		haveHoldPoint = true;
	} else if (reach.key && g_cyclopeanCameraWorldValid &&
	    (g_hand.grabWithLeftHand ? g_hand.leftHandValid : g_hand.rightHandValid)) {
		const bool left = g_hand.grabWithLeftHand;
		const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
			g_cyclopeanCameraWorldTransform.rot, g_cyclopeanCameraWorldTransform.pos,
			left ? g_hand.leftHandRotation : g_hand.rightHandRotation,
			left ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits,
			config.hands.laserPitchDegrees,
			left ? -config.hands.laserYawDegrees : config.hands.laserYawDegrees,
			config.hands.heldObjectMetres, config.tracker.unitsPerMetre);
		holdPoint = ray.origin;
		haveHoldPoint = true;
	}
	game::SetGrabAtHand(reach.key, grabUnits, haveHoldPoint, holdPoint, 0.0f);
	// The held body's physics: always through the player's body while held
	// (gameplay will build on it: an object brought to the body), thrown
	// with the hand's speed when let go (game::GrabPhysics).
	{
		// The hand that held it, kept past the grip opening: the engine lets
		// go a frame or two after the key, and the throw is that hand's speed.
		static bool s_holdLeft = false;
		if (reach.key) {
			s_holdLeft = g_hand.grabWithLeftHand;
		}
		// SteamVR's velocity of that controller, into the world, carried to
		// the held point: v + w x r (game::PointVelocity).
		const bool throwValid = g_cyclopeanCameraWorldValid &&
		                        (s_holdLeft ? g_hand.leftHandValid : g_hand.rightHandValid);
		NiPoint3 throwVelocity{0.0f, 0.0f, 0.0f};
		if (throwValid) {
			const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
			const float perMetre = config.tracker.unitsPerMetre;
			const NiPoint3 v =
				camRot * (s_holdLeft ? g_hand.leftVelocity : g_hand.rightVelocity) * perMetre;
			const NiPoint3 w =
				camRot * (s_holdLeft ? g_hand.leftAngularVelocity : g_hand.rightAngularVelocity);
			const NiPoint3 controller =
				g_cyclopeanCameraWorldTransform.pos +
				camRot * (s_holdLeft ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits);
			const NiPoint3 r = haveHoldPoint ? holdPoint - controller : NiPoint3{0.0f, 0.0f, 0.0f};
			throwVelocity = game::PointVelocity(v, w, r);
		}
		// Stowing (vr::StepStow): an item let go at the chest or the belly
		// goes into the inventory, taken as activating it would take it, once
		// the engine has let go of it. It is not thrown on the way.
		const vr::HandPose& holding = s_holdLeft ? frame.left : frame.right;
		vr::StowInput stowIn;
		stowIn.allowed = active && !menuIsUp && frame.inWorld;
		stowIn.keyDown = reach.key;
		stowIn.heldRef = game::GrabbedRef();
		stowIn.heldIsItem = stowIn.heldRef != 0 && game::RefIsItem(stowIn.heldRef, nullptr);
		stowIn.handValid = holding.valid && frame.headValid;
		if (stowIn.handValid) {
			stowIn.handRelative = vr::BodyRelative(frame.head, frame.headPosition, holding.position);
		}
		stowIn.dt = g_deltaSeconds;
		const vr::StowVerdict stow = vr::StepStow(g_stow, stowIn, config.hands.stow);
		// The spot, while an item is held and the ring is switched on
		// ([Hands] StowSpotVisible, off by default); lit while the hand is in
		// it. While it is being placed, UpdateStowPlacing draws it.
		const bool placing = game::StowPlaceActive();
		if (!placing && vr::StowRingShown(false, config.hands.stow.spotVisible, stow.showSpot) &&
		    frame.headValid) {
			if (g_stowSpotView.lit != stow.atBody) {
				g_stowSpotView.lit = stow.atBody;
				++g_stowSpotRevision;
			}
			const NiPoint3 at =
				vr::StowSpotInTracking(frame.head, frame.headPosition, config.hands.stow);
			g_stowSpotLayer.Show(backend, render::GetGameDevice(),
			                     StowSpotPose(at, frame.headPosition),
			                     2.0f * config.hands.stow.radius, g_stowSpotRevision,
			                     ui::PaintStowSpotFor, &g_stowSpotView);
		} else if (!placing) {
			g_stowSpotLayer.Hide(backend);
		}
		const bool stowing = stow.waiting || stow.take != 0;
		// The held object pushes what it meets: its body driven to the hand
		// (GrabPhysics.h). Not while it is being stowed - it goes into the
		// inventory, not into the chest.
		game::StepGrabPhysics(true, stowing ? 0.0f : config.hands.throwStrength, throwValid,
		                      throwVelocity, g_deltaSeconds,
		                      config.hands.heldObjectsPush && !stowing);
		if (!stowIn.keyDown) {
			g_stowWasAtBody = false;  // let go: said by the lines below instead
		} else if (stow.atBody != g_stowWasAtBody) {
			g_stowWasAtBody = stow.atBody;
			if (g_stowLinesLeft > 0) {
				--g_stowLinesLeft;
				OBVR_LOG("Stow: the held item is %s the body (hand at %.2f right, %.2f forward, "
				         "%.2f up)",
				         stow.atBody ? "at" : "away from",
				         static_cast<double>(stowIn.handRelative.x),
				         static_cast<double>(stowIn.handRelative.y),
				         static_cast<double>(stowIn.handRelative.z));
			}
		}
		if (stow.take != 0) {
			UInt32 owner = 0;
			const game::TakeResult taken = game::TakeIntoInventory(stow.take, &owner);
			if (g_stowLinesLeft > 0) {
				--g_stowLinesLeft;
				char ownerText[32] = "no owner";
				if (owner != 0) {
					std::snprintf(ownerText, sizeof(ownerText), "owner %08X", owner);
				}
				OBVR_LOG("Stow: let go at the body - %s (%s; the game's activation decides "
				         "any crime)",
				         game::TakeResultName(taken), ownerText);
			}
		} else if ((stow.notItem || stow.gaveUp) && g_stowLinesLeft > 0) {
			--g_stowLinesLeft;
			OBVR_LOG("Stow: let go at the body - %s",
			         stow.notItem ? "not an item, dropped as usual"
			                      : "the engine kept holding it, nothing taken");
		}
	}

	// The reach marker ([Hands] ReachMarker): a light-brown ring on the object
	// under the pick when it is within ReachMarkerMetres of either hand - the
	// object a closed grip would reach for. The pick is the laser's with the grip
	// open and the hand's while reaching; either way what it found is asked.
	{
		bool markerShown = false;
		vr::openvr::HmdMatrix34 markerPose{};
		vr::openvr::HmdMatrix34 head{};
		if (active && !menuIsUp && (config.hands.reachTooltip || config.hands.reachRing) &&
		    g_cyclopeanCameraWorldValid &&
		    backend.GetRenderPoseMatrix(head)) {
			const game::CrosshairTarget target = game::ReadCrosshairTarget();
			NiPoint3 hit = target.position;
			game::ReadPickHit(hit);
			const float reachUnits = config.hands.reachMarkerMetres * config.tracker.unitsPerMetre;
			const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
			const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
			const bool nearRight =
				g_hand.rightHandValid &&
				vr::WithinReach(camPos + camRot * g_hand.rightHandOffsetUnits, hit,
				                reachUnits);
			const bool nearLeft =
				g_hand.leftHandValid &&
				vr::WithinReach(camPos + camRot * g_hand.leftHandOffsetUnits, hit,
				                reachUnits);
			if (game::ReachMarkerWanted(target.haveRef,
			                            game::RefBaseFormType(target.haveRef ? target.refAddress : 0),
			                            nearRight, nearLeft)) {
				markerShown = true;
				markerPose = vr::FacingHeadAt(
					head, vr::WorldPointInTracking(head, camRot, camPos, hit,
					                               config.tracker.unitsPerMetre));
			}
		}
		g_reachMarker.Submit(backend, markerShown && config.hands.reachRing, markerPose,
		                     config.hands.reachMarkerOpacity);
		// The crosshair's icon moves onto the object, into the ring's middle.
		g_reachIconShown = markerShown && config.hands.reachTooltip;
		g_reachIconPose = render::ReachIconPose(markerPose);
	}
	UpdateTeleport(config, backend, active, menuIsUp, dt);
	// Whether the engine took it: its grab update runs only while it holds
	// something, so a count unchanged a quarter of a second after the key
	// went down is a grab it refused - with where the target was then, the
	// question behind "the hand has to hover in front of it" (2026-09-25).
	{
		static UInt32 s_updatesAtKey = 0;
		static int s_framesHeld = -1;
		static UInt32 s_takeLinesLeft = 12;
		if (reach.key && s_framesHeld < 0) {
			s_framesHeld = 0;
			s_updatesAtKey = game::GrabUpdateCount();
		} else if (!reach.key) {
			s_framesHeld = -1;
		} else if (s_framesHeld >= 0 && ++s_framesHeld == 22 && s_takeLinesLeft > 0) {
			--s_takeLinesLeft;
			const bool took = game::GrabUpdateCount() != s_updatesAtKey;
			const game::CrosshairTarget target = game::ReadCrosshairTarget();
			const NiPoint3& offset =
				g_hand.grabWithLeftHand ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits;
			const NiPoint3 handWorld = g_cyclopeanCameraWorldTransform.pos +
			                           g_cyclopeanCameraWorldTransform.rot * offset;
			const NiPoint3 toTarget = target.position - g_cyclopeanCameraWorldTransform.pos;
			NiPoint3 hit = target.position;
			game::ReadPickHit(hit);
			const NiPoint3 handToTarget = hit - handWorld;
			OBVR_LOG("Hands: grab - the engine %s it (target now %08X, %.0f units from the eyes, "
			         "the laser's point %.0f from the hand, the hand %.0f from the eyes)",
			         took ? "TOOK" : "did NOT take", target.haveRef ? target.refAddress : 0u,
			         static_cast<double>(math::Sqrt(toTarget.LengthSquared())),
			         static_cast<double>(math::Sqrt(handToTarget.LengthSquared())),
			         static_cast<double>(math::Sqrt(offset.LengthSquared())));
		}
	}
	// Each step logged: the 2026-09-25 run left no trace of a grab at all.
	static int s_grabPhase = 0;  // 0 open, 1 reaching, 2 holding
	const int grabPhase = reach.key ? 2 : (reach.reachPick ? 1 : 0);
	if (grabPhase != s_grabPhase) {
		if (grabPhase == 2) {
			OBVR_LOG("Hands: grab - %s grip took %08X within %.2f m, key %02X down",
			         g_hand.grabWithLeftHand ? "left" : "right", reachRef,
			         static_cast<double>(config.hands.pullReachMetres > config.hands.grabReachMetres
			                                 ? config.hands.pullReachMetres
			                                 : config.hands.grabReachMetres), config.handKeys.grab);
		} else if (grabPhase == 1) {
			OBVR_LOG("Hands: grab - %s grip closed, reaching for something within %.2f m",
			         g_hand.grabWithLeftHand ? "left" : "right",
			         static_cast<double>(config.hands.grabReachMetres));
		} else {
			OBVR_LOG("Hands: grab - grip open%s", s_grabPhase == 2 ? ", the object let go" : "");
		}
		s_grabPhase = grabPhase;
	}

	if (g_hand.blocking != g_handBlocking) {
		g_handBlocking = g_hand.blocking;
		OBVR_LOG("Hands: %s", g_handBlocking ? "the left hand is up - blocking"
		                                      : "the left hand is down - block released");
	}
	if (g_hand.reachBack != g_handReachBack) {
		g_handReachBack = g_hand.reachBack;
		OBVR_LOG("Hands: the right hand is %s", g_handReachBack ? "reaching back" : "in front");
	}
	if (g_hand.swing != vr::SwingVerdict::None && g_handSwingLinesLeft > 0) {
		--g_handSwingLinesLeft;
		OBVR_LOG("Hands: a %s%s swing, %.1f m/s at its fastest, %.2f m long (Swing speed %.1f, a power attack from "
		         "%.2f m)",
		         g_hand.swing == vr::SwingVerdict::Heavy ? vr::PowerDirectionName(g_hand.powerDirection) : "",
		         g_hand.swing == vr::SwingVerdict::Heavy ? " power" : "light", static_cast<double>(g_hand.swingPeakSpeed),
		         static_cast<double>(g_hand.swingMetres), static_cast<double>(config.hands.gestures.swingLight),
		         static_cast<double>(config.hands.gestures.powerSwingMetres));
	}

	// A swing that has become a power attack grunts once (vr::GruntDue), as
	// vanilla does when its power attack starts.
	{
		static UInt32 lastGrunted = 0;
		if (active && !menuIsUp && g_hand.rightHandValid &&
		    vr::GruntDue(g_hand.strikeByMotion, g_hand.swingHeavy, g_hand.swingSerial, lastGrunted)) {
			lastGrunted = g_hand.swingSerial;
			game::PlayPowerAttackGrunt();
		}
	}
	// Each swing that may strike swishes once, as it starts (game::SwishDue).
	{
		static UInt32 lastSwished = 0;
		if (active && !menuIsUp && g_hand.rightHandValid &&
		    game::SwishDue(g_hand.strikeByMotion, g_hand.swingActive, g_hand.swingSerial, lastSwished)) {
			lastSwished = g_hand.swingSerial;
			game::PlaySwingSwish();
		}
	}

	// The strike by motion: while the right hand is swinging a drawn melee
	// weapon, the blade is tested against the bodies near the player and each
	// one it passes through is handed to the engine's hit function - once per
	// swing, heavy when the swing has been fast enough. Not in a menu, not in
	// third person (no hand pose there), and only while the hand is tracked.
	if (active && g_hand.strikeByMotion && g_hand.swingActive && g_hand.rightHandValid &&
	    !menuIsUp) {
		game::MotionStrike strike;
		strike.swingSerial = g_hand.swingSerial;
		strike.heavy = g_hand.swingHeavy;
		strike.attackGroup = g_hand.swingHeavy ? vr::PowerAttackGroup(g_hand.powerDirection) : vr::kAnimGroupAttackLight;
		strike.handRotation = g_hand.rightHandRotation;
		strike.handOffsetUnits = g_hand.rightHandOffsetUnits;
		strike.cameraValid = g_cyclopeanCameraWorldValid;
		strike.cameraRotation = g_cyclopeanCameraWorldTransform.rot;
		strike.cameraPosition = g_cyclopeanCameraWorldTransform.pos;
		strike.boundFactor = config.hands.hitBoundFactor;
		strike.padUnits = config.hands.hitPadUnits;
		game::StrikeByMotion(strike);
	}

	// The weapon and the hands push what they meet (game/WorldPush.h): the
	// weapon hand's blade while a melee weapon is drawn, else the hand; the
	// other hand always. In the world only, in first person, not in a menu.
	{
		const bool inPlace =
			active && !menuIsUp && frame.inWorld && frame.firstPerson && g_cyclopeanCameraWorldValid;
		const bool pushing = inPlace && config.hands.pushWorld;
		game::PushFrame push;
		push.dtSeconds = g_deltaSeconds;
		if (inPlace && (config.hands.pushWorld || config.hands.bodyCollision)) {
			const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
			const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
			const int weapon = static_cast<int>(game::Pusher::WeaponHand);
			const int other = static_cast<int>(game::Pusher::OtherHand);
			if (g_hand.rightHandValid) {
				const NiPoint3 grip = camPos + camRot * g_hand.rightHandOffsetUnits;
				const NiPoint3 forward = camRot * (g_hand.rightHandRotation * NiPoint3{0.0f, 1.0f, 0.0f});
				const bool blade = game::MeleeInHand(nullptr) &&
				                   game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
				float length = game::kPushFallbackBladeUnits;
				if (blade) {
					const NiAVObject* const node = game::FindFirstPersonNode("Weapon");
					length = game::BladeLengthFromBound(node != nullptr ? node->worldBound.radius : 0.0f);
				}
				push.valid[weapon] = true;
				push.blade[weapon] = blade;
				push.segment[weapon] = blade ? game::BladeSegment(grip, forward, length)
				                             : game::HandSegment(grip, forward);
			}
			if (g_hand.leftHandValid) {
				const NiPoint3 grip = camPos + camRot * g_hand.leftHandOffsetUnits;
				const NiPoint3 forward = camRot * (g_hand.leftHandRotation * NiPoint3{0.0f, 1.0f, 0.0f});
				push.valid[other] = true;
				push.segment[other] = game::HandSegment(grip, forward);
			}
		}
		// The hands and the drawn melee weapon as Havok bodies
		// (game/HandBodies.h); the rays below then only cover what has none.
		game::HandBodyFrame bodies;
		bodies.enabled = inPlace && config.hands.bodyCollision;
		bodies.physicsRate = config.hands.physicsRate;
		bodies.dtSeconds = g_deltaSeconds;
		bool bodyBlade = false;
		if (bodies.enabled) {
			const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
			const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
			if (g_hand.rightHandValid) {
				bodyBlade = push.blade[static_cast<int>(game::Pusher::WeaponHand)];
				const int slot = static_cast<int>(bodyBlade ? game::HandBodySlot::Weapon : game::HandBodySlot::RightHand);
				bodies.valid[slot] = true;
				bodies.rot[slot] = camRot * g_hand.rightHandRotation;
				bodies.pos[slot] = camPos + camRot * g_hand.rightHandOffsetUnits;
				if (bodyBlade) {
					const game::PushSegment& s = push.segment[static_cast<int>(game::Pusher::WeaponHand)];
					bodies.bladeUnits = math::Sqrt((s.b - s.a).LengthSquared());
				}
			}
			if (g_hand.leftHandValid) {
				const int slot = static_cast<int>(game::HandBodySlot::LeftHand);
				bodies.valid[slot] = true;
				bodies.rot[slot] = camRot * g_hand.leftHandRotation;
				bodies.pos[slot] = camPos + camRot * g_hand.leftHandOffsetUnits;
			}
			// Each body's span from what is drawn (game::HandSpanFromBones,
			// BladeSpanFromNode), in the body's own frame.
			for (int slot = 0; slot < static_cast<int>(game::HandBodySlot::Count); ++slot) {
				if (!bodies.valid[slot]) {
					continue;
				}
				const NiMatrix33& r = bodies.rot[slot];
				const NiPoint3& p = bodies.pos[slot];
				if (slot == static_cast<int>(game::HandBodySlot::Weapon)) {
					const NiAVObject* const node = game::FindFirstPersonNode("Weapon");
					if (node != nullptr) {
						const NiMatrix33& w = node->worldTransform.rot;
						const NiPoint3 axis{w.data[0][1], w.data[1][1], w.data[2][1]};
						bodies.span[slot] = game::BladeSpanFromNode(
							game::ToBodyFrame(r, p, node->worldTransform.pos),
							game::ToBodyFrame(r, NiPoint3{0.0f, 0.0f, 0.0f}, axis),
							game::ToBodyFrame(r, p, node->worldBound.center), node->worldBound.radius);
					}
					continue;
				}
				const bool left = slot == static_cast<int>(game::HandBodySlot::LeftHand);
				const NiAVObject* const wrist = game::FindFirstPersonNode(left ? "Bip01 L Hand" : "Bip01 R Hand");
				const NiAVObject* const knuckle =
					game::FindFirstPersonNode(left ? "Bip01 L Finger2" : "Bip01 R Finger2");
				if (wrist != nullptr && knuckle != nullptr) {
					bodies.span[slot] = game::HandSpanFromBones(game::ToBodyFrame(r, p, wrist->worldTransform.pos),
					                                            game::ToBodyFrame(r, p, knuckle->worldTransform.pos));
				}
			}
			// People are pushed out of combat only, and not by a hand made a fist
			// (game::HandBodyPushesActors; the tester, 2026-09-29).
			const float closeCurl = config.hands.fist.closeCurl;
			const float openLimit = vr::FistOpenLimit(config.hands.fist);
			g_rightBodyFist = game::HandBodyFist(g_rightBodyFist, g_hand.rightCurlValid, g_hand.rightCurl, closeCurl, openLimit);
			g_leftBodyFist = game::HandBodyFist(g_leftBodyFist, g_hand.leftCurlValid, g_hand.leftCurl, closeCurl, openLimit);
			const bool inCombat = game::PlayerInCombat();
			bodies.pushesActors[static_cast<int>(game::HandBodySlot::RightHand)] =
				game::HandBodyPushesActors(config.hands.pushPeople, true, inCombat, g_rightBodyFist);
			bodies.pushesActors[static_cast<int>(game::HandBodySlot::LeftHand)] =
				game::HandBodyPushesActors(config.hands.pushPeople, true, inCombat, g_leftBodyFist);
			bodies.pushesActors[static_cast<int>(game::HandBodySlot::Weapon)] =
				game::HandBodyPushesActors(config.hands.pushPeople, false, inCombat, false);
		}
		const game::HandBodyReport live = game::StepHandBodies(bodies);
		if (frame.inWorld && !menuIsUp) {
			game::StepPlayerCapsule(config.hands.bodyRadiusScale);
		}
		if (bodies.enabled) {
			MeasureBodies(bodies, bodyBlade, test::HandScriptMarkedThisFrame());
		}
		if (live.live[static_cast<int>(bodyBlade ? game::HandBodySlot::Weapon : game::HandBodySlot::RightHand)]) {
			push.valid[static_cast<int>(game::Pusher::WeaponHand)] = false;
		}
		if (live.live[static_cast<int>(game::HandBodySlot::LeftHand)]) {
			push.valid[static_cast<int>(game::Pusher::OtherHand)] = false;
		}
		game::StepWorldPush(pushing, push);
		if (inPlace) {
			StepShoves(config, g_deltaSeconds);
		}
	}

	// What the laser asked of the cursor on a flat frame, a few times, and

	// the cursor's answer a frame later: whether the hit lands where it
	// should, whether the cursor can be read at all, and whether the mouse
	// motion moves it. The first headset run saw the beam and no cursor.
	if (frame.flat.valid && g_flatLaserLinesLeft > 0) {
		if (!frame.cursorValid) {
			if (!g_flatCursorInvalidReported) {
				g_flatCursorInvalidReported = true;
				float rawX = 0.0f;
				float rawY = 0.0f;
				const bool managerThere = game::InterfaceCursorRaw(rawX, rawY);
				OBVR_LOG("Hands: the game's cursor position cannot be read on this flat frame, "
				         "so the laser has nothing to walk (interface manager %s, raw %.1f, %.1f)",
				         managerThere ? "present" : "ABSENT", static_cast<double>(rawX),
				         static_cast<double>(rawY));
			}
		} else if (g_hand.laserHit) {
			--g_flatLaserLinesLeft;
			OBVR_LOG("Hands: flat laser hit pixel %.0f,%.0f - cursor at %.0f,%.0f, step %d,%d "
			         "(controls %s)",
			         static_cast<double>(g_hand.laserPixelX),
			         static_cast<double>(g_hand.laserPixelY), static_cast<double>(frame.cursorX),
			         static_cast<double>(frame.cursorY), g_hand.cursorDx, g_hand.cursorDy,
			         g_hand.controlsActive ? "active" : "INACTIVE - nothing is sent");
		}
	}

	// The laser is the pointer: the game's own cursor sprite goes for as long
	// as the hand-tracked mode runs - it showed on the HUD in game as well -
	// and, with only the menus on the controllers, while a laser points at a
	// game menu. It comes back when neither holds.
	game::SetMenuCursorHidden(
		(active || (menuIsUp && g_hand.laserVisible)) && !frame.settingsMenuOpen);

	if (menuIsUp && !g_quadLaserMenuWasUp) {
		g_quadLaserLinesLeft = 4;
	}
	g_quadLaserMenuWasUp = menuIsUp;
	if (menuIsUp && !frame.flat.valid && g_quadLaserLinesLeft > 0 &&
	    (g_hand.laserVisible || g_hand.menuOnWrist)) {
		// Once on the first frame, then only while the trigger is pulled, so
		// the lines that are left show what a click saw.
		const bool pulled = frame.right.trigger > 0.5f || frame.left.trigger > 0.5f;
		if (g_quadLaserLinesLeft == 4 || pulled) {
			--g_quadLaserLinesLeft;
			OBVR_LOG("Hands: menu laser - %s quad %s (%.0fx%.0f layer px), cursor %s %.0f,%.0f, "
			         "%s hand points, hit %s %.0f,%.0f, step %d,%d, triggers %.2f/%.2f, click %d, "
			         "controls %s",
			         g_hand.menuOnWrist ? "wrist" : "big", frame.menuQuad.valid || g_hand.menuOnWrist
			                                                   ? "valid" : "MISSING",
			         static_cast<double>(frame.layerPixelsWidth),
			         static_cast<double>(frame.layerPixelsHeight),
			         frame.cursorValid ? "at" : "UNREADABLE", static_cast<double>(frame.cursorX),
			         static_cast<double>(frame.cursorY), g_hand.laserRight ? "right" : "left",
			         g_hand.laserHit ? "at" : "NONE", static_cast<double>(g_hand.laserPixelX),
			         static_cast<double>(g_hand.laserPixelY), g_hand.cursorDx, g_hand.cursorDy,
			         static_cast<double>(frame.right.trigger), static_cast<double>(frame.left.trigger),
			         g_hand.controls.menuClick ? 1 : 0,
			         g_hand.controlsActive ? "active" : "INACTIVE - nothing is sent");
		}
	}

	if (g_hand.controlsActive) {
		// Snap turning: the stick's turn becomes whole steps written into the
		// player's heading, and the continuous mouse turn is dropped. Written
		// here, at Present, so the next camera pass builds the view on it.
		vr::HandControlsWanted controls = g_hand.controls;
		const SnapTurnStep snap =
			StepSnapTurn(g_snapTurnState, controls.turn, config.look, g_deltaSeconds,
			             !menuIsUp && game::PlayerInWorld());
		if (snap.ownsStick) {
			controls.turn = 0.0f;
		}
		if (snap.fired && config.look.snapTurnVignette) {
			g_vignetteLayer.Trigger();
		}
		if (snap.yawRadians != 0.0f) {
			game::PlayerRotation rotation{};
			// rotZ grows clockwise and PlayerYawForGaze subtracts its step,
			// so a turn to the right goes in negated.
			const bool turned = game::ReadPlayerRotation(rotation) &&
			                    game::WritePlayerYaw(PlayerYawForGaze(rotation.yaw, -snap.yawRadians));
			if (snap.fired && !g_snapTurnReported) {
				g_snapTurnReported = true;
				OBVR_LOG("Look: snap turn %.0f degrees %s", static_cast<double>(config.look.snapTurnAngle),
				         turned ? "written into the player's heading"
				                : "could not be written - the player could not be reached");
			}
		}

		// The ready weapon tap, the first several times, with where the
		// weapon stood: "the button does nothing" was reported on
		// 2026-09-25, and this says whether the press left OBVR at all.
		static bool readyWasWanted = false;
		static UInt32 readyLinesLeft = 60;
		if (controls.readyWeapon && !readyWasWanted && readyLinesLeft > 0) {
			--readyLinesLeft;
			const game::WeaponState weapon = game::ReadPlayerWeaponState();
			OBVR_LOG("Hands: ready weapon sent (the weapon was %s)",
			         weapon == game::WeaponState::Drawn      ? "drawn"
			         : weapon == game::WeaponState::Sheathed ? "sheathed"
			                                                 : "unknown");
		}
		// How long the draw or sheathe took, from the key to the game showing
		// it with no animation left: the length of the animation, which
		// WeaponDrawSpeed shortens.
		static float s_readySeconds = 0.0f;
		if (controls.readyWeapon && !readyWasWanted) {
			s_readySeconds = 0.0f;
		} else {
			s_readySeconds += g_deltaSeconds;
		}
		readyWasWanted = controls.readyWeapon;
		static UInt32 readyEndLinesLeft = 60;
		if ((g_hand.ready.reached || g_hand.ready.gaveUp) && readyEndLinesLeft > 0) {
			--readyEndLinesLeft;
			// With the player's action code: a give-up while blocking or in an
			// equip animation throughout never sent the key at all.
			// And the weapon's type: two-handers were still seen settling for
			// about a second after the draw (2026-09-27).
			SInt32 weaponType = -1;
			game::EquippedWeaponForm(&weaponType);
			OBVR_LOG("Hands: ready weapon %s after %.2f s (player action %d, weapon type %d)",
			         g_hand.ready.reached ? "done - the game shows the wanted state"
			                              : "given up - the game did not follow within 2.5 s",
			         static_cast<double>(s_readySeconds), static_cast<int>(game::ReadPlayerAction()),
			         static_cast<int>(weaponType));
		}

		// Every click the laser sends into a game menu, the first several
		// dozen: a message box that could not be clicked away (2026-09-27)
		// left no trace of whether a click was sent at all.
		static bool clickWasSent = false;
		static UInt32 clickLinesLeft = 40;
		if (controls.menuClick && !clickWasSent && menuIsUp && clickLinesLeft > 0) {
			--clickLinesLeft;
			OBVR_LOG("Hands: click sent to the %s menu at cursor %.0f,%.0f",
			         game::MenuIdName(game::ActiveMenuId()), static_cast<double>(frame.cursorX),
			         static_cast<double>(frame.cursorY));
		}
		clickWasSent = controls.menuClick;
		// Every jump sent, the first several dozen: the right stick's flick
		// and a diagonal turn must be told apart (docs/controls-spec.md 2).
		static bool jumpWasSent = false;
		static UInt32 jumpLinesLeft = 40;
		if (controls.jump && !jumpWasSent && jumpLinesLeft > 0) {
			--jumpLinesLeft;
			OBVR_LOG("Hands: jump sent (right stick flicked up)");
		}
		jumpWasSent = controls.jump;
		game::ApplyHandControls(controls, config.handKeys, config.hands.turnSpeed);
		g_handControlsHeld = true;
		if ((g_hand.laserHit || g_hand.pokeHover) &&
		    (g_hand.cursorDx != 0 || g_hand.cursorDy != 0)) {
			game::MoveMouseBy(g_hand.cursorDx, g_hand.cursorDy);
		}
		if (g_hand.menuScroll != 0) {
			game::ScrollMouseWheel(g_hand.menuScroll);
			// Said a few times: the headset run of 2026-09-25 had the lists not
			// scroll, and whether the notches are sent at all is the first
			// question.
			static UInt32 wheelLinesLeft = 6;
			if (wheelLinesLeft > 0) {
				--wheelLinesLeft;
				OBVR_LOG("Hands: mouse wheel %+d notch(es) sent to the %s menu at cursor %.0f,%.0f",
				         g_hand.menuScroll, game::MenuIdName(game::ActiveMenuId()),
				         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY));
			}
		}
		if (g_hand.pokePress && g_handPokeLinesLeft > 0) {
			--g_handPokeLinesLeft;
			OBVR_LOG("Hands: the finger pressed the menu at cursor %.0f, %.0f", frame.cursorX,
			         frame.cursorY);
		}
	} else if (g_handControlsHeld) {
		game::ReleaseHandControls(config.handKeys);
		g_handControlsHeld = false;
	}

	if (g_hand.menuOnWrist) {
		g_hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(g_hand.menuWristRight, g_handRolesSwapped)),
		                             g_hand.menuTransform, config.hands.wristMenuWidth);
	} else if (g_hand.hudOnRightWrist) {
		g_hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(true, g_handRolesSwapped)), g_hand.hudTransform,
		                             config.hands.wristHudWidth);
	} else {
		g_hudLayer.ClearWristPlacement();
	}
}

// The matching third-person correction. It is decided from the exact same
// source-aim pose as the wrapped attack call, then written after animation in
// BeforeFirstScenePass. Unlike g_weaponTurnRadians it is deliberately scaled:
// the gameplay aim remains exact while the visible torso only takes the share
// selected in the INI.
ThirdPersonAimVisualState g_thirdPersonAimVisualState{};
float g_thirdPersonAimVisualYaw = 0.0f;
float g_thirdPersonAimVisualPitch = 0.0f;
bool g_thirdPersonAimVisualWanted = false;
float g_thirdPersonHeadVisualYaw = 0.0f;
float g_thirdPersonHeadVisualPitch = 0.0f;
bool g_thirdPersonHeadVisualWanted = false;

// The visible body, decided in the camera pass and written after animation
// in BeforeFirstScenePass like the corrections above. The camera node is
// the one the pass placed; by the time the scene is drawn it stands on the
// first eye, and g_bodyFirstEyeStep is the step it took from the head so
// the body can be stood under the head and not under one eye.
bool g_bodyWanted = false;
NiAVObject* g_bodyCameraNode = nullptr;
NiPoint3 g_bodyFirstEyeStep{0.0f, 0.0f, 0.0f};
NiTransform g_cyclopeanCameraLocalTransform{};

// The body's share as the ARMS' BASE has it, which is one frame behind the
// heading itself.
//
// Measured, and it is the last piece of the aiming. The arms are turned on top
// of whatever the engine's animation left, and that base carries the body's
// rotation - but it carries the rotation the animation ran with, which is the
// one from BEFORE this frame's heading write. rotZ itself is current at render
// time; the skeleton built from it is not.
//
// The trace shows it directly, in the world angle read back off the arms:
//
//   143 drawing   base=-171.0  asked=20.6  ->  168.4   right
//   144 released  base=-171.0  asked= 0.0  -> -171.0   the base has not caught up
//   145           base= 168.4  asked= 0.0  ->  168.4   right again
//   151           base= 168.3  asked= 0.0  ->  168.3
//   152 given back base=168.3  asked=20.7  ->  147.6   the base has not let go
//   153           base=-171.0  asked=20.7  ->  168.3   right again
//
// One frame of the arms giving up a share the base had not taken, and one of
// them taking back a share the base had not released. Subtracting the previous
// frame's offset instead of this one's makes both disappear, because that is
// the share the base actually holds.
float g_aimBodyOffsetLastFrame = 0.0f;

// The view's heading before and after the compensation takes the body's turn
// back out. The second is what the wearer is actually looking along, and with
// the head still it must not move.
float g_aimViewYawBefore = 0.0f;
float g_aimViewYawAfter = 0.0f;

// Whether the attack control was held on the previous camera pass, so the
// frame it is RELEASED on can be recognised. A KeyEdge would answer the
// opposite question.
bool g_aimWasHeld = false;

// The cast control last frame, and the window its press opened. Carried across
// frames because a cast is decided by an EDGE and lasts past it - see
// NextCastWindow for why a spell cannot be followed the way a bow shot is.
bool g_castWasHeld = false;
CastWindow g_castWindow{};

// Said once, when a spell has finished holding the body: how long it held it
// for, and which of the two things ended it.
bool g_castReported = false;

// Whether a spell of the player's may be aimed at all, published by the camera
// pass each frame so the hook itself decides nothing it would have to look up.
//
// The hook runs inside the engine, on a call OBVR does not own, and the less it
// does there the better: read one value, set one flag, leave.
bool g_castAimWanted = false;

// Set by the hook when the player's own cast begins, cleared by the camera pass
// that consumes it. It is a signal that a cast has STARTED - the turn itself is
// made much later, shortly before the animation ends. See NextCastArm.
bool g_castBegan = false;
CastArm g_castArm{};

// How long the last cast animation actually ran, in seconds, or zero until
// one has been watched to its end. What the next spell turns on - see
// CastArmDecision::measuredSeconds for why it cannot be a fixed number.
float g_castMeasuredSeconds = 0.0f;

// How often a spell's turn has been armed and made, and how many of those say
// so in the log.
//
// Counted per cast rather than reported once per run, because "the hook fired"
// was exactly the claim the one-line version could not support: a single line
// in a log of seven spells says nothing about the other six.
UInt32 g_castHookTurns = 0;
UInt32 g_castMisses = 0;
UInt32 g_castMeasurementsReported = 0;
constexpr UInt32 kCastHookReports = 5;

// Whether the hook actually went in. Without it the setting means nothing and
// the key window has to go on doing the turning - a refusal to patch must not
// leave spell aiming switched off by a setting that says it is on.
bool g_castHookInstalled = false;

// How long since the attack control was released, in seconds. Negative means
// nothing is being waited for - the control is held, or the last release has
// already been settled.
//
// It exists because releasing is not the same moment as shooting. The arrow
// leaves several frames after the control does, and the heading in between is
// the heading it leaves along.
float g_aimSecondsSinceRelease = -1.0f;

// Frames left of the shot trace: one line a frame from the moment the attack
// control goes up, saying what the engine's action is and whether the body is
// being held turned.
//
// It exists to answer one question with a measurement instead of a guess. The
// body is currently turned for the WHOLE attack, and the headset says that is
// too long: "fuer den zeitraum des schiessen laeuft er in die richtung wo ich
// ziele", and the bow "springt nach dem schuss zurueck mittig". The window has
// to shrink to the frame the arrow is actually made on - and which action
// change that is has been read out of an enum exactly once, with nothing to
// check it against.
//
// Forty frames is about two thirds of a second at sixty, which comfortably
// covers a bow release and its follow-through.
UInt32 g_shotTraceLeft = 0;
UInt32 g_shotTraceFrame = 0;
bool g_shotTraceHeld = false;

// The depth the crosshair quad is hung at, in metres, eased towards whatever
// is under the crosshair.
//
// Kept here rather than worked out where the overlay is submitted, because the
// easing needs the frame time and the projection needs the camera, and both of
// those live in the camera pass. The overlay pass reads the answer.
//
// Zero means "not yet decided", which is how the first frame gets the fallback
// without easing up to it from nowhere - a crosshair that slid out from the
// wearer's nose on every load would be a strange way to start.
float g_crosshairDepthMetres = 0.0f;

// The camera as the ENGINE left it, in world space, from the last pass that
// ran. Both taken together so they describe the same moment.
//
// World rather than local, because the reference under the crosshair has its
// position in world space and a difference between two spaces is not a
// distance. Read before OBVR writes the local transform, so this is last
// frame's world transform - the scene graph recomputes world from local after
// the hook, which is the whole reason the hook writes local at all. A frame of
// age is nothing here: the depth is eased over several frames anyway, and a
// crosshair one frame behind the head is not a thing eyes can see.
NiPoint3 g_cameraWorldPos{0.0f, 0.0f, 0.0f};

// This frame's camera placement as the engine left it, and the player's own
// position. Together they say what the viewpoint's step is measured against.
NiPoint3 g_cameraLocalPos{0.0f, 0.0f, 0.0f};
NiPoint3 g_playerWorldPos{0.0f, 0.0f, 0.0f};
bool g_playerWorldValid = false;

// What the arc correction moved the eye by this frame, for the trace. Its own
// variable rather than a recomputation, so what the log shows is the vector
// that was actually applied.
NiPoint3 g_aimArcApplied{0.0f, 0.0f, 0.0f};

NiMatrix33 g_cameraWorldRot = NiMatrix33::Identity();
bool g_cameraWorldValid = false;

// The crosshair probe's tally. It counts rather than samples, because the one
// number it exists for is a SHARE - how much of ordinary play has nothing under
// the crosshair, and so how much of it the fallback is showing.
//
// Roughly five seconds at sixty frames. Long enough that the share means
// something, short enough that walking into a room and looking round is
// several readings rather than one.
constexpr UInt32 kCrosshairProbeWindow = 300;

// Whether something activatable was under the crosshair on the last camera
// pass - which is exactly when Oblivion puts a context icon and a name on
// screen, and so exactly when the crosshair is of use to somebody who asked
// for it only then. The same reference the depth is taken from, so knowing
// this costs nothing.
bool g_crosshairHasTarget = false;

// The reference whose depth was used on the previous camera pass.  Its
// identity matters as well as have/don't-have: moving straight from a nearby
// NPC to a chest is still a new tooltip and must arrive at the new depth in
// that same frame rather than visibly sliding there.
UInt32 g_crosshairTargetAddress = 0;

bool g_crosshairProbeIntroduced = false;
UInt32 g_crosshairProbeFrames = 0;
UInt32 g_crosshairProbeWithMenu = 0;
UInt32 g_crosshairProbeWithRef = 0;
float g_crosshairProbeNearest = 0.0f;
float g_crosshairProbeFarthest = 0.0f;

// A frame number that counts every presented frame, not every camera pass.
//
// The redirect uses this to decide when to clear its texture - once per frame,
// never twice - and State::frameCount cannot serve, because the camera hook
// increments it and the camera hook does not run while a menu is up. The
// counter would stand still for the whole menu, the clear would never come
// round, and every frame's menu would be drawn on top of the last one: a
// cursor that smears a trail behind it, which is precisely the kind of fault
// that gets blamed on the compositor.
//
// Incremented at the end of the frame, so every redirect within one frame sees
// the same number and the next frame sees a new one.
UInt32 g_presentedFrame = 0;

// Whether the previous frame was a menu being held in the world.
//
// How many frames in a row arrived with neither a camera pass nor a menu,
// counting the ones BEFORE the current frame - the delivery reads it, then
// OnFrameEnd advances it. Up to kWorldlessBridgeFrames such strays are
// bridged with the held pair instead of flashing the cinema screen: the seam
// that closes a menu, and the gap a dialogue's exit transition leaves, which
// arrived as a split-second grey flash. See DeliverFrame.
//
// Advanced only in OnFrameEnd, deliberately: the redirect decision earlier
// in the same frame must see exactly the value the delivery will.
UInt32 g_worldlessStreak = 0;

// How many bridged strays are still reported. The line is the evidence for
// where the strays actually occur - the grey flash was diagnosed from theory
// because no trace covered it, and this is the trace that would have.
UInt32 g_bridgesReported = 8;

// The frame the current menu opened on, for telling a pause menu's held run
// from a dialogue's exit fade - see MenuDressingWanted. And one dressing
// report per menu episode, budgeted, because whether the dressing was
// granted or skipped is the evidence the washed-grey diagnosis rests on.
UInt32 g_menuOpenedFrame = 0;
bool g_dialogMenuEpisode = false;
bool g_dressingReportedThisMenu = false;
UInt32 g_dressingReportsLeft = 8;

// The menu-world probe's budget for the current menu episode. Refilled when
// a menu opens, spent one self-initiated render per held frame while
// Debug.MenuWorldProbe is on - see MenuWorldProbeWanted for why each gate
// exists.
UInt32 g_menuProbeAttemptsLeft = 0;

// World frames the scene graph probe still reports on, alongside the menu
// frames it reports on through the menu-world probe's budget. The comparison
// is the whole measurement - the same fields read where the render provably
// works and where it provably draws nothing - so a run has to carry both, and
// a handful of each is a comparison while every frame is a flood.
UInt32 g_sceneGraphWorldReportsLeft = 3;

// The control probe's budget: self-initiated renders on ordinary world
// frames, where the engine is drawing the scene perfectly well. See
// MaybeRunWorldControlProbe for what the comparison decides. Each one costs a
// whole wasted world render, so this stays small.
UInt32 g_worldControlProbesLeft = 3;

// The frame the layout probe last measured on, shared by both of its
// measurements - cinema and menu - because the cost being rationed is the
// same GPU stall either way.
UInt32 g_lastLayoutProbeFrame = 0;
UInt32 g_lastCursorProbeFrame = 0;

// Every fifth cursor probe also writes quarter-scale pictures of the layer
// texture and the back buffer next to the log - the direct look at where
// the cursor actually stands in each, for the offset hunt no bounding box
// could settle.
UInt32 g_cursorDumpTick = 0;

// Says, once, which part of the frame Oblivion is drawing into.
//
// OBVR now asks for a frame the game did not choose, so "the frame" and "the
// picture in it" are no longer the same rectangle by construction. If the
// viewport is smaller than the back buffer, everything copied outside it is
// black - and a menu cropped to a cinema shape out of a frame whose picture
// only fills part of it loses the wrong part.
void ReportViewportOnce(bool flat) {
	const int slot = flat ? 1 : 0;
	if (g_viewportReported[slot]) {
		return;
	}

	render::d3d9::Viewport viewport{};
	if (!render::ReadViewport(render::GetGameDevice(), viewport)) {
		return;
	}

	g_viewportReported[slot] = true;
	OBVR_LOG("Render: on a %s frame Oblivion draws into x=%u..%u y=%u..%u of the frame",
	         flat ? "menu" : "world", viewport.x, viewport.x + viewport.width, viewport.y,
	         viewport.y + viewport.height);
}

float Abs(float value) { return value < 0.0f ? -value : value; }

// Runs from inside Present, with Oblivion's finished frame in the back buffer.
//
// Declared ahead of OnFrameEnd, which needs it: the recenter key has to work
// on frames where the camera hook does not run, which is every video, the main
// menu, and every menu opened in game.
bool PollRecenterEdge();

// Carries out a recenter that something else has already decided happened.
// Defined next to MaybePollRecenter, declared here because OnFrameEnd needs it
// for the stereo frames no camera hook ran on.
void DoRecenter(const char* where);

// Pays the HUD overlay at the end of the frame: shows this frame's captured
// layer on a world frame, hides it on a flat one so a stale HUD does not hang
// in front of the menu the flat path is showing. Declared ahead of OnFrameEnd,
// defined next to the redirect callbacks it belongs with.
void MaybeSubmitOverlays(bool worldFrame);

// The menu-world probe, run at the very end of a menu frame so the frame's
// own submissions are already paid whatever the probe does. It runs after the
// frame's own EndScene - this is called from Present - so the render gets a
// scene bracket of its own; a draw outside one is an invalid call, not a slow
// one. Opened the way the sepia pass opens its bracket: the probe's EndScene
// is only owed when its BeginScene was accepted.
//
// Called from both menu paths, because both sit on the same stopped engine
// render and the question is about that, not about how the frame is shown.
// The picture lands in the back buffer - unread on a held frame, and the
// shown picture itself on a cinema one, so the monitor may show the world
// instead of the menu for these few frames. That is the probe being visible,
// not a fault.
// The probe's one render, wherever it is being asked from. Split out so the
// control below can ask the identical question at the identical point of the
// frame, with only the menu differing - which is the whole design of it.
void RunProbeRender(FrameDelivery delivery, const char* occasion) {
	// Before the render, so what is reported is the state the render is about
	// to walk rather than whatever the render left behind.
	render::ProbeSceneGraph(g_presentedFrame, occasion);

	void* const device = render::GetGameDevice();
	auto beginScene =
		render::d3d9::Method<render::d3d9::SceneBracketFn>(device, render::d3d9::kDeviceBeginScene);
	auto endScene =
		render::d3d9::Method<render::d3d9::SceneBracketFn>(device, render::d3d9::kDeviceEndScene);
	const bool bracketOpened = beginScene != nullptr && beginScene(device) >= 0;
	UInt32 draws = 0;
	UInt32 vertexSetup = 0;
	const char* const refusal = render::MenuWorldProbeRefusal();
	const bool ran = render::RunMenuWorldProbe(draws, vertexSetup);
	if (bracketOpened && endScene != nullptr) {
		endScene(device);
	}

	// The whole result is this line: a self-initiated render that draws makes
	// the live menu background an engineering task, one that runs empty makes
	// it a research task - the 2D pass under the dual pass set the precedent
	// for a pass that refuses. The refusal is named rather than left bare,
	// because a probe that fires in the main menu refuses for a reason that
	// says nothing about the question: it has never seen a world render.
	//
	// The occasion is what turns the number into evidence. A zero on a menu
	// frame accuses the menu only if the same call on a world frame, from the
	// same place, draws.
	if (ran) {
		// The vertex setup beside the draws is what makes a zero readable: no
		// setup either means the render turned back at some gate upstream, and
		// the work is finding it; setup without draws means it walked a scene
		// that had nothing in it.
		OBVR_LOG("Menu world probe: on a %s a self-initiated world render ran and made %u "
		         "draw call(s) with %u vertex setup call(s) - %s (%s frame, bracket %s, "
		         "frame %u)",
		         occasion, draws, vertexSetup,
		         draws > 0 ? "it draws"
		                   : (vertexSetup > 0 ? "it ran the pipeline but found nothing to draw"
		                                      : "it turned back before setting anything up"),
		         delivery == FrameDelivery::HeldStereo ? "held" : "cinema",
		         bracketOpened ? "opened" : "NOT opened", g_presentedFrame);
	} else {
		OBVR_LOG("Menu world probe: on a %s refused - %s (%s frame, bracket %s, frame %u)",
		         occasion, refusal, delivery == FrameDelivery::HeldStereo ? "held" : "cinema",
		         bracketOpened ? "opened" : "NOT opened", g_presentedFrame);
	}
}

void MaybeRunMenuWorldProbe(FrameDelivery delivery, bool menuIsUp) {
	if (!MenuWorldProbeWanted(GetConfig().menuWorldProbe, delivery, menuIsUp,
	                          g_menuProbeAttemptsLeft)) {
		return;
	}
	--g_menuProbeAttemptsLeft;
	RunProbeRender(delivery, "in Present, menu frame");
}

// The control the menu measurement was missing.
//
// The menu render draws nothing, and the obvious reading is that the menu is
// why. But the dual pass calls this same engine function twice in a row with
// no engine update between the two, and its second call draws - that is what
// stereo IS - so a render repeated without an update is not the problem by
// itself. What the menu probe changes is not only the menu: it also moves the
// call out of the engine's own render moment and into Present, after the
// frame's EndScene, frames away from anything the engine set up.
//
// Two suspects, one experiment. This runs the identical call, from the
// identical place in Present, on frames where the world is being drawn
// normally and no menu is anywhere near. If it draws here, the menu is the
// cause and the work is upstream of the menu. If it comes back empty here
// too, the menu is innocent and what breaks the render is where it is called
// from - a far cheaper thing to fix, and one that would have been missed by
// reading the menu numbers alone.
//
// The cost is a wasted render on a handful of frames and a back buffer
// briefly overwritten after both eyes have already been captured, so the
// headset never sees it; the monitor may flicker for those frames.
void MaybeRunWorldControlProbe(FrameDelivery delivery, bool menuIsUp, bool hadCameraPass) {
	if (!WorldControlProbeWanted(GetConfig().menuWorldProbe, menuIsUp, hadCameraPass,
	                            g_worldControlProbesLeft)) {
		return;
	}
	--g_worldControlProbesLeft;
	RunProbeRender(delivery, "in Present, world frame (control)");
}

// Deliberately does nothing but pay the frame that BeginFrame opened. Anything
// else that wanted doing at the end of a frame would be tempting to put here,
// and this runs on the renderer's thread inside a call the game is waiting on.
void OnFrameEnd();

// The Present hook's entry: the frame's delivery to the headset first, then
// the monitor's copy of the 2D layer.
//
// The copy comes after the delivery on purpose. The flat and held paths take
// their pictures from the back buffer inside OnFrameEnd, and a menu drawn
// onto it beforehand would reach the headset twice - once in the picture,
// once on the overlay. Drawn afterwards, only the monitor sees it.
//
// Both facts the copy depends on are read before the delivery: the overlay
// submit inside it consumes the layer's capture flag, and a menu can close
// on this very frame.
void OnPresent() {
	watchdog::Start();
	watchdog::NoteFrame();
	const Config& config = GetConfig();
	const bool layerCaptured = g_hudLayer.HasCapture();
	const bool menuIsUp = config.tracker.showMenus && game::IsMenuMode();
	game::SetNoPlayerStagger(config.look.noPlayerStagger);
	UpdateHandMode(config, game::IsMenuMode());
	OnFrameEnd();
	test::AdvanceWaterVRTest(game::PlayerInWorld() && !game::IsMenuMode());
	if (config.tracker.mirrorMenusToMonitor && menuIsUp && layerCaptured) {
		g_headsetRenderer.MirrorLayerToMonitor(render::GetGameDevice(),
		                                       g_hudLayer.CaptureTexture());
	}
}

void OnFrameEnd() {
	const Config& config = GetConfig();
	const bool liveMenuCanRun = LiveMenuBackgroundCanRun(
		config.tracker.liveMenuBackground, config.tracker.renderToHeadset,
		config.tracker.showMenus, g_headTracker.IsHeadsetConnected(),
		config.tracker.stereo == vr::StereoMode::DualPass,
		config.tracker.menusInWorld, config.tracker.hudOverlay,
		config.tracker.hudBetweenPasses, config.tracker.menuStandIn);

	// The engine's own live menu background, asked for every frame because the
	// INI is hot reloaded and the call is a byte comparison. It is enabled
	// only when the complete stereo-and-overlay path is available; otherwise
	// two extra world renders would buy an incomplete picture. Off leaves the
	// engine's setting alone rather than forcing the static background on -
	// see ApplyLiveMenuBackground.
	game::ApplyLiveMenuBackground(liveMenuCanRun);

	// The same shape for the simulation: asked every frame because the INI
	// is hot reloaded, redirected once, and the answer follows the option.
	game::ApplyUnpausedMenus(config.tracker.unpausedMenus);

	// Remembered for this frame's delivery before clearing the guard for the
	// next one. This is what distinguishes a fresh pause-menu stereo pair from
	// an ordinary stereo dialogue frame when the dressing is applied below.
	const bool liveMenuFrame = g_menuLiveThisFrame;
	g_menuLiveThisFrame = false;

	// Consumed, not merely read.
	//
	// This flag is set by the camera hook and nothing else, so on a frame
	// where that hook does not run - an inventory, an ESC menu, anything that
	// pauses the world - it would still be holding last frame's true. Present
	// would then take the normal path, EndFrame would find no frame open and
	// return at once, and nothing would reach the headset at all.
	//
	// Which is precisely what was reported: menus in game showed nothing,
	// while the main menu worked. The main menu works because the camera hook
	// has never run at that point, so the flag is still its initial false.
	const bool hadCameraPass = g_frameOpen;
	g_frameOpen = false;

	// Polled once, here, and used by whichever branch this frame takes.
	//
	// It used to be polled inside the branches, and the stereo branch had no
	// poll at all. That was invisible for as long as every stereo frame came
	// from the camera hook, because the hook polls the key itself - but a
	// stereo frame armed from inside the scene render has no camera pass, so
	// on those frames the key did nothing. Reported from the headset as not
	// being able to recenter during the intro films.
	//
	// Sharing one edge is what makes polling early safe: the camera hook runs
	// before this and consumes the press on an ordinary world frame, so this
	// reads false there and no branch acts twice.
	const bool recenterPressed = PollRecenterEdge();

	// Counted here because here is the one place that runs on every frame,
	// whatever else did or did not happen. Every return below is a frame that
	// still ended, so the increment goes before all of them.
	++g_presentedFrame;

	// What the game says, rather than what its timing suggests.
	//
	// A menu open does not stop Oblivion drawing the world behind it, and it
	// does not make it draw the world every frame either. So the camera hook
	// runs on some of those frames and not others, and deciding the mode from
	// that alone made the presentation alternate between a full stereo view
	// and a small flat rectangle - the menu snapping open and shut, at frame
	// rate, which is what opening the ESC menu looked like in the headset.
	// Gated on ShowMenus: with menus switched off there is no flat presentation
	// to hold steady, so the question does not arise.
	const bool menuIsUp = config.tracker.showMenus && game::IsMenuMode();
	const FrameDelivery delivery = DeliverFrame(
		hadCameraPass, menuIsUp,
		MenusCanReachTheWorld(config.tracker.menusInWorld, config.tracker.hudOverlay),
		g_headsetRenderer.HasHeldEyes(), g_worldlessStreak);
	perf::DeliveryMode profileMode = perf::DeliveryMode::Unknown;
	if (delivery == FrameDelivery::Cinema) {
		profileMode = perf::DeliveryMode::Flat;
	} else if (delivery == FrameDelivery::HeldStereo) {
		profileMode = perf::DeliveryMode::HeldMenu;
	} else if (menuIsUp) {
		profileMode = perf::DeliveryMode::LiveMenu;
	} else if (config.tracker.stereo == vr::StereoMode::DualPass) {
		profileMode = perf::DeliveryMode::WorldDual;
	} else if (config.tracker.stereo == vr::StereoMode::AlternateEyes) {
		profileMode = perf::DeliveryMode::WorldAer;
	} else {
		profileMode = perf::DeliveryMode::Mono;
	}
	perf::Profiler::Instance().SetFrameDetails(
		profileMode, perf::Profiler::Instance().CurrentVrFrameId(), -1, 0xFF, 0xFF, 0xFF);
	const RecenterPlan recenter = PlanRecenter(recenterPressed, delivery);
	bool recenterFrameOpen = false;
	if (recenter.freshPoseBeforeTracker && !hadCameraPass) {
		recenterFrameOpen =
			g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame());
		if (recenterFrameOpen) g_headTracker.Update(g_state.frameCount);
	}

	// The streak the NEXT frame's delivery will see: this frame joins it when
	// it was worldless, and any frame with a world - or a menu, whose held
	// run is a picture of its own - starts it over.
	const bool worldlessFrame = !hadCameraPass && !menuIsUp;
	if (worldlessFrame && delivery == FrameDelivery::HeldStereo && g_bridgesReported > 0) {
		--g_bridgesReported;
		OBVR_LOG("Render: a worldless frame was bridged with the held pair (streak %u, "
		         "frame %u)",
		         g_worldlessStreak, g_presentedFrame);
	}
	g_worldlessStreak = worldlessFrame ? g_worldlessStreak + 1 : 0;

	ReportViewportOnce(delivery == FrameDelivery::Cinema);

	// The world side of the scene graph comparison, taken on frames the engine
	// drew itself. Its menu side rides the menu-world probe's budget, below.
	if (GetConfig().menuWorldProbe && !menuIsUp && hadCameraPass &&
	    g_sceneGraphWorldReportsLeft > 0) {
		--g_sceneGraphWorldReportsLeft;
		render::ProbeSceneGraph(g_presentedFrame, "in Present, world frame");
	}

	// One line per frame for a short window after a menu opens or closes, with
	// every number the question needs in the same place: how the frame was
	// delivered, whether the camera pass ran, how many world renders happened
	// since the last frame, and whether the layer OBVR is about to show has
	// anything in it. A menu that is on the monitor and not in the headset is
	// one of those going wrong, and guessing which costs a session each time.
	// Which menu, and not only whether there is one. The persuasion minigame
	// opens from inside a dialogue, so menuIsUp is already true when it
	// appears and never changes across the step - which left the trace silent
	// about the one menu that has a bug in it. A change of type is a change of
	// menu whatever the flag does.
	//
	// A type of none is not a change: the field OBVR reads means "the menu the
	// mouse is over", so it empties whenever the cursor is not on the menu,
	// and treating that as a change would retrigger the trace all day.
	const UInt32 menuId = menuIsUp ? game::ActiveMenuId() : game::kMenuIdNone;
	const bool loadingFrame = menuId == game::kMenuIdLoading || game::LoadingThreadActive();
	g_dialogMenuEpisode = DialogMenuEpisode(
		g_dialogMenuEpisode, menuIsUp, menuId == game::kMenuIdDialog);
	const bool menuFlagChanged = menuIsUp != g_menuTraceWasUp;
	const bool menuTypeChanged = menuId != game::kMenuIdNone && menuId != g_menuTraceLastId;

	if (menuFlagChanged || menuTypeChanged) {
		g_menuTraceWasUp = menuIsUp;
		g_menuTraceLastId = menuId;
		g_menuTraceLeft = 12;
		render::ArmBetweenTrace();
		if (menuIsUp) {
			g_menuOpenedFrame = g_presentedFrame;
			g_dressingReportedThisMenu = false;
			g_menuProbeAttemptsLeft = kMenuWorldProbeAttempts;
		}
		OBVR_LOG("Menu trace: a menu just %s - %s (0x%03X), top of the stack 0x%03X",
		         menuFlagChanged ? (menuIsUp ? "opened" : "closed") : "changed",
		         game::MenuIdName(menuId), menuId, menuIsUp ? game::TopVisibleMenu() : 0u);
	}
	// Counted down once a frame, here rather than beside each mark: the marks
	// come several to a frame and would otherwise burn the window in two.
	if (g_stepTraceLeft > 0) {
		--g_stepTraceLeft;
	}

	const UInt32 menuAge = menuIsUp ? g_presentedFrame - g_menuOpenedFrame
	                                : kMenuDressingWindowFrames + 1;
	const MenuFrameDressing menuDressing = MenuDressingForFrame(
		delivery, menuIsUp, liveMenuFrame, menuAge, config.tracker.menuShade,
		config.tracker.menuSingleBorder, g_dialogMenuEpisode);
	const UInt32 menuShadeColor = menuDressing.shade
	                                  ? render::ComposeShadeColor(
		                                    config.tracker.menuShadeColorRgb,
		                                    config.tracker.menuShadeStrength)
	                                  : 0;

	if (g_menuTraceLeft > 0) {
		--g_menuTraceLeft;
		const UInt32 scene = render::CurrentSceneCall();
		const char* how = delivery == FrameDelivery::Stereo
		                      ? "stereo"
		                      : (delivery == FrameDelivery::Cinema ? "cinema" : "held");
		OBVR_LOG("Menu trace: %s, camera pass=%d, armed=%d, world renders this frame=%u "
		         "(scene call %u), layer captured=%d, held pair=%d",
		         how, hadCameraPass ? 1 : 0, g_dualArmed ? 1 : 0, scene - g_menuTraceLastScene,
		         scene, g_hudLayer.HasCapture() ? 1 : 0,
		         g_headsetRenderer.HasHeldEyes() ? 1 : 0);
		g_menuTraceLastScene = scene;
	} else {
		g_menuTraceLastScene = render::CurrentSceneCall();
	}

	if (delivery == FrameDelivery::Stereo) {
		g_flatFramesSinceCamera = 0;

		// A live pause-menu pair is fresh stereo, but it still wants the
		// desaturated sepia treatment that tells the player the world is
		// paused. The single black border belongs only to a held pair; the pure
		// decision above refuses it here so the fresh eyes keep their full view.
		// Dead: the whole picture grey (Look.DeathGrey).
		g_pendingRequest.menuShadeColor =
			ShadeForFrame(menuShadeColor, game::PlayerIsDead(), config.look.deathGrey,
			              config.look.deathMenuTint, config.tracker.menuShadeColorRgb);
		g_pendingRequest.menuSingleBorder = menuDressing.singleBorder;
		if (menuIsUp && liveMenuFrame && !g_dressingReportedThisMenu &&
		    g_dressingReportsLeft > 0) {
			g_dressingReportedThisMenu = true;
			--g_dressingReportsLeft;
			OBVR_LOG("Render: live stereo menu frames %s the sepia dressing and keep "
			         "their full eye borders",
			         menuDressing.shade ? "carry" : "skip");
		}

		// A stereo frame the camera hook never ran on - armed from inside the
		// scene render, for a menu the engine is drawing the world behind. On
		// an ordinary world frame the hook has already consumed the press and
		// this reads false; on these frames nothing else would act on it at
		// all, and the key was dead exactly where the wearer is most likely
		// to reach for it.
		if (recenterPressed) {
			DoRecenter("stand-in path");
		}

		// The layer is the HUD on an ordinary frame and the menu on a menu
		// one, and either way it is OBVR's to show: the redirect took it out
		// of the picture the eyes were captured from, so if the overlay does
		// not show it, nothing does.
		TraceStep("about to submit the eyes");
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), g_pendingRequest);
		TraceStep("eyes submitted");
		MaybeSubmitOverlays(true);
		TraceStep("overlays submitted");

		// Last, after the eyes and the overlay are paid, exactly as on a menu
		// frame - the point of the control is that everything about the call
		// matches except the menu.
		MaybeRunWorldControlProbe(delivery, menuIsUp, hadCameraPass);
		return;
	}

	// Not a stereo frame. Deliver something anyway.
	//
	// Without this the headset shows nothing at all while the main menu is up,
	// which means loading a save requires taking the headset off - and after
	// ten frames without a submit the compositor drops to its own Home scene,
	// so it is not even a black screen, it is somebody else's room.
	if (!GetConfig().tracker.showMenus) {
		return;
	}

	// A menu in the world on a frame where Oblivion did not redraw what is
	// behind it. The last pair of eyes goes out again, unchanged, with the
	// pose they were actually drawn from - and the overlay is deliberately
	// left alone, because touching it with nothing captured would hide it and
	// the menu would blink out on every frame the world did not redraw.
	//
	// Nothing here is stale: the world is paused, so the picture is still
	// true, and the compositor reprojects it for the head movement that did
	// happen. That is the whole reason this is a delivery of its own rather
	// than a fall back to the cinema screen, which is what used to make menus
	// snap open and shut at frame rate.
	if (delivery == FrameDelivery::HeldStereo) {
		if (recenter.tracker && (hadCameraPass || recenterFrameOpen)) {
			DoRecenter("held-menu path");
		}

		render::HeadsetRenderer::FrameRequest held;
		held.gameDevice = render::GetGameDevice();
		held.submitGameFrame = GetConfig().tracker.submitGameFrame;
		held.heldEyes = true;

		// The pause-menu dressing rides only on held frames whose menu just
		// opened. Both gates matter: a bridged stray frame has no menu at
		// all, and a dialogue - which IS a menu - holds only during its exit
		// fade, minutes after the DialogMenu opened; dressing that fade
		// painted it sepia, seen as a half-second washed-grey picture at the
		// end of every conversation. Colour and strength are folded here so
		// the renderer sees one word - zero, or the ARGB to paint.
		held.menuShadeColor = menuShadeColor;
		held.menuSingleBorder = menuDressing.singleBorder;

		// Once per menu episode: whether its held run wears the dressing.
		// This line is the evidence the washed-grey diagnosis rests on - a
		// dialogue's end should log "skips", a pause menu "carries".
		if (menuIsUp && !g_dressingReportedThisMenu && g_dressingReportsLeft > 0) {
			g_dressingReportedThisMenu = true;
			--g_dressingReportsLeft;
			OBVR_LOG("Render: held frames %s the menu dressing - the menu opened %u "
			         "frames ago%s",
			         (menuDressing.shade || menuDressing.singleBorder) ? "carry" : "skip",
			         menuAge,
			         MenuDressingWanted(menuAge)
			             ? ""
			             : " (a dialogue's exit fade, most likely)");
		}
		if (recenterFrameOpen ||
		    g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame())) {
			g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), held);
		}

		// The menu itself. These frames are where it is drawn - Oblivion stops
		// rendering the world entirely while a menu is up, so every menu frame
		// is one of these and none of them carries a camera pass.
		//
		// Only submitted when this frame captured something. Submitting with an
		// empty capture hides the overlay, and on a run of menu frames that
		// would blink the menu out and back on whatever rhythm the pass happens
		// to draw at; leaving it alone keeps the last good picture hanging,
		// which is exactly what a paused menu should do.
		if (g_hudLayer.HasCapture()) {
			// Before the submit, which consumes the capture flag. What the box
			// answers: whether the in-game menus lay out against the frame's
			// real size or the one the game believes - the two sizes stopped
			// being the same rectangle when the frame went eye-sized, and the
			// square-looking unclickable menus are the open symptom.
			if (LayoutProbeDue(GetConfig().layoutProbe, g_presentedFrame,
			                   g_lastLayoutProbeFrame)) {
				g_lastLayoutProbeFrame = g_presentedFrame;
				render::CoveredRect box{};
				const bool measured = render::MeasureCoveredRect(
					render::GetGameDevice(), g_hudLayer.CaptureSurface(),
					g_hudLayer.CaptureWidth(), g_hudLayer.CaptureHeight(),
					render::d3d9::kFormatA8R8G8B8, true, box);
				if (measured && box.covered > 0) {
					OBVR_LOG("Layout probe: the menu pass drew x=%u..%u y=%u..%u of the "
					         "%ux%u layer texture (%u px, frame %u)",
					         box.minX, box.maxX, box.minY, box.maxY,
					         g_hudLayer.CaptureWidth(), g_hudLayer.CaptureHeight(),
					         box.covered, g_presentedFrame);
				} else {
					OBVR_LOG("Layout probe: the menu measurement %s (frame %u)",
					         measured ? "found nothing drawn" : "was refused",
					         g_presentedFrame);
				}
			}
			MaybeSubmitOverlays(true);
		}

		// The cursor's side of the same question, on the same cadence as the
		// layout probe but its own switch: where the sprite is planted versus
		// where the game holds the mouse. Menu frames are where a cursor
		// exists to measure.
		if (LayoutProbeDue(GetConfig().cursorProbe, g_presentedFrame,
		                   g_lastCursorProbeFrame)) {
			g_lastCursorProbeFrame = g_presentedFrame;
			render::ProbeCursor(g_presentedFrame);
			if (++g_cursorDumpTick % 5 == 0) {
				const bool layerOk = render::DumpSurfaceBmp(
					render::GetGameDevice(), g_hudLayer.CaptureSurface(),
					g_hudLayer.CaptureWidth(), g_hudLayer.CaptureHeight(),
					render::d3d9::kFormatA8R8G8B8, "OBVR-layer.bmp");
				const bool backOk =
					render::DumpBackBufferBmp(render::GetGameDevice(), "OBVR-back.bmp");
				OBVR_LOG("Cursor dump: layer %s, back buffer %s (frame %u)",
				         layerOk ? "written" : "refused", backOk ? "written" : "refused",
				         g_presentedFrame);
			}
		}

		// Whether the 2D pass ran at all on this held frame, and what it drew.
		// The menus are reported missing from the headset while the trace shows
		// held frames submitting as designed, so the open question is this one.
		// Consuming the stats here steals them from the next scene-call line,
		// which is acceptable for the trace window's dozen frames.
		if (g_menuTraceLeft > 0) {
			UInt32 hudPasses = 0;
			UInt32 hudDraws = 0;
			render::TakeInterfaceStats(hudPasses, hudDraws);
			OBVR_LOG("Menu trace: on this held frame the 2D pass ran %u time(s) "
			         "and drew %u",
			         hudPasses, hudDraws);
		}

		MaybeRunMenuWorldProbe(delivery, menuIsUp);
		return;
	}

	// How many frames in a row have been flat.
	//
	// This was the diagnostic for the menu flicker and it did its job: it
	// showed bursts of up to seven, which is Oblivion presenting several times
	// per step. That alone is harmless - a frozen pose means each picture just
	// stands still until the next replaces it. What was not harmless was the
	// mode changing between them, and that is now decided by IsMenuMode rather
	// than by which of those frames happened to carry a camera pass.
	//
	// Kept, because it is the cheapest way to see that the run of flat frames
	// is now unbroken while a menu is open.
	++g_flatFramesSinceCamera;
	if (g_flatFramesSinceCamera > 1 && g_flatBurstsReported < 6) {
		++g_flatBurstsReported;
		OBVR_LOG("Render: %u flat frames since the last camera pass",
		         g_flatFramesSinceCamera);
	}

	// The recenter key, polled here because nothing else does on these frames.
	//
	// A flat picture is anchored where the head was when it appeared. If that
	// was mid-turn, or the wearer has since settled into a different position,
	// the picture hangs somewhere awkward and there is no way to move it - the
	// camera hook polls the key, and the camera hook is exactly what is not
	// running. An intro film that started while looking down stays down.
	if (recenter.tracker && (hadCameraPass || recenterFrameOpen)) {
		DoRecenter("flat path");
		if (recenter.flatAnchor) g_headsetRenderer.ResetFlatAnchor();
	}

	// The cinema-side layout measurement, taken while the finished frame is
	// still in the back buffer. What the box answers: whether films, loading
	// screens and the main menu lay out against the frame's real size or the
	// one the game believes - a picture sitting small in the top-left corner
	// of the buffer is the second of those, measured rather than presumed.
	if (LayoutProbeDue(GetConfig().layoutProbe, g_presentedFrame, g_lastLayoutProbeFrame)) {
		g_lastLayoutProbeFrame = g_presentedFrame;
		UInt32 bufferWidth = 0;
		UInt32 bufferHeight = 0;
		render::CoveredRect box{};
		const bool measured = render::MeasureBackBufferCoveredRect(
			render::GetGameDevice(), bufferWidth, bufferHeight, box);
		if (measured && box.covered > 0) {
			OBVR_LOG("Layout probe: this cinema frame's picture covers x=%u..%u "
			         "y=%u..%u of the %ux%u back buffer (%u px, frame %u)",
			         box.minX, box.maxX, box.minY, box.maxY, bufferWidth, bufferHeight,
			         box.covered, g_presentedFrame);
		} else {
			OBVR_LOG("Layout probe: the cinema measurement %s (frame %u)",
			         measured ? "found only black" : "was refused", g_presentedFrame);
		}
	}

	// The main menu is a cinema frame, and its unclickable buttons are the
	// sharpest form of the offset - so the cursor is measured here too.
	if (LayoutProbeDue(GetConfig().cursorProbe, g_presentedFrame, g_lastCursorProbeFrame)) {
		g_lastCursorProbeFrame = g_presentedFrame;
		render::ProbeCursor(g_presentedFrame);
		if (++g_cursorDumpTick % 5 == 0) {
			const bool backOk =
				render::DumpBackBufferBmp(render::GetGameDevice(), "OBVR-back.bmp");
			OBVR_LOG("Cursor dump: back buffer %s (frame %u)",
			         backOk ? "written" : "refused", g_presentedFrame);
		}
	}

	render::HeadsetRenderer::FrameRequest menu;
	menu.gameDevice = render::GetGameDevice();
	menu.submitGameFrame = GetConfig().tracker.submitGameFrame;
	menu.flatFrame = true;
	menu.backBufferIsThisFrame = true;
	menu.gameFovDegrees = GetConfig().tracker.gameFovDegrees;
	menu.gameFovIsFor4x3 = GetConfig().tracker.gameFovIsFor4x3;
	menu.cameraTanHalfWidth = g_state.cameraTanHalfWidth;
	menu.cameraTanHalfHeight = g_state.cameraTanHalfHeight;
	menu.menuScale = GetConfig().tracker.menuScale;
	menu.menuAspect = GetConfig().tracker.menuAspect;
	if (CinemaLoadingShadeWanted(delivery, loadingFrame, config.tracker.menuShade)) {
		menu.menuShadeColor = render::ComposeShadeColor(
			config.tracker.menuShadeColorRgb, config.tracker.menuShadeStrength);
	}

	// A camera pass means BeginFrame has already run for this frame and the
	// frame is open. Calling it again would call WaitGetPoses a second time,
	// which blocks until the next frame - the whole point of a menu being flat
	// is that it costs nothing extra.
	if (hadCameraPass || recenterFrameOpen ||
	    g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame())) {
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), menu);
		MaybeSubmitOverlays(false);
	}

	// After the submit, for the same reason it comes last on a held frame: the
	// probe's render lands in the back buffer this path has just shown, so
	// running it earlier would measure the world over the top of the very
	// picture being delivered. A cinema frame with a menu on it is the case a
	// run with a sleeping headset produces - stereo never arms, nothing is
	// ever held - and gating the probe on held frames alone meant it never
	// fired in exactly those runs.
	MaybeRunMenuWorldProbe(delivery, menuIsUp);
}

bool ReadIsThirdPerson() {
	auto* player = *reinterpret_cast<UInt8**>(addr::kPlayerPointer);
	if (player == nullptr) {
		return false;
	}
	return player[addr::kPlayerIsThirdPersonOffset] != 0;
}

bool RunHudPassWithCrosshairView() {
	const Config& config = GetConfig();
	const bool isThirdPerson = ReadIsThirdPerson();

	// A menu frame: the reticle - crosshair and action icon - is not drawn
	// into the layer at all. While a menu comes in the game still draws its
	// fading HUD into the same layer (the 2026-09-25 log: 20 HUD draws on a
	// dialogue's first frames, 81 against a container's own 64), and with
	// the centre no longer lifted out on a menu frame, the action icon
	// flashed into the menu's picture. Lifting the centre on those frames
	// instead cut a square out of the lock-pick menu. HUDReticle is hidden
	// for this one draw and put back as vanilla keeps it: shown in first
	// person, hidden in third (the root the third-person draw has to expose).
	if (game::IsMenuMode()) {
		const bool hid = game::SetHudReticleEnabled(false);
		const bool captured = render::RunHudPassBetweenScenes();
		if (hid) {
			game::SetHudReticleEnabled(!isThirdPerson);
		}
		static bool s_reported = false;
		if (!s_reported) {
			s_reported = true;
			OBVR_LOG("Crosshair: HUDReticle %s for the menu frames' 2D layer",
			         hid ? "hidden" : "could not be resolved - not hidden");
		}
		return captured;
	}
	if (!HudCrosshairNeedsFirstPersonView(config.tracker.crosshairTooltipsThirdPerson,
	                                       g_crosshairHasTarget, isThirdPerson)) {
		return render::RunHudPassBetweenScenes();
	}

	const UInt32 playerAddress = *reinterpret_cast<UInt32*>(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(playerAddress)) {
		return render::RunHudPassBetweenScenes();
	}

	// The world renders on either side of this call and therefore retains the
	// real third-person camera. Only the isolated 2D draw sees first person, so
	// Oblivion supplies the centre HUD pixels that the crosshair lift needs.
	auto* const thirdPerson = reinterpret_cast<UInt8*>(
		playerAddress + addr::kPlayerIsThirdPersonOffset);
	const UInt8 saved = *thirdPerson;
	*thirdPerson = 0;
	const bool wantsReticle = HudReticleForceWanted(
		config.tracker.crosshairTooltipsThirdPerson, g_crosshairHasTarget);
	const bool forcedReticle = wantsReticle && game::SetHudReticleEnabled(true);
	static bool s_reticleEnableReported = false;
	if (wantsReticle && !s_reticleEnableReported) {
		s_reticleEnableReported = true;
		OBVR_LOG("Crosshair: HUDReticle menu %s for the isolated third-person draw",
		         forcedReticle ? "enabled" : "could not be resolved");
	}
	const bool captured = render::RunHudPassBetweenScenes();
	if (forcedReticle) {
		game::SetHudReticleEnabled(false);
	}
	*thirdPerson = saved;

	static bool s_reported = false;
	if (!s_reported) {
		s_reported = true;
		OBVR_LOG("Crosshair: the isolated third-person HUD draw uses its first-person centre");
	}
	return captured;
}

void MaybeReloadConfig() {
	Config& config = GetConfig();
	if (!IsDue(g_state.frameCount, config.reloadEveryFrames)) {
		return;
	}

	if (config.Reload("OBVR.ini")) {
		perf::Profiler::Instance().Configure(config.performance);
		g_headTracker.Configure(config.tracker);
		g_lookControl.Configure(config.look);
	}
}

// Polls the recenter key once per frame.
//
// Polled rather than hooked, and the reason is robustness, not speed. The
// obvious alternative would be a WH_KEYBOARD_LL hook, but Microsoft documents
// three properties that rule it out here:
//
//   * "This hook is called in the context of the thread that installed it.
//     The call is made by sending a message to the thread that installed the
//     hook. Therefore, the thread that installed the hook must have a message
//     loop." OBVR is a plugin inside Oblivion and owns no message loop it can
//     service on its own terms.
//   * The hook sits in the system-wide input path. Every keystroke in every
//     application waits for it to sign off before the key state updates.
//   * "If the hook procedure times out ... on Windows 7 and later, the hook is
//     silently removed without being called. There is no way for the
//     application to know whether the hook is removed." A game that stutters
//     is precisely where a timeout happens, and OBVR could not even detect
//     that recentering had stopped working.
//
// Microsoft's own advice is to prefer raw input over low-level hooks. That
// would work, but it needs a window to register against and a message queue to
// drain - considerably more machinery than one call per rendered frame.
//
// Cost is not the argument either way: this runs once per frame rather than in
// a spin loop, so it is a single user32 call every 8 to 16 milliseconds.
//
// The edge itself is detected by KeyEdge in FrameLogic.h, from a stored
// previous state. GetAsyncKeyState does carry a "pressed since the last call"
// bit, but that bit is consumed process wide by whoever reads it first, so it
// cannot be relied on next to the game's own input handling.
//
// The key state is global rather than per window. That is harmless here:
// Oblivion pauses when it loses focus, so this callback does not run at all
// while another application has the keyboard.
// Whether the recenter key went down this call, without acting on it.
//
// Separate from MaybePollRecenter because on a flat frame there is no camera
// to recenter - the key means "put the picture where I am looking now"
// instead. Both share one KeyEdge, which is what stops a single press from
// counting twice when a menu opens on the frame the key is pressed.
bool PollRecenterEdge() {
	const UInt32 key = GetConfig().recenterKey;
	if (key == 0) {
		g_recenterEdge.Reset();
		return false;
	}

	const bool isDown = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	return g_recenterEdge.Update(isDown);
}

// Whether the attack control is being held right now - the moment at which
// aiming is actually happening, as opposed to merely having a weapon out.
//
// Held rather than an edge: this is a state for as long as it lasts, and on a
// bow it is the whole of the draw. No KeyEdge, therefore, and nothing is
// consumed - the recenter key is the one that must fire once per press.
//
// Read the same way the recenter key is, through GetAsyncKeyState, and for the
// same reasons: no window to register against, no message queue to drain, one
// call per frame. The default is the left mouse button, which is Oblivion's
// attack control unless the player has rebound it - hence the INI key. What
// this cannot see is a rebinding: it reads a virtual key, not the game's
// control map, so somebody who moved attack elsewhere has to say so in the
// INI. That is stated rather than solved, because reading the control map
// means another address to find.
bool AttackHeld() {
	const UInt32 key = GetConfig().aimAttackKey;
	if (key == 0) {
		return false;
	}
	return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
}

// The cast control, read the same way and for the same reason.
//
// A separate key rather than a mode of the attack: Oblivion binds Cast on its
// own, to C by default, and a spell readied does not change what the attack
// button does. Both can be down at once and it costs nothing - the window each
// opens is the same window.
bool CastHeld() {
	const UInt32 key = GetConfig().aimCastKey;
	if (key == 0 || !GetConfig().aimCastFollowsGaze) {
		return false;
	}
	return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
}

// The three callbacks of the scene render hook, in the order they run.
//
// All three fire on the game's thread, inside the frame whose camera pass
// armed them - after the camera hook, before Present. g_pendingRequest is
// therefore this frame's request, and the node pointer is this frame's node.

// The rung this frame runs on. Asked rather than read from the config so
// that DualPassProbe=9 can walk the rungs itself - see SweepProbeStage - and
// so that the trace, the second pass decision and the two callbacks below
// cannot disagree about which rung a frame belonged to.
UInt32 DualProbeRung() {
	return SweepProbeStage(render::CurrentSceneCall(), GetConfig().dualPassProbe);
}

// Places the camera on one eye for a menu frame, built from the transform the
// game itself last wrote rather than from whatever is in the node now.
//
// The node still holds the camera hook's last output - head offset and eye
// step included - and on a menu frame nothing overwrites it, so building on
// it would add a head pose to a head pose every frame and walk the camera out
// of the room within a second. g_menuBasePos and g_menuBaseRot are the game's
// own values, taken at the one moment they can be: after the engine wrote
// them and before OBVR did.
//
// The picture used to sit offset from where the world was the instant before
// the menu opened - reported from the headset on 2026-08-30. The cause was
// here, and it was the base rotation: this function started from the raw
// rotation the engine wrote, while a world frame is built on the levelled one
// the look control hands back, with the vertical tilt lifted out into a
// height. Since that rotation carries the head offset and the eye step as well
// as the facing, the whole viewpoint moved.
//
// Fixed at the source instead of here: g_menuBaseRot and
// g_menuBaseVerticalOffset are now the values the last world frame was
// actually built on, so this function starts from exactly what the world last
// looked like. Found by laying the two paths side by side - they agreed on
// everything else, which is what makes it the cause rather than a candidate -
// and not yet confirmed in the headset.
void PlaceMenuCamera(bool leftEye) {
	if (g_menuBaseNode == nullptr) {
		return;
	}

	const Config& config = GetConfig();

	// Read, never stepped. The look control smooths over time and no time is
	// passing here - the world is paused, and driving it from a menu would let
	// the camera drift on its own while the player reads.
	const NiMatrix33 baseRotation = g_menuBaseRot;
	const NiMatrix33 finalRotation = baseRotation * g_headTracker.GetCameraRotation();

	NiPoint3 pos = MenuCameraBase(g_menuBasePos, g_deathView, config.look.deathBodyView) +
	               baseRotation * g_headTracker.GetCameraOffset();
	pos.z += g_menuBaseVerticalOffset;

	// The same two numbers the camera hook uses, from the same place. They were
	// worked out separately here once, and the shift's sign was backwards: the
	// camera stepped to the left eye and then moved further left, so both eyes
	// came from the same side and everything doubled.
	const float half = ScaledEyeHalfSeparation(g_headTracker.GetHalfEyeSeparationUnits(),
	                                           config.tracker.eyeSeparationScale);
	const EyeStep step = StereoEyeStep(half, leftEye);

	g_menuBaseNode->localTransform.pos =
		pos + finalRotation * NiPoint3{step.toFirstEye, 0.0f, 0.0f};
	g_menuBaseNode->localTransform.rot = finalRotation;
	game::UpdateNodeTransforms(g_menuBaseNode);

	// The step to the other eye, on the same terms as the dual pass: what the
	// second pass moves the camera by, and what the bone lock rebases the
	// replayed palettes by.
	g_menuEyeShift = finalRotation * NiPoint3{step.toSecondEye, 0.0f, 0.0f};
}

// Stands in for the camera pass on a menu frame the engine renders itself.
//
// Only the arming is done here, not the rendering: the engine is already
// drawing the world, and all this adds is the pose it should have been drawn
// from and the two flags the rest of OBVR reads. After it, the frame is
// indistinguishable from a world frame - ScenePassWanted says yes, the
// callbacks capture both eyes, and DeliverFrame calls it stereo because
// g_frameOpen is set.
UInt32 DualProbeRung();

void PrepareMenuFrameIfNeeded(bool menuIsUp) {
	const Config& config = GetConfig();
	if (!MenuFrameNeedsCameraStandIn(config.tracker.menuStandIn,
	                                 config.tracker.stereo == vr::StereoMode::DualPass,
	                                 menuIsUp, g_headTracker.IsHeadsetConnected(),
	                                 g_menuBaseNode != nullptr, g_menuLiveThisFrame,
	                                 g_frameOpen)) {
		return;
	}

	if (!g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame())) {
		return;
	}
	g_headTracker.Update(g_state.frameCount);

	g_pendingRequest = render::HeadsetRenderer::FrameRequest{};
	g_pendingRequest.gameDevice = render::GetGameDevice();
	g_pendingRequest.submitGameFrame = config.tracker.submitGameFrame;
	g_pendingRequest.dualEyes = true;
	g_pendingRequest.gameFovDegrees = config.tracker.gameFovDegrees;
	g_pendingRequest.gameFovIsFor4x3 = config.tracker.gameFovIsFor4x3;
	g_pendingRequest.cameraTanHalfWidth = g_state.cameraTanHalfWidth;
	g_pendingRequest.cameraTanHalfHeight = g_state.cameraTanHalfHeight;

	// The camera onto the first eye, and the step to the other one, on the
	// same terms the camera hook sets them: the shift is what the second pass
	// moves by and what the bone lock rebases the replayed palettes by.
	const bool firstIsLeft = FirstPassDrawsLeftEye(config.swapEyeOrder);
	PlaceMenuCamera(firstIsLeft);
	g_dualShift = g_menuEyeShift;
	g_dualNode = g_menuBaseNode;
	g_dualArmed = true;
	g_frameOpen = true;
	g_menuLiveThisFrame = true;

	if (g_menuLiveReportsLeft > 0) {
		--g_menuLiveReportsLeft;
		// Whether the arming actually buys a second pass is the one thing this
		// cannot be assumed to have done: the same decision is asked again a
		// moment later with the menu settings folded in, and Menus=cinema or
		// HudOverlay=0 would answer no - leaving one eye drawn and the frame
		// delivered as if both were.
		const bool secondPass = WantsSecondScenePass(
			g_frameOpen, g_dualArmed, menuIsUp,
			MenusCanReachTheWorld(config.tracker.menusInWorld, config.tracker.hudOverlay),
			DualProbeRung());
		OBVR_LOG("Menu background: the frame was armed for stereo without a camera pass "
		         "(first eye %s, second pass %s, scene call %u, frame %u)",
		         firstIsLeft ? "left" : "right", secondPass ? "yes" : "NO - one eye only",
		         render::CurrentSceneCall(), g_presentedFrame);
	}
}

// One hand bone to its controller, or - adjusting the hands - held still in
// the world while its grip is closed, and fitted to the controller when the
// grip opens (game::StepHandAdjust, game::FitHandToPose). The fit is kept in
// the running config and written to the INI.
// The casting hand's effect at the pinned hand (the tester, 2026-09-29: "der
// zauber effekt bei heal zb erscheint irgendwo über der hand in der luft
// statt an der hand"). The engine hangs a spell's hand effect on the node
// "magicNode", looked up by name at each cast (0x005EDCB8, 0x00602D42) and
// given the effect as a child (0x00602D71). In the first-person skeleton
// that node is a child of Bip01 Spine2, and the cast animations key it
// (1stperson_castself.kf: its own translation track) to where the animated
// hand would be. The pins move the forearms and so the hands, not it: the
// effect stayed where the animation's hand would have been. So after the
// pins its local position is set to the middle of the pinned left palm -
// the game's casting hand - under its parent; its rotation stays the
// animation's.
UInt32 g_magicNodeLines = 3;

void PlaceMagicNodeAtCastingHand() {
	NiAVObject* const node = game::FindFirstPersonNode("magicNode");
	const NiAVObject* const wrist = game::FindFirstPersonNode("Bip01 L Hand");
	const NiAVObject* const knuckle = game::FindFirstPersonNode("Bip01 L Finger2");
	if (node == nullptr || wrist == nullptr || knuckle == nullptr || node->parent == nullptr) {
		if (g_magicNodeLines > 0) {
			--g_magicNodeLines;
			OBVR_LOG("Hands: the casting hand's effect node not placed - magicNode %s, the left hand %s, its "
			         "finger %s",
			         node != nullptr ? "found" : "missing", wrist != nullptr ? "found" : "missing",
			         knuckle != nullptr ? "found" : "missing");
		}
		return;
	}
	const NiAVObject* const parent = node->parent;
	game::BonePose wanted;
	wanted.rot = node->worldTransform.rot;
	wanted.pos = game::PalmCentre(wrist->worldTransform.pos, knuckle->worldTransform.pos);
	const game::BonePose local = game::LocalUnderParent(parent->worldTransform.rot, parent->worldTransform.pos,
	                                                    parent->worldTransform.scale, wanted);
	node->localTransform.pos = local.pos;
	game::UpdateNodeTransforms(node);
	if (g_magicNodeLines > 0) {
		--g_magicNodeLines;
		OBVR_LOG("Hands: the casting hand's effect node set to the left palm (%.1f %.1f %.1f, under a %s)",
		         static_cast<double>(wanted.pos.x), static_cast<double>(wanted.pos.y),
		         static_cast<double>(wanted.pos.z), game::NiClassNameOf(parent));
	}
}

bool PinAdjustableHand(bool right, const vr::HandSettings& hands, bool adjusting, bool handValid,
                       bool gripDown,
                       const NiMatrix33& relativeRot, const NiPoint3& offsetUnits,
                       const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                       const NiPoint3& sharedGripMetres, float perMetre) {
	static game::HandAdjustState s_adjust[2];
	game::HandAdjustState& adjust = s_adjust[right ? 0 : 1];
	if (!handValid) {
		adjust = game::HandAdjustState{};
		return false;
	}
	const float roll = right ? hands.rightHandRoll : hands.leftHandRoll;
	const float pitch = right ? hands.rightHandPitch : hands.leftHandPitch;
	const float yaw = right ? hands.rightHandYaw : hands.leftHandYaw;
	const NiPoint3 ownGrip = right ? NiPoint3{hands.rightHandGripX, hands.rightHandGripY,
	                                          hands.rightHandGripZ}
	                               : NiPoint3{hands.leftHandGripX, hands.leftHandGripY,
	                                          hands.leftHandGripZ};
	const NiPoint3 grip = (sharedGripMetres + ownGrip) * perMetre;
	const NiMatrix33 calibration = game::HandCalibration(roll, pitch, yaw);
	const char* const bone = right ? hands.rightHandBone : hands.leftHandBone;
	const game::BonePose current =
		game::HandBoneWorld(cameraRot, cameraPos, relativeRot, offsetUnits, calibration, grip);

	switch (game::StepHandAdjust(adjust, adjusting, gripDown, current)) {
	case game::HandAdjustStep::Follow:
		game::PinHandBone(right, bone, relativeRot, offsetUnits, calibration, cameraRot,
		                  cameraPos, grip);
		return false;
	case game::HandAdjustStep::Hold: {
		// The held pose through the same pin: a head-relative rotation and
		// offset that carry the camera to exactly the frozen world pose.
		const NiMatrix33 inverseCamera = InverseRotation(cameraRot);
		game::PinHandBone(right, bone, inverseCamera * adjust.frozen.rot,
		                  inverseCamera * (adjust.frozen.pos - cameraPos),
		                  NiMatrix33::Identity(), cameraRot, cameraPos, NiPoint3{0.0f, 0.0f, 0.0f});
		return false;
	}
	case game::HandAdjustStep::Commit:
		break;
	}

	const game::HandFit fit =
		game::FitHandToPose(cameraRot, cameraPos, relativeRot, offsetUnits, adjust.frozen);
	float newRoll = 0.0f;
	float newPitch = 0.0f;
	float newYaw = 0.0f;
	game::CalibrationAngles(fit.calibration, newRoll, newPitch, newYaw);
	const NiPoint3 newOwn = fit.gripUnits * (1.0f / perMetre) - sharedGripMetres;
	vr::HandSettings& live = GetConfig().hands;
	(right ? live.rightHandRoll : live.leftHandRoll) = newRoll;
	(right ? live.rightHandPitch : live.leftHandPitch) = newPitch;
	(right ? live.rightHandYaw : live.leftHandYaw) = newYaw;
	(right ? live.rightHandGripX : live.leftHandGripX) = newOwn.x;
	(right ? live.rightHandGripY : live.leftHandGripY) = newOwn.y;
	(right ? live.rightHandGripZ : live.leftHandGripZ) = newOwn.z;
	const char* const side = right ? "Right" : "Left";
	const float values[] = {newRoll, newPitch, newYaw, newOwn.x, newOwn.y, newOwn.z};
	const char* const suffixes[] = {"HandRoll", "HandPitch", "HandYaw",
	                                "HandGripX", "HandGripY", "HandGripZ"};
	bool saved = true;
	for (UInt32 i = 0; i < 6; ++i) {
		char key[32];
		char value[32];
		std::snprintf(key, sizeof(key), "%s%s", side, suffixes[i]);
		std::snprintf(value, sizeof(value), "%.4f", static_cast<double>(values[i]));
		saved = SaveSetting("Hands", key, value) && saved;
	}
	OBVR_LOG("Hands: %s hand adjusted - roll %.1f, pitch %.1f, yaw %.1f degrees, grip "
	         "%.3f/%.3f/%.3f m on top of the shared one%s",
	         right ? "right" : "left", static_cast<double>(newRoll), static_cast<double>(newPitch),
	         static_cast<double>(newYaw), static_cast<double>(newOwn.x),
	         static_cast<double>(newOwn.y), static_cast<double>(newOwn.z),
	         saved ? ", saved" : " - COULD NOT SAVE the INI");
	game::PinHandBone(right, bone, relativeRot, offsetUnits, fit.calibration, cameraRot, cameraPos,
	                  fit.gripUnits);
	return true;
}

// The first person weapon, turned at the last moment before anything is drawn.
//
// It has to be here rather than in the camera pass, and that is a measurement
// rather than a preference: the engine advances animation AFTER the camera hook
// runs, so a bone turned there is overwritten before it is ever drawn. The
// first attempt did exactly that and the headset saw nothing - "nein, er schaut
// nur nach vorne". By this point the animation has finished and what is written
// is what gets rendered.
//
// The angle was decided in the camera pass, where the head is known.
void BeforeFirstScenePass() {
	g_stumpStepped = false;
	// The hands go the moment a menu or a conversation starts, in the frame
	// that draws it: the decision at Present comes after that frame's world
	// is drawn, and a book or a dialogue opened with a weapon drawn showed the
	// hands for that one frame (2026-09-25). Present decides the same again
	// and keeps it; this only moves the first frame earlier.
	if (GetConfig().fullVrMode && !ReadIsThirdPerson() &&
	    (game::IsMenuMode() || game::DialogCameraCallPending() || g_handsAwayForDialog)) {
		// The same list Present builds with the hands away. A shorter one here
		// un-hid the drawn weapon every frame (HideFirstPersonNodes shows again
		// what is no longer listed), so it floated through the conversation.
		game::HideFirstPersonNodes(true, HandsHideList(GetConfig().hands, true, false));
	}
	// Snapshot the cyclopean world camera before the reflection subpass changes it.
	g_cyclopeanCameraWorldValid = mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(g_bodyCameraNode));
	if (g_cyclopeanCameraWorldValid) {
		g_cyclopeanCameraWorldTransform = g_bodyCameraNode->worldTransform;
		g_cyclopeanCameraWorldTransform.pos = game::CyclopeanCamera(g_bodyCameraNode->worldTransform.pos, g_bodyFirstEyeStep);
		g_cyclopeanCameraLocalTransform = g_bodyCameraNode->localTransform;
	}
	// And where NPCs look at the player (game::PlayerLookAt): these eyes,
	// not the first-person Camera01 that the hand mode carries with the arms.
	game::SetPlayerLookAtEyes(g_headTracker.IsHeadsetConnected(), g_cyclopeanCameraWorldValid,
	                          g_cyclopeanCameraWorldTransform.pos);
	// The other end of the same frame, and the one measurement left worth
	// taking. The camera pass logs what the two halves SHOULD be; this logs
	// what they are at the moment the picture is built - in particular whether
	// the heading written a moment ago has actually arrived in the player.
	//
	// The arithmetic is already known to be right: the trace shows body +
	// weapon constant across the handover in both directions. So a jump means
	// the two do not reach the picture together, and rotZ read HERE against
	// what the camera pass wrote is what says which one is late.
	// Same condition as the camera-pass line, so the draw is covered too. The
	// first version of this only ran while the countdown was going, which is
	// exactly the phase that had already run out during a long draw - so the
	// interesting frames had no render line at all.
	const bool tracing =
		GetConfig().aimShotTrace && (g_shotTraceLeft > 0 || g_shotTraceHeld);

	// Where the arms point BEFORE this frame's turn - which is where the
	// engine's animation just left them, body rotation and all.
	const float armsBefore = tracing ? game::FirstPersonArmsWorldYaw() : 0.0f;

	// The body first: it moves the whole third-person skeleton, and the
	// corrections below on Spine2 and Head want to sit on top of that. The
	// menu question is asked again here because the camera pass does not run
	// on menu frames, and a decision from before the menu opened would stand
	// the body under a camera the dialogue has since moved.
	{
		const Config::BodySettings& body = GetConfig().body;
		const bool wanted = g_bodyWanted && body.visible && !game::IsMenuMode() &&
		                    mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(g_bodyCameraNode));
		if (wanted) {
			game::BodyFrame frame;
			frame.cameraWorld = game::CyclopeanCamera(g_bodyCameraNode->worldTransform.pos,
			                                          g_bodyFirstEyeStep);
			frame.eyeOffsetUnits = NiPoint3{0.0f, body.eyeForward, body.eyeUp};
			frame.hideHead = body.hideHead;
			frame.hideArms = body.hideArms;
			game::ShowPlayerBody(frame);
		} else {
			game::ReleasePlayerBody(ReadIsThirdPerson());
		}
	}

	if (g_handArmsWanted) {
		game::PlaceFirstPersonArms(g_hand.armsRotation, g_hand.armsOffsetUnits);
		// After the arms, so the bones' parents carry this frame's placement:
		// the hand bones go where the controllers are, animation or not.
		// Relative to the head the hands were measured from: the camera the
		// camera pass wrote, cyclopean, snapshotted above.
		const vr::HandSettings& hands = GetConfig().hands;
		if (hands.pinHands && g_cyclopeanCameraWorldValid) {
			const NiMatrix33& cameraRot = g_cyclopeanCameraWorldTransform.rot;
			const NiPoint3& cameraPos = g_cyclopeanCameraWorldTransform.pos;
			const float perMetre = GetConfig().tracker.unitsPerMetre;
			const NiPoint3 sharedGrip{0.0f, hands.handGripForwardMetres, hands.handGripUpMetres};
			// Adjusting: from the INI switch or the guided window (game::HandAdjust).
			const bool adjusting = hands.adjustHands || game::HandAdjustActive();
			// A bare hand's wrist closed (BonePin.h, "Bare wrists") - not with the
			// forearm stump, which needs the forearm its own length.
			game::SetBareWristTaper(hands.closeBareWrists && !g_stumpShown);
			const bool rightCommitted = PinAdjustableHand(
				true, hands, adjusting, g_hand.rightHandValid, g_hand.rightGripDown,
			                  g_hand.rightHandRotation, g_hand.rightHandOffsetUnits, cameraRot,
			                  cameraPos, sharedGrip, perMetre);
			const bool leftCommitted = PinAdjustableHand(
				false, hands, adjusting, g_hand.leftHandValid, g_hand.leftGripDown,
			                  g_hand.leftHandRotation, g_hand.leftHandOffsetUnits, cameraRot,
			                  cameraPos, sharedGrip, perMetre);
			PlaceMagicNodeAtCastingHand();
			// The fingers close around what the engine holds for this hand, or
			// follow the controller's fingers while the hand is empty.
			const bool holding = g_grabKeyDown && game::PlayerHoldsGrab();
			const game::FingerPose rightPose = game::FingerPoseFor(
				hands.fingerTracking, g_hand.rightCurlValid,
				game::HandGripWanted(true, holding, g_hand.grabWithLeftHand),
				hands.fingerTracking && game::HandHoldsItem(true, hands.rightHandBone));
			const game::FingerPose leftPose = game::FingerPoseFor(
				hands.fingerTracking, g_hand.leftCurlValid,
				game::HandGripWanted(false, holding, g_hand.grabWithLeftHand),
				hands.fingerTracking && game::HandHoldsItem(false, hands.leftHandBone));
			game::FingerCurls rightCurls;
			game::FingerCurls leftCurls;
			for (int finger = 0; finger < 5; ++finger) {
				rightCurls.curl[finger] = g_hand.rightCurl[finger];
				leftCurls.curl[finger] = g_hand.leftCurl[finger];
			}
			rightCurls.thumbJoints = g_hand.rightThumbValid;
			leftCurls.thumbJoints = g_hand.leftThumbValid;
			for (int joint = 0; joint < 3; ++joint) {
				rightCurls.thumb[joint] = g_hand.rightThumb[joint];
				leftCurls.thumb[joint] = g_hand.leftThumb[joint];
			}
			game::StepHandFingers(true, hands.rightHandBone, rightPose, hands.gripCurlDegrees, &rightCurls);
			game::StepHandFingers(false, hands.leftHandBone, leftPose, hands.gripCurlDegrees, &leftCurls);
			// And a small held object sits fixed in that palm.
			{
				NiPoint3 touched{0.0f, 0.0f, 0.0f};
				const bool haveTouched = game::GrabStartHit(touched);
				game::HeldHand held;
				held.rightHand = !g_hand.grabWithLeftHand;
				held.palmAlongUnits = (game::kPalmAlongMetres + hands.heldObjectMetres) * perMetre;
				held.floatUnitsPerSecond = hands.pullSpeedMetres * perMetre;
				// The weapon's grip, the right hand's Weapon node, only while holding.
				if (holding && held.rightHand && !hands.levitateObjects) {
					const NiAVObject* const weaponNode = game::FindFirstPersonNode("Weapon");
					if (weaponNode != nullptr) {
						held.gripPoint = weaponNode->worldTransform.pos;
						held.haveGripPoint = true;
					}
				}
				game::StepHeldObject(!hands.levitateObjects || hands.attachSmallObjects, holding,
				                     held, haveTouched, touched, !hands.levitateObjects,
				                     g_deltaSeconds);
			}
			game::NoteHandAdjustFrame(rightCommitted, leftCommitted,
			                          g_hand.rightGripDown || g_hand.leftGripDown, g_deltaSeconds);
			// The hands are where the controllers are, not where the animation
			// would have them: their bounds follow, so the engine does not cull
			// a hand in view by an arm swung out of it. Arm's reach around the
			// camera - 100 units is about 1.4 m.
			game::KeepFirstPersonNodesInView("Hand", cameraPos, 100.0f);
			// And the forearm stump, now the forearms are where the pins put them.
			UInt32 armShapes = 0;
			UInt32 skinShapes = 0;
			game::CountArmShapes(armShapes, skinShapes);
			const bool sleeves = game::ArmsAreSleeves(armShapes, skinShapes);
			if ((sleeves ? 1 : 0) != g_stumpSleevesLogged && g_stumpLinesLeft > 0) {
				--g_stumpLinesLeft;
				OBVR_LOG("Hand bones: the arms are %s (%u arm shape(s), %u of skin) - %s", sleeves ? "sleeves" : "skin or none",
				         armShapes, skinShapes, sleeves ? "a bare hand gets the sleeve's stump" : "a bare hand gets the lid");
				g_stumpSleevesLogged = sleeves ? 1 : 0;
			}
			g_stumpShown = game::StepForearmStumps(
				game::StumpWanted(game::FirstPersonHandsBare(), sleeves, g_stumpHandsAway));
			g_stumpStepped = true;
			if (g_stumpShown) {
				game::KeepFirstPersonNodesInView("Arms", cameraPos, 100.0f);
			}
		}
	} else if (g_weaponTurnWanted) {
		game::StepHandFingers(true, GetConfig().hands.rightHandBone, game::FingerPose::Animation, 0.0f, nullptr);
		game::StepHandFingers(false, GetConfig().hands.leftHandBone, game::FingerPose::Animation, 0.0f, nullptr);
		game::StepHeldObject(false, false, game::HeldHand{}, false, NiPoint3{0.0f, 0.0f, 0.0f});
		game::TurnFirstPersonArms(g_weaponTurnRadians);
	} else {
		// Third person, a menu, or switched off. Put the arms back rather than
		// leaving them holding a turn nothing is going to update.
		game::StepHandFingers(true, GetConfig().hands.rightHandBone, game::FingerPose::Animation, 0.0f, nullptr);
		game::StepHandFingers(false, GetConfig().hands.leftHandBone, game::FingerPose::Animation, 0.0f, nullptr);
		game::StepHeldObject(false, false, game::HeldHand{}, false, NiPoint3{0.0f, 0.0f, 0.0f});
		game::ReleaseFirstPersonArms();
	}
	// The forearm stump (game/ArmStump.h): given back on any frame the pins
	// did not place it.
	if (!g_stumpStepped) {
		g_stumpShown = game::StepForearmStumps(false);
	}

	bool thirdPersonBodyApplied = false;
	if (g_thirdPersonAimVisualWanted) {
		thirdPersonBodyApplied =
			game::TurnThirdPersonAimVisual(g_thirdPersonAimVisualYaw,
			                               g_thirdPersonAimVisualPitch);
	} else {
		game::ReleaseThirdPersonAimVisual();
	}

	if (g_thirdPersonHeadVisualWanted) {
		game::TurnThirdPersonHeadVisual(
			g_thirdPersonHeadVisualYaw, g_thirdPersonHeadVisualPitch,
			thirdPersonBodyApplied ? g_thirdPersonAimVisualYaw : 0.0f,
			thirdPersonBodyApplied ? g_thirdPersonAimVisualPitch : 0.0f);
	} else {
		game::ReleaseThirdPersonHeadVisual();
	}

	if (tracing) {
		game::PlayerRotation atRender{};
		const bool read = game::ReadPlayerRotation(atRender);

		// ARMS AFTER is the only figure here that is not an intention: it is
		// read back out of the world transform once the turn has been applied,
		// so it is where the bow is actually pointing. With the head still it
		// must not move, and the frame it does is the jump.
		//
		// ARMS BEFORE says what the engine's animation had just left, body
		// rotation included - which is what tells apart "my turn is wrong" from
		// "the base I turned from moved under me".
		OBVR_LOG("Shot trace     render: arms before=%7.1f after=%7.1f | asked %6.1f wanted=%d "
		         "| rotZ=%.4f (wrote %.4f) offset=%.1f",
		         static_cast<double>(armsBefore * math::kRadiansToDegrees),
		         static_cast<double>(game::FirstPersonArmsWorldYaw() * math::kRadiansToDegrees),
		         static_cast<double>(g_weaponTurnRadians * math::kRadiansToDegrees),
		         g_weaponTurnWanted ? 1 : 0, read ? static_cast<double>(atRender.yaw) : -1.0,
		         static_cast<double>(g_aimYawWrote),
		         static_cast<double>(g_aimBodyOffset * math::kRadiansToDegrees));
	}
	// Dead, with the view held: the body drawn ahead of it instead of the view
	// stepped back (game::DeathBody), for this render only - AfterWorldRender
	// puts it back.
	{
		const Config& config = GetConfig();
		const bool wanted = g_deathView.held && g_deathView.haveAliveTurn && !game::IsMenuMode();
		const float perMetre = config.tracker.unitsPerMetre;
		game::ShiftDeadPlayerBody(
			wanted, wanted ? DeathBodyAhead(g_deathView.lastAliveRot,
			                                config.look.deathBodyAheadMetres * perMetre) +
			                     NiPoint3{0.0f, 0.0f, config.look.deathBodyUpMetres * perMetre}
			               : NiPoint3{0.0f, 0.0f, 0.0f});
	}
}

void AfterWorldRender() { game::RestoreDeadPlayerBody(); }

bool ScenePassWanted() {
	// The same menu question, asked of the same source, as the delivery
	// decision in OnFrameEnd. The two must agree on what kind of frame this
	// is: a frame bound for the cinema screen ignores the captures, so drawing
	// a second pass for it would be pure cost - while a menu delivered in the
	// world is a stereo frame like any other and wants both eyes.
	const Config& config = GetConfig();
	const bool menuIsUp = config.tracker.showMenus && game::IsMenuMode();

	// A menu frame the engine is rendering by itself. With its static menu
	// background cleared it draws the world behind the menu on every frame -
	// measured, the scene counter runs on where it used to stand still - but
	// the camera hook does not run with it, because that hangs off the
	// camera update the paused simulation never reaches. So the world is
	// drawn from wherever the camera stood when the menu opened, and nothing
	// has opened a compositor frame or armed the second pass.
	//
	// This is the first OBVR code inside the render, which makes it the place
	// to stand in for the missing camera pass: take a pose, put the camera on
	// the first eye, and arm the frame. Everything after it - the second
	// pass, the two captures, the stereo delivery - is the ordinary machinery
	// running on an ordinary armed frame, with nothing added.
	PrepareMenuFrameIfNeeded(menuIsUp);
	return WantsSecondScenePass(
		g_frameOpen, g_dualArmed, menuIsUp,
		MenusCanReachTheWorld(config.tracker.menusInWorld, config.tracker.hudOverlay),
		DualProbeRung());
}

// The first dual-pass run lost the GPU (VK_ERROR_DEVICE_LOST) somewhere in
// its first world frames, and a lost device is reported by the next
// submission rather than by its cause. These two narrow it down:
//
//   * the trace logs each stage of the first few dual frames, so the log
//     ends at - or shortly after - the stage that killed the run
//   * Debug.DualPassProbe cuts the mechanism down rung by rung, so one run
//     per rung names the culprit: the second render itself, the camera move,
//     or the captures
UInt32 g_dualTraceFramesLeft = 3;

bool DualTraceOn() { return g_dualTraceFramesLeft > 0; }

// Where the real controllers and this eye's camera stand in the world, for the
// headset renderer to draw the controllers into the eye's picture against its
// depth (render::ControllerModels::DrawIntoWorld). The controllers go through
// the same cyclopean camera the hands are pinned from; the eye is the camera
// node as this pass drew with it.
void HandWorldControllersToRenderer() {
	render::ControllerModels::WorldView view;
	const NiAVObject* const eye = g_dualNode != nullptr ? g_dualNode : g_bodyCameraNode;
	view.eyeValid = g_cyclopeanCameraWorldValid &&
	                mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(eye));
	if (view.eyeValid) {
		view.eyeRot = eye->worldTransform.rot;
		view.eyePos = eye->worldTransform.pos;
		const NiMatrix33& camRot = g_cyclopeanCameraWorldTransform.rot;
		const NiPoint3& camPos = g_cyclopeanCameraWorldTransform.pos;
		// Role right is the physical right controller unless left-handed
		// swapped the roles.
		view.hands[0].valid = g_hand.rightHandValid;
		view.hands[0].rightModel = !g_handRolesSwapped;
		view.hands[0].rot = camRot * g_hand.rightHandRotation;
		view.hands[0].pos = camPos + camRot * g_hand.rightHandOffsetUnits;
		view.hands[1].valid = g_hand.leftHandValid;
		view.hands[1].rightModel = g_handRolesSwapped;
		view.hands[1].rot = camRot * g_hand.leftHandRotation;
		view.hands[1].pos = camPos + camRot * g_hand.leftHandOffsetUnits;
	}
	view.unitsPerMetre = GetConfig().tracker.unitsPerMetre;
	view.tanHalfWidth = g_state.cameraTanHalfWidth;
	g_headsetRenderer.SetWorldControllers(view);
}

void BetweenScenePasses() {
	perf::EventContext betweenContext{};
	betweenContext.sceneId = render::CurrentSceneCall();
	betweenContext.passIndex = 0;
	perf::Profiler::ScopedSpan between(perf::Profiler::Instance(),
	                                  perf::EventType::BetweenPasses, betweenContext);
	const UInt32 probe = DualProbeRung();
	if (DualTraceOn()) {
		OBVR_LOG("Dual trace: first pass returned (probe %u)", probe);
	}

	// The finished first-eye picture is in the back buffer - tone mapping
	// done, 2D layer not yet drawn. Captured now, because at Present it will
	// have been drawn over twice.
	const bool firstIsLeft = FirstPassDrawsLeftEye(GetConfig().swapEyeOrder);

	// Reported whenever it changes, because a diagnostic that can be flipped
	// mid-game leaves no other mark in the log once the opening trace is
	// spent - and reading a run without knowing which order it ran in is
	// reading nothing.
	static int s_reportedOrder = -1;
	if (s_reportedOrder != (firstIsLeft ? 1 : 0)) {
		s_reportedOrder = firstIsLeft ? 1 : 0;
		OBVR_LOG("Dual pass: the %s eye is drawn first", firstIsLeft ? "left" : "right");
	}

	if (probe != 1 && probe != 2) {
		perf::EventContext captureContext = betweenContext;
		captureContext.eye = firstIsLeft ? 0 : 1;
		perf::Profiler::ScopedSpan capture(perf::Profiler::Instance(),
		                                  perf::EventType::EyeCapture, captureContext);
		HandWorldControllersToRenderer();
		const bool captured = g_headsetRenderer.CaptureEye(g_pendingRequest, firstIsLeft);
		perf::Profiler::Instance().SetFrameDetails(
				perf::DeliveryMode::Unknown, perf::Profiler::Instance().CurrentVrFrameId(), -1,
			captured ? 1 : 0, 0, 2);
		if (DualTraceOn()) {
			OBVR_LOG("Dual trace: %s eye captured from the first pass",
			         firstIsLeft ? "left" : "right");
		}
	}

	// The 2D layer, taken here because here is where it still draws. After
	// the second render the same pass is entered with every gate open and
	// draws nothing; before it, with one world rendered, it draws exactly
	// what a mono frame draws. See Tracker::hudBetweenPasses.
	//
	// After the capture above, so the layer never reaches the eye pictures,
	// and before the camera moves, so it is drawn from the viewpoint the
	// game itself computed.
	if (GetConfig().tracker.hudBetweenPasses) {
		perf::Profiler::ScopedSpan hud(perf::Profiler::Instance(),
		                              perf::EventType::HudBetween, betweenContext);
		const bool captured = RunHudPassWithCrosshairView();
		if (DualTraceOn()) {
			OBVR_LOG("Dual trace: the 2D layer was %s between the renders",
			         captured ? "captured" : "not captured");
		}
	}

	// To the other eye, the way the game itself moves the camera: edit the
	// local transform, then have the engine recompute the world transform
	// downward. Render re-reads the camera node's position at the start of
	// the pass to place the sky and LOD roots, so those follow on their own.
	if (probe != 1 && g_dualNode != nullptr) {
		perf::Profiler::ScopedSpan shift(perf::Profiler::Instance(),
		                                perf::EventType::EyeCameraShift, betweenContext);
		g_dualNode->localTransform.pos = g_dualNode->localTransform.pos + g_dualShift;
		game::UpdateNodeTransforms(g_dualNode);
		// The bone lock shifts the replayed palettes by the same vector the
		// camera just moved by - told here, the one place that knows whether
		// and how far the camera actually stepped this frame.
		render::SetBoneEyeShift(g_dualShift.x, g_dualShift.y, g_dualShift.z);
		if (DualTraceOn()) {
			OBVR_LOG("Dual trace: camera moved to the %s eye",
			         firstIsLeft ? "right" : "left");
		}
	} else {
		// The camera stays put, so the palettes must too.
		render::SetBoneEyeShift(0.0f, 0.0f, 0.0f);
	}
}

void AfterSecondScenePass() {
	perf::EventContext afterContext{};
	afterContext.sceneId = render::CurrentSceneCall();
	afterContext.passIndex = 1;
	perf::Profiler::ScopedSpan after(perf::Profiler::Instance(),
	                               perf::EventType::AfterSecondPass, afterContext);
	const UInt32 probe = DualProbeRung();
	if (DualTraceOn()) {
		OBVR_LOG("Dual trace: second pass returned");
	}

	const bool firstIsLeft = FirstPassDrawsLeftEye(GetConfig().swapEyeOrder);
	if (probe != 1 && probe != 2) {
		perf::EventContext captureContext = afterContext;
		captureContext.eye = firstIsLeft ? 1 : 0;
		perf::Profiler::ScopedSpan capture(perf::Profiler::Instance(),
		                                  perf::EventType::EyeCapture, captureContext);
		HandWorldControllersToRenderer();
		const bool captured = g_headsetRenderer.CaptureEye(g_pendingRequest, !firstIsLeft);
		perf::Profiler::Instance().SetFrameDetails(
				perf::DeliveryMode::Unknown, perf::Profiler::Instance().CurrentVrFrameId(), -1, 1,
			captured ? 1 : 0, 2);
		if (DualTraceOn()) {
			OBVR_LOG("Dual trace: %s eye captured from the second pass",
			         firstIsLeft ? "right" : "left");
		}
	}

	// Back where the game left it, and updated again, so everything that
	// reads the camera later in the frame - the 2D layer, next frame's
	// smoothing - sees the camera the game computed rather than an eye.
	if (probe != 1 && g_dualNode != nullptr) {
		perf::Profiler::ScopedSpan restore(perf::Profiler::Instance(),
		                                  perf::EventType::EyeCameraRestore, afterContext);
		g_dualNode->localTransform.pos = g_dualNode->localTransform.pos - g_dualShift;
		game::UpdateNodeTransforms(g_dualNode);
		if (DualTraceOn()) {
			OBVR_LOG("Dual trace: camera restored");
		}
	}

	if (g_dualTraceFramesLeft > 0) {
		--g_dualTraceFramesLeft;
	}

	g_dualArmed = false;
}

// The two callbacks of the interface render hook, and the submit that pays
// them. All on the game's thread: the redirect between the world render and
// Present, the submit inside Present.

void* HudBeginRedirect() {
	const Config& config = GetConfig();
	if (!config.tracker.hudOverlay) {
		return nullptr;
	}

	// The delivery this frame is heading for, worked out from exactly what
	// OnFrameEnd will work it out from. Every input is stable across the frame:
	// g_frameOpen is not cleared until OnFrameEnd reads it, and the held pair
	// only changes when a dual submit runs, which is later than this.
	//
	// Asked here rather than re-deciding, because the redirect and the delivery
	// disagreeing is not a subtle fault - it is a menu that exists in neither
	// the frame nor the overlay.
	const bool menuIsUp = config.tracker.showMenus && game::IsMenuMode();
	const FrameDelivery delivery = DeliverFrame(
		g_frameOpen, menuIsUp,
		MenusCanReachTheWorld(config.tracker.menusInWorld, config.tracker.hudOverlay),
		g_headsetRenderer.HasHeldEyes(), g_worldlessStreak);
	if (!WantsHudRedirect(delivery)) {
		// Part of the menu trace, because "the redirect said no" and "the
		// pass never ran" look identical from the outside - a menu on the
		// monitor and not in the headset - and only the log can tell them
		// apart.
		if (g_menuTraceLeft > 0) {
			OBVR_LOG("Menu trace: redirect declined - delivery=%s",
			         delivery == FrameDelivery::Cinema ? "cinema" : "stereo/held");
		}
		return nullptr;
	}

	// The first few invocations with their frame numbers, because "how often
	// does this pass run per frame" turned out to be the question: two
	// invocations sharing a frame number is the wipe that emptied the
	// texture, written down as numbers.
	static UInt32 s_invocationsTraced = 0;
	if (s_invocationsTraced < 6) {
		++s_invocationsTraced;
		OBVR_LOG("Hud: redirect invocation %u on frame %u", s_invocationsTraced,
		         g_state.frameCount);
	}

	void* const surface = g_hudLayer.BeginCapture(render::GetGameDevice(), g_presentedFrame);
	if (g_menuTraceLeft > 0) {
		OBVR_LOG("Menu trace: redirect granted - delivery=%s, surface=%p",
		         delivery == FrameDelivery::HeldStereo ? "held" : "stereo", surface);
	}
	return surface;
}

void HudEndRedirect() { g_hudLayer.EndCapture(); }

// Whether the probe instruments should run this pass: the recognisable
// clear through the binding, and the first-draw pipeline sample. Hot
// reloaded with the rest of [Debug], like the probe square it belongs to.
bool HudProbeActive() { return GetConfig().hudProbe; }

// Reads the keys that drive the settings menu and acts on them, then puts the
// menu in front of the wearer.
//
// Runs on every frame rather than only on world frames, and that is deliberate:
// the point of a settings menu in the headset is to be reachable without taking
// it off, which includes while a game menu is up or a film is playing. It is
// also why the keys are read here through GetAsyncKeyState rather than from
// anything the game provides - the camera hook does not run on those frames.
// Writes a setting the menu just changed back into OBVR.ini.
//
// Not merely so the change survives a quit. ReloadEveryFrames re-reads the file
// while the game runs, so a value held only in memory is overwritten by the
// file within a couple of seconds - which is precisely what the first run of
// this menu did, and from inside the headset it looked like the arrow keys were
// being ignored. Writing to the file makes the file agree, and then the reload
// has nothing to undo.
//
// Nothing to do when null, which is every movement and every press against the
// end of a range.
void SaveChangedSetting(const ui::SettingDefinition* definition, const Config& config) {
	static bool s_saveFailureReported = false;

	if (definition == nullptr) {
		return;
	}

	// A button: fired here, saved nowhere. The only one so far is the
	// recenter at the top of the menu.
	if (definition->kind == ui::ItemKind::Action) {
		if (definition->action == ui::SettingAction::FitHolsters) {
			g_holsterFitRequested = true;
			if (g_settingsMenu.IsOpen()) {
				g_settingsMenu.Toggle();  // the fit needs the triggers and the view
			}
		}
		if (definition->action == ui::SettingAction::AdjustHands) {
			game::StartHandAdjust();
		}
		if (definition->action == ui::SettingAction::Recenter) {
			DoRecenter("settings menu");
		}
		return;
	}

	char text[32];
	FormatValueForIni(ui::ItemFor(*definition, config), definition->falseWord,
	                  definition->trueWord, text, sizeof(text));

	if (SaveSetting(definition->iniSection, definition->iniKey, text)) {
		// Every write, named: a setting changed by a laser click that nobody
		// meant is otherwise found a day later by reading the INI, as the
		// 2026-09-07 run's BlockVerticalLook and PositionalTracking were.
		OBVR_LOG("Menu: wrote %s.%s=%s to OBVR.ini", definition->iniSection, definition->iniKey,
		         text);
		return;
	}

	// Once. A read-only INI would otherwise fill the log with one line per
	// keypress, and the wearer would still be looking at a menu that appears to
	// work while nothing sticks.
	if (!s_saveFailureReported) {
		s_saveFailureReported = true;
		OBVR_LOG("Menu: could not write %s.%s to OBVR.ini - changes will hold until the next "
		         "reload of the file and then go back. A read-only INI, or one a mod manager "
		         "is not passing writes through, would both do this.",
		         definition->iniSection, definition->iniKey);
	}
}

// The sticks' arrow presses the hand-tracked mode decided this frame, taken
// once: whichever menu polls first gets them, and a second poll in the same
// frame finds nothing, so one push of a stick moves one highlight.
vr::StickNavVerdict TakeHandMenuNavigation() {
	const vr::StickNavVerdict taken = g_hand.settingsNav;
	g_hand.settingsNav = vr::StickNavVerdict{};
	return taken;
}

// The laser on OBVR's own panel: the row it points at takes the highlight,
// the way a mouse hovers, and the pointing hand's pull is that row's own
// click - a toggle flips, a number steps down on its left half and up on
// its right, a button fires. The pixel comes from the hand mode, which
// met the pointing hand's ray with the panel's quad; which row that pixel
// is on is the painter's answer, since the painter laid the rows out.
// Both of OBVR's menus have the same shape here, hence the template.
SInt32 g_panelHoveredRow = -1;

template <typename Menu>
void PointAtPanel(Menu& menu, Config& config, const ui::MenuItem* items,
                  const char* const* categories, UInt32 count) {
	if (!g_hand.settingsPointerValid) {
		g_hand.settingsClick = false;
		g_panelHoveredRow = -1;
		return;
	}
	UInt32 canvasWidth = 0;
	UInt32 canvasHeight = 0;
	ui::SettingsMenuLayer::CanvasSize(canvasWidth, canvasHeight);
	const SInt32 row = ui::RowAtPixel(items, categories, count, menu.State(), canvasWidth,
	                                  canvasHeight, ui::SettingsMenuLayer::Scale(),
	                                  static_cast<SInt32>(g_hand.settingsPointerX),
	                                  static_cast<SInt32>(g_hand.settingsPointerY));
	const bool click = g_hand.settingsClick;
	g_hand.settingsClick = false;
	if (row < 0) {
		g_panelHoveredRow = -1;
		return;
	}
	// The highlight follows the beam onto a NEW row, and only then: hovered
	// every frame it would pull the highlight straight back from wherever
	// the stick just moved it - the first headset run could not scroll.
	if (row != g_panelHoveredRow) {
		g_panelHoveredRow = row;
		menu.Hover(static_cast<UInt32>(row));
	}
	// Only the row that actually took the highlight is clicked: a text row
	// the walkthrough refuses stays as it is.
	if (click && menu.State().selected == static_cast<UInt32>(row)) {
		const float xFraction = canvasWidth > 0
		                            ? g_hand.settingsPointerX / static_cast<float>(canvasWidth)
		                            : 0.5f;
		const ui::MenuAction action =
			ui::ClickActionFor(items[static_cast<UInt32>(row)], xFraction);
		if (action != ui::MenuAction::None) {
			SaveChangedSetting(menu.Apply(action, config), config);
		}
	}
}

// The walkthrough: opened once per start while [Onboarding] ShowAtStart is
// on and a headset is connected, steered with the same arrow keys as the
// settings menu - or the sticks in the hand-tracked mode - and drawn on its
// own layer. While it is open the settings menu leaves the arrows alone.
// Answers whether it consumed the keys.
bool PollOnboarding(const Config& config) {
	if (game::NativeMenuSuppressesLegacy()) {
		return false;
	}
	if (!g_onboardingOffered && config.onboardingShowAtStart &&
	    g_headTracker.IsHeadsetConnected()) {
		g_onboardingOffered = true;
		g_onboardingLayer.SetIdentity("obvr.onboarding", "OBVR Introduction");
		g_onboarding.Open();
		OBVR_LOG("Onboarding: the walkthrough opens - Onboarding.ShowAtStart=0 or its last page "
		         "keeps it closed next time");
	}

	const auto down = [](UInt32 key) {
		return key != 0 && (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	};

	const bool open = g_onboarding.IsOpen();
	if (open) {
		g_onboarding.SetVisibleRows(g_onboardingLayer.VisibleRows());
		Config& writable = GetConfig();
		{
			ui::MenuItem pointed[16];
			const char* pointedCategories[16];
			const UInt32 pointedCount =
				g_onboarding.BuildRows(GetConfig(), pointed, pointedCategories, 16);
			PointAtPanel(g_onboarding, writable, pointed, pointedCategories, pointedCount);
		}
		const vr::StickNavVerdict sticks = TakeHandMenuNavigation();
		if (g_menuUpEdge.Update(down(0x26)) || sticks.up) {
			g_onboarding.Apply(ui::MenuAction::Up, writable);
		}
		if (g_menuDownEdge.Update(down(0x28)) || sticks.down) {
			g_onboarding.Apply(ui::MenuAction::Down, writable);
		}
		if (g_menuLeftEdge.Update(down(0x25)) || sticks.left) {
			SaveChangedSetting(g_onboarding.Apply(ui::MenuAction::Decrease, writable), writable);
		}
		if (g_menuRightEdge.Update(down(0x27)) || sticks.right) {
			SaveChangedSetting(g_onboarding.Apply(ui::MenuAction::Increase, writable), writable);
		}
		if (!g_onboarding.IsOpen()) {
			OBVR_LOG("Onboarding: finished - the introduction will %s at the next start",
			         GetConfig().onboardingShowAtStart ? "open again" : "stay closed");
		}
	}

	ui::MenuItem items[16];
	const char* categories[16];
	const UInt32 count = g_onboarding.BuildRows(GetConfig(), items, categories, 16);
	g_onboardingLayer.SetTitle(g_onboarding.Title());
	g_onboardingLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                         g_onboarding.IsOpen(), items, categories, count,
	                         g_onboarding.State(), g_onboarding.Revision(),
	                         config.settingsMenuDistanceMetres, config.settingsMenuWidthMetres,
	                         config.settingsMenuInWorld);
	return open;
}

void PollSettingsMenu() {
	const Config& config = GetConfig();

	const auto down = [](UInt32 key) {
		return key != 0 && (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	};

	if (PollOnboarding(config)) {
		// The walkthrough has the arrows; the settings menu is not opened
		// over it either, so the two never fight for the same keys. A stick
		// chord or a menu button pressed meanwhile is dropped rather than
		// kept for the moment the walkthrough closes.
		g_menuToggleEdge.Reset();
		g_hand.settingsMenuToggle = false;
		return;
	}

	if (game::NativeSettingsAvailable()) {
		if (g_hand.settingsMenuToggle) {
			g_hand.settingsMenuToggle = false;
			game::RequestNativeSettingsToggle();
		}
		if (game::TakeNativeRecenterRequest()) {
			DoRecenter("native settings menu");
		}
		return;
	}

	// The key, or both sticks clicked together in the hand-tracked mode.
	bool toggled = false;
	if (config.settingsMenuKey == 0) {
		g_menuToggleEdge.Reset();
	} else if (g_menuToggleEdge.Update(down(config.settingsMenuKey))) {
		toggled = true;
	}
	if (g_hand.settingsMenuToggle) {
		g_hand.settingsMenuToggle = false;  // consumed: one press, one toggle
		toggled = true;
	}
	if (toggled) {
		g_settingsMenu.Toggle();
		OBVR_LOG("Menu: the settings menu is now %s",
		         g_settingsMenu.IsOpen() ? "open" : "closed");
	}

	// The arrows are read only while the menu is open. Oblivion leaves them
	// unbound, but a mod may not, and consuming nothing is the polite way to
	// share a keyboard.
	if (g_settingsMenu.IsOpen()) {
		// The layer owns the texture and therefore knows how many rows fit, so
		// it is asked rather than told - two places computing this separately
		// is two places for the highlight to scroll a row early or a row late.
		g_settingsMenu.SetVisibleRows(g_settingsMenuLayer.VisibleRows());

		Config& writable = GetConfig();
		{
			ui::MenuItem pointed[ui::SettingsMenu::kRowCapacity];
			const char* pointedCategories[ui::SettingsMenu::kRowCapacity];
			const UInt32 pointedCount =
				g_settingsMenu.BuildRows(GetConfig(), pointed, pointedCategories,
				                         ui::SettingsMenu::kRowCapacity);
			PointAtPanel(g_settingsMenu, writable, pointed, pointedCategories, pointedCount);
		}
		const vr::StickNavVerdict sticks = TakeHandMenuNavigation();
		if (g_menuUpEdge.Update(down(0x26)) || sticks.up) {  // VK_UP
			g_settingsMenu.Apply(ui::MenuAction::Up, writable);
		}
		if (g_menuDownEdge.Update(down(0x28)) || sticks.down) {  // VK_DOWN
			g_settingsMenu.Apply(ui::MenuAction::Down, writable);
		}
		if (g_menuLeftEdge.Update(down(0x25)) || sticks.left) {  // VK_LEFT
			SaveChangedSetting(g_settingsMenu.Apply(ui::MenuAction::Decrease, writable),
			                   writable);
		}
		if (g_menuRightEdge.Update(down(0x27)) || sticks.right) {  // VK_RIGHT
			SaveChangedSetting(g_settingsMenu.Apply(ui::MenuAction::Increase, writable),
			                   writable);
		}
	} else {
		g_menuUpEdge.Reset();
		g_menuDownEdge.Reset();
		g_menuLeftEdge.Reset();
		g_menuRightEdge.Reset();
	}

	// Built fresh every frame rather than kept, so a value changed from
	// somewhere else - the INI hot reload, most likely - shows here instead of
	// the menu holding a stale copy.
	ui::MenuItem items[ui::SettingsMenu::kRowCapacity];
	const char* categories[ui::SettingsMenu::kRowCapacity];
	const UInt32 count =
		g_settingsMenu.BuildRows(GetConfig(), items, categories, ui::SettingsMenu::kRowCapacity);

	g_settingsMenuLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                           g_settingsMenu.IsOpen(), items, categories, count,
	                           g_settingsMenu.State(), g_settingsMenu.Revision(),
	                           config.settingsMenuDistanceMetres,
	                           config.settingsMenuWidthMetres, config.settingsMenuInWorld);
}

void MaybeSubmitOverlays(bool worldFrame) {
	const Config& config = GetConfig();

	PollSettingsMenu();

	// Dead: the HUD and the crosshair go until the load menu opens
	// (HudHiddenForDeath) - the game keeps drawing its HUD for seconds.
	const bool hiddenForDeath = HudHiddenForDeath(config.look.hideHudWhenDead,
	                                              game::PlayerIsDead(), game::IsMenuMode());

	// The crosshair first, and outside the HUD's two gates on purpose: it is
	// not drawn by the interface pass, so whether that pass is redirected has
	// nothing to say about it. Submitted even when switched off, because an
	// overlay that has been shown once stays shown until something hides it,
	// and that something is this call.
	//
	// The menu question is asked here of the same source everything else asks,
	// rather than inferred from worldFrame. They are not the same thing: with
	// Menus=world a dialogue is delivered in stereo, so worldFrame is true
	// with a menu wide open, and the crosshair used to hang there through
	// every conversation.
	CrosshairVisibility visibility;
	visibility.enabled = config.tracker.crosshair;
	visibility.worldFrame = worldFrame;
	visibility.menuIsUp = config.tracker.showMenus && game::IsMenuMode();
	visibility.onlyWhenNeeded = config.tracker.crosshairOnlyWhenNeeded;
	visibility.onlyWhenNeededThirdPerson = config.tracker.crosshairOnlyWhenNeededThirdPerson;
	visibility.thirdPerson = ReadIsThirdPerson();
	visibility.somethingAimedAt = g_crosshairHasTarget;

	// ASKED ONLY WHEN THE ANSWER IS USED, which is the shape every raw read of
	// the game's object model in OBVR now has. Reading the player's process
	// means following a pointer into an object the engine tears down and
	// rebuilds - on a cell change, a load, a point-of-view switch - and the
	// checks around it cannot tell a half-built object from a finished one.
	// A question nobody asked is not worth that risk, and with the restriction
	// switched off nobody is asking.
	//
	// Unknown counts as drawn. A crosshair wrongly present is a much smaller
	// fault than one wrongly missing while somebody is lining up a shot.
	visibility.weaponDrawn = true;
	if (CrosshairOnlyWhenNeededApplies(visibility)) {
		visibility.weaponDrawn = game::ReadPlayerWeaponState() != game::WeaponState::Sheathed;
	}
	// The sneak eye is the game's, drawn where the crosshair is: wanted while
	// sneaking even with the weapon away, and in third person it is the live
	// eye rather than the remembered crosshair. Asked on world frames only.
	if (worldFrame && !visibility.menuIsUp) {
		visibility.sneaking = game::IsPlayerSneaking();
	}

	const bool crosshairWanted = CrosshairWanted(visibility);
	const bool tooltipsEnabled = visibility.thirdPerson
	                               ? config.tracker.crosshairTooltipsThirdPerson
	                               : config.tracker.crosshairTooltipsFirstPerson;
	const bool tooltipAboveName = TooltipAboveNameWanted(
		config.tracker.crosshairTooltipsAboveName, visibility.thirdPerson,
		g_crosshairHasTarget, tooltipsEnabled);

	// Oblivion's own crosshair, lifted out of the captured layer and into the
	// depth quad - which is also what takes it out of the flat one, so it is
	// not shown twice at two distances.
	//
	// On every playable frame with the feature enabled, including the hidden
	// branch of "only when needed".  Hiding only this overlay would otherwise
	// leave Oblivion's original crosshair behind in the flat HUD.  Menu and held
	// frames are excluded by CrosshairCaptureWanted, so their centre is never
	// punched out.
	bool crosshairLifted = false;
	if (CrosshairCentreCaptureWanted(config.tracker.crosshair,
	                                g_crosshairHasTarget, tooltipsEnabled,
	                                worldFrame, visibility.menuIsUp) &&
	    config.tracker.hudOverlay &&
	    g_hudLayer.HasCapture()) {
		UInt32 believedWidth = 0;
		UInt32 believedHeight = 0;
		render::GameBelievedSize(believedWidth, believedHeight);

		// The square follows the picture rather than being a fixed count of
		// pixels - Oblivion's interface scales with the frame, and a fixed
		// square stopped covering the crosshair as the resolution rose.
		const UInt32 sourcePixels = CrosshairSourcePixels(
			believedHeight > 0 ? believedHeight : g_hudLayer.CaptureHeight(),
			config.tracker.crosshairSourceShare);

		crosshairLifted = g_crosshairLayer.TakeFromHud(
			render::GetGameDevice(), g_hudLayer.CaptureSurface(), g_hudLayer.CaptureWidth(),
			g_hudLayer.CaptureHeight(), believedWidth, believedHeight, sourcePixels);
	}

	// Third person shows THE GAME'S crosshair, borrowed from first person.
	//
	// Oblivion draws none in third person - vanilla behaviour, stated on
	// Bethesda's own support page - so there is nothing in the layer to lift
	// there. But the picture exists: the engine draws it every frame in first
	// person, where it is lifted anyway. So it is kept from the view that has
	// one and put back in the view that does not, and what shows is the real
	// crosshair rather than an approximation of it - the player's own
	// replacement texture included.
	//
	// Kept only while nothing is under the crosshair and the player is not
	// sneaking. A target makes the lifted square a context icon; sneaking makes
	// it the eye. Both are live state, and persisting either would freeze the
	// wrong picture into a later ordinary third-person frame.
	if (crosshairLifted && !visibility.thirdPerson && !g_crosshairHasTarget &&
	    !visibility.sneaking) {
		g_crosshairLayer.RememberCrosshair(render::GetGameDevice(),
		                                  config.tracker.crosshairPersistentCache);
	}

	if (crosshairLifted && tooltipAboveName) {
		UInt32 believedWidth = 0;
		UInt32 believedHeight = 0;
		render::GameBelievedSize(believedWidth, believedHeight);
		const UInt32 sourcePixels = CrosshairSourcePixels(
			believedHeight > 0 ? believedHeight : g_hudLayer.CaptureHeight(),
			config.tracker.crosshairSourceShare);
		const bool placedAboveName = g_crosshairLayer.PutTakenAboveName(
			render::GetGameDevice(), g_hudLayer.CaptureSurface(),
			g_hudLayer.CaptureWidth(), g_hudLayer.CaptureHeight(), believedWidth,
			believedHeight, sourcePixels);
		static bool s_aboveNameReported = false;
		if (!s_aboveNameReported) {
			s_aboveNameReported = true;
			OBVR_LOG("Crosshair: contextual reticle %s the lower-right HUD position",
			         placedAboveName ? "copied to" : "could not reach");
		}
	}

	// The live centre survives when it is already the wanted picture: plain
	// reticle, sneak eye, or an enabled action tooltip. A remembered clean
	// reticle replaces it only when a target tooltip was disabled or moved to
	// the lower-right HUD; that keeps those settings independent.
	const CrosshairContent content = CrosshairContentWanted(
		crosshairWanted, g_crosshairHasTarget, tooltipsEnabled,
		tooltipAboveName, visibility.thirdPerson, visibility.sneaking);
	if (crosshairLifted && content == CrosshairContent::RememberedCrosshair) {
		// If this session has not captured a clean first-person crosshair yet,
		// leave the overlay empty. Drawing an OBVR substitute here would make a
		// mod-made reticle appear instead of the player's Oblivion crosshair.
		crosshairLifted = g_crosshairLayer.UseRememberedCrosshair(
			render::GetGameDevice(), config.tracker.crosshairPersistentCache);
	}

	// The depth was decided in the camera pass, where the camera and the frame
	// time are. A zero means no camera pass has run yet - the main menu, the
	// first frames of a load - and the fixed distance stands in until one has.
	const float crosshairDepth = g_crosshairDepthMetres > 0.0f
	                                 ? g_crosshairDepthMetres
	                                 : config.tracker.crosshairDistanceMetres;
	const CrosshairPlacement crosshair =
		PlaceCrosshair(crosshairDepth, config.tracker.crosshairSizeAtOneMetre);
	// In the hand-tracked mode the aim is the right hand's, so the crosshair
	// and the tooltip it carries hang ahead of that controller.
	// On the hand the pick follows, the one the tooltip belongs to.
	const bool crosshairLeft = g_hand.pickWithLeftHand;
	const bool crosshairOnHand =
		config.fullVrMode && (crosshairLeft ? g_hand.leftAimValid : g_hand.aimValid);
	g_crosshairLayer.SetHandPlacement(
		crosshairOnHand,
		g_headTracker.GetBackendForFrame().HandDeviceIndex(
			vr::HandDeviceForRole(!crosshairLeft, g_handRolesSwapped)),
		config.hands.laserPitchDegrees,
		crosshairLeft ? -config.hands.laserYawDegrees : config.hands.laserYawDegrees,
		config.hands.laserOriginMetres);
	g_crosshairLayer.SetRoomPlacement(
		g_reachIconShown, g_reachIconPose,
		HandTooltipWidth(render::kReachIconWidthMetres, true, config.hands.tooltipScale));
	g_crosshairLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                        crosshairLifted && content != CrosshairContent::Hidden &&
	                            !hiddenForDeath,
	                        crosshair.distanceMetres,
	                        HandTooltipWidth(crosshair.widthMetres, crosshairOnHand,
	                                         config.hands.tooltipScale));

	// Snap turn vignette: fades in when a snap fires, out after. Updated every frame
	// so the fade advances even when nothing is happening (keeps it hidden).
	g_vignetteLayer.Update(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                       (config.look.snapTurnVignette || config.hands.teleport.vignette) && worldFrame,
	                       g_deltaSeconds,
	                       config.look.snapTurnVignetteRadius, config.look.snapTurnVignetteStrength);

	// The laser beam from the hand that points at a menu, as long as the way
	// to it. Its own overlay, raw pixels, no game texture behind it.
	g_laserLayer.Submit(g_headTracker.GetBackendForFrame(),
	                    g_hand.laserVisible,
	                    g_headTracker.GetBackendForFrame().HandDeviceIndex(vr::HandDeviceForRole(g_hand.laserRight, g_handRolesSwapped)),
	                    config.hands.laserPitchDegrees,
	                    g_hand.laserRight ? config.hands.laserYawDegrees
	                                      : -config.hands.laserYawDegrees,
	                    config.hands.laserOriginMetres, g_hand.laserLengthMetres,
	                    config.hands.laserBeam, config.hands.laserDot);

	if (!config.tracker.hudOverlay || !render::IsInterfaceRenderHooked()) {
		return;
	}
	g_hudLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                  worldFrame && !hiddenForDeath, config.tracker.hudDistanceMetres,
	                  config.tracker.hudWidthMetres, config.tracker.hudAnchorWorld,
	                  config.hudProbe);
}

// What the recenter key does to a frame that has a camera, without asking
// whether it was pressed.
//
// Split from the poll because two callers need the action and only one of them
// can afford to consume the edge: the camera hook polls it before the frame
// ends, and a stereo frame armed from inside the scene render has no camera
// hook to have done so. See the single poll at the top of OnFrameEnd.
void DoRecenter(const char* where) {
	g_headTracker.Recenter();

	// A HUD hanging in the room is brought back in front of the wearer by the
	// same key, because "put things where I am looking now" is the one thing
	// that key means. Harmless when the HUD rides the head: there is no
	// anchor to drop.
	g_hudLayer.ResetAnchor();

	// Recentering is meant to take effect at once. Easing the camera into the
	// new zero would be the opposite of what the key is pressed for.
	g_lookControl.Reset();
	OBVR_LOG("Camera: recentered on key 0x%02X (frame %u, %s)", GetConfig().recenterKey,
	         g_state.frameCount, where);
}

void MaybePollRecenter() {
	if (!PollRecenterEdge()) {
		return;
	}
	DoRecenter("camera path");
}

// Moves the crosshair's depth towards whatever is under the crosshair.
//
// The reading and the arithmetic are elsewhere on purpose - game::
// ReadCrosshairTarget goes through the game's own object model and cannot be
// tested, CrosshairDepth is plain values and is tested exhaustively. What is
// left here is the joining, the easing, and the log.
void UpdateCrosshairDepth(const Config& config, float deltaSeconds) {
	const float fallback = config.tracker.crosshairDistanceMetres;

	// Read whenever anything wants it, not only for the depth. Two features now
	// rest on the same reference - where the crosshair sits, and whether it is
	// shown at all - and tying the read to the first would have left the second
	// silently dead whenever the depth was switched off.
	const bool wantTarget = CrosshairTargetReadWanted(
		config.tracker.crosshairDynamic,
		config.tracker.crosshairOnlyWhenNeeded ||
			config.tracker.crosshairOnlyWhenNeededThirdPerson,
		config.tracker.crosshairProbe,
		config.tracker.crosshairInThirdPerson ||
			config.tracker.crosshairTooltipsFirstPerson ||
			config.tracker.crosshairTooltipsThirdPerson);

	const game::CrosshairTarget target =
		wantTarget ? game::ReadCrosshairTarget() : game::CrosshairTarget{};
	g_crosshairHasTarget = target.haveRef;
	const UInt32 targetAddress = target.haveRef ? target.refAddress : 0;
	const bool immediateTargetDepth =
		CrosshairTargetNeedsImmediateDepth(g_crosshairTargetAddress, targetAddress);
	g_crosshairTargetAddress = targetAddress;

	if (!config.tracker.crosshairDynamic) {
		// Straight to the fixed distance rather than eased towards it. Turning
		// the feature off in the settings menu should show the difference at
		// once, or the comparison it exists for cannot be made.
		g_crosshairDepthMetres = fallback;
		if (!config.tracker.crosshairProbe) {
			return;
		}
	}

	CrosshairDepthInput input;
	input.haveTarget = target.haveRef && g_cameraWorldValid;
	input.cameraPosition = g_cameraWorldPos;
	input.gazeDirection = ForwardOf(g_cameraWorldRot);
	input.targetPosition = target.position;
	input.unitsPerMetre = config.tracker.unitsPerMetre;
	input.fallbackMetres = fallback;

	const float wanted = CrosshairDepth(input);

	// Worked out even with the dynamic depth switched off, because the probe
	// reports it - but not APPLIED then, or turning the feature off while the
	// probe ran would quietly turn it back on.
	if (config.tracker.crosshairDynamic) {
		// The first frame arrives rather than eases. Easing from zero would
		// slide the crosshair out from the wearer's face on every load.
		if (g_crosshairDepthMetres <= 0.0f || immediateTargetDepth) {
			g_crosshairDepthMetres = wanted;
		} else {
			g_crosshairDepthMetres =
				Approach(g_crosshairDepthMetres, wanted, config.tracker.crosshairDepthSpeed,
				         deltaSeconds);
		}
	}

	if (!config.tracker.crosshairProbe) {
		return;
	}

	// The first frame says whether this works at all, once and immediately.
	//
	// Deliberately not on the periodic clock below: if the global does not lead
	// to HUDInfoMenu in this build, that is the finding, and waiting several
	// seconds to be told it - or missing it because the run was short - would
	// be a poor way to learn it. rejectedId is the first clue where it does
	// lead instead.
	if (!g_crosshairProbeIntroduced) {
		g_crosshairProbeIntroduced = true;
		if (target.haveMenu) {
			OBVR_LOG("Crosshair depth: HUDInfoMenu at %08X identified itself, "
			         "tile menu array entry %08X",
			         target.menuAddress, target.arrayEntry);
		} else {
			OBVR_LOG("Crosshair depth: %08X is not HUDInfoMenu - it answers id %04X, "
			         "wanted %04X. The crosshair keeps its fixed distance",
			         target.menuAddress, target.rejectedId,
			         static_cast<UInt32>(game::kMenuIdHudInfo));
		}
	}

	// COUNTED, not sampled. The question this probe exists for is what SHARE of
	// ordinary play has nothing under the crosshair - that is the share of the
	// time the fallback is showing, and it decides whether this feature is
	// enough on its own or wants the depth buffer after all. A line every so
	// often would answer that only if somebody tallied the lines by hand, and
	// would answer it badly, because the moments worth counting are exactly the
	// ones a periodic sample walks past.
	++g_crosshairProbeFrames;
	if (target.haveMenu) {
		++g_crosshairProbeWithMenu;
	}
	if (target.haveRef) {
		++g_crosshairProbeWithRef;
		if (g_crosshairProbeNearest == 0.0f || wanted < g_crosshairProbeNearest) {
			g_crosshairProbeNearest = wanted;
		}
		if (wanted > g_crosshairProbeFarthest) {
			g_crosshairProbeFarthest = wanted;
		}
	}

	if (g_crosshairProbeFrames < kCrosshairProbeWindow) {
		return;
	}

	const float frames = static_cast<float>(g_crosshairProbeFrames);
	OBVR_LOG("Crosshair depth over %u frames: menu found in %.0f%%, something aimed at in "
	         "%.0f%% - nearest %.2f m, farthest %.2f m, showing %.2f m now",
	         g_crosshairProbeFrames,
	         static_cast<double>(100.0f * static_cast<float>(g_crosshairProbeWithMenu) / frames),
	         static_cast<double>(100.0f * static_cast<float>(g_crosshairProbeWithRef) / frames),
	         static_cast<double>(g_crosshairProbeNearest),
	         static_cast<double>(g_crosshairProbeFarthest),
	         static_cast<double>(g_crosshairDepthMetres));

	g_crosshairProbeFrames = 0;
	g_crosshairProbeWithMenu = 0;
	g_crosshairProbeWithRef = 0;
	g_crosshairProbeNearest = 0.0f;
	g_crosshairProbeFarthest = 0.0f;
}

// Puts the cast hook in, or explains in the log why it did not go in.
//
// A REFUSAL IS NOT A FAILURE HERE. If the bytes are not what this build
// expects - a different game version, or another mod that got to the same
// function first - the patch is skipped and spell aiming falls back to the key
// window it had before. That is worse aiming, not broken aiming, and it is
// vastly better than writing a jump into an instruction stream that turned out
// to be somebody else's.
//
// Declared before the callback it points at, which is defined below.
void InstallCastHook();

}  // namespace

// Called from the trampoline at the top of MagicCaster::CastMagicItem, with
// `this` - the caster - passed through from ecx.
//
// THIS IS THE WHOLE OF WHAT THE HOOK EXISTS FOR. A spell leaves along the
// caster's heading, and so does walking, so aiming by turning the heading makes
// the character walk the way the spell went for as long as the turn stands.
// What this call is good for is saying WHEN, not WHERE.
//
// MagicCaster::CastMagicItem is the start of a cast, and the log says so
// plainly: the hook fires on a frame whose action field still reads None, and
// the animation that follows runs 53 frames of Attack before the field turns
// to AttackFollowThrough. A heading turned here is turned nine tenths of a
// second before the spell leaves, and given back long before it. Two runs in
// the headset showed exactly that - spells going forwards while the hook
// reported turn after turn.
//
// So nothing is turned here any more. The call arms a clock, the turn is made
// shortly before the animation is due to end, and from that moment it is held
// and returned by the machinery the bow already has. See NextCastArm.
//
// Runs inside the engine, on a call OBVR does not own. It reads one value the
// camera pass has already worked out, sets one flag, and leaves - no
// allocation, no arithmetic, nothing that can throw.
extern "C" void __cdecl OBVR_OnMagicCastItem(void* caster) {
	if (!g_castAimWanted || caster == nullptr) {
		return;
	}

	const auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	const UInt32 playerAddress = reinterpret_cast<UInt32>(player);
	if (!mem::LooksLikeObjectAddress(playerAddress)) {
		return;
	}

	// Unsigned on purpose. A caster that sits BELOW the player wraps to an
	// enormous number and misses, which is the answer wanted: it is not a base
	// subobject of the player, so it is somebody else casting.
	//
	// Written down rather than learned - see kPlayerMagicCasterOffset. Learning
	// it needed a cast OBVR could already prove was the player's, which meant
	// the cast key held; a spell cast without that key went out unturned. In
	// one recorded run the offset was not learned until the seventh window, so
	// six spells left before this could do anything at all, and from the
	// outside that is indistinguishable from a hook that does not work.
	const UInt32 delta = reinterpret_cast<UInt32>(caster) - playerAddress;
	if (delta != addr::kPlayerMagicCasterOffset) {
		return;
	}

	g_castBegan = true;
}

namespace {

// Big enough for the trampoline above with room to spare; it is under thirty
// bytes and a page is the smallest thing that can be allocated anyway.
constexpr UInt32 kCastTrampolineSize = 64;

void InstallCastHook() {
	if (!mem::Verify(addr::kHookMagicCastItem, kCastOriginalBytes,
	                 addr::kHookMagicCastItemPatchSize)) {
		OBVR_LOG("Aim: bytes at %08X are not MagicCaster::CastMagicItem's prologue - the cast "
		         "hook is not installed, and spells fall back to the key window",
		         addr::kHookMagicCastItem);
		mem::ReportForeignCode("Aim", addr::kHookMagicCastItem);
		return;
	}

	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kCastTrampolineSize));
	if (trampoline == nullptr) {
		OBVR_LOG("Aim: no executable memory for the cast trampoline");
		return;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize =
		BuildCastTrampoline(trampoline, kCastTrampolineSize, trampolineAddress,
	                        reinterpret_cast<UInt32>(&OBVR_OnMagicCastItem));
	if (trampolineSize == 0) {
		OBVR_LOG("Aim: cast trampoline does not fit into %u bytes", kCastTrampolineSize);
		return;
	}

	UInt8 patch[addr::kHookMagicCastItemPatchSize];
	const UInt32 patchSize =
		BuildCastPatch(patch, sizeof(patch), addr::kHookMagicCastItem, trampolineAddress);
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Aim: cast patch has unexpected length %u", patchSize);
		return;
	}

	if (!mem::SafeWrite(addr::kHookMagicCastItem, patch, patchSize)) {
		OBVR_LOG("Aim: SafeWrite to %08X failed", addr::kHookMagicCastItem);
		return;
	}

	g_castHookInstalled = true;
	OBVR_LOG("Aim: cast hook installed at %08X, trampoline at %08X (%u bytes) - a spell now "
	         "turns the heading for the length of one call instead of one animation",
	         addr::kHookMagicCastItem, trampolineAddress, trampolineSize);
}

}  // namespace

// Called from the trampoline after Oblivion has finished computing the
// camera. eax held the CameraNode there; the trampoline passes it through as
// the single argument.
//
// All registers are saved at this point, so this function may be ordinary
// C++. It does have to stay fast and free of exceptions though - it runs on
// every rendered frame.
extern "C" void __cdecl OBVR_OnCameraUpdated(NiAVObject* cameraNode) {
	if (cameraNode == nullptr) {
		return;
	}

	const bool isThirdPerson = ReadIsThirdPerson();

	switch (g_state.ObservePointOfView(isThirdPerson)) {
	case PovEvent::FirstPass: {
		OBVR_LOG("Camera: first hook pass, CameraNode=%08X, %s",
		         reinterpret_cast<UInt32>(cameraNode),
		         isThirdPerson ? "third person" : "first person");
		// The camera's place in the scene graph, for the body: if a bone of
		// the third-person skeleton is among its ancestors, moving or
		// collapsing that bone moves the view.
		{
			char chain[256];
			UInt32 at = 0;
			const NiAVObject* node = cameraNode;
			for (int depth = 0; depth < 8 && mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node)); ++depth) {
				const char* name = node->name;
				if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(name))) {
					name = "?";
				}
				for (UInt32 i = 0; name[i] != '\0' && i < 40 && at + 4 < sizeof(chain); ++i) {
					const char c = name[i];
					chain[at++] = (c >= 0x20 && c <= 0x7E) ? c : '?';
				}
				if (at + 4 < sizeof(chain)) {
					chain[at++] = ' ';
					chain[at++] = '<';
					chain[at++] = ' ';
				}
				node = node->parent;
			}
			chain[at] = '\0';
			OBVR_LOG("Camera: the camera node's ancestry (child < parent): %s", chain);
		}

		// The first look at Oblivion's own renderer, and the question 0.1.0
		// turns on: is Direct3D 9 here being served by DXVK, which hands out
		// the Vulkan objects behind a texture, or by Microsoft's own, which
		// does not? Asked here because the renderer certainly exists by the
		// time a frame is being drawn, and asked once because the answer
		// cannot change within a run.
		void* device = render::GetGameDevice();
		const render::DeviceKind kind = render::IdentifyDevice(device);
		OBVR_LOG("Render: Oblivion's D3D9 device %08X is %s",
		         reinterpret_cast<UInt32>(device), render::DeviceKindName(kind));

		// Five of the ten fields OpenVR wants for a Vulkan texture, and the
		// reason the DXVK route exists. Logged before anything is submitted,
		// because a handle that arrives null here fails inside the compositor
		// later - reported as a bad texture, which would send the search to
		// entirely the wrong place.
		if (kind == render::DeviceKind::Dxvk) {
			render::VulkanContext vulkan;
			if (render::GetVulkanContext(device, vulkan)) {
				OBVR_LOG("Render: Vulkan instance=%08X physical=%08X device=%08X",
				         reinterpret_cast<UInt32>(vulkan.instance),
				         reinterpret_cast<UInt32>(vulkan.physicalDevice),
				         reinterpret_cast<UInt32>(vulkan.device));
				OBVR_LOG("Render: Vulkan queue=%08X index=%u family=%u",
				         reinterpret_cast<UInt32>(vulkan.queue), vulkan.queueIndex,
				         vulkan.queueFamilyIndex);
			} else {
				OBVR_LOG("Render: DXVK did not hand over a complete set of Vulkan handles");
			}

			// The other five fields, and the first look at Oblivion's own
			// picture as something OpenVR could take. The size is the check
			// worth reading: it should match the game's window, and anything
			// else means this is not the surface it appears to be.
			render::BackBufferImage backBuffer;
			if (render::GetBackBufferImage(device, backBuffer)) {
				OBVR_LOG("Render: back buffer image=%08X%08X %ux%u format=%u samples=%u layout=%u",
				         static_cast<UInt32>(backBuffer.image >> 32),
				         static_cast<UInt32>(backBuffer.image), backBuffer.width,
				         backBuffer.height, backBuffer.format, backBuffer.sampleCount,
				         backBuffer.layout);

				// The line that decides whether Oblivion's own frame can go
				// to the compositor as it stands. Usage is the part that
				// cannot be repaired afterwards: it is fixed when the image
				// is created, so missing bits mean copying the frame rather
				// than handing it over.
				OBVR_LOG("Render: back buffer usage=%08X transfer_src=%d sampled=%d, %s",
				         backBuffer.usage,
				         (backBuffer.usage & render::dxvk::kImageUsageTransferSrc) != 0 ? 1 : 0,
				         (backBuffer.usage & render::dxvk::kImageUsageSampled) != 0 ? 1 : 0,
				         render::IsSubmittableImage(backBuffer)
				             ? "submittable once transitioned"
				             : "NOT submittable as it stands");
			} else {
				OBVR_LOG("Render: no Vulkan image behind the back buffer");
			}
		}
		break;
	}
	case PovEvent::Switched:
		// First and third person put the camera in entirely different places,
		// so there is no continuity for the easing to preserve across the
		// change.
		g_lookControl.Reset();

		// The remembered menu camera goes with it. It is a raw pointer to the
		// node the game was drawing through, and a change of view swaps that
		// node - so what is held here afterwards is a node the game has let go
		// of. Writing a transform into it and walking the scene graph from it
		// is not a risk worth carrying for the sake of one menu frame; the
		// next camera pass puts a fresh one here a frame later.
		g_menuBaseNode = nullptr;

		// And the frame trace is armed, because a game that froze solid a
		// moment after this line left nothing else behind: the log's last
		// entry was the switch itself, and every question about what happened
		// next was unanswerable. Twelve frames of what the delivery, the
		// camera pass and the world renders were doing costs nothing and is
		// the difference between diagnosing that and guessing at it.
		g_menuTraceLeft = 12;
		g_menuTraceLastScene = render::CurrentSceneCall();

		// The blocking steps as well, for the same reason and for longer: two
		// sessions have now ended within a few frames of this line, and neither
		// log could say which call did not come back.
		g_stepTraceLeft = 30;

		// The chase camera's record of the aim starts over with the camera it
		// belongs to. A view change rebuilds that camera outright, so whatever
		// turn or pitch is standing is taken as already in it - which is an
		// assumption, and a safe one only because both are almost always zero
		// here: the turn is given back after every shot.
		g_aimChasedOffset = g_aimBodyOffset;
		g_aimChasedPitch = g_aimPitchOffset;

		OBVR_LOG("Camera: switched to %s (frame %u) - tracing the next frames",
		         isThirdPerson ? "third person" : "first person",
		         g_state.frameCount);
		break;
	case PovEvent::Unchanged:
		break;
	}

	++g_state.frameCount;
	MaybeReloadConfig();

	// Asked per frame, acted on only when the wish and the patch disagree -
	// which is the first frame, and any frame after the INI hot-reloads the
	// other way. Applied from here rather than plugin load so the code being
	// patched has long finished initialising.
	game::ApplyDialogZoom(GetConfig().dialogZoom);

	// The counter frequency is fixed for the lifetime of the process, so it
	// is read once rather than every frame.
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const float deltaSeconds = g_frameClock.Tick(ReadPerformanceCounter(), ticksPerSecond);
	g_deltaSeconds = deltaSeconds;

	// The compositor first, before anything asks the tracker where the head
	// is. This blocks until the headset wants the next frame and hands back
	// the pose to draw that frame with - and it has to come first, because
	// the tracker is about to be read and the pose from here is the answer it
	// should give.
	//
	// The order is the one OpenVR's own overview specifies: WaitGetPoses,
	// render, submit. It used to be render, submit, WaitGetPoses, which
	// meant the poses arrived a frame after they were wanted and were thrown
	// away instead. See OpenVRBackend::WaitGetPoses for what that cost.
	//
	// Does nothing when rendering is off or the compositor was never reached,
	// so the cost on a machine without a headset is one comparison.
	// THE ONE CALL IN OBVR THAT CAN WAIT FOR EVER. BeginFrame goes into
	// WaitGetPoses, which blocks until the compositor wants the next frame and
	// has no timeout. A log that ends on the first of these two lines has
	// located the hang; one that reaches the second has cleared it.
	TraceStep("about to wait for poses");
	g_frameOpen = g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame());
	TraceStep("poses are in");


	// Watch Oblivion's own render frustum, a handful of times.
	//
	// Reading it once at startup found a view 1.458 times wider in tangents
	// than the projection matrix reported - the same factor in both axes. That
	// is one view at the wrong size rather than a different view, and the two
	// candidates are a camera meant for something else or the right camera at
	// the wrong moment. Whether it moves is what separates them.
	if (GetConfig().tracker.renderToHeadset) {
		game::NiFrustum frustum{};
		if (game::ReadGameCameraFrustum(frustum)) {
			// The headset's own view first, because it outranks an angle: an
			// eye is not a 4:3 frustum and forcing it through one throws away
			// the shape that is the entire point of matching it.
			float headsetW = 0.0f;
			float headsetH = 0.0f;
			const bool matching = GetConfig().tracker.matchHeadsetFov &&
			                      g_headsetRenderer.GetHeadsetFrustum(headsetW, headsetH);

			if (matching) {
				game::SetFrustumTangents(frustum, headsetW, headsetH);
				if (!game::WriteGameCameraFrustum(frustum)) {
					OBVR_LOG("Camera: the headset frustum could not be written");
				}
			} else {
				const float override = GetConfig().tracker.gameFovOverride;
				if (override > 1.0f && override < 179.0f) {
					game::SetFrustumFov(frustum, override);
					if (!game::WriteGameCameraFrustum(frustum)) {
						// Cannot happen after a successful read, but silence
						// here would mean the world quietly kept its old field
						// of view.
						OBVR_LOG("Camera: the field of view override could not be written");
					}
				}
			}

			// Kept for the placement. Absolute values, because which edge
			// carries which sign is not settled and the half-width does not
			// depend on it.
			const float halfWidth = (Abs(frustum.l) + Abs(frustum.r)) * 0.5f;
			const float halfHeight = (Abs(frustum.t) + Abs(frustum.b)) * 0.5f;
			if (halfWidth > 0.0f && halfHeight > 0.0f) {
				g_state.cameraTanHalfWidth = halfWidth;
				g_state.cameraTanHalfHeight = halfHeight;
			}
		}

		if (game::ReadGameCameraFrustum(frustum) && g_frustumWatcher.Observe(frustum)) {
			OBVR_LOG("Camera: frustum #%u l=%.4f r=%.4f t=%.4f b=%.4f n=%.4f f=%.1f "
			         "(%.1f deg across)",
			         g_frustumWatcher.Reported(), static_cast<double>(frustum.l),
			         static_cast<double>(frustum.r), static_cast<double>(frustum.t),
			         static_cast<double>(frustum.b), static_cast<double>(frustum.n),
			         static_cast<double>(frustum.f),
			         static_cast<double>(2.0f * math::Atan(frustum.r) *
			                             math::kRadiansToDegrees));
		}
	}

	g_headTracker.Update(g_state.frameCount);

	// After Update, so that the recenter reference is this frame's
	// orientation rather than the previous one. The new zero therefore takes
	// effect from the next frame - a single frame of delay that nobody can
	// see, in exchange for the reference being exactly the pose the user was
	// holding when they pressed the key.
	MaybePollRecenter();

	const Config& config = GetConfig();
	if (IsDue(g_state.frameCount, config.logEveryFrames)) {
		const vr::Quaternion& raw = g_headTracker.GetRawOrientation();
		const NiPoint3& pos = cameraNode->localTransform.pos;
		const NiPoint3& offset = g_headTracker.GetCameraOffset();
		// The lean is reported twice over: the offset actually applied, and
		// how far it reached for before MaxLeanUnits cut it. Equal means the
		// limit never came into play; a raw figure stuck at MaxLeanUnits
		// across several lines means it is the limit doing the deciding, not
		// the head - and that is not something the headset can show you.
		OBVR_LOG("Camera: frame %u, %s, %.1f ms, pos=(%.1f, %.1f, %.1f), "
		         "head=(%.3f, %.3f, %.3f, %.3f), lean=(%.1f, %.1f, %.1f) raw=%.1f",
		         g_state.frameCount,
		         isThirdPerson ? "3rd" : "1st",
		         static_cast<double>(deltaSeconds) * 1000.0,
		         static_cast<double>(pos.x),
		         static_cast<double>(pos.y),
		         static_cast<double>(pos.z),
		         static_cast<double>(raw.x),
		         static_cast<double>(raw.y),
		         static_cast<double>(raw.z),
		         static_cast<double>(raw.w),
		         static_cast<double>(offset.x),
		         static_cast<double>(offset.y),
		         static_cast<double>(offset.z),
		         static_cast<double>(g_headTracker.GetRawOffsetUnits()));
	}

	// The heart of it: the vanilla rotation stays the base, the head rotation
	// acts in local camera space.
	//
	// The order matters. The head offset is measured in the camera's own
	// space, so it has to be carried over by the vanilla rotation - the one
	// the game computed, without the head laid on top. Taking the product
	// instead would tie leaning to where the head is looking, and leaning
	// forward while glancing sideways would slide the camera sideways.
	// The look controls are only taken away from the player while a headset is
	// actually delivering poses. Without one there is nothing to hand them to,
	// and somebody starting Oblivion without SteamVR has to get the game they
	// had before.
	// The camera as the game left it, before OBVR lays a head on top.
	//
	// Kept for the menu background, which has to place the camera on frames
	// this hook does not run on. The node's transform is not a usable base
	// there: it still holds whatever this hook last wrote into it, head
	// offset and eye step included, so building on it would pile one frame's
	// head pose onto the next and walk the camera away across a menu. The
	// game's own value is the only fixed point, and this is the one moment it
	// can be read - after the engine has written it, before OBVR has.
	g_menuBaseNode = cameraNode;
	g_menuBasePos = cameraNode->localTransform.pos;
	g_menuBaseThirdPerson = isThirdPerson;

	// Taken in the same breath and from the same moment: the engine's camera in
	// world space, which is the space the thing under the crosshair has its
	// position in. See the globals for why world and not local, and why a frame
	// of age costs nothing here.
	g_cameraWorldPos = cameraNode->worldTransform.pos;

	// The engine's own placement for THIS frame, before OBVR has added
	// anything to it, and the point the body turns about.
	//
	// EYE found the fault - the viewpoint steps 1.7 units sideways when the
	// body takes the turn and steps back when it gives it up, eighteen times
	// in nine shots without an exception. Correcting it means rotating the
	// camera back about the player, and a rotation needs a centre. These two
	// are what pins that centre down, so the correction can be arithmetic
	// rather than another guess.
	//
	// Local rather than world for the camera, because EYE is read from the
	// world transform and is therefore a frame stale - which is why its step
	// lands a frame later than the heading's. The local transform is this
	// frame's, in step with the offset that has to cancel it.
	g_cameraLocalPos = cameraNode->localTransform.pos;
	g_playerWorldValid = game::PlayerWorldPosition(g_playerWorldPos);
	g_cameraWorldRot = cameraNode->worldTransform.rot;
	g_cameraWorldValid = true;

	UpdateCrosshairDepth(config, deltaSeconds);

	// The rotation as the engine built it, before the look control levels it
	// - what the chase camera's own angle is read from.
	const NiMatrix33 engineRotation = cameraNode->localTransform.rot;

	// The player's rotation as the engine left it this frame. Read BEFORE
	// anything is written, and that ordering is the measurement rather than
	// a tidiness: what is in the field at this moment is what the engine left
	// there, which the probe could not see while it read afterwards.
	//
	// Read up here, ahead of the compensation, because the third person
	// camera's rate is measured against it.
	game::PlayerRotation asEngineLeftIt{};
	const bool readPlayer = game::ReadPlayerRotation(asEngineLeftIt);

	// What the CAMERA carries of the aim this frame, which is not the same
	// question in the two views.
	//
	// The engine built this frame's camera from the fields OBVR wrote last
	// frame - the offsets standing here are the ones those fields contain. In
	// first person the camera takes them whole; in third person it eases
	// towards them, and the share it has reached is stepped here by the rate
	// the engine applied THIS frame - read off the camera, not assumed. The
	// headset showed why: the physics steps that camera at sixty hertz under
	// a ninety hertz renderer, so every third frame it does not move at all,
	// and a modelled rate turned the picture on exactly those frames. See
	// camera::MeasuredChaseRate.
	//
	// The camera's heading sits at minus rotZ and its pitch at minus rotX
	// once settled, both measured by the probe, which is where the targets'
	// signs come from.
	Heading engineHeading{};
	const bool haveEngineHeading = HeadingOf(engineRotation, engineHeading);
	const float engineYaw =
		haveEngineHeading ? math::Atan2(engineHeading.sine, engineHeading.cosine) : 0.0f;
	const float enginePitch = math::Asin(SinPitchOf(engineRotation));

	float yawRate = kChaseDeltaMult;
	float pitchRate = kChaseDeltaMult;
	if (isThirdPerson && readPlayer && haveEngineHeading) {
		ChaseRateInput yawInput;
		yawInput.cameraNow = engineYaw;
		yawInput.cameraBefore = g_chaseYawBefore;
		yawInput.haveBefore = g_chaseHaveBefore;
		yawInput.target = -asEngineLeftIt.yaw;
		yawRate = MeasuredChaseRate(yawInput);

		ChaseRateInput pitchInput;
		pitchInput.cameraNow = enginePitch;
		pitchInput.cameraBefore = g_chasePitchBefore;
		pitchInput.haveBefore = g_chaseHaveBefore;
		pitchInput.target = -asEngineLeftIt.pitch;
		pitchRate = MeasuredChaseRate(pitchInput);
	}
	g_chaseYawBefore = engineYaw;
	g_chasePitchBefore = enginePitch;
	g_chaseHaveBefore = isThirdPerson && haveEngineHeading;
	g_chaseYawRate = yawRate;
	g_chasePitchRate = pitchRate;

	g_aimChasedOffset = ChaseStep(g_aimChasedOffset, g_aimBodyOffset, yawRate);
	g_aimChasedPitch = ChaseStep(g_aimChasedPitch, g_aimPitchOffset, pitchRate);
	const float cameraShare = AimCameraShare(isThirdPerson, g_aimBodyOffset, g_aimChasedOffset);
	const float pitchShare = isThirdPerson ? g_aimChasedPitch : 0.0f;

	NiMatrix33 baseRotation = cameraNode->localTransform.rot;
	float verticalOffset = 0.0f;

	if (g_headTracker.IsHeadsetConnected()) {
		g_lookControl.Update(baseRotation, isThirdPerson, deltaSeconds, pitchShare);
		baseRotation = g_lookControl.GetRotation();
		verticalOffset = g_lookControl.GetVerticalOffset();
	} else {
		g_lookControl.Reset();
	}

	// The turn already handed to the body, taken back out of the base.
	//
	// Written into the player below and read back here one frame later, which
	// is exactly the right way round: the engine builds this rotation from the
	// heading OBVR wrote last frame, so the offset standing here is the one
	// that heading contains. Applied on the right of the base and therefore to
	// the left of everything measured in camera space - the head's rotation,
	// its offset, the eye step - so all of them come round with it and the
	// picture holds still while the body turns underneath.
	//
	// By the CAMERA'S share of it rather than the offset itself, which is the
	// one line that makes the third person work: its camera swings round over
	// a second, and taking the whole turn out of it on the first frame would
	// swing the picture the other way by everything the camera had not yet
	// done. In first person the two are the same number.
	//
	// Only the yaw is taken back. The base has already been levelled by the
	// look control, so its z axis is the world's up and a rotation about it
	// cannot disturb the pitch this rotation is being composed with.
	// Kept for the trace: the view's heading as the engine left it, and again
	// once the compensation has taken the body's turn back out.
	//
	// The second is what the wearer is looking along. Everything else about the
	// aim has been proved right by reading a result back rather than an
	// intention, and this is the same measurement applied to the one thing that
	// was never checked - the picture itself. "es ist die ganze view. das ganze
	// bild. beim loslassen."
	Heading traceBefore{};
	g_aimViewYawBefore =
		HeadingOf(baseRotation, traceBefore) ? math::Atan2(traceBefore.sine, traceBefore.cosine)
		                                     : 0.0f;

	if (cameraShare != 0.0f) {
		baseRotation = baseRotation * RotationFromHeading(Heading{
			math::Cos(cameraShare), -math::Sin(cameraShare)});
	}

	Heading traceAfter{};
	g_aimViewYawAfter =
		HeadingOf(baseRotation, traceAfter) ? math::Atan2(traceAfter.sine, traceAfter.cosine)
		                                    : 0.0f;

	// The rotation this frame was actually built on - which is not the one the
	// engine wrote. With a headset connected the look control levels it and
	// lifts the vertical tilt out into a height, and everything downstream
	// uses what comes back.
	//
	// This is what a menu frame has to start from too. Taking the raw rotation
	// there instead leaves the menu's viewpoint facing a different way and
	// sitting at a different height than the world did the instant before the
	// menu opened - and laying the two paths side by side, that is the only
	// difference between them: position, head offset, head rotation and eye
	// step are all built the same way from the same values. So it is the
	// offset the live menu background was reported with.
	//
	// Remembered here rather than read back out of the look control from the
	// menu path, because the look control is only stepped by this hook. A
	// session where this hook first ran without a headset would leave it
	// holding the identity rotation, and the menu path would then place the
	// camera facing world north for reasons nothing in that path explains.
	g_menuBaseRot = baseRotation;
	g_menuBaseVerticalOffset = verticalOffset;

	// The other half of taking the body's turn back out - and the half that was
	// missing for as long as the aim has jumped.
	//
	// The compensation above holds the view's DIRECTION still while the body
	// turns. Nothing held its POSITION still, and the first person camera does
	// not stand on the axis the body turns about, so every turn walked the eye
	// along an arc: 1.7 units sideways, in one frame, eighteen times in nine
	// traced shots. See AimArcCorrection for the two independent measurements
	// of the arm that produces it.
	//
	// Added to the local position rather than to the world one because that is
	// what this hook writes and what the scene graph recomputes from. The two
	// are the same space here, which is measured rather than assumed: the
	// camera's local and world positions differ only by the head offset OBVR
	// itself added the frame before, so the node's parent neither rotates the
	// camera nor moves it, and a world vector can go straight into the local
	// transform. The head offset below has been relying on exactly that since
	// positional tracking went in.
	//
	// The same holds in third person, and it was measured there too before
	// anything was built on it: the probe's LOCAL and EYE columns differed by
	// OBVR's own offset and nothing else, through the whole sweep.
	//
	// By the camera's share of the turn, as the rotation above - in third
	// person the eased value, because that is how far round the arc the
	// camera has actually walked. The arc's centre is the player in both
	// views: measured in third person as well, where the camera's distance
	// from the player's axis held at radius times cosine of its pitch at
	// every tilt.
	g_aimArcApplied = AimArcCorrection(AimArcInput{g_cameraLocalPos, g_playerWorldPos,
	                                               g_playerWorldValid, cameraShare});
	cameraNode->localTransform.pos = cameraNode->localTransform.pos + g_aimArcApplied;

	// And the vertical half, which only third person has: the swing of that
	// camera about its pivot for the share of OBVR's pitch it has eased
	// towards. Taken from the position the arc correction has already put
	// back, because the sphere's arm is read off that position and the turn
	// about the vertical changes where the arm points without changing its
	// tilt. See AimTiltCorrection for the sphere and how it is read back.
	AimTiltInput tiltInput;
	tiltInput.cameraPosition = g_cameraLocalPos + g_aimArcApplied;
	tiltInput.feet = g_playerWorldPos;
	tiltInput.centreKnown = g_playerWorldValid;
	tiltInput.cameraSinPitch = SinPitchOf(engineRotation);
	tiltInput.pitchShare = pitchShare;
	g_aimTiltApplied = AimTiltCorrection(tiltInput);
	cameraNode->localTransform.pos = cameraNode->localTransform.pos + g_aimTiltApplied;

	// Dead: the camera stays where it was when the player died, the head
	// still free (StepDeathView) - the game's death view sinks otherwise.
	{
		const bool dead = game::PlayerIsDead();
		const bool wasHeld = g_deathView.held;
		cameraNode->localTransform.pos = StepDeathView(
			g_deathView, config.look.deathViewStill, dead, cameraNode->localTransform.pos,
			g_deathView.haveAliveTurn
				? DeathStepBack(g_deathView.lastAliveRot,
				                config.look.deathViewBackMetres * config.tracker.unitsPerMetre)
				: NiPoint3{0.0f, 0.0f, 0.0f});
		// And the turn and height it is built on, or the chase camera's swing
		// towards the body still turns the view (StepDeathTurn).
		StepDeathTurn(g_deathView, baseRotation, verticalOffset);
		g_menuBaseRot = baseRotation;
		g_menuBaseVerticalOffset = verticalOffset;
		if (g_deathView.held != wasHeld) {
			OBVR_LOG("Camera: the death view is %s", g_deathView.held ? "held still" : "released");
		}
	}

	// The head offset is measured in the camera's own space, so it is carried
	// over by the base rotation. The vertical look is not: it is a height, and
	// heights are along the world up axis whichever way the camera faces.
	cameraNode->localTransform.pos =
		cameraNode->localTransform.pos + baseRotation * g_headTracker.GetCameraOffset();
	cameraNode->localTransform.pos.z += verticalOffset;

	const NiMatrix33 finalRotation = baseRotation * g_headTracker.GetCameraRotation();

	// This is the cyclopean HMD direction, before the stereo eye step below.
	// The engine's activation pick runs elsewhere in the frame and consumes
	// the last complete value through WorldPickHook; its origin deliberately
	// remains at Oblivion's player-safe position rather than back here at the
	// chase camera.
	game::SetWorldPickDirection(ForwardOf(finalRotation),
	                            g_headTracker.IsHeadsetConnected());
	// In the hand-tracked mode the pick is the right hand's laser instead -
	// the same ray the menus are pointed with and the crosshair is hung on -
	// so the tooltip, Activate and the grab take what the laser points at.
	// From last frame's hand, as the hand mode runs at Present.
	{
		const vr::HandSettings& hands = config.hands;
		// The hand an item is near (game::FindNearestItem), else the right one.
		const bool pickLeft = g_hand.pickWithLeftHand;
		const bool handRay = config.fullVrMode && g_headTracker.IsHeadsetConnected() &&
		                     (pickLeft ? g_hand.leftHandValid : g_hand.rightHandValid);
		const bool nearRay = handRay && g_nearItem.valid;
		const bool reachRay = config.fullVrMode && g_headTracker.IsHeadsetConnected() &&
		                      g_grabReachPick &&
		                      (g_hand.grabWithLeftHand ? g_hand.leftHandValid : g_hand.rightHandValid);
		if (nearRay) {
			// An item near a hand: the pick from that hand straight at it, so
			// the tooltip, the marker and the grab take it without pointing.
			const NiPoint3 from = cameraNode->localTransform.pos +
			                      finalRotation * (pickLeft ? g_hand.leftHandOffsetUnits
			                                                : g_hand.rightHandOffsetUnits);
			// Towards the side nearest the hand as it comes closer
			// (game::NearSideWeight): the ring moves to the hand, and from
			// ReachNearSideMetres in the near side is what the grip takes.
			NiPoint3 aim = g_nearItem.centre;
			NiPoint3 nearSurface;
			float nearDistance = 0.0f;
			if (game::NearestVertexOf(g_nearItem.ref, from, nearSurface, nearDistance)) {
				const float perMetre = config.tracker.unitsPerMetre;
				aim = game::NearSideAimPoint(
					g_nearItem.centre, nearSurface,
					game::NearSideWeight(nearDistance, hands.reachMarkerMetres * perMetre,
					                     hands.reachNearSideMetres * perMetre));
			}
			const vr::LaserWorldRay ray = vr::RayTowards(
				from, aim, hands.grabReachMetres * config.tracker.unitsPerMetre,
				ForwardOf(finalRotation));
			game::SetWorldPickHandRay(ray.origin, ray.direction, true);
		} else if (reachRay) {
			// A grip closed: the pick runs along that hand's laser - the beam
			// the settings tilt, not the line from the head - starting the
			// grab's reach behind the hand, so an object the hand is already
			// inside is still ahead of the ray.
			const bool left = g_hand.grabWithLeftHand;
			const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
				finalRotation, cameraNode->localTransform.pos,
				left ? g_hand.leftHandRotation : g_hand.rightHandRotation,
				left ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits,
				hands.laserPitchDegrees, left ? -hands.laserYawDegrees : hands.laserYawDegrees,
				hands.laserOriginMetres - hands.grabReachMetres, config.tracker.unitsPerMetre);
			game::SetWorldPickHandRay(ray.origin, ray.direction, true);
		} else if (handRay) {
			const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
				finalRotation, cameraNode->localTransform.pos,
				pickLeft ? g_hand.leftHandRotation : g_hand.rightHandRotation,
				pickLeft ? g_hand.leftHandOffsetUnits : g_hand.rightHandOffsetUnits,
				hands.laserPitchDegrees, pickLeft ? -hands.laserYawDegrees : hands.laserYawDegrees,
				hands.laserOriginMetres, config.tracker.unitsPerMetre);
			game::SetWorldPickHandRay(ray.origin, ray.direction, true);
		} else {
			game::SetWorldPickHandRay(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, false);
		}
	}

	// The player's own pitch, pointed where the view is pointed.
	//
	// This is the half of "the arrow does not go where I am looking" that the
	// crosshair could not fix. A projectile is a TESObjectREFR and leaves
	// along the player's rotation; the view is the camera's, which OBVR
	// replaces wholesale. So the mouse went on aiming invisibly while the
	// head looked elsewhere, and the probe below measured exactly that: the
	// head sweeping a sine of -0.32 to +0.24 with rotX sitting at 0.0000
	// throughout.
	//
	// finalRotation rather than the head alone, because it is the rotation
	// the frame is actually drawn with - the vanilla heading, levelled, with
	// the head laid on top. That is what the wearer sees down, and an arrow
	// should leave along what is seen rather than along a component of it.
	//
	// The field itself was read at the top of the frame, before anything was
	// written - see asEngineLeftIt. The question that read answers is the one
	// the sideways half of this turns on. If a written rotation survives into
	// the next frame, then writing the YAW would feed back - the camera is
	// built on the player's heading, and the head's turn is added on top of
	// it, so the view would keep drifting round for as long as the head
	// stayed turned. If the engine instead sets the field afresh every frame
	// from its own input state, there is no loop and the yaw can be written
	// as plainly as the pitch is.
	//
	// Not guessed from either side: the log prints what the engine left
	// beside what OBVR wrote last frame, and says whether they match.
	//
	// The pitch itself is written further down, once the frame knows whether
	// something is being aimed - first person does not care, third person
	// does. What it will write is decided here, from the rotation the frame
	// is actually drawn with.
	const float gazePitch = PlayerPitchForGaze(SinPitchOf(finalRotation));

	// The body turned to face the gaze, but only while something is being
	// aimed - see AimYawWanted for why this half is gated where the vertical
	// half is not.
	//
	// Last frame's write is judged first, before this frame's is made. What is
	// in the field right now is the engine's answer to what OBVR put there,
	// and it is the only place that answer can be read.
	if (readPlayer && g_aimYawPending) {
		g_aimYawPending = false;

		// A step that did not land is a step the body never took, and the
		// offset above must not go on claiming it - the base would be turned
		// back for a turn that is not in the player's heading, and the view
		// would sit crooked by exactly that much. Taking it out again is the
		// whole correction: the next frame simply finds more turn remaining.
		if (!YawWriteLanded(g_aimYawWrote, asEngineLeftIt.yaw, g_aimYawStepTaken)) {
			g_aimBodyOffset = math::WrapAngle(g_aimBodyOffset - g_aimYawStepTaken);

			if (!g_aimYawLostReported) {
				g_aimYawLostReported = true;
				OBVR_LOG("Aim: a written heading did not land (wrote %.4f, found %.4f after "
				         "a step of %.4f) - something else moved the player, so the step is "
				         "taken back out of the camera and the body will come round again.",
				         static_cast<double>(g_aimYawWrote),
				         static_cast<double>(asEngineLeftIt.yaw),
				         static_cast<double>(g_aimYawStepTaken));
			}
		}
	}

	const bool attackHeld = AttackHeld();

	// Kept before the flag is updated, because two decisions below need the
	// EDGE rather than the level - the frame the control went up.
	const bool attackWasHeld = g_aimWasHeld;

	// The cast, stepped alongside it.
	//
	// The casting flag is read only while there is something to read it for -
	// the window standing, or the key just pressed. On every other frame, which
	// is nearly all of them, no pointer into the process is followed at all;
	// the same economy the attack state above is written for.
	const bool castHeld = CastHeld();
	const bool castWasHeld = g_castWasHeld;
	g_castWasHeld = castHeld;

	// Third person is a view the aim now covers, on its own switch - the same
	// one the bow's turn answers to.
	const bool viewAimed = !isThirdPerson || GetConfig().aimInThirdPerson;

	// THE AIM SET AT THE SOURCE. What the wrapped engine call will write is
	// decided here, once a frame, from the same head reading and the same
	// pitch the rest of the aim uses; the call itself reads it and does no
	// arithmetic of its own. Gated on the hook having gone in, because a
	// slot that could not be written leaves the turn machinery below its old
	// job - the cast window, the cast hook and the body's turn all answer to
	// atSource.
	//
	// The head's turn is taken relative to where the body NOW faces - what is
	// left after any turn already handed to it, which is AimYawRemaining -
	// so that a body the turn has moved is not turned twice.
	const bool atSource = GetConfig().aimAtSource && game::AimAtSourceInstalled();
	float sourceAimYaw = 0.0f;
	{
		Heading sourceTurn{};
		const float sourceHeadYaw =
			HeadingOf(g_headTracker.GetCameraRotation(), sourceTurn)
				? math::Atan2(sourceTurn.sine, sourceTurn.cosine)
				: 0.0f;
		game::AimSourcePose pose;
		pose.wanted = readPlayer &&
		              AimAtSourceWanted(GetConfig().aimFollowsGaze, GetConfig().aimAtSource,
		                                g_headTracker.IsHeadsetConnected(), game::IsMenuMode(),
		                                viewAimed);
		pose.headYaw = AimYawRemaining(sourceHeadYaw, g_aimBodyOffset);
		pose.pitch = gazePitch;
		// The hand-tracked mode: the shot goes along the right hand instead
		// of the gaze - its heading as the head's plus the hand's turn from
		// it, its pitch its own. Same hand-over, same engine sites.
		// For grab: use whichever hand is holding it; for attacks/spells: always right hand.
		// The hand's aim turns the player - rotX and the body's heading - to
		// where the hand points, and the NPCs then looked at the hand rather
		// than at the wearer's eyes (2026-09-25). So it is a switch,
		// Hands.AimWithHand, off by default; the grab keeps its hand either way,
		// since the grabbed object is carried along the aim.
		const bool aimWithHand = GetConfig().hands.aimWithHand;
		if (GetConfig().fullVrMode) {
			// From the reach on, not only once held: the grab starts inside the
			// engine's grab handler a frame after the key, and that start has to
			// look along the same line the pick found the object on.
			if ((g_grabKeyDown || g_grabReachPick) && g_hand.grabDirectionValid) {
				// Something is held: it goes along the line from the head to
				// the holding hand, as far as the hand is - so it moves where
				// the hand moves, and a throw of the hand throws it.
				pose.headYaw = AimYawRemaining(sourceHeadYaw + g_hand.grabYawTurn, g_aimBodyOffset);
				pose.pitch = PlayerPitchForGaze(g_hand.grabSinPitch);
			} else if (g_hand.aimValid && aimWithHand) {
				// Right hand: attacks and spells, with Hands.AimWithHand
				pose.headYaw = AimYawRemaining(sourceHeadYaw + g_hand.aimYawTurn, g_aimBodyOffset);
				pose.pitch = PlayerPitchForGaze(g_hand.aimSinPitch);
			}
		}
		game::SetAimSourcePose(pose);
		sourceAimYaw = pose.headYaw;
	}

	// The source swap above is intentionally invisible to animation. Supply
	// only that missing picture in third person: the pure decision owns all
	// gates and action phases, while the render callback applies its angles to
	// Spine2 after Oblivion has finished animating it.
	ThirdPersonAimVisualInput visualInput;
	// Not on a corpse: at death the game goes to third person, and the head and
	// spine were turned to the headset on the falling ragdoll (2026-09-27 log,
	// "Bip01 Head follows the HMD" right after "the death view is held").
	const bool livingPlayer = readPlayer && !game::PlayerIsDead();
	visualInput.enabled = livingPlayer && config.aimFollowsGaze;
	visualInput.aimAtSource = atSource;
	visualInput.headsetConnected = g_headTracker.IsHeadsetConnected();
	visualInput.isThirdPerson = isThirdPerson;
	visualInput.thirdPersonAllowed = config.aimInThirdPerson;
	visualInput.menuIsUp = game::IsMenuMode();
	visualInput.weaponDrawn =
		readPlayer && isThirdPerson &&
		game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
	visualInput.bodyWithoutWeapon = config.thirdPersonBodyFollowsGazeUnarmed;
	visualInput.attackHeld = attackHeld;
	visualInput.castActive = castHeld || g_castWindow.open || g_castBegan ||
	                         g_castArm.seconds >= 0.0f;
	visualInput.action = readPlayer ? game::ReadPlayerAction() : addr::kActionNone;
	visualInput.percent = config.thirdPersonAimVisualPercent;
	visualInput.gazeYaw = sourceAimYaw;
	visualInput.playerPitch = gazePitch;
	const ThirdPersonAimVisualDecision visualDecision =
		NextThirdPersonAimVisual(g_thirdPersonAimVisualState, visualInput);
	g_thirdPersonAimVisualState = visualDecision.next;
	g_thirdPersonAimVisualWanted = visualDecision.write;
	g_thirdPersonAimVisualYaw = visualDecision.yaw;
	g_thirdPersonAimVisualPitch = visualDecision.pitch;
	g_thirdPersonHeadVisualWanted = ThirdPersonHeadVisualWanted(
		livingPlayer && config.aimFollowsGaze, config.thirdPersonHeadFollowsGaze,
		g_headTracker.IsHeadsetConnected(), isThirdPerson,
		config.aimInThirdPerson, game::IsMenuMode());
	g_thirdPersonHeadVisualYaw = sourceAimYaw;
	g_thirdPersonHeadVisualPitch = -gazePitch;

	g_bodyWanted = game::VisibleBodyWanted(config.body.visible, isThirdPerson,
	                                       game::IsMenuMode());
	g_bodyCameraNode = cameraNode;
	g_bodyFirstEyeStep = NiPoint3{0.0f, 0.0f, 0.0f};

	CastWindowInput castInput;
	castInput.enabled = readPlayer && GetConfig().aimCastFollowsGaze &&
	                    g_headTracker.IsHeadsetConnected() && viewAimed &&
	                    !game::IsMenuMode();
	castInput.castHeld = castHeld;
	castInput.castWasHeld = castWasHeld;
	castInput.deltaSeconds = deltaSeconds;

	if (castInput.enabled && (g_castWindow.open || castHeld)) {
		castInput.spellStillLeaving = game::IsShotUnreleased();
	}

	const bool castWasOpen = g_castWindow.open;
	const float castHeldSeconds = g_castWindow.secondsOpen;
	g_castWindow = NextCastWindow(g_castWindow, castInput, GetConfig().aimCastHoldSeconds);

	// Said once, and what it reports is how long the body was actually held.
	//
	// It is the number the wearer feels: for as long as the window stands, the
	// character walks the way the spell went rather than the way the stick is
	// pushed. "während des castens laufe ich in die richtung vom cast." So it
	// is measured and printed rather than left to be estimated from the code.
	//
	// Not with the aim set at the source: the window then turns nothing, and
	// a line saying the body was held would be untrue.
	if (castWasOpen && !g_castWindow.open && !g_castReported && !atSource) {
		g_castReported = true;
		OBVR_LOG("Aim: a spell held the body turned for %.2f s - %s",
		         static_cast<double>(castHeldSeconds),
		         castHeldSeconds >= kCastWindowLimitSeconds - 0.05f
		             ? "the LIMIT ended it, so the action field never said FollowThrough and "
		               "this build is not reading the cast the way the trace measured it"
		             : "ended by the action field reading FollowThrough, which means the spell "
		               "had gone");
	}

	// One window, whichever control opened it. Everything downstream - the
	// turn, the return, the compensation that holds the view still and the arc
	// correction that holds the eye still - is written against "is the body
	// being turned", not against what is in the player's hands, so a spell
	// needs none of it changed.
	//
	// UNLESS THE HOOK IS DOING IT, in which case the window turns nothing and
	// only keeps its other job: saying that a cast standing right now is the
	// player's, which is what lets the MagicCaster offset be learned rather
	// than guessed. The turn itself moves to the one moment it is needed.
	const bool castAtSpawn = GetConfig().aimCastAtSpawn && g_castHookInstalled;
	// And not when the spell takes the gaze at the source: then neither the
	// window nor the hook turns anything, and a cast is invisible to all of
	// the turn machinery below.
	const bool castTurning = g_castWindow.open && !castAtSpawn && !atSource;

	// Whether a spell of the player's may be aimed at all. The hook asks
	// nothing beyond this - no head reading, no arithmetic - because it runs
	// inside a call the engine owns.
	g_castAimWanted = castAtSpawn && !atSource && readPlayer && viewAimed &&
	                  GetConfig().aimCastFollowsGaze &&
	                  g_headTracker.IsHeadsetConnected() && !game::IsMenuMode();

	// When a spell's heading is turned, which is neither where the cast is made
	// nor where the engine says it left.
	//
	// MagicCaster::CastMagicItem is the START of the cast - measured: the hook
	// fires on a frame whose action field still reads None, and the animation
	// that follows runs 53 frames of Attack. Turning there is nine tenths of a
	// second early, and two runs in the headset showed the result: spells going
	// forwards while the log reported turn after turn.
	//
	// Waiting for the field to change instead would be too late. OBVR's own
	// reading of the bow, in PlayerAim.h, records that on the frame the field
	// first reads AttackFollowThrough the projectile HAS GONE.
	//
	// So the turn goes in shortly before the animation is due to end, and from
	// that moment the bow's own machinery holds it and gives it back the moment
	// the field says the spell has left.
	//
	// "Due to end" is measured rather than written down. A fixed 0.70 s was
	// wrong five casts out of five, and the reason is that the animation is 53
	// FRAMES - 0.88 s at 60 Hz, 0.59 s at 90 Hz, and the headset picks the
	// rate. So each cast is watched to its end and the next one turns that
	// long after the cast, less a margin.
	const float measured = g_castMeasuredSeconds;
	const float margin = GetConfig().aimCastTurnMarginSeconds;
	const float leadFromMeasurement = measured - margin;

	CastArmInput armInput;
	armInput.castBegan = g_castBegan;
	armInput.deltaSeconds = deltaSeconds;
	armInput.turnAfterSeconds =
		measured > 0.0f ? (leadFromMeasurement > 0.0f ? leadFromMeasurement : 0.0f)
		                : GetConfig().aimCastTurnAfterSeconds;
	armInput.limitSeconds = GetConfig().aimCastArmLimitSeconds;

	// The action field only while a cast is actually being watched, which is
	// under a second per spell and never on an ordinary frame.
	armInput.actionIsAttack =
		g_castArm.seconds >= 0.0f && game::ReadPlayerAction() == addr::kActionAttack;
	g_castBegan = false;

	const CastArmDecision castArm = NextCastArm(g_castArm, armInput);
	g_castArm = castArm.next;

	if (castArm.measuredSeconds > 0.0f) {
		const bool first = g_castMeasuredSeconds == 0.0f;
		g_castMeasuredSeconds = castArm.measuredSeconds;
		if (g_castMeasurementsReported < kCastHookReports) {
			++g_castMeasurementsReported;
			const float next = g_castMeasuredSeconds - margin;
			OBVR_LOG("Aim: that cast's animation ran %.2f s%s, so the next spell turns "
			         "%.2f s after the cast",
			         static_cast<double>(g_castMeasuredSeconds),
			         first ? " (the first one measured)" : "",
			         static_cast<double>(next > 0.0f ? next : 0.0f));
		}
	}

	if (castArm.turnNow) {
		++g_castHookTurns;
		if (g_castHookTurns <= kCastHookReports) {
			OBVR_LOG("Aim: a spell's turn goes in %.2f s after the cast, with the "
			         "animation still running - cast %u of this run",
			         static_cast<double>(armInput.turnAfterSeconds), g_castHookTurns);
		}
	}

	if (castArm.missed) {
		++g_castMisses;
		if (g_castMisses <= kCastHookReports) {
			OBVR_LOG("Aim: the cast animation had already ended %.2f s after the cast, so "
			         "the spell left unaimed (miss %u). The next one turns earlier.",
			         static_cast<double>(armInput.turnAfterSeconds), g_castMisses);
		}
	}

	// The clock that separates "let go" from "shot". Released starts it, held
	// stops it, and settling the turn stops it too.
	//
	// THE CAST WINDOW COUNTS AS BEING HELD, and its closing counts as a
	// release. Without that a spell would turn the body and nothing would ever
	// turn it back: the return refuses to act on a negative count, and the
	// count only ever left negative because the ATTACK control was never
	// pressed. The body would hold the cast's heading for good, and the wearer
	// would walk sideways from then on - the exact fault the return was written
	// for, reached by a door it did not know was there.
	//
	// AND WITH THE HOOK DOING THE TURNING, THE CAST MUST NOT START THIS CLOCK
	// AT ALL. This is what stopped the hook from changing anything on its first
	// run, and it is worth writing down because the route was not obvious.
	//
	// A cast reads to the engine as an ATTACK - measured, 53 frames of Attack
	// then FollowThrough. So the moment this clock starts, the bow's own
	// machinery recognises a shot in flight: turningOnShot needs only a started
	// clock, IsShotUnreleased says Attack, and AimTurnDue turns the body on
	// attackInProgress alone. The cast window was never turning the body
	// directly - it was starting the clock, and the BOW's logic did the rest,
	// for the full 53 frames, which is exactly the 0.88 seconds the log showed
	// after the hook had gone in and supposedly taken over.
	//
	// So with the hook in charge, a cast is invisible to all of that. The hook
	// starts the clock itself when it turns, which is the one moment a return
	// is owed.
	ReleaseClockInput clockInput;
	clockInput.attackHeld = attackHeld;
	clockInput.attackWasHeld = attackWasHeld;
	clockInput.castTurning = castTurning;
	clockInput.castWasOpen = castWasOpen;
	clockInput.castFeedsClock = !castAtSpawn && !atSource;
	clockInput.castTurnDue = castArm.turnNow;
	clockInput.deltaSeconds = deltaSeconds;
	g_aimSecondsSinceRelease = NextReleaseClock(g_aimSecondsSinceRelease, clockInput);
	g_aimWasHeld = attackHeld;

	// The turn given back once the shot is actually gone, so the character
	// stops walking the way the arrow went while the view faces forwards.
	//
	// AFTER the attack finishes, not on the release - the arrow leaves several
	// frames later, and the heading in between is the heading it leaves along.
	// Straightening on the release frame would send the shot forwards instead
	// of at what was aimed at, which is the aiming this exists to serve.
	//
	// In one frame rather than eased, which is the other half of what makes
	// this different from the version that caused nausea. See AimReturnWanted.
	// The attack state, asked only on the frames it decides something: the
	// return is switched on, a turn is standing, the control is released, and
	// a release is being waited out. On every other frame - which is nearly all
	// of them - no raw pointer is followed at all.
	//
	// These conditions are AimReturnWanted's own, repeated here on purpose.
	// They cannot be left to it: its arguments are evaluated before it runs, so
	// the read would happen whatever it then decided.
	//
	// Two things want it now: the return, and the OnShot turn window. Asked
	// once, and only when one of them could act on it - which is never while
	// the control is held, since both are about what happens after it goes up.
	const bool onShotMode = GetConfig().aimTurnOnShotOnly;
	// A turn standing, or the third person's borrowed pitch - either is owed
	// a return, and a pitch held with the head level sideways is still a shot
	// that must not be straightened before it has gone.
	const bool aimStanding = g_aimBodyOffset != 0.0f || g_aimPitchHold.held;
	const bool waitingOnAShot = readPlayer && GetConfig().aimReturnOnRelease &&
	                            aimStanding && !attackHeld &&
	                            g_aimSecondsSinceRelease >= 0.0f;
	const bool turningOnShot = readPlayer && onShotMode && GetConfig().aimFollowsGaze &&
	                           !attackHeld && g_aimSecondsSinceRelease >= 0.0f;
	// A cast counts as an attack in progress for the return, and only for the
	// return.
	//
	// Both are the same question asked once: is there still something being
	// aimed that the body's heading decides? For an arrow the action field
	// answers it; for a spell the window does. Giving the turn back while the
	// window still stands would straighten the body before the spell has left,
	// which is precisely the fault that made every arrow fly forwards when the
	// return was first tried on the release frame.
	//
	// A spell now waits on exactly the same answer as an arrow, and that is the
	// point of arming the turn late rather than making it in the hook. The turn
	// goes in while the action field still reads Attack, so the field keeps the
	// body turned for the rest of the animation - which is now a fraction of a
	// second, not the whole of it - and the moment it reads FollowThrough the
	// spell has left and the body straightens.
	//
	// With the aim set at the source no action counts - see AimTurnOwnsAction;
	// with it off this is IsShotUnreleased by another name.
	const bool attackInProgress =
		((waitingOnAShot || turningOnShot) &&
		 AimTurnOwnsAction(atSource, game::ReadPlayerAction())) ||
		castTurning;
	// Decided once for both halves of the aim. The pitch is given back further
	// down, where it is written; it reads this rather than asking again,
	// because giving the turn back ends the release clock, and a second
	// asking would find it already ended.
	g_aimReturnDue =
		readPlayer &&
		AimReturnWanted(GetConfig().aimReturnOnRelease, g_headTracker.IsHeadsetConnected(),
		                game::IsMenuMode(), attackHeld, g_aimSecondsSinceRelease,
		                attackInProgress, aimStanding);
	if (g_aimReturnDue && g_aimBodyOffset == 0.0f) {
		// Only the pitch stood; the clock still has to end, or it would ask
		// for a return on every frame until the next shot.
		g_aimSecondsSinceRelease = -1.0f;
	}
	if (g_aimReturnDue && g_aimBodyOffset != 0.0f) {
		const float step = -g_aimBodyOffset;
		const float target = PlayerYawForGaze(asEngineLeftIt.yaw, step);
		if (game::WritePlayerYaw(target)) {
			// Booked the same way a step of the turn is, so the landing check
			// next frame covers the return as well: if something else moved the
			// player in the same frame, the offset comes back rather than the
			// view being left crooked.
			const float gaveBack = -step;
			const float waited = g_aimSecondsSinceRelease;
			g_aimBodyOffset = 0.0f;
			g_aimYawWrote = target;
			g_aimYawStepTaken = step;
			g_aimYawPending = true;
			g_aimSecondsSinceRelease = -1.0f;

			if (!g_aimReturnReported) {
				g_aimReturnReported = true;
				// Which of the two ended the wait is the thing worth knowing.
				// The attack state should be; the limit ending it says the
				// action values in GameAddresses.h are not this build's.
				OBVR_LOG("Aim: the body gives the turn back after the shot - %.1f degrees in "
				         "one frame, %.2f s after the control was released (%s), heading "
				         "%.4f to %.4f, and the view should not move",
				         static_cast<double>(gaveBack * math::kRadiansToDegrees),
				         static_cast<double>(waited),
				         waited >= kAimReturnLimitSeconds ? "the time limit, so the action "
				                                            "values are wrong for this build"
				                                          : "the attack finished",
				         static_cast<double>(asEngineLeftIt.yaw), static_cast<double>(target));
			}
		}
	}

	// WHEN the body is turned, which is the whole of Naragorn's suggestion.
	//
	// WhileAiming turns it for as long as the control is held, and leaves it
	// turned - so walking goes the way the shot went until something puts it
	// back. OnShot leaves the draw alone entirely and turns only for the few
	// frames in which the arrow is made, which is the only time the heading has
	// to be right. Aim anywhere, walk where the mouse says, nothing left
	// standing afterwards.
	//
	// They cannot be separated in space - the arrow and the walking read the
	// same rotZ, which is what the sideways-walking fault has been saying all
	// along - but they can be separated in time, because the arrow does not
	// exist until the shot is released.
	//
	// With the aim set at the source the turn is off altogether: a release
	// would otherwise still turn the body for its one frame, which is the
	// one frame this whole route exists to remove.
	const bool turnDue =
		(!atSource &&
		 AimTurnDue(onShotMode ? AimTurnMode::OnShot : AimTurnMode::WhileAiming, attackHeld,
		            attackWasHeld, attackInProgress)) ||
		castTurning;

	// The shot trace, armed by the release and running for forty frames. One
	// line a frame, so the sequence of actions across a real bow shot can be
	// read off rather than assumed - which is what deciding the shortest
	// possible window needs.
	if (readPlayer && (GetConfig().aimShotTrace || GetConfig().aimCastTrace)) {
		// Armed on the PRESS as well as the release, so the draw is in the
		// record too - the jump left is at the moment of letting go, and half
		// of what decides it happened before that.
		if (GetConfig().aimShotTrace && attackHeld && !attackWasHeld) {
			g_shotTraceLeft = 90;
			g_shotTraceFrame = 0;
		} else if (GetConfig().aimShotTrace && attackWasHeld && !attackHeld &&
		           g_shotTraceLeft < 40) {
			g_shotTraceLeft = 40;
		} else if (GetConfig().aimCastTrace && castArm.next.seconds == 0.0f) {
			// Armed by the cast itself rather than by the key, which is the
			// only way the record covers the spells a person actually casts.
			// The key window was never a condition of aiming; while the trace
			// hung off it, six casts out of seven left no record at all.
			//
			// The same instrument pointed at a cast, rather than a second one
			// beside it. Its columns are already the right columns: the head's
			// angle, the body's share, the view's heading either side of the
			// compensation, and the eye's own position - which between them are
			// what proved the bow's aim correct and found the last jump in it.
			//
			// Eighty frames, because the whole of a cast has to fit inside one
			// arming: the animation is 53 and the turn is made near its end.
			g_shotTraceLeft = 80;
			g_shotTraceFrame = 0;
		} else if (GetConfig().aimCastTrace && castHeld && !castWasHeld) {
			g_shotTraceLeft = 80;
			g_shotTraceFrame = 0;
		}
		if (g_shotTraceLeft > 0) {
			--g_shotTraceLeft;
			g_shotTraceHeld = attackHeld;
		}
	}

	// THE PITCH, written where the frame knows whether something is aimed.
	//
	// First person: every frame, as it has been since the aim went in - the
	// camera there does not depend on the field. Third person: borrowed while
	// aiming and given back with the body's turn, because that camera is built
	// from the field and the mouse's tilt is still the player's height control
	// - see camera::AimPitchHold for the bookkeeping, and the compensation
	// above for how the camera's share of it is taken back out of the picture.
	const bool pitchWanted =
		readPlayer && AimPitchWanted(GetConfig().aimFollowsGaze, g_headTracker.IsHeadsetConnected(),
		                             isThirdPerson, GetConfig().aimInThirdPerson,
		                             game::IsMenuMode(), turnDue);
	if (!isThirdPerson) {
		// Nothing is borrowed in first person, so nothing is owed. A hold left
		// over from third person ends here; the next third person frame starts
		// from whatever the field then holds.
		g_aimPitchHold = AimPitchHold{};
		g_aimPitchOffset = 0.0f;
		if (pitchWanted && game::WritePlayerPitch(gazePitch)) {
			g_aimLastWrittenPitch = gazePitch;
			g_aimEverWrote = true;
			if (!g_aimPitchReported) {
				g_aimPitchReported = true;
				OBVR_LOG("Aim: the player's pitch now follows the gaze - first write %.4f rad "
				         "(%.1f degrees, positive looks down)",
				         static_cast<double>(gazePitch),
				         static_cast<double>(gazePitch * math::kRadiansToDegrees));
			}
		}
	} else if (readPlayer) {
		AimPitchHoldInput holdInput;
		holdInput.enginePitch = asEngineLeftIt.pitch;
		// The return wins over a write on the one frame both could be due - the
		// release frame in OnShot mode - so a hold never outlives the clock
		// that would have ended it.
		holdInput.writeGaze = pitchWanted && !g_aimReturnDue;
		holdInput.gazePitch = gazePitch;
		holdInput.returnDue = g_aimReturnDue;
		const AimPitchHoldDecision hold = NextAimPitchHold(g_aimPitchHold, holdInput);
		const bool wasHeld = g_aimPitchHold.held;
		g_aimPitchHold = hold.next;
		g_aimPitchOffset = hold.offset;

		if (hold.write && game::WritePlayerPitch(hold.value)) {
			g_aimLastWrittenPitch = hold.value;
			g_aimEverWrote = true;
			if (!g_aimThirdPersonReported && !wasHeld && hold.next.held) {
				g_aimThirdPersonReported = true;
				OBVR_LOG("Aim: third person - the pitch is borrowed for the shot, %.4f rad written "
				         "over the mouse's %.4f, and the chase camera's share of the "
				         "difference is taken back out of the picture as it arrives",
				         static_cast<double>(hold.value),
				         static_cast<double>(hold.next.mouseTilt));
			}
		}
	}

	const bool aimTurnsBody =
		readPlayer &&
		AimYawWanted(GetConfig().aimFollowsGaze, g_headTracker.IsHeadsetConnected(), isThirdPerson,
		             GetConfig().aimInThirdPerson, game::IsMenuMode(), turnDue);
	if (aimTurnsBody) {
		// How far the head is turned away from the camera's base. The head
		// rotation is already relative to that base, so its heading is the turn
		// itself rather than a direction in the world - which is what lets this
		// be added to the engine's own heading without converting between the
		// two conventions.
		Heading headTurn{};
		if (HeadingOf(g_headTracker.GetCameraRotation(), headTurn)) {
			const float headYaw = math::Atan2(headTurn.sine, headTurn.cosine);

			// What is left after everything already handed over. With the head
			// held still this falls to zero and the body stops, facing the
			// gaze; it is not the head's angle, which would keep turning the
			// body for as long as the head was off centre.
			const float remaining = AimYawRemaining(headYaw, g_aimBodyOffset);
			const float step =
				Approach(0.0f, remaining, GetConfig().aimTurnSpeed, deltaSeconds);

			if (step != 0.0f) {
				const float target = PlayerYawForGaze(asEngineLeftIt.yaw, step);
				if (game::WritePlayerYaw(target)) {
					// Counted as taken here and checked next frame, rather than
					// counted only once it is known to have landed. The base
					// rotation this offset corrects is built by the engine from
					// the heading just written, so it arrives already turned;
					// waiting a frame would leave the view lurching by one step
					// every frame the body moved.
					g_aimBodyOffset = math::WrapAngle(g_aimBodyOffset + step);
					g_aimYawWrote = target;
					g_aimYawStepTaken = step;
					g_aimYawPending = true;

					if (!g_aimYawReported) {
						g_aimYawReported = true;
						OBVR_LOG("Aim: the body now follows the gaze while the attack "
						         "control is held - first step %.1f degrees of %.1f "
						         "remaining, heading %.4f to %.4f",
						         static_cast<double>(step * math::kRadiansToDegrees),
						         static_cast<double>(remaining * math::kRadiansToDegrees),
						         static_cast<double>(asEngineLeftIt.yaw),
						         static_cast<double>(target));
					}
				}
			}
		}
	}

	// The walk direction (vr/WalkDirection.h, [Hands] WalkDirection): while
	// the left stick walks, the body faces the head, a hand, or halfway
	// between the head and the stick's hand - through the same hand-over as
	// the aim's turn, so the camera takes the turn back out and the picture
	// holds still. The game then walks the stick's keys along that heading.
	// Not while the aim turns the body, nor after anything else wrote the
	// heading this frame (the return): that write is not in asEngineLeftIt.
	{
		const vr::StickDirections& walk = g_hand.controls.move;
		const bool walking = walk.forward || walk.back || walk.left || walk.right;
		if (readPlayer &&
		    vr::WalkSteerWanted(config.fullVrMode, g_headTracker.IsHeadsetConnected(), !isThirdPerson,
		                        game::IsMenuMode(), walking, aimTurnsBody || g_aimYawPending)) {
			vr::WalkYaws yaws;
			Heading headTurn{};
			yaws.headValid = HeadingOf(g_headTracker.GetCameraRotation(), headTurn);
			yaws.head = yaws.headValid ? math::Atan2(headTurn.sine, headTurn.cosine) : 0.0f;
			// The setting names the controllers as they are; g_hand has them
			// in their roles, swapped when left-handed.
			const bool swapped = g_handRolesSwapped;
			yaws.rightValid = swapped ? g_hand.leftWalkYawValid : g_hand.rightWalkYawValid;
			yaws.right = swapped ? g_hand.leftWalkYaw : g_hand.rightWalkYaw;
			yaws.leftValid = swapped ? g_hand.rightWalkYawValid : g_hand.leftWalkYawValid;
			yaws.left = swapped ? g_hand.rightWalkYaw : g_hand.leftWalkYaw;
			yaws.stickHandRight = swapped;
			float targetYaw = 0.0f;
			if (vr::WalkTargetYaw(config.hands.walkDirection, yaws, targetYaw)) {
				const float step = AimYawRemaining(targetYaw, g_aimBodyOffset);
				if (vr::WalkStepWorthWriting(step)) {
					const float target = PlayerYawForGaze(asEngineLeftIt.yaw, step);
					if (game::WritePlayerYaw(target)) {
						g_aimBodyOffset = math::WrapAngle(g_aimBodyOffset + step);
						g_aimYawWrote = target;
						g_aimYawStepTaken = step;
						g_aimYawPending = true;
						if (!g_walkSteerReported) {
							g_walkSteerReported = true;
							OBVR_LOG("Walk: the body turns to the walk direction (%s) - first step %.1f "
							         "degrees, head %.1f, right hand %.1f%s, left hand %.1f%s from the "
							         "recenter",
							         vr::kWalkDirectionNames[static_cast<UInt32>(config.hands.walkDirection)],
							         static_cast<double>(step * math::kRadiansToDegrees),
							         static_cast<double>(yaws.head * math::kRadiansToDegrees),
							         static_cast<double>(yaws.right * math::kRadiansToDegrees),
							         yaws.rightValid ? "" : " (not tracked)",
							         static_cast<double>(yaws.left * math::kRadiansToDegrees),
							         yaws.leftValid ? "" : " (not tracked)");
						}
					}
				}
			}
		}
	}

	// How far the weapon should be turned, DECIDED here and APPLIED at the top
	// of the render.
	//
	// LAST IN THIS FUNCTION, AFTER EVERYTHING THAT CAN MOVE THE BODY, and that
	// position is the whole of the one-frame jump: "nach dem schiessen um genau
	// zu sein springt die anim".
	//
	// The arms take what is LEFT after the body, so the two only add up to the
	// same total if they are talking about the same frame. Computed before the
	// body was moved, they used last frame's share: on the frame the shot turns
	// the body, both were turned; on the frame the turn is given back, neither
	// was. One frame of double, or of nothing - which is precisely a jump, at
	// each end of the shot.
	//
	// The write itself still cannot happen here. The engine's animation step
	// runs after this hook and overwrites any bone set in it, which is why the
	// angle is carried to the render and applied there.
	g_weaponTurnWanted = false;
	// The hand-tracked mode's first rung: the weapon hand follows the right
	// controller's heading instead of the gaze. The hand and the head are
	// read in the same seated space and converted the same way, so the turn
	// is the plain difference of their headings - the recenter reference
	// cancels out of it - applied on top of the camera, which already faces
	// the head. Pitch and position of the hand are the next rungs, see
	// docs/hand-tracked-mode.md. Reported on change of tracking so a run
	// says when the controller was seen and when it was lost.
	g_handArmsWanted = readPlayer && !isThirdPerson && config.fullVrMode &&
	                   g_headTracker.IsHeadsetConnected() && !game::IsMenuMode() &&
	                   g_hand.armsValid;
	if (!g_handArmsWanted && readPlayer && !isThirdPerson && config.aimFollowsGaze &&
	    config.aimWeaponFollowsGaze && g_headTracker.IsHeadsetConnected() &&
	    !game::IsMenuMode()) {
		Heading weaponTurn{};
		if (HeadingOf(g_headTracker.GetCameraRotation(), weaponTurn)) {
			const float headYaw = math::Atan2(weaponTurn.sine, weaponTurn.cosine);
			// The PREVIOUS frame's offset, because that is the share the base
			// the arms are turned on top of actually holds. See the global's
			// note, and the trace lines quoted there.
			g_weaponTurnRadians = AimYawRemaining(headYaw, g_aimBodyOffsetLastFrame);
			g_weaponTurnWanted = true;
		}
	}

	// Kept for the next frame, after everything that could change it. The arms
	// are the only thing that reads it: the body's own arithmetic uses the
	// current value throughout, because the heading it writes takes effect
	// within the frame - which the trace also settled.
	g_aimBodyOffsetLastFrame = g_aimBodyOffset;

	// The aim's gates, written whenever they change and not otherwise. Sitting
	// at the end of the pass, the line also says the pass got this far: a
	// session whose log has no gate line at all had every frame stop before
	// here. Six values, so a headset run that aims nowhere can be read
	// without a second run.
	{
		const UInt32 gates = (readPlayer ? 1u : 0u) | (g_headTracker.IsHeadsetConnected() ? 2u : 0u) |
		                     (game::IsMenuMode() ? 4u : 0u) | (isThirdPerson ? 8u : 0u) |
		                     (atSource ? 16u : 0u) | (viewAimed ? 32u : 0u);
		static UInt32 s_gatesSeen = 0xFFFFFFFFu;
		static int s_gateLinesLeft = 24;
		if (gates != s_gatesSeen && s_gateLinesLeft > 0) {
			s_gatesSeen = gates;
			--s_gateLinesLeft;
			OBVR_LOG("Aim gates: player=%d headset=%d menu=%d third=%d atSource=%d viewAimed=%d",
			         (gates & 1u) ? 1 : 0, (gates & 2u) ? 1 : 0, (gates & 4u) ? 1 : 0,
			         (gates & 8u) ? 1 : 0, (gates & 16u) ? 1 : 0, (gates & 32u) ? 1 : 0);
		}

		// Once, the first time the player can be read: where it is, which
		// method table its MagicCaster base carries, and what that table's
		// key handler slot holds. The source aim re-points the slot in the
		// PlayerCharacter table read out of the file; if the object in the
		// running game carries some other table, this is the line that says so.
		static bool s_playerReported = false;
		if (readPlayer && !s_playerReported) {
			s_playerReported = true;
			const UInt32 playerAddress = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
			const UInt32 casterTable =
				*reinterpret_cast<const UInt32*>(playerAddress + addr::kPlayerMagicCasterOffset);
			const UInt32 slotValue =
				mem::LooksLikeObjectAddress(casterTable) || casterTable >= 0x00400000u
					? *reinterpret_cast<const UInt32*>(casterTable + 0x18)
					: 0;
			OBVR_LOG("Aim: player at %08X, its MagicCaster table at %08X (the file's is %08X), "
			         "key handler slot holds %08X (the file's handler is %08X)",
			         playerAddress, casterTable,
			         addr::kPlayerCasterVtableKeyHandlerSlot - 0x18, slotValue,
			         addr::kAnimationKeyHandler);
		}
	}

	// The trace line, written HERE rather than where the trace is counted down,
	// because only at this point are both halves of the aim settled for this
	// frame. body + weapon is what the wearer actually sees the bow pointing
	// along, and if that column moves while the head is still, the frame it
	// moves on is the jump, reported as: "the arm or the bow jumps to the left
	// when I aim left, and then back to the target".
	if (g_shotTraceLeft > 0 || (readPlayer && GetConfig().aimShotTrace && g_shotTraceHeld)) {
		Heading traceTurn{};
		const float headYaw = HeadingOf(g_headTracker.GetCameraRotation(), traceTurn)
		                          ? math::Atan2(traceTurn.sine, traceTurn.cosine)
		                          : 0.0f;
		const float weapon = g_weaponTurnWanted ? g_weaponTurnRadians : 0.0f;

		// VIEW is the heading the picture is built along, read back after the
		// compensation rather than assumed from it. It has now been measured
		// across five shots and it does not move: 152.9 in every frame of
		// every window, through both handovers, while raw swung the body's
		// whole 32.5 degrees and back. The compensation is exact, and the
		// frame it lands in is the one its comment claims.
		//
		// EYE is where that leaves the search. With the vertical look dropped
		// in first person the camera's rotation is a pure heading, so VIEW
		// holding still means the rotation holds still - there is no third
		// angle left for a jump to hide in. What is left is the one channel
		// nothing here has ever measured: the camera's POSITION. A body that
		// turns 32.5 degrees swings anything standing off its axis through an
		// arc, and a picture whose viewpoint steps sideways for one frame is
		// indistinguishable from one that turns. "es ist die ganze view. das
		// ganze bild."
		//
		// EYE answered that, and the answer was yes. Across nine shots it
		// stepped 1.7 units sideways when the body took the turn and 1.7 back
		// when it gave it up - eighteen steps, no exceptions, nothing else in
		// the log moving at all. Two and a half centimetres of viewpoint, in
		// one frame, twice a shot. The jump is a TRANSLATION and was never an
		// angle.
		//
		// LOCAL and FEET are what turns that into a correction. Undoing the
		// step means rotating the camera back about the player, and a rotation
		// needs a centre; FEET is the centre and LOCAL is the arm's other end.
		// EYE cannot serve as that end because it is read from the world
		// transform and so is one frame stale - which is exactly why its step
		// lands a frame after the heading's, while LOCAL is this frame's and
		// in step with the offset that has to cancel it.
		//
		// What one run of these settles: whether the camera node's parent
		// carries the player's position or sits at the world origin. LOCAL
		// near EYE means the origin and the arm is LOCAL minus FEET; LOCAL
		// near zero means the parent carries it and the arm is LOCAL itself.
		// Both give exact arithmetic. Guessing between them would put a
		// rotation centre in the code that nothing measured, which is the one
		// mistake this search has already paid for three times.
		//
		// CHASE and the pitch pair are the third person's columns. In first
		// person CHASE trails body by nothing that matters; in third person it
		// is the share the camera has taken, and VIEW holding still while body
		// and CHASE part company is the whole proof that the eased
		// compensation is right. The pitch pair is the borrowed pitch's offset
		// from the mouse and the camera's share of it, in radians, and TILT
		// the position put back for that share.
		OBVR_LOG("Shot trace %2u: %s action=%d turn=%d | head=%6.1f body=%6.1f chase=%6.1f "
		         "weapon=%6.1f | view raw=%7.1f VIEW=%7.1f | EYE %8.2f %8.2f %8.2f "
		         "| LOCAL %8.2f %8.2f %8.2f | FEET %8.2f %8.2f | ARC %6.2f %6.2f "
		         "| pitch %.3f/%.3f TILT %6.2f %6.2f %6.2f | rate %.3f/%.3f",
		         g_shotTraceFrame++, attackHeld ? "held" : "----", game::ReadPlayerAction(),
		         turnDue ? 1 : 0, static_cast<double>(headYaw * math::kRadiansToDegrees),
		         static_cast<double>(g_aimBodyOffset * math::kRadiansToDegrees),
		         static_cast<double>(g_aimChasedOffset * math::kRadiansToDegrees),
		         static_cast<double>(weapon * math::kRadiansToDegrees),
		         static_cast<double>(g_aimViewYawBefore * math::kRadiansToDegrees),
		         static_cast<double>(g_aimViewYawAfter * math::kRadiansToDegrees),
		         static_cast<double>(g_cameraWorldPos.x),
		         static_cast<double>(g_cameraWorldPos.y),
		         static_cast<double>(g_cameraWorldPos.z),
		         static_cast<double>(g_cameraLocalPos.x),
		         static_cast<double>(g_cameraLocalPos.y),
		         static_cast<double>(g_cameraLocalPos.z),
		         static_cast<double>(g_playerWorldPos.x),
		         static_cast<double>(g_playerWorldPos.y),
		         static_cast<double>(g_aimArcApplied.x),
		         static_cast<double>(g_aimArcApplied.y),
		         static_cast<double>(g_aimPitchOffset),
		         static_cast<double>(g_aimChasedPitch),
		         static_cast<double>(g_aimTiltApplied.x),
		         static_cast<double>(g_aimTiltApplied.y),
		         static_cast<double>(g_aimTiltApplied.z),
		         static_cast<double>(g_chaseYawRate),
		         static_cast<double>(g_chasePitchRate));
	}

	// The three pitches side by side. This measured how to make an arrow go
	// where the wearer is looking, and the answer is applied just above.
	//
	// What it found: rotX is the player's own and OBVR never wrote it, so the
	// mouse aimed while the view went elsewhere. Units and direction came from
	// two sources found separately - xOBSE stores the triple in radians with
	// rotX as pitch, and the Construction Set wiki says a positive value looks
	// DOWN, which is the opposite of every other pitch in this file. Sines
	// rather than degrees here because the matrix holds the sine directly; the
	// one conversion to an angle is math::Asin, built out of atan.
	//
	// Kept switched on by default from here rather than removed, because it is
	// now the way to see whether the write took: rotX following the view means
	// it did, rotX at 0.0000 while the view moves means it did not.
	if (GetConfig().aimProbe && g_aimProbeLeft > 0 && readPlayer &&
	    (g_state.frameCount % 20) == 0) {
		--g_aimProbeLeft;

		// Does a written rotation survive the frame? The engine's value is
		// held up against what OBVR put there last time. Matching means the
		// field is kept and added to, which is what would make a written yaw
		// feed back into the camera it is read from.
		const float pitchDrift = asEngineLeftIt.pitch - g_aimLastWrittenPitch;
		const char* const survived =
			!g_aimEverWrote ? "nothing written yet"
			                : (Abs(pitchDrift) < 0.0005f ? "KEPT - a written rotation survives"
			                                             : "REPLACED - the engine sets it afresh");

		// Which way the two headings run against each other. This measured the
		// sign the sideways aim is built on: the camera's heading is read off
		// the levelled vanilla rotation, so it carries the player's heading and
		// nothing of the head, and their sum held at 0.0000 across sixty frames
		// while their difference wandered. Opposite conventions, so the step is
		// subtracted in PlayerYawForGaze.
		//
		// The base is now corrected for the turn already handed to the body, so
		// the sum is no longer expected to be zero while aiming - it is the
		// negative of that offset, and holding at exactly that is what says the
		// correction is landing. A sum that instead follows the head is the
		// correction failing, and the view would be turning with it.
		Heading vanillaHeading{};
		const bool haveHeading = HeadingOf(baseRotation, vanillaHeading);
		const float cameraYaw =
			haveHeading ? math::Atan2(vanillaHeading.sine, vanillaHeading.cosine) : 0.0f;

		Heading headTurn{};
		const float headYaw = HeadingOf(g_headTracker.GetCameraRotation(), headTurn)
			? math::Atan2(headTurn.sine, headTurn.cosine)
			: 0.0f;

		OBVR_LOG("Aim probe: engine left rotX=%.4f rotZ=%.4f | wrote %.4f last frame (%s) | "
		         "camera yaw=%.4f | sum=%.4f | head yaw=%.4f body offset=%.4f remaining=%.4f "
		         "| view sinPitch=%.4f | %s%s",
		         static_cast<double>(asEngineLeftIt.pitch),
		         static_cast<double>(asEngineLeftIt.yaw),
		         static_cast<double>(g_aimLastWrittenPitch), survived,
		         static_cast<double>(cameraYaw),
		         static_cast<double>(math::WrapAngle(asEngineLeftIt.yaw + cameraYaw)),
		         static_cast<double>(headYaw),
		         static_cast<double>(g_aimBodyOffset),
		         static_cast<double>(AimYawRemaining(headYaw, g_aimBodyOffset)),
		         static_cast<double>(SinPitchOf(finalRotation)),
		         isThirdPerson ? "third" : "first",
		         AttackHeld() ? ", aiming" : "");
	}

	// Where the engine put the third person camera, beside the rotation it
	// built it from. Every fourth frame, budgeted, and only in third person:
	// the whole measurement is a few seconds of the mouse tilting the view up
	// and down while the columns say how the viewpoint answers.
	//
	// LOCAL is the node as the engine left it this frame, before anything
	// OBVR adds; ARM is that position taken from the player's feet, which is
	// the number a compensation would have to turn; EYE is the previous
	// frame's world position, one frame stale and carrying OBVR's own offsets,
	// kept so LOCAL can be checked against the world it is meant to equal.
	//
	// CAM is the engine's own rotation for the camera, as a heading and a
	// sine of pitch, beside the player's rotX and rotZ it was built from. The
	// position is known to swing into place over a second; whether the
	// rotation does the same, or follows the player's field at once, decides
	// whether a turn written into the player can be taken back out of the
	// base rotation on the same frame - as the first person aim does - or
	// has to wait for the camera.
	if (GetConfig().thirdPersonProbe && g_thirdPersonProbeLeft > 0 && readPlayer &&
	    isThirdPerson && (g_state.frameCount % 4) == 0) {
		--g_thirdPersonProbeLeft;
		OBVR_LOG("Third person probe: rotX=%8.4f rotZ=%8.4f | CAM yaw=%8.4f sinPitch=%7.4f "
		         "| LOCAL %8.2f %8.2f %8.2f "
		         "| ARM %8.2f %8.2f %8.2f | EYE %8.2f %8.2f %8.2f | FEET %8.2f %8.2f %8.2f%s "
		         "| height %6.2f | head sinPitch=%.4f",
		         static_cast<double>(asEngineLeftIt.pitch),
		         static_cast<double>(asEngineLeftIt.yaw),
		         static_cast<double>(engineYaw),
		         static_cast<double>(SinPitchOf(engineRotation)),
		         static_cast<double>(g_cameraLocalPos.x),
		         static_cast<double>(g_cameraLocalPos.y),
		         static_cast<double>(g_cameraLocalPos.z),
		         static_cast<double>(g_cameraLocalPos.x - g_playerWorldPos.x),
		         static_cast<double>(g_cameraLocalPos.y - g_playerWorldPos.y),
		         static_cast<double>(g_cameraLocalPos.z - g_playerWorldPos.z),
		         static_cast<double>(g_cameraWorldPos.x),
		         static_cast<double>(g_cameraWorldPos.y),
		         static_cast<double>(g_cameraWorldPos.z),
		         static_cast<double>(g_playerWorldPos.x),
		         static_cast<double>(g_playerWorldPos.y),
		         static_cast<double>(g_playerWorldPos.z),
		         g_playerWorldValid ? "" : " (feet unknown)",
		         static_cast<double>(verticalOffset),
		         static_cast<double>(SinPitchOf(g_headTracker.GetCameraRotation())));
	}

	// The camera steps to an eye. Which eye, and for how long, is what
	// separates the two stereo modes:
	//
	//   * alternate eyes: one eye per frame, the other next frame. Depth
	//     without drawing the world twice, at the price of the two eyes
	//     holding pictures a frame apart.
	//   * dual pass: the left eye now, and the scene render hook moves the
	//     camera to the right eye between the two passes it runs. Both eyes
	//     drawn this frame, from this frame's pose.
	//
	// Carried by the final rotation rather than the base one, and the
	// difference matters. The head offset above is measured in the frame the
	// wearer recentered in, so the levelled rotation is what belongs under it.
	// The eyes are attached to the head: where "right" is for them depends on
	// where the head is looking, which is what the head rotation adds.
	const bool stereoAer = config.tracker.stereo == vr::StereoMode::AlternateEyes;
	const bool stereoDual = config.tracker.stereo == vr::StereoMode::DualPass;

	// Re-decided every camera pass, so a frame whose passes never ran - a
	// menu opening, a mode change - cannot leave last frame's arming behind.
	g_dualArmed = false;
	g_dualNode = nullptr;

	if ((stereoAer || stereoDual) && g_headTracker.IsHeadsetConnected()) {
		// The tracker reports the measured separation; the multiplier from the
		// INI is a preference and is applied here, where the camera steps to
		// an eye. Both modes and the dual shift below run off this one figure,
		// so the scale cannot reach one of them and miss another.
		const float half = ScaledEyeHalfSeparation(g_headTracker.GetHalfEyeSeparationUnits(),
		                                           config.tracker.eyeSeparationScale);
		// Which eye this frame steps to first. One answer for the offset here,
		// the shift below and the captures in the callbacks - see
		// FirstPassDrawsLeftEye. Alternate eyes takes its eye from the frame
		// number instead, one per frame.
		const bool firstIsLeft = FirstPassDrawsLeftEye(config.swapEyeOrder);
		const bool thisEyeIsLeft =
			stereoDual ? firstIsLeft : IsLeftEyeFrame(g_state.frameCount);
		const EyeStep step = StereoEyeStep(half, thisEyeIsLeft);

		const NiPoint3 eyeOffset{step.toFirstEye, 0.0f, 0.0f};
		cameraNode->localTransform.pos =
			cameraNode->localTransform.pos + finalRotation * eyeOffset;
		g_bodyFirstEyeStep = finalRotation * eyeOffset;

		if (stereoDual && render::IsSceneRenderHooked()) {
			// From one eye to the other is the whole interpupillary distance,
			// along the same head-carried axis the offset above used, and in
			// whichever direction that offset went - so the step always lands on
			// the eye the first render did not draw. Worked out in StereoEyeStep
			// rather than here, because the menu path needs the same two numbers
			// and the copy of this line that used to live there had the sign
			// backwards.
			g_dualNode = cameraNode;
			g_dualShift = finalRotation * NiPoint3{step.toSecondEye, 0.0f, 0.0f};
			g_dualArmed = true;
		}
	}

	cameraNode->localTransform.rot = finalRotation;

	// Last, and on purpose. This blocks until the compositor wants the next
	// frame, so from here Oblivion runs on the compositor's clock rather than
	// its own - which is what keeps the picture in step with the headset, and
	// is the arrangement 0.1.0 needs, since the texture submitted then has to
	// be the frame the game has just drawn.
	//
	// It does nothing at all unless rendering was asked for and the
	// compositor was reached, so the cost on every other machine is one
	// comparison.
	render::HeadsetRenderer::FrameRequest request;
	request.gameDevice = render::GetGameDevice();
	request.submitGameFrame = config.tracker.submitGameFrame;
	request.alternateEyes = stereoAer;

	// Only claimed when the second pass can actually happen. With the scene
	// render unhooked the captures never arrive, and the submit would fall
	// back every frame; saying mono from the start keeps the fallback path
	// the one that was chosen rather than the one that was reached. The
	// probe rung that cuts the second render counts as the same thing, which
	// is what lets that rung be switched on and off mid-session against a
	// live picture rather than a frozen one.
	request.dualEyes =
		DeliversDualEyes(stereoDual, render::IsSceneRenderHooked(), DualProbeRung());

	// The same call the camera offset above used, so the eye the camera moved
	// to and the eye the picture is given to cannot drift apart.
	request.isLeftEye = IsLeftEyeFrame(g_state.frameCount);
	request.gameFovDegrees = config.tracker.gameFovDegrees;
	request.gameFovIsFor4x3 = config.tracker.gameFovIsFor4x3;

	// Read at the start of this pass, not here at the end. The frustum differs
	// between the two: 1.0231 across when the camera is computed, 1.1188 by the
	// time Present runs. Which of those the frame was drawn with is not settled,
	// but the first is the one that matches fDefaultFOV exactly, and the second
	// is read after every pass of the frame has had its turn with the camera.
	request.cameraTanHalfWidth = g_state.cameraTanHalfWidth;
	request.cameraTanHalfHeight = g_state.cameraTanHalfHeight;
	request.menuScale = config.tracker.menuScale;
	request.menuAspect = config.tracker.menuAspect;

	if (!g_frameOpen) {
		// BeginFrame declined at the top of this pass: rendering is off, the
		// compositor was never reached, or it asked us to stop. Nothing is
		// owed, and submitting anyway would be a Submit with no WaitGetPoses
		// in front of it.
		return;
	}

	if (!config.tracker.submitAtFrameEnd) {
		// Submitted here, which means the picture is whatever the back buffer
		// held from last time. One frame of latency, and the arrangement that
		// is known to work.
		request.backBufferIsThisFrame = false;
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), request);
		return;
	}

	// The other arrangement: the wait already happened at the top of this
	// pass, Oblivion is about to draw, and Present hands the finished picture
	// over.
	//
	// The request is kept rather than rebuilt at the end, because the eye it
	// names has to be the eye the camera was moved to a few lines above. Two
	// separate readings of the frame counter would be two chances to disagree,
	// and disagreeing means each eye showing the other's viewpoint.
	request.backBufferIsThisFrame = true;
	g_pendingRequest = request;

	if (!render::IsPresentHooked() && !g_presentHookRefused) {
		if (!render::InstallPresentHook(request.gameDevice, &OnPresent)) {
			// Said once. Without the hook the end of the frame never arrives,
			// so falling back to submitting here is better than a headset that
			// quietly stops being fed.
			g_presentHookRefused = true;
			OBVR_LOG("Render: the frame end could not be hooked, so the submit stays at the "
			         "start of the frame and the picture is one frame old");
		}
	}

	if (g_presentHookRefused) {
		g_pendingRequest.backBufferIsThisFrame = false;
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), g_pendingRequest);
	}
}

vr::HeadTracker& GetHeadTracker() { return g_headTracker; }

bool GetCyclopeanCameraWorldPosition(NiPoint3& position) {
	if (!g_cyclopeanCameraWorldValid) return false;
	position = g_cyclopeanCameraWorldTransform.pos;
	return true;
}

bool GetHeadIndependentWaterCameraTransforms(NiTransform& local, NiTransform& world) {
	if (!g_cyclopeanCameraWorldValid) return false;
	NiTransform parentWorld{}; parentWorld.rot = NiMatrix33::Identity(); parentWorld.scale = 1.0f;
	if (g_bodyCameraNode != nullptr && g_bodyCameraNode->parent != nullptr)
		parentWorld = g_bodyCameraNode->parent->worldTransform;
	render::BuildHeadIndependentWaterCamera(g_cyclopeanCameraLocalTransform, g_cyclopeanCameraWorldTransform, parentWorld, g_menuBaseRot, g_menuBasePos, local, world);
	return true;
}

bool GetCurrentCameraWorldTransform(NiTransform& transform) {
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(g_bodyCameraNode))) return false;
	transform = g_bodyCameraNode->worldTransform;
	return true;
}

const State& GetState() { return g_state; }

bool Install() {
	const Config& config = GetConfig();
	g_headTracker.Configure(config.tracker);
	g_lookControl.Configure(config.look);

	// Check first, patch second. If something other than the expected bytes
	// sits there, it is a different game version or another mod got there
	// first - in either case patching would be a shot in the dark.
	if (!mem::Verify(addr::kHookCameraUpdate, kOriginalBytes, addr::kHookCameraUpdatePatchSize)) {
		OBVR_LOG("Camera: bytes at %08X differ, hook will not be installed",
		         addr::kHookCameraUpdate);
		mem::ReportForeignCode("Camera", addr::kHookCameraUpdate);
		return false;
	}

	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineSize));
	if (trampoline == nullptr) {
		OBVR_LOG("Camera: no executable memory for the trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = BuildTrampoline(
		trampoline, kTrampolineSize, trampolineAddress,
		reinterpret_cast<UInt32>(&OBVR_OnCameraUpdated));

	if (trampolineSize == 0) {
		OBVR_LOG("Camera: trampoline does not fit into %u bytes", kTrampolineSize);
		return false;
	}

	UInt8 patch[addr::kHookCameraUpdatePatchSize];
	const UInt32 patchSize =
		BuildPatch(patch, sizeof(patch), addr::kHookCameraUpdate, trampolineAddress);

	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Camera: patch has unexpected length %u", patchSize);
		return false;
	}

	if (!mem::SafeWrite(addr::kHookCameraUpdate, patch, patchSize)) {
		OBVR_LOG("Camera: SafeWrite to %08X failed", addr::kHookCameraUpdate);
		return false;
	}

	OBVR_LOG("Camera: hook installed at %08X, trampoline at %08X (%u bytes)",
	         addr::kHookCameraUpdate, trampolineAddress, trampolineSize);

	InstallCastHook();
	game::InstallAimAtSource();
	game::InstallTeleportNoise();
	game::VerifyHandBodyAddresses();
	game::VerifyGameSoundAddresses();
	game::VerifyShoveAddresses();
	game::InstallPlayerStagger();
	game::InstallPlayerLookAt();
	game::InstallWorldPickHook();

	// The end of the frame, hooked as soon as there is a device to hook it on
	// rather than when the first world camera runs.
	//
	// The difference is everything before that camera: the intro videos, the
	// main menu, and the dialogue that loads a save. Installing from the camera
	// hook meant none of those reached the headset - which was reported as
	// seeing only the loading screen, and only because by then the hook had
	// gone in.
	if (GetConfig().tracker.renderToHeadset && GetConfig().tracker.submitAtFrameEnd) {
		render::InstallPresentHookWhenReady(&OnPresent);
	}

	// Dual pass: the world drawn twice per frame, once per eye. The detour
	// goes in at load; whether a given frame actually runs twice is decided
	// per frame by ScenePassWanted.
	if (GetConfig().tracker.renderToHeadset &&
	    GetConfig().tracker.stereo == vr::StereoMode::DualPass) {
		if (!GetConfig().tracker.submitAtFrameEnd) {
			// The captures happen mid-frame and the submit pays them at
			// Present. Submitting at the start of the frame instead would
			// hand over pictures that have not been drawn yet.
			OBVR_LOG("Config: Stereo=dual needs SubmitAtFrameEnd=1, so the world stays "
			         "single-pass");
		} else {
			render::ScenePassCallbacks callbacks;
			callbacks.beforeFirstPass = &BeforeFirstScenePass;
			callbacks.wantsSecondPass = &ScenePassWanted;
			callbacks.betweenPasses = &BetweenScenePasses;
			callbacks.afterSecondPass = &AfterSecondScenePass;
			callbacks.afterWorldRender = &AfterWorldRender;
			callbacks.probeStage = &DualProbeRung;
			// Logs its own outcome either way; on failure the mode quietly
			// renders like mono, and request.dualEyes says so per frame.
			render::InstallSceneRenderHook(callbacks);
		}
	}

	// The HUD overlay: the 2D layer redirected to its own texture on world
	// frames and hung in the room. The hook itself goes in whenever the
	// frame-end submit is on, not only when the overlay is: with
	// HudOverlay=0 every pass runs vanilla - beginRedirect answers null -
	// and only the invocation window watches. That watching is the point:
	// the monitor lost its HUD with no redirect installed at all, so what
	// the untouched pass does is now evidence this hook collects.
	if (GetConfig().tracker.renderToHeadset) {
		if (!GetConfig().tracker.submitAtFrameEnd) {
			// The capture happens between the world render and Present, and
			// the submit pays it at Present. Submitting at the start of the
			// frame would hand over a picture that has not been drawn yet.
			if (GetConfig().tracker.hudOverlay) {
				OBVR_LOG("Config: HudOverlay needs SubmitAtFrameEnd=1, so the 2D layer "
				         "stays in the frame");
			}
		} else {
			render::InterfaceRedirect redirect;
			redirect.beginRedirect = &HudBeginRedirect;
			redirect.endRedirect = &HudEndRedirect;
			redirect.probeActive = &HudProbeActive;
			// Logs its own outcome either way; on failure the HUD simply
			// stays in the frame, which on a flat frame is still shown.
			render::InstallInterfaceRenderHook(redirect);
		}


		// And the hover's other half: the tile search runs under the believed
		// viewport, so highlight and click answer in the drawn space. Logs its
		// own outcome; inert while belief and frame agree.
		render::InstallCursorPickHook();
	}

	// Oblivion's frame size, set where it is decided.
	//
	// This used to write iSize into Oblivion.ini and let the game read it next
	// time - a detour past the place the decision is made, costing two
	// restarts and editing a file that belongs to the user. The device
	// creation is where the size actually lives, so that is where this is now.
	if (!GetConfig().tracker.setRenderSize) {
		OBVR_LOG("Config: SetGameResolution is off, so the frame is the game's own size");
	} else {
		UInt32 width = GetConfig().tracker.renderWidth;
		UInt32 height = GetConfig().tracker.renderHeight;

		if (width == 0 || height == 0) {
			// Whatever the headset asks for. That figure already accounts for
			// the distortion margin the compositor needs, so it is the honest
			// answer to "what can this headset use".
			if (!g_headTracker.GetBackend().GetRecommendedRenderTargetSize(width, height)) {
				OBVR_LOG("Config: the headset reported no render size, so the frame is the "
				         "game's own");
				width = 0;
				height = 0;
			}
		}

		if (width != 0 && height != 0) {
			render::SetWantedResolution(width, height);
			if (render::InstallResolutionHook()) {
				OBVR_LOG("Config: the frame will be created at %ux%u", width, height);
			} else {
				// Not a silent fallback. The reason has already been logged by
				// whichever of the two routes was tried; this says what it cost.
				OBVR_LOG("Config: neither way into Direct3DCreate9 was open, so the "
				         "frame stays at the game's own size");
			}
		}
	}

	return true;

}

}  // namespace obvr::camera

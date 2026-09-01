#include "camera/CameraHook.h"

#include "camera/CameraTrampoline.h"
#include "camera/CastTrampoline.h"
#include "camera/LookControl.h"
#include "core/Config.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "core/MathFns.h"
#include "game/CrosshairTarget.h"
#include "game/DialogZoom.h"
#include "game/FirstPersonArms.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/MenuBackground.h"
#include "core/Rotation.h"
#include "game/MenuMode.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"
#include "render/DxvkInterop.h"
#include "render/GameDevice.h"
#include "render/GameProjection.h"
#include "render/HeadsetRenderer.h"
#include "render/CrosshairLayer.h"
#include "render/HudLayer.h"
#include "ui/SettingsMenu.h"
#include "ui/SettingsMenuLayer.h"
#include "render/InterfaceRenderHook.h"
#include "render/CursorPickHook.h"
#include "render/CursorProbe.h"
#include "render/SceneGraphProbe.h"
#include "render/LayoutProbe.h"
#include "render/MenuShade.h"
#include "render/PresentHook.h"
#include "render/ResolutionHook.h"
#include "render/SceneRenderHook.h"

namespace obvr::camera {
namespace {

State g_state;
vr::HeadTracker g_headTracker;

constexpr UInt32 kTrampolineSize = 64;

KeyEdge g_recenterEdge;
FrameClock g_frameClock;
LookControl g_lookControl;
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

// OBVR's own settings menu: what it is showing, and the quad it shows it on.
//
// Separate objects on purpose. The menu is state and decisions and can be
// driven from a test; the layer is a texture and an overlay and cannot.
ui::SettingsMenu g_settingsMenu;
ui::SettingsMenuLayer g_settingsMenuLayer;

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
bool g_aimThirdPersonReported = false;

// How far the first person weapon should be turned, and whether it should be
// turned at all. Decided in the camera pass, applied at the top of the render -
// see BeforeFirstScenePass for why those cannot be the same place.
float g_weaponTurnRadians = 0.0f;
bool g_weaponTurnWanted = false;

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
void OnFrameEnd() {
	// The engine's own live menu background, asked for every frame because the
	// INI is hot reloaded and the call is a byte comparison. Off leaves the
	// engine's setting alone rather than forcing the static background on -
	// see ApplyLiveMenuBackground.
	game::ApplyLiveMenuBackground(GetConfig().tracker.liveMenuBackground);

	// Cleared for the next frame, exactly as g_frameOpen is: it is the guard
	// that keeps one menu frame from being armed twice, and a frame that armed
	// itself must not leave that standing.
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
	const bool menuIsUp = GetConfig().tracker.showMenus && game::IsMenuMode();
	const FrameDelivery delivery = DeliverFrame(
		hadCameraPass, menuIsUp,
		MenusCanReachTheWorld(GetConfig().tracker.menusInWorld, GetConfig().tracker.hudOverlay),
		g_headsetRenderer.HasHeldEyes(), g_worldlessStreak);

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
		OBVR_LOG("Menu trace: a menu just %s - %s (0x%03X)",
		         menuFlagChanged ? (menuIsUp ? "opened" : "closed") : "changed",
		         game::MenuIdName(menuId), menuId);
	}
	// Counted down once a frame, here rather than beside each mark: the marks
	// come several to a frame and would otherwise burn the window in two.
	if (g_stepTraceLeft > 0) {
		--g_stepTraceLeft;
	}

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
		if (recenterPressed) {
			g_hudLayer.ResetAnchor();
			OBVR_LOG("Render: the menu overlay was re-anchored on the recenter key "
			         "(held path)");
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
		const bool dressing =
			menuIsUp && MenuDressingWanted(g_presentedFrame - g_menuOpenedFrame);
		held.menuShadeColor =
			(dressing && GetConfig().tracker.menuShade)
				? render::ComposeShadeColor(GetConfig().tracker.menuShadeColorRgb,
			                                GetConfig().tracker.menuShadeStrength)
				: 0;
		held.menuSingleBorder = dressing && GetConfig().tracker.menuSingleBorder;

		// Once per menu episode: whether its held run wears the dressing.
		// This line is the evidence the washed-grey diagnosis rests on - a
		// dialogue's end should log "skips", a pause menu "carries".
		if (menuIsUp && !g_dressingReportedThisMenu && g_dressingReportsLeft > 0) {
			g_dressingReportedThisMenu = true;
			--g_dressingReportsLeft;
			OBVR_LOG("Render: held frames %s the menu dressing - the menu opened %u "
			         "frames ago%s",
			         dressing ? "carry" : "skip", g_presentedFrame - g_menuOpenedFrame,
			         dressing ? "" : " (a dialogue's exit fade, most likely)");
		}
		if (g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame())) {
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
	if (recenterPressed) {
		g_headsetRenderer.ResetFlatAnchor();

		// The HUD's room anchor goes with it. The layer is hidden on a flat
		// frame, so nothing moves while the menu is up - but the anchor it
		// would otherwise come back to is the one from before the menu, and
		// the key was pressed because that one is in the wrong place.
		g_hudLayer.ResetAnchor();
		OBVR_LOG("Render: the flat picture was re-anchored on the recenter key (flat path)");
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

	// A camera pass means BeginFrame has already run for this frame and the
	// frame is open. Calling it again would call WaitGetPoses a second time,
	// which blocks until the next frame - the whole point of a menu being flat
	// is that it costs nothing extra.
	if (hadCameraPass || g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame())) {
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

void MaybeReloadConfig() {
	Config& config = GetConfig();
	if (!IsDue(g_state.frameCount, config.reloadEveryFrames)) {
		return;
	}

	if (config.Reload("OBVR.ini")) {
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

	NiPoint3 pos = g_menuBasePos + baseRotation * g_headTracker.GetCameraOffset();
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

	if (g_weaponTurnWanted) {
		game::TurnFirstPersonArms(g_weaponTurnRadians);
	} else {
		// Third person, a menu, or switched off. Put the arms back rather than
		// leaving them holding a turn nothing is going to update.
		game::ReleaseFirstPersonArms();
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
}

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

void BetweenScenePasses() {
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
		g_headsetRenderer.CaptureEye(g_pendingRequest, firstIsLeft);
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
		const bool captured = render::RunHudPassBetweenScenes();
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
	const UInt32 probe = DualProbeRung();
	if (DualTraceOn()) {
		OBVR_LOG("Dual trace: second pass returned");
	}

	const bool firstIsLeft = FirstPassDrawsLeftEye(GetConfig().swapEyeOrder);
	if (probe != 1 && probe != 2) {
		g_headsetRenderer.CaptureEye(g_pendingRequest, !firstIsLeft);
		if (DualTraceOn()) {
			OBVR_LOG("Dual trace: %s eye captured from the second pass",
			         firstIsLeft ? "right" : "left");
		}
	}

	// Back where the game left it, and updated again, so everything that
	// reads the camera later in the frame - the 2D layer, next frame's
	// smoothing - sees the camera the game computed rather than an eye.
	if (probe != 1 && g_dualNode != nullptr) {
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

	char text[32];
	FormatValueForIni(ui::ItemFor(*definition, config), definition->falseWord,
	                  definition->trueWord, text, sizeof(text));

	if (SaveSetting(definition->iniSection, definition->iniKey, text)) {
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

void PollSettingsMenu() {
	const Config& config = GetConfig();

	const auto down = [](UInt32 key) {
		return key != 0 && (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	};

	if (config.settingsMenuKey == 0) {
		g_menuToggleEdge.Reset();
	} else if (g_menuToggleEdge.Update(down(config.settingsMenuKey))) {
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
		if (g_menuUpEdge.Update(down(0x26))) {  // VK_UP
			g_settingsMenu.Apply(ui::MenuAction::Up, writable);
		}
		if (g_menuDownEdge.Update(down(0x28))) {  // VK_DOWN
			g_settingsMenu.Apply(ui::MenuAction::Down, writable);
		}
		if (g_menuLeftEdge.Update(down(0x25))) {  // VK_LEFT
			SaveChangedSetting(g_settingsMenu.Apply(ui::MenuAction::Decrease, writable),
			                   writable);
		}
		if (g_menuRightEdge.Update(down(0x27))) {  // VK_RIGHT
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
	ui::MenuItem items[64];
	const char* categories[64];
	const UInt32 count =
		g_settingsMenu.BuildRows(GetConfig(), items, categories, 64);

	g_settingsMenuLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                           g_settingsMenu.IsOpen(), items, categories, count,
	                           g_settingsMenu.State(), g_settingsMenu.Revision(),
	                           config.settingsMenuDistanceMetres,
	                           config.settingsMenuWidthMetres, config.settingsMenuInWorld);
}

void MaybeSubmitOverlays(bool worldFrame) {
	const Config& config = GetConfig();

	PollSettingsMenu();

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
	if (visibility.thirdPerson ? visibility.onlyWhenNeededThirdPerson
	                           : visibility.onlyWhenNeeded) {
		visibility.weaponDrawn = game::ReadPlayerWeaponState() != game::WeaponState::Sheathed;
	}

	const bool crosshairWanted = CrosshairWanted(visibility);

	// Oblivion's own crosshair, lifted out of the captured layer and into the
	// depth quad - which is also what takes it out of the flat one, so it is
	// not shown twice at two distances.
	//
	// Only on a frame that will actually show it, because the lift erases what
	// it takes: doing this where no crosshair is wanted would punch a hole in
	// the middle of a menu for nothing.
	bool crosshairLifted = false;
	if (crosshairWanted && config.tracker.hudOverlay && g_hudLayer.HasCapture()) {
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
	// Kept only while nothing is under the crosshair. With a target the lifted
	// square holds a context icon instead, and keeping a hand or a speech
	// bubble would freeze it into every third-person frame afterwards.
	if (crosshairLifted && !visibility.thirdPerson && !g_crosshairHasTarget) {
		g_crosshairLayer.RememberCrosshair(render::GetGameDevice());
	}

	// Pasted in only where the game leaves a gap. Oblivion draws no plain
	// crosshair in third person, which is what this fills - but it DOES draw
	// the context icons and the sneak eye, and the first version pasted over
	// both of them.
	//
	// The sneak state is read behind this gate rather than passed in as an
	// argument, because arguments are always evaluated: written the obvious
	// way, every first-person frame would follow a raw pointer into the
	// player's process to answer a question only third person asks.
	const bool couldBorrow = crosshairWanted && visibility.thirdPerson &&
	                         config.tracker.crosshairInThirdPerson;
	if (couldBorrow &&
	    BorrowedCrosshairWanted(visibility.thirdPerson, config.tracker.crosshairInThirdPerson,
	                            g_crosshairHasTarget, game::IsPlayerSneaking())) {
		// The drawn cross is the last resort, not the first choice: it is only
		// reached before this session has been in first person at all, and
		// stops being used the moment it has.
		if (!g_crosshairLayer.UseRememberedCrosshair(render::GetGameDevice())) {
			g_crosshairLayer.DrawCross(render::GetGameDevice(), !crosshairLifted);
		}
	}

	// The depth was decided in the camera pass, where the camera and the frame
	// time are. A zero means no camera pass has run yet - the main menu, the
	// first frames of a load - and the fixed distance stands in until one has.
	const float crosshairDepth = g_crosshairDepthMetres > 0.0f
	                                 ? g_crosshairDepthMetres
	                                 : config.tracker.crosshairDistanceMetres;
	const CrosshairPlacement crosshair =
		PlaceCrosshair(crosshairDepth, config.tracker.crosshairSizeAtOneMetre);
	g_crosshairLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                        crosshairWanted, crosshair.distanceMetres, crosshair.widthMetres);

	if (!config.tracker.hudOverlay || !render::IsInterfaceRenderHooked()) {
		return;
	}
	g_hudLayer.Submit(g_headTracker.GetBackendForFrame(), render::GetGameDevice(),
	                  worldFrame, config.tracker.hudDistanceMetres,
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
	const bool wantTarget = config.tracker.crosshairDynamic ||
	                        config.tracker.crosshairOnlyWhenNeeded ||
	                        config.tracker.crosshairProbe;

	const game::CrosshairTarget target =
		wantTarget ? game::ReadCrosshairTarget() : game::CrosshairTarget{};
	g_crosshairHasTarget = target.haveRef;

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
		if (g_crosshairDepthMetres <= 0.0f) {
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
	if (playerAddress < 0x00010000u || playerAddress > 0x7FFFFFFFu) {
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

	// The head offset is measured in the camera's own space, so it is carried
	// over by the base rotation. The vertical look is not: it is a height, and
	// heights are along the world up axis whichever way the camera faces.
	cameraNode->localTransform.pos =
		cameraNode->localTransform.pos + baseRotation * g_headTracker.GetCameraOffset();
	cameraNode->localTransform.pos.z += verticalOffset;

	const NiMatrix33 finalRotation = baseRotation * g_headTracker.GetCameraRotation();

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
	if (castWasOpen && !g_castWindow.open && !g_castReported) {
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
	const bool castTurning = g_castWindow.open && !castAtSpawn;

	// Whether a spell of the player's may be aimed at all. The hook asks
	// nothing beyond this - no head reading, no arithmetic - because it runs
	// inside a call the engine owns.
	g_castAimWanted = castAtSpawn && readPlayer && viewAimed &&
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
	clockInput.castFeedsClock = !castAtSpawn;
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
	const bool attackInProgress =
		((waitingOnAShot || turningOnShot) && game::IsShotUnreleased()) || castTurning;
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
	const bool turnDue =
		AimTurnDue(onShotMode ? AimTurnMode::OnShot : AimTurnMode::WhileAiming, attackHeld,
	               attackWasHeld, attackInProgress) ||
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

	if (readPlayer &&
	    AimYawWanted(GetConfig().aimFollowsGaze, g_headTracker.IsHeadsetConnected(), isThirdPerson,
	                 GetConfig().aimInThirdPerson, game::IsMenuMode(), turnDue)) {
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
	if (readPlayer && !isThirdPerson && config.aimFollowsGaze &&
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

	// The trace line, written HERE rather than where the trace is counted down,
	// because only at this point are both halves of the aim settled for this
	// frame. body + weapon is what the wearer actually sees the bow pointing
	// along, and if that column moves while the head is still, the frame it
	// moves on is the jump: "der arm oder der bogen springt nach links wenn ich
	// nach links ziele und dann wieder zum ziel".
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
		if (!render::InstallPresentHook(request.gameDevice, &OnFrameEnd)) {
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

	// The end of the frame, hooked as soon as there is a device to hook it on
	// rather than when the first world camera runs.
	//
	// The difference is everything before that camera: the intro videos, the
	// main menu, and the dialogue that loads a save. Installing from the camera
	// hook meant none of those reached the headset - which was reported as
	// seeing only the loading screen, and only because by then the hook had
	// gone in.
	if (GetConfig().tracker.renderToHeadset && GetConfig().tracker.submitAtFrameEnd) {
		render::InstallPresentHookWhenReady(&OnFrameEnd);
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

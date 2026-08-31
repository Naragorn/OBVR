#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/OpenVRBackend.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// Supplies the head rotation that is laid onto the vanilla camera.
//
// The source is interchangeable because bitness is the problem here:
// Oblivion.exe is 32 bit, so OBVR.dll is too. OpenVR has always supported
// 32-bit applications, OpenXR only on some runtimes - SteamVR for instance
// only from beta 2.17.2, so not on the stable branch yet. Under Proton
// OpenXR drops out entirely, because wineopenxr is only built for 64 bit
// there.
//
// Hence several sources instead of one fixed binding. The simulated head also
// makes it possible to check the whole chain from quaternion to camera matrix
// in the running game without a headset.
//
// Every source delivers its orientation in OpenXR convention (Y up,
// -Z forward), the OpenVR backend included. The conversion to Oblivion
// happens in exactly one place, so that every source takes the same route.
enum class TrackerSource {
	None,       // no additional rotation
	Fixed,      // fixed test angles from the configuration
	Simulated,  // slow head movement, for checking without an HMD
	OpenVR,     // real headset through SteamVR; covers 32 bit reliably
	OpenXR,     // real headset through OpenXR; only with a 32-bit capable runtime
};

enum class StereoMode {
	// One image to both eyes. Oblivion is in the headset, and flat: there is
	// only one picture, so there is nothing between the eyes to see depth in.
	None,

	// Alternate eye rendering. The camera steps to one eye, the frame is drawn
	// once from there, and both eyes are submitted - the drawn one with this
	// frame's picture, the other with its own last one, kept in a copy OBVR
	// owns. Both every frame is not an optimisation: the compositor counts a
	// frame as delivered only when both eyes have arrived.
	AlternateEyes,

	// Drawing the world twice per tick, once per eye. The honest way, and the
	// only one without a time disparity between the eyes. Not built: it needs
	// Gamebryo to render twice without advancing the simulation twice, and
	// whether it will is an open question rather than a matter of wiring.
	DualPass,
};

struct TrackerSettings {
	TrackerSource source = TrackerSource::Fixed;

	// For TrackerSource::Fixed. Degrees, in Oblivion axes.
	float fixedPitch = 0.0f;
	float fixedRoll = 0.0f;
	float fixedYaw = 0.0f;

	// For TrackerSource::Simulated. Degrees, and period length in frames.
	float simulatedYawAmplitude = 25.0f;
	float simulatedPitchAmplitude = 12.0f;
	UInt32 simulatedPeriodFrames = 600;

	// Whether head movement through space reaches the camera as well, not
	// only head rotation. Only TrackerSource::OpenVR delivers a position; the
	// other sources leave the camera where the game put it.
	bool positionalTracking = true;

	// Whether OBVR renders to the headset rather than only reading poses from
	// it.
	//
	// False here and 1 in the shipped INI, and the two do not disagree: this
	// value applies when there is no INI at all, which means a broken
	// installation rather than a configured one. Claiming the VR scene from
	// one of those turns the headset black with no file present to say why.
	//
	// It decides which kind of application OBVR registers as, and the two are
	// exclusive: submitting a frame requires VRApplication_Scene, which takes
	// the headset away from whatever else was showing something. Reading
	// poses wants VRApplication_Background, which does not.
	bool renderToHeadset = false;

	// Whether the picture in the headset is Oblivion's own frame rather than
	// the generated test pattern. Needs DXVK, and falls back to the pattern
	// with a line in the log if that is missing.
	//
	// Off until it has been seen to work. The pattern is what proved the
	// compositor path, and keeping it reachable is what makes a fault in the
	// game frame path attributable rather than merely visible.
	bool submitGameFrame = false;

	// How the two eyes are given different views of the world.
	//
	// None: one image, both eyes, no depth between them. What 0.1.0 reached
	// first, and still the honest fallback.
	//
	// AlternateEyes: the camera is offset to one eye per frame and the frame
	// goes to that eye alone, alternating. Depth without drawing the world
	// twice, at the price of the two eyes seeing pictures a frame apart -
	// which is a disparity in time rather than distance, and shows on
	// anything moving quickly.
	//
	// The offset uses unitsPerMetre and NOT EffectiveUnitsPerMetre. The
	// distance between two eyes is a fact about a head, not a matter of taste,
	// and scaling it would change the apparent size of the world rather than
	// how far leaning moves. Keeping those two apart is why unitsPerMetre was
	// never allowed to absorb movementScale.
	StereoMode stereo = StereoMode::None;

	// How much further apart than the wearer's real eyes the two viewpoints
	// are placed. 1 is the headset's figure, geometrically true, and the
	// default; above 1 is hyperstereo - more parallax, more felt depth, and a
	// world that reads proportionally smaller.
	//
	// This is the one sanctioned bend of the rule above. The separation
	// itself stays a fact - HeadTracker reports what it measured, unscaled -
	// and this multiplier is applied downstream, in the camera hook, through
	// ScaledEyeHalfSeparation, which also clamps what taste may ask for.
	float eyeSeparationScale = 1.0f;

	// Oblivion's own horizontal field of view, in degrees. fDefaultFOV out of
	// Oblivion.ini, which is 75 unless it has been changed.
	//
	// Read from OBVR's own file rather than the game's, because the console
	// command "fov" overrides the setting at runtime and OBVR has no way to
	// see that it did. A figure that disagrees with the game shows up as a
	// world that is the wrong size, so the log prints what was used.
	//
	// It is needed because the game's picture has to be laid into the eye's
	// view at the angle it actually covers. Without it the world is magnified
	// by whatever ratio two unrelated frustums happen to have - measured at
	// 1.61 times across and 2.31 times down on the headset this was built
	// against, which is both far too large and visibly stretched.
	float gameFovDegrees = 75.0f;

	// How to read that number when the frame is not 4:3. See PlacePicture: the
	// two readings differ by a third, evenly in both axes, so the wrong one
	// makes the world too large or too small with no stretching to show it.
	bool gameFovIsFor4x3 = false;

	// Whether the picture is handed to the compositor after Oblivion has drawn
	// it rather than before. Off is every version up to now: one whole frame of
	// latency, which in a headset is felt rather than seen.
	bool submitAtFrameEnd = false;

	// Whether menus and loading screens reach the headset. They have no camera,
	// so they arrive flat - but the alternative is a headset showing nothing
	// while the main menu is up.
	bool showMenus = true;

	// Where an in-game menu is shown: on the cinema screen, or in the world.
	//
	// False is the cinema screen, which is what OBVR has always done and
	// stays the default. The whole back buffer becomes one flat picture on a
	// held pose, menu and world together, and a flat picture is legible in a
	// way a quad at a fixed distance is not. Reading an inventory is what
	// menus are for, so that is not a small thing.
	//
	// True delivers the world in stereo and lets the 2D layer reach the
	// headset through the HUD's own overlay instead, so the world stays where
	// it is and the menu hangs in front of it. It costs a second world render
	// per menu frame, which the cinema screen does not pay.
	//
	// Only in-game menus are affected. Videos, loading screens and the main
	// menu take the cinema screen either way: no world render happens behind
	// them, so there is nothing for a menu to hang in front of, and a quad in
	// an empty room is worse than a screen. See DeliverFrame, which is where
	// that distinction is actually made - and it is made on whether a camera
	// pass ran, not on a list of menu names.
	bool menusInWorld = false;

	// Draws the world again, in stereo, on the frames a pause menu is up -
	// so the background is a live 3D scene rather than the last stereo pair
	// held and reprojected.
	//
	// What it fixes: the held pair survives a turn of the head, because the
	// compositor reprojects it, but it has no parallax to give. Lean, and the
	// world does not shift with you; that mismatch is the thing a headset
	// notices first. Two fresh renders from where the head now is restore it.
	//
	// The world itself stays paused. The renders advance no clock - the frame
	// delta is zeroed across both, exactly as it is for the second pass of a
	// dual frame - and Oblivion's update step is not running on these frames
	// at all. Only the picture moves.
	//
	// The one place this can be done from is the 2D pass: a world render OBVR
	// starts itself draws the full scene from there and nothing whatsoever
	// from Present, which is where every earlier attempt was made. Measured,
	// 461 draws against 0 in the same open Esc menu.
	//
	// Off by default: it costs two world renders on every menu frame, and the
	// held pair it replaces costs none.
	bool liveMenuBackground = false;

	// Overrides Oblivion's own field of view, in degrees. 0 leaves it alone.
	//
	// Written into the camera's frustum rather than into fDefaultFOV, because
	// that setting also resizes the menu layer and leaves mouse clicks landing
	// somewhere other than where the buttons appear.
	float gameFovOverride = 0.0f;

	// Whether the game is made to render the headset's own field of view rather
	// than an angle. Outranks gameFovOverride.
	//
	// This is the answer to "why 16:9 at all" - nothing forces it once OBVR
	// writes the frustum, and the eye is nearly square. What it does not fix by
	// itself is the frame's shape: 2560x1440 pixels spread over a square view
	// are stretched vertically, so iSize W and H in Oblivion.ini want to be
	// closer to equal for the resolution to follow the geometry.
	bool matchHeadsetFov = false;

	// Oblivion's own render resolution, applied where it is decided: the
	// parameters of CreateDevice, changed in place by the resolution hook.
	// 0 means the size the headset asks for. Takes effect at the next game
	// start - the device exists long before any hot reload runs.
	//
	// It is here because a headset wants a shape a monitor never does. OBVR
	// can write the frustum every frame, but not the frame's shape - that is
	// fixed when the device is made. A frame at the headset's own size is
	// also what makes the eye copies a real resolution rather than an
	// upscale, and a size typed above it is supersampling.
	UInt32 renderWidth = 0;
	UInt32 renderHeight = 0;
	bool setRenderSize = false;

	// Whether the working copy of the screen size - the pair the whole 2D
	// reads, found by disassembly - is raised to the frame's real size at
	// CreateDevice time, so menus, films and the mouse live in one
	// coordinate space again on an eye-sized frame. Process-local: unlike
	// the iSize settings, the engine never writes this copy to disk. See
	// UiScreenSize.h for the trail and the checks.
	bool uiFollowsFrameSize = true;

	// How large a flat picture - menu, video, loading screen - is drawn, as a
	// fraction of the world's own placement. Below 1 puts it at arm's length
	// with black around it instead of pressed against the face.
	float menuScale = 0.7f;

	// The shape menus and videos are given, width over height. 1.7778 is 16:9 -
	// a screen in front of you rather than a view wrapped round your face. 0
	// keeps whatever shape the game is rendering.
	float menuAspect = 1.7778f;

	// Whether the 2D layer is redirected to a texture of OBVR's own on world
	// frames and hung in the room as an overlay, instead of being drawn into
	// the frame the eyes are copied from.
	//
	// This is what puts the HUD back once the world is drawn per eye: under
	// dual pass the eye pictures are captured before the 2D layer draws, so
	// without this the HUD exists only on the monitor. It applies on world
	// frames alone - menus, videos and loading screens keep the flat path,
	// which is known to work.
	//
	// Off by default until it has been seen in a headset.
	bool hudOverlay = false;

	// Whether the 2D pass is run between the two world renders rather than
	// waited for after them.
	//
	// This exists because of what the probe sweep measured. With the world
	// rendered once, the 2D pass draws its 21 primitives; with it rendered
	// twice, the same pass is entered, walks past every gate with all of
	// them open, and draws nothing and clears nothing. Cutting the second
	// render back off restores it within the same run, so the state the pass
	// depends on is per frame and recovers on its own - it is not a switch
	// that stays thrown.
	//
	// That leaves exactly one moment in a dual frame when the renderer is in
	// the state the pass needs: after the first render, before the second.
	// Which is where the overlay wants the layer anyway, because the eye
	// pictures are captured without it. So instead of asking why the second
	// render leaves the pass unable to draw - a question the disassembly has
	// not answered across the interface manager's gates, none of which
	// close - the pass is run where it demonstrably works.
	//
	// The pass the game itself makes later still happens and still draws
	// nothing; it is left alone rather than suppressed, and the redirect
	// declines it so it cannot clear the texture this one filled.
	bool hudBetweenPasses = false;

	// Whether the overlay hangs in the room instead of on the head.
	//
	// Off, it is head-relative: the compositor carries it with the head every
	// frame, so it sits in the same place in the view whatever the wearer
	// looks at. That is a cockpit HUD, and it is what the layer's first run
	// used.
	//
	// On, it is placed in the tracking space and stays there, so turning the
	// head looks past it and turning back finds it again. The anchor is
	// taken the first time the layer appears and again on the recenter key,
	// levelled to heading only - the same treatment, for the same reasons,
	// that the flat menu picture gets.
	//
	// Turning with the stick does not move it, and that is correct rather
	// than incidental: the stick turns the character in the world while the
	// wearer's head has not moved in the room, so a room-anchored quad stays
	// exactly where they are still looking. What moves it is moving, and the
	// recenter key is how it is brought back.
	bool hudAnchorWorld = false;

	// The dressing the held world pair wears while a pause menu is up - the
	// frames where Oblivion stops redrawing the world, which is also what
	// keeps all of this out of dialogue, where the world renders on. See
	// MenuShade.h for what each piece is.
	//
	// menuShade is vanilla's own static-menu-background treatment: the
	// paused world desaturated and re-toned sepia (what bStaticMenuBackground
	// gives on the monitor), not a colour laid over it - a flat brown tint
	// was tried first and looked nothing like the game. The colour is the
	// tone the grey picture is multiplied with, the strength how far the
	// result replaces the original; both tunable because the exact vanilla
	// tone lives in a shader, and matching it is done by eye. menuSingleBorder
	// trims each eye's picture to the window both eyes show, so the frozen
	// pair's edges fuse into one frame instead of doubling at the sides.
	bool menuShade = true;
	UInt32 menuShadeColorRgb = 0xFFE3B2;
	float menuShadeStrength = 1.0f;
	bool menuSingleBorder = true;

	// Where the overlay hangs: straight ahead, this far away, in metres.
	float hudDistanceMetres = 1.2f;

	// How wide the overlay quad is, in metres, at that distance. Height
	// follows from the texture's shape.
	float hudWidthMetres = 1.6f;

	// Whether OBVR stands in for the camera pass on menu frames the engine is
	// drawing the world on anyway - the persuasion minigame is the measured
	// case, where the world is redrawn every frame and was being discarded for
	// a held still, freezing the NPC's face.
	//
	// On by default, because off means a known bug is back. The key exists for
	// a different reason than taste: this path opens a compositor frame and
	// arms a second render pass from inside the scene render, on frames the
	// camera hook never saw, and that is the newest and least travelled code
	// in the plugin. When a session ends in a hang or a crash, being able to
	// take one suspect out of the picture without a rebuild is what turns a
	// guess into a bisection - and a player who hits it can keep playing.
	bool menuStandIn = true;

	// The crosshair, given a quad of its own at the depth it is aiming at.
	//
	// What it fixes is a doubled crosshair, and the cause is not a drawing
	// error: the crosshair rides in the flat 2D overlay above, which hangs at
	// one fixed distance, and anything outside the plane the eyes are
	// converged on is seen twice. A crosshair at 1.2 m over a target ten
	// metres away is doubled by the same optics that let anyone see two
	// fingers when they focus past their own hand.
	//
	// Off by default, and that is not caution but arithmetic: Oblivion draws
	// its own crosshair unless bCrossHair under [GamePlay] is 0, and switching
	// this on without switching that off puts two crosshairs on screen. Turn
	// both, or neither.
	bool crosshair = false;

	// Whether the crosshair is placed at the depth of whatever is under it.
	//
	// The fixed distance below cannot be right everywhere, and not because it
	// was chosen badly. The vergence error between a crosshair at c and a
	// target at d goes as IPD * (1/d - 1/c), which is bounded as d grows and
	// divergent as d shrinks: at 3 m the error never exceeds about a degree
	// however distant the target, and passes two degrees before the target is
	// a metre away. So a fixed quad is always fine in the distance and always
	// fails close up, and raising the distance to help the far case makes the
	// near one worse. 10 m was tried and read worse than 3 m for exactly that
	// reason.
	//
	// What this switch turns on covers the near half: Oblivion records the
	// reference under the crosshair, within iActivatePickLength - 150 units by
	// default, 2.14 m - and the quad follows it. Beyond that range, and for
	// everything that cannot be activated, the fixed distance below is used,
	// which is what it is now for.
	bool crosshairDynamic = true;

	// Where the crosshair goes when there is nothing to measure against: no
	// reference under it, out of pick range, or the dynamic depth switched off.
	//
	// 3.0 rather than the old 10.0. Reported from the headset as the better of
	// the two, which the arithmetic above agrees with - a lower fixed value
	// costs little in the distance and helps everywhere nearer.
	float crosshairDistanceMetres = 3.0f;

	// How fast the crosshair's depth eases towards where it should be, as a
	// share of the remaining distance per second.
	//
	// Not optional decoration. The depth steps whenever the gaze crosses an
	// edge - onto a person, off them and back to the fallback - and a quad that
	// jumps between depths reads as breathing, which is more distracting than
	// a quad at a constant wrong depth. Too slow is its own fault: the
	// crosshair then lags behind the look and is at the right depth only for
	// things stared at.
	float crosshairDepthSpeed = 8.0f;

	// Logs what the crosshair depth is being built from: whether HUDInfoMenu
	// identified itself, what the tile menu array says about the same menu,
	// whether a reference is under the crosshair, and the depth that came out.
	//
	// Off by default and worth switching on once. The number to look for is how
	// often there is NO reference during ordinary play: that is the share of
	// the time the crosshair is back on its fixed distance, and it decides
	// whether this feature is enough on its own or wants the depth buffer after
	// all. It is not a thing the headset can show you, because the fallback
	// looks exactly like the feature working.
	bool crosshairProbe = false;

	// How wide the crosshair would be at one metre. The width actually used is
	// this times the distance, which is what holds the apparent size steady
	// however far away the quad is placed - so this number is an angle wearing
	// the units of the rest of the section. 0.025 is about 1.4 degrees.
	float crosshairSizeAtOneMetre = 0.025f;

	// Whether the crosshair stays out of the way until it is of use: in first
	// person it then appears only while something activatable is under it - the
	// moment Oblivion would put a context icon and a name on screen - or while
	// a weapon or spell is readied.
	//
	// Third person has its own switch below, because the two views start from
	// opposite places - see crosshairInThirdPerson.
	bool crosshairOnlyWhenNeeded = false;

	// Whether OBVR draws a crosshair in third person, where Oblivion draws
	// none.
	//
	// It really is none, and it is vanilla rather than anything OBVR does.
	// Bethesda's own support page: "The crosshair is only visible in Oblivion
	// in First Person mode. It does not appear in 3rd Person mode." OBVR lifts
	// the game's crosshair out of the 2D layer rather than drawing one, so with
	// nothing drawn there is nothing to lift and no setting could switch it on.
	//
	// This is therefore the one place a drawn cross is allowed, and the reason
	// the project's rule against one does not apply: that rule exists so a
	// hand-drawn near-miss is never mistaken for the game's own, and in third
	// person there is no game crosshair to mistake it for. It is deliberately
	// not a copy - four strokes with a gap in the middle.
	bool crosshairInThirdPerson = false;

	// The "only when it is of use" restriction, for third person.
	//
	// Its own switch rather than sharing the first person one: there the
	// setting takes away a crosshair the game always draws, here it governs one
	// OBVR puts up in a view that had none, and wanting them set differently is
	// entirely reasonable.
	bool crosshairOnlyWhenNeededThirdPerson = false;

	// How big a square is lifted out of the 2D layer, as a percentage of the
	// height the game believes it drew in.
	//
	// A share rather than a count of pixels, because Oblivion's interface
	// scales with the frame. The old setting was an absolute 96 pixels, picked
	// while looking at an ordinary picture; on a 5696x3164 layout that is three
	// per cent of the height where it had been over ten, so the square stopped
	// covering the crosshair the game had grown to match. The leftovers showed
	// as fragments in the middle of the flat layer, worst while sneaking, where
	// the icon is largest.
	//
	// 6.0 is roughly twice the old square at the resolution it was reported
	// from. It is an estimate rather than a measurement - the icon's real size
	// is not written down anywhere OBVR can read - so it is in the settings
	// menu, where it can be turned up until nothing is left behind.
	float crosshairSourceShare = 6.0f;

	// Oblivion units per metre, for converting the head offset.
	//
	// The Construction Set wiki gives "21.3 units to a foot ... 64 units per
	// yard [~70 units per metre]", the Creation Kit wiki the exact engine
	// figure hk * 69.99125 = wu with 1 unit = 1.428 cm. Both agree.
	//
	// It is configurable regardless, because the number that matters is not
	// the documented one but the one that makes a real head feel right in a
	// real headset - and that can only be found by trying it.
	float unitsPerMetre = 69.99125f;

	// How much further than life the head is allowed to move. 1 is one to one,
	// 2 moves the camera twice as far as the head really went.
	//
	// A separate number rather than an inflated unitsPerMetre, and the reason
	// is not tidiness: unitsPerMetre is the engine's documented figure and
	// stereo rendering will need it for the eye separation. Overloading it
	// with taste now would silently double the eye separation later, and the
	// world would look like a model railway with no obvious cause.
	//
	// Above 1 the world moves further than the head that moved it, which is
	// the very mismatch VR comfort rests on avoiding. It is a knob because
	// OBVR still renders one image rather than two: without stereo, parallax
	// is the only depth cue there is, and it reads weaker than it will once
	// there are two eyes.
	//
	// 3 is where the headset is now. The path was 1, 2, 1.7, 2, 3 across four
	// sessions, and the path is the point: a number that keeps climbing is
	// chasing something the scale cannot supply. What is actually missing is
	// the second eye - so this should be tried at 1 again once stereo renders,
	// and the expectation is that it will want to come back down a long way.
	float movementScale = 3.0f;

	// The conversion actually applied to head movement, taste included.
	float EffectiveUnitsPerMetre() const { return unitsPerMetre * movementScale; }

	// How far the camera may be displaced from where the game put it, in
	// Oblivion units. 0 removes the limit.
	//
	// It has to track movementScale, or it quietly becomes the thing deciding
	// how far a lean reaches. 120 units at a scale of 3 is 57 cm of real
	// leaning - the same physical headroom 80 gave at a scale of 2, which is
	// the figure worth holding constant. The limit exists for the tracking
	// glitch and the person standing up and walking off, not for the lean.
	float maxOffsetUnits = 120.0f;

};

class HeadTracker {
public:
	void Configure(const TrackerSettings& settings);

	// Once per frame. For the simulated head, frameIndex takes the place of
	// time - OBVR needs no real clock for that, and without one the behaviour
	// stays reproducible.
	//
	// The head is deliberately never smoothed. Easing a tracked head would
	// show the user where their head was rather than where it is, and that
	// latency is felt directly in a headset. What OBVR does ease is the
	// camera motion it generates itself, in camera/LookControl.
	void Update(UInt32 frameIndex);

	// Takes the current head pose as the new zero. Everything after that is
	// reported relative to it.
	void Recenter();

	// Rotation relative to the zero pose, ready in Oblivion's camera space.
	const NiMatrix33& GetCameraRotation() const { return m_cameraRotation; }

	// Head displacement relative to the zero pose, in Oblivion units and in
	// the camera's own space. The caller has to rotate it into the space the
	// camera's position lives in, exactly as it multiplies the rotation on.
	//
	// Zero whenever the source delivers no position or positional tracking is
	// switched off, which leaves the vanilla camera position untouched.
	//
	// One to one with the head, never eased. See Update.
	const NiPoint3& GetCameraOffset() const { return m_cameraOffset; }

	// How far the head asked to move, in Oblivion units, before maxOffsetUnits
	// had its say. Only for the log, and it earns its place there: it is the
	// one way to tell a lean that is genuinely small from one the limit cut
	// short, which otherwise look identical from inside the headset.
	float GetRawOffsetUnits() const { return m_rawOffsetUnits; }

	// Half the distance between the eyes, in Oblivion units. The caller adds
	// it along the camera's right axis for one eye and subtracts it for the
	// other.
	//
	// Converted with unitsPerMetre rather than EffectiveUnitsPerMetre, and
	// that is the point of having kept the two apart: how far leaning moves
	// the camera is a preference, how far apart two eyes are is not. Scaling
	// this would not make leaning stronger, it would make the world look
	// smaller.
	//
	// Zero until a headset has reported its eye transforms, which makes the
	// caller's fallback the flat picture rather than a guessed separation.
	float GetHalfEyeSeparationUnits() const { return m_halfEyeSeparation; }

	// The orientation last read, still in OpenXR convention. Mostly for
	// diagnostics in the log.
	const Quaternion& GetRawOrientation() const { return m_rawOrientation; }

	// Whether a real headset is actually delivering poses. The look controls
	// are only taken away from the player when this is true - without a
	// headset there is nothing to hand them to.
	bool IsHeadsetConnected() const {
		return m_settings.source == TrackerSource::OpenVR && m_openVR.IsRunning();
	}

	// Whether OBVR actually holds the compositor and could submit a frame.
	// False whenever rendering is switched off, could not be set up, or the
	// source is not a real headset - so a caller that checks this never has
	// to ask why it failed.
	bool IsRenderingToHeadset() const {
		return m_settings.source == TrackerSource::OpenVR && m_openVR.IsSceneApplication();
	}

	// The connection to SteamVR, for whatever else needs it.
	//
	// Handed out rather than wrapped in pass-through methods. There has to be
	// exactly one registration with SteamVR and this class owns it, so the
	// alternatives are to expose it or to grow a second set of methods here
	// for every part of OpenVR that has nothing to do with head tracking.
	// Exposing it is the smaller lie: it says plainly that ownership sits
	// here for historical reasons rather than good ones, and the day a second
	// caller needs to change the connection rather than read it, that is the
	// day this becomes a session object of its own.
	const OpenVRBackend& GetBackend() const { return m_openVR; }

	// The mutable one, for the frame loop. WaitGetPoses keeps the pose it is
	// given, so the call that makes it cannot be const - and that is the right
	// shape: waiting on the compositor changes what the tracker will report.
	OpenVRBackend& GetBackendForFrame() { return m_openVR; }

private:
	// Reads orientation and position from the configured source. Returns
	// false when the source delivers no position, which is every source
	// except a connected headset.
	bool ReadSource(UInt32 frameIndex, Quaternion& orientation, NiPoint3& position) const;

	TrackerSettings m_settings;
	Quaternion m_rawOrientation = Quaternion::Identity();
	Quaternion m_reference = Quaternion::Identity();
	NiMatrix33 m_cameraRotation = NiMatrix33::Identity();

	// Positions in metres, in the tracking universe.
	//
	// m_hasReferencePosition is what keeps the camera from being flung
	// upwards on the first frame. The reference orientation can start as the
	// identity because that simply means "no extra rotation", but there is no
	// such harmless value for a position: the seated origin sits on the floor,
	// so an uncalibrated reference would read the head as roughly 1.2 metres
	// above zero and displace the camera by some 85 units for good. The first
	// valid pose therefore becomes the reference on its own, without waiting
	// for the recenter key.
	NiPoint3 m_rawPosition{0.0f, 0.0f, 0.0f};
	NiPoint3 m_referencePosition{0.0f, 0.0f, 0.0f};
	bool m_hasReferencePosition = false;

	NiPoint3 m_cameraOffset{0.0f, 0.0f, 0.0f};
	float m_rawOffsetUnits = 0.0f;

	// Read once from the headset, not per frame. A person's eyes do not move
	// apart during a session, and the two vtable calls it costs would be two
	// per frame for an answer that cannot change.
	float m_halfEyeSeparation = 0.0f;
	bool m_eyeSeparationRead = false;

	// Only used for TrackerSource::OpenVR, but it belongs here regardless:
	// the connection to SteamVR has to persist across frames rather than be
	// rebuilt for every query.
	OpenVRBackend m_openVR;
};

}  // namespace obvr::vr

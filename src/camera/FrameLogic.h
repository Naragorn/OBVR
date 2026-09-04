#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera {

// The per-frame bookkeeping of the camera hook, kept apart from the hook
// itself.
//
// The callback in CameraHook.cpp cannot be tested: it needs a live CameraNode
// and it reads the player through a hard-coded address. The decisions it makes
// on the way, however, are plain state machines over plain values - has the key
// just gone down, has the point of view changed, is a periodic action due this
// frame. Those live here so they can be exercised without the game running.

// Rising-edge detection for a polled key.
//
// GetAsyncKeyState carries a "pressed since the last call" bit of its own, but
// that bit is consumed process wide by whichever caller reads it first, so it
// cannot be relied on next to Oblivion's own input handling. The previous state
// is therefore kept here instead.
class KeyEdge {
public:
	// Feeds this frame's key state in and reports whether the key went down on
	// exactly this frame. Holding the key reports true once, not every frame.
	bool Update(bool isDown);

	// Forgets the previous state. Used when the key is switched off in the
	// configuration, so that a key still held at that moment does not fire an
	// edge once it is switched back on.
	void Reset();

	bool IsDown() const { return m_wasDown; }

private:
	bool m_wasDown = false;
};

// What the point of view did this frame.
enum class PovEvent {
	Unchanged,
	FirstPass,  // the very first hook pass, where there is nothing to compare to
	Switched,
};

// State of the last hook pass, for logging and later use.
struct State {
	bool sawCameraNode = false;
	bool isThirdPerson = false;
	UInt32 frameCount = 0;

	// Oblivion's own view frustum, as tangents of the half-angles, read off its
	// NiCamera at the start of the pass. Zero until one has been read.
	//
	// Kept here rather than fetched where it is used, because it differs across
	// a frame - 1.0231 across when the camera is computed, 1.1188 by the time
	// Present runs - and the placement wants the one that matches fDefaultFOV.
	float cameraTanHalfWidth = 0.0f;
	float cameraTanHalfHeight = 0.0f;

	// Records this frame's point of view and reports what changed. The caller
	// logs; keeping the log out of here is what makes it testable.
	PovEvent ObservePointOfView(bool isThirdPerson);
};

// Turns successive readings of a high resolution counter into a frame time in
// seconds.
//
// It takes the counter reading rather than reading the clock itself, which is
// what makes it testable: a test hands it whatever sequence of ticks it wants
// to describe, including the awkward ones.
//
// The upper limit is the point of it. A loading screen, an alt-tab or a
// breakpoint leaves a gap of seconds between two frames. Fed into an
// exponential approach, such a delta covers the entire remaining distance at
// once and the smoothing it was meant to provide disappears exactly where it
// is most visible - the first frame back.
class FrameClock {
public:
	// Longest frame time this will report, in seconds. Five frames at 20 fps;
	// anything longer was not a frame but a pause.
	static constexpr float kMaxDeltaSeconds = 0.25f;

	// Returns 0 on the first call, since there is no previous reading to
	// measure against, and 0 means "no time information" to the caller.
	float Tick(long long nowTicks, long long ticksPerSecond);

	void Reset();

private:
	long long m_lastTicks = 0;
	bool m_hasLast = false;
};

// Whether a periodic action is due on this frame.
//
// An interval of 0 means the action is switched off, which is why this is not
// simply a modulo at the call site: that would divide by zero. frameCount is
// counted from 1 upwards by the callback, so the first due frame of an
// interval of n is frame n rather than frame 0.
bool IsDue(UInt32 frameCount, UInt32 interval);

// Which eye this frame belongs to, when the two are being rendered on
// alternate frames.
//
// Alternate eye rendering gives depth without drawing the world twice: the
// camera is offset to one eye, the frame is drawn once, and it goes to that
// eye alone. The next frame does the other. It is what Luke Ross's mods do -
// "shifting the in-game camera into the eye positions on alternate frames" -
// and the reason it is worth trying before dual-pass is that it asks nothing
// of the engine at all.
//
// The price is that each eye sees a picture drawn one frame apart from the
// other, so anything moving fast has a disparity that is time rather than
// distance. It is a real artefact and not a small one; it is also cheap
// enough to find out about by looking.
//
// Here rather than in the camera hook because two different places have to
// agree on the answer - the hook offsets the camera, the renderer submits to
// one eye - and two implementations of "every other frame" would agree until
// one of them was edited.
bool IsLeftEyeFrame(UInt32 frameCount);

// Which eye the first of the dual pass's two world renders draws. False swaps
// the order, so the right eye is drawn first and the left second.
//
// A diagnostic for a fault that sits in one eye. Bodies collapse in the left
// eye and not the right, and there are two ways for that to be true which no
// measurement taken from inside one ordering can separate: either the first
// render is the one that goes wrong - because of what the frame leaves in
// front of it, or simply because it is first - or the left eye is the one that
// goes wrong, which would have to be the camera position. Swapping the order
// tells them apart. A fault that follows the order belongs to the render; a
// fault that stays in the left eye belongs to the eye.
//
// Here rather than at the three places that need it, because three is exactly
// how many chances there are to disagree: the sign of the camera step that
// starts the frame, the direction of the step between the passes, and which
// eye each capture fills. Two of those agreeing and one not is a stereo pair
// with both pictures taken from the same side of the head.
bool FirstPassDrawsLeftEye(bool swapEyeOrder);

// Which eye the picture sitting in the back buffer belongs to.
//
// Two inputs because the answer depends on when the question is asked, and
// this is the one decision in the whole alternate-eye path that has already
// been got wrong once.
//
// isLeftEye is the eye the camera was moved to for this frame.
// backBufferIsThisFrame is whether the game has drawn it yet.
//
// From the camera hook the answer is no: that runs while the camera is being
// computed, so the back buffer still holds the previous frame, drawn from the
// previous camera position - which under alternate eyes is the other eye. From
// a hook at the end of the frame the answer is yes, and the eye is simply the
// one the camera was moved to.
//
// Getting this backwards does not lose a depth cue, it inverts one: each eye
// is shown the other eye's viewpoint, which the eyes cannot fuse. It was
// reported from a headset as a picture that would not hold still and got worse
// when the head moved sideways.
bool BackBufferEyeIsLeft(bool isLeftEye, bool backBufferIsThisFrame);

// Half the eye separation the camera is actually offset by, given the half
// separation the headset reported and the multiplier from the INI.
//
// At 1 the two viewpoints sit exactly where the wearer's eyes are, which is
// geometrically true and shows the world at its real scale. Above 1 the
// viewpoints move further apart than any head is wide - hyperstereo. The
// parallax of everything grows, near things gain depth and presence, and the
// world as a whole reads as proportionally smaller; the same trade the
// "3D depth boost" mods for OpenXR make, except applied here to the camera
// before rendering rather than warped onto finished pictures after.
//
// The scale is a preference where the separation itself is not - the reason
// HeadTracker refuses to scale what it measured, and the reason this function
// exists apart from it: the tracker keeps reporting the fact, and the taste is
// applied at the one place the camera steps to an eye, where it can be seen
// next to the stereo mode it belongs to.
//
// A scale that is NaN, zero or negative is not a choice anyone can have meant
// - zero would stack both eyes in one place and a negative one would cross
// them - so all of those fall back to the truth of 1. That test also catches
// what ReadFloat makes of a word in the INI, which is 0. Positive values are
// clamped to the range below: past 4 the eyes cannot fuse the images and the
// result is strain rather than depth, and below a quarter the depth is as
// good as gone while the number looks deliberately set.
constexpr float kMinEyeSeparationScale = 0.25f;
constexpr float kMaxEyeSeparationScale = 4.0f;

float ScaledEyeHalfSeparation(float halfUnits, float scale);

// How a frame reaches the headset.
enum class FrameDelivery {
	// The world, drawn this frame, as two eyes.
	Stereo,

	// One flat picture of the back buffer to both eyes, hanging on a held
	// pose. The cinema screen: no viewpoint is claimed, because there is no
	// world render to have claimed one from.
	Cinema,

	// Last frame's two eyes again, unchanged. Nothing new was drawn, and the
	// compositor reprojects what it already has rather than being handed a
	// different kind of picture. See DeliverFrame.
	HeldStereo,
};

// Which of the three this frame is.
//
// No menu is the simple case: a camera pass means the world was drawn and the
// frame is stereo; no camera pass means nothing drew a world at all, and the
// cinema screen is the only honest answer. Videos, loading screens and the
// main menu are that second case, and they stay on the screen whatever
// menusInWorld says - there is no world behind them for a menu to hang in
// front of, so "in the world" would be a quad floating in nothing.
//
// A menu over a rendered world is the case with a choice in it.
//
// menusInWorld false is what OBVR has always done: the frame goes to the
// cinema screen, the whole back buffer with the menu already drawn into it.
// This is deliberately kept - a flat picture is legible in a way a quad at a
// fixed distance is not, and reading an inventory is the one thing menus are
// for.
//
// menusInWorld true delivers the world in stereo and lets the 2D layer reach
// the headset as its own overlay instead, so the world stays where it is and
// the menu hangs in front of it.
//
// HeldStereo is what makes that second option possible at all, and it is the
// old flicker bug in disguise. Oblivion keeps drawing the world behind an open
// menu, but not on every frame. So the camera hook runs on some of those
// frames and not others, and switching between stereo and the cinema screen at
// frame rate is exactly the menu snapping open and shut that ShowMenus was
// gated on IsMenuMode to cure. Holding the last pair instead changes nothing
// on those frames: the world is paused, so the picture is not stale, and the
// compositor reprojects it for the head movement that did happen.
//
// haveHeldEyes is what keeps that honest. There is nothing to hold before the
// first world render has been captured - the main menu is exactly that case,
// and so is any menu opened while the dual pass is off. Asking here rather
// than letting the submit discover it means the frame falls back to the cinema
// screen, which is a picture, instead of to the test pattern, which is not.
//
// worldlessStreak bridges the stray frames where the world simply is not
// drawn: no camera pass, no menu. The first was the seam that closes a menu -
// the menu already gone, the world not back yet. That is the cinema screen by
// every other rule, and for one frame the wearer got the flat picture with
// its black bars, seen as a picture flashing up in the middle on every close;
// worse, the cinema path fills the eye copies with that letterboxed layout,
// so the pair the next menu held was not the world at all. The second was the
// end of a dialogue: the exit transition holds a frame or two without a world
// render, mid-gameplay, no menu anywhere near - reported as a grey flash a
// split second long. One rule covers both, and any stray frame like them: as
// long as a held pair exists and fewer than kWorldlessBridgeFrames such
// frames have come in a row (the streak counts the ones before this frame),
// the pair is held again and the compositor reprojects it.
//
// The limit is what keeps a real flat presentation honest. A video or a
// loading screen is this same shape sustained, and an unlimited bridge would
// hold the old world in front of it for ever - the streak reaching the limit
// is how "a stray frame" becomes "a flat presentation", at the price of the
// first few frames of every video showing the world it interrupted, which is
// also roughly what the monitor shows.
FrameDelivery DeliverFrame(bool hadCameraPass, bool menuIsUp, bool menusInWorld,
                           bool haveHeldEyes, UInt32 worldlessStreak);

// How many worldless frames in a row are bridged with the held pair before
// the delivery concedes that a flat presentation has begun. Two to three
// frames covers every stray gap seen so far; at ordinary frame rates the
// world a video displaces lingers for under fifty milliseconds.
inline constexpr UInt32 kWorldlessBridgeFrames = 3;

// Whether a run of held menu frames gets the pause-menu dressing - the sepia
// shade and the single border - given how long ago the menu opened.
//
// The distinction this draws is pause menu versus dialogue, and it draws it
// from timing because both are IsMenuMode. Esc and Tab stop the world on the
// spot: their held frames begin within a frame or two of the menu opening.
// A dialogue keeps the world rendering for its whole length - its held
// frames, when they come, are the exit fade, minutes after the DialogMenu
// opened. Dressing those painted the fade sepia, which the headset reported
// as a half-second washed-grey picture at the end of every conversation; a
// fade should show the world as it is.
inline constexpr UInt32 kMenuDressingWindowFrames = 10;

constexpr bool MenuDressingWanted(UInt32 framesSinceMenuOpened) {
	return framesSinceMenuOpened <= kMenuDressingWindowFrames;
}

// Whether the live pause-menu world can be complete rather than merely
// expensive. One switch asks for the feature, but it needs the existing dual
// world path and the existing menu overlay path together: two fresh eye
// pictures are no use if the menu itself has nowhere to go.
//
// Keeping this decision here makes every refused combination testable and,
// more importantly, keeps LiveMenuBackground=0 as an exact return to the
// previous held-pair path.
bool LiveMenuBackgroundCanRun(bool enabled, bool renderToHeadset, bool showMenus,
                              bool headsetConnected, bool dualPass,
                              bool menusInWorld, bool hudOverlay,
                              bool hudBetweenPasses, bool cameraStandIn);

struct MenuFrameDressing {
	bool shade = false;
	bool singleBorder = false;
};

// Which paused-world pictures receive the menu dressing.
//
// A recently opened held pair gets both configured parts: the sepia shader
// and, optionally, the common black border that makes a frozen pair's two
// outer edges agree. Its age gate keeps a dialogue's much later exit hold
// untouched. A freshly rendered live pair gets the shader for every menu
// frame but never that border: every copy replaces the previous shade, and
// each eye is current and correct on its own, so trimming them to their
// overlap would put black strips back into genuine stereo.
MenuFrameDressing MenuDressingForFrame(FrameDelivery delivery, bool menuIsUp,
                                       bool liveStereoFrame,
                                       UInt32 framesSinceMenuOpened,
                                       bool shadeEnabled, bool singleBorderEnabled,
                                       bool dialogEpisode);

// Loading screens are flat presentations rather than paused-world eye pairs,
// but an in-game one can still show the last world behind its loading art.
// LiveMenuBackground has disabled Oblivion's own static sepia treatment by
// then, so the same INI-controlled replacement shade has to follow the frame
// onto the cinema screen. Other flat presentations (films and the main menu)
// remain untouched.
constexpr bool CinemaLoadingShadeWanted(FrameDelivery delivery, bool loadingFrame,
                                         bool shadeEnabled) {
	return delivery == FrameDelivery::Cinema && loadingFrame && shadeEnabled;
}

// Dialogue remains a dialogue episode even when ActiveMenuId briefly reports
// none or another child menu during the exit transition. It ends only when
// menu mode itself ends, preventing that late transition being reclassified
// as a freshly opened pause menu and painted sepia.
constexpr bool DialogMenuEpisode(bool previous, bool menuIsUp, bool observedDialog) {
	return menuIsUp && (previous || observedDialog);
}

// Whether this menu frame runs the menu-world probe: one self-initiated
// world render, its draw calls counted and logged, and nothing else done with
// the picture.
//
// The question it answers is the one keeping the world live behind pause
// menus turns on. Oblivion stops calling its render function entirely while
// a pause menu is up - measured, not assumed: the scene counter stands still
// across every held run - so a live background means OBVR calling that
// function itself, on frames the engine decided not to. Whether a render the
// engine did not ask for draws anything is not something to reason out from
// the outside: the 2D pass already set the precedent of a pass that runs to
// completion and draws nothing. One counted render answers it.
//
// The gates: the probe was asked for; the frame is not a stereo one, because
// a stereo frame's world was just drawn by the engine and there is nothing to
// ask - held and cinema menu frames both sit on a stopped engine render,
// which is the condition the question is about. Held frames are where a live
// background would run in the headset; the cinema case is what a headless
// run produces, where a sleeping headset never arms stereo and no pair is
// ever held. On a cinema frame the probe's render draws over the very back
// buffer being shown, so the menu vanishes from the monitor for the probe
// frames - visible, and a measurement artefact, not a fault. The menu must
// actually be up, because a held frame can also be a bridged stray with no
// menu anywhere near it; and the per-episode budget is not spent - the
// probe is a measurement, not a mechanism, and a measurement that repeats
// every frame of every menu is a log nobody can read.
bool MenuWorldProbeWanted(bool probeEnabled, FrameDelivery delivery, bool menuIsUp,
                          UInt32 attemptsLeft);

// Whether this frame runs the probe's control: the same self-initiated world
// render, from the same point in Present, on a frame where the engine drew
// the world itself and no menu is up.
//
// Without it the menu measurement cannot be read. A menu render that draws
// nothing looks like the menu's doing, but the probe changes two things at
// once - the menu, and the moment: it calls the engine's render from Present,
// after the frame's own EndScene, rather than from inside the render moment
// the engine chose. The dual pass shows a repeated render is not the problem
// by itself, since it calls the same function twice in a row with no update
// between and its second call is what stereo is made of. So the control asks
// the same question with only the menu removed, and the pair of answers
// separates the two suspects.
//
// The gates: the probe is on; no menu, which is what makes this the control;
// a camera pass happened, so the engine really did draw this frame; and the
// budget is not spent, because each one costs an entire wasted world render.
bool WorldControlProbeWanted(bool probeEnabled, bool menuIsUp, bool hadCameraPass,
                             UInt32 attemptsLeft);

// Whether this menu frame needs OBVR to stand in for the camera pass.
//
// Measured, and reshaped by the measurement. Some menus are drawn over a world
// Oblivion is still rendering: in the persuasion minigame the scene counter
// runs on through every frame the menu is open - 5869 to 5877 across nine of
// them - while the camera hook does not run at all, because it hangs off the
// camera update the paused simulation never reaches. So the world is redrawn
// and then thrown away: with no camera pass the frame is delivered as a held
// still, and the NPC's face stops moving. That face is what the minigame is
// played by watching, which is how the bug was reported.
//
// Standing in is only the arming, never a render. The engine is already
// drawing; all this adds is the pose it should have been drawn from and the
// flags the rest of the frame reads.
//
// It also costs nothing where it does not apply, and that is structural
// rather than careful: this is asked from inside the scene render, so by
// construction the engine is drawing the world as it is called. A menu that
// shows a snapshot instead never reaches here - the Esc menu's scene counter
// stands still, measured.
//
// The gates:
//
// stereoDual: the pair is captured the way the dual pass captures it, one
// eye per render from the back buffer. Alternate-eyes has no second capture
// to fill and would submit one fresh eye beside one stale one.
//
// menuIsUp: an ordinary world frame has a camera pass of its own.
//
// headsetConnected: without poses there is no head to draw from, and the
// whole point is drawing from where the head now is.
//
// haveCameraBase: the camera hook has run at least once, so there is a
// transform the game itself wrote to build the eyes from. Before the first
// world render there is none - the main menu is exactly that case.
//
// alreadyRanThisFrame: the 2D pass runs more than once per frame, and each
// entry would otherwise arm its own frame. One per frame.
//
// engineDrewThisFrame: a menu frame whose camera pass did run is already an
// ordinary stereo frame and wants nothing added. A dialogue is one, measured
// at camera pass=1 - which is why dialogues always looked right while the
// persuasion menu did not.
//
// enabled is MenuStandIn, not LiveMenuBackground. The live switch asks the
// engine to render behind menus that would otherwise show a snapshot; this
// stand-in supplies the missing VR camera whenever such a render occurs and
// is therefore one of the live feature's prerequisites. It remains a separate
// switch for operational bisection:
// it opens a compositor frame and arms a second render pass from inside the
// scene render, on frames the camera hook never saw. When a session ends
// badly, being able to take one suspect out of the picture without a rebuild
// is worth an INI key - and a player who hits it should be able to keep
// playing while it is looked at.
bool MenuFrameNeedsCameraStandIn(bool enabled, bool stereoDual, bool menuIsUp,
                                 bool headsetConnected, bool haveCameraBase,
                                 bool alreadyRanThisFrame, bool engineDrewThisFrame);

// Attempts per menu episode. A handful rather than one, because the first
// held frame after a menu opens may be special - the engine may still be
// mid-transition - and a probe that only ever measured that frame would
// mistake the transition for the answer.
inline constexpr UInt32 kMenuWorldProbeAttempts = 5;

// Where the camera goes for the first eye, and how far it moves to reach the
// second.
//
// Both numbers together, and from one place, because they were written out
// twice - once in the camera hook, once for menu frames - and the second copy
// had the shift's sign backwards. The camera stepped to the left eye and then
// moved further left, so both eyes were drawn from the same side and the
// second one three half-separations out. In the headset that is everything
// doubled, with the right eye's picture pushed right. It went unseen while
// the menu path was switched off, which is exactly how a duplicated formula
// fails: not when it is written, but months later when the copy is switched
// on.
//
// So neither path computes it any more, and the invariant that catches this
// class of mistake is a test rather than a reading: the second eye must land
// exactly opposite the first.
struct EyeStep {
	float toFirstEye;    // along the head-carried x axis, from the centre
	float toSecondEye;   // added to the above, reaching the other eye
};

// half is the scaled half-separation; firstIsLeft says which eye this frame's
// first render draws.
EyeStep StereoEyeStep(float half, bool firstIsLeft);

// Whether the crosshair quad is shown this frame.
//
// worldFrame is false when the world was not drawn, which is the case the held
// pair covers - an aiming point over a frozen picture would be aiming at
// nothing.
//
// menuIsUp is asked separately, and the first version of this got that wrong
// by assuming worldFrame covered it. It does not: worldFrame means the world
// was delivered in stereo, and with Menus=world a dialogue is exactly that -
// a menu, over a world the engine is still drawing. The crosshair stayed up
// through every conversation, which is what a player reported. Nothing is
// aimed while a menu is open, whatever the world behind it is doing.
struct CrosshairVisibility {
	// The three that have always decided this.
	bool enabled = false;
	bool worldFrame = false;
	bool menuIsUp = false;

	// Whether the crosshair should stay out of the way until it is of use.
	//
	// A crosshair is an aiming aid, and in a headset it is also a small bright
	// thing permanently in the middle of the view. Off unless it is doing its
	// job is a reasonable way to want it, and it is the setting this struct was
	// introduced for.
	bool onlyWhenNeeded = false;

	// The same restriction for third person, under its own switch.
	//
	// Two switches rather than one, because the two views start from opposite
	// places. In first person Oblivion always draws a crosshair, so the setting
	// takes one away. In third person it draws none at all - Bethesda's own
	// support page says so - so there the setting governs a crosshair OBVR
	// itself puts up, and somebody may well want it always in one view and only
	// when useful in the other.
	bool onlyWhenNeededThirdPerson = false;

	// Which of the two switches above applies this frame.
	bool thirdPerson = false;

	// Something activatable is under the crosshair, which is exactly when
	// Oblivion puts a context icon and a name on screen. The same reference the
	// depth is taken from, so this costs nothing extra to know.
	bool somethingAimedAt = false;

	// A weapon or spell is readied. Also true when the state could not be read
	// at all - see game::WeaponState, and the note there on why unknown leans
	// towards showing: a crosshair wrongly present is a much smaller fault than
	// one wrongly missing while somebody is trying to shoot.
	bool weaponDrawn = false;
};

// Whether the crosshair quad is shown this frame.
bool CrosshairWanted(const CrosshairVisibility& visibility);

// Whether the centre of the captured HUD must be removed from the flat layer.
//
// This deliberately does not share CrosshairWanted's "only when needed"
// decision.  Hiding the depth overlay without taking the game's original out
// of the HUD merely reveals that original at HudDistanceMetres, which leaves a
// permanent crosshair in first person.  A playable frame with the feature on
// must therefore always take the centre; CrosshairWanted decides separately
// whether the taken pixels are submitted or discarded.
bool CrosshairCaptureWanted(const CrosshairVisibility& visibility);

// What the depth overlay shows after the centre has been lifted. A tooltip is
// the game's live action icon. When it is moved above the target name, the
// overlay may still carry the remembered plain reticle independently.
enum class CrosshairContent {
	Hidden,
	CapturedHudCentre,
	RememberedCrosshair,
};

CrosshairContent CrosshairContentWanted(bool crosshairWanted, bool haveTarget,
	                                    bool tooltipsEnabled, bool tooltipsAboveName);

// Tooltips remain useful with the plain crosshair disabled, so their capture
// gate is deliberately independent from CrosshairVisibility::enabled.
bool CrosshairCentreCaptureWanted(bool crosshairEnabled, bool haveTarget,
	                              bool tooltipsEnabled, bool worldFrame,
	                              bool menuIsUp);

struct PixelRectangle {
	SInt32 left = 0;
	SInt32 top = 0;
	SInt32 right = 0;
	SInt32 bottom = 0;
};

// Fixed HUD-relative home for the action icon when the user asks for it above
// the lower-right target name. Kept pure so clipping and tiny-frame fallbacks
// are covered without Direct3D.
PixelRectangle TooltipAboveNameRectangle(UInt32 width, UInt32 height,
	                                     UInt32 sizePixels);

// Whether the isolated HUD draw should briefly see first person while the
// world and player remain in third person. Oblivion suppresses its centre HUD
// content in third person; the lift needs that draw to obtain both the plain
// crosshair and the contextual target icon. The caller restores the POV byte
// immediately after this one HUD pass.
bool HudCrosshairNeedsFirstPersonView(bool crosshairEnabled,
                                      bool crosshairInThirdPerson,
	                                  bool tooltipsInThirdPerson,
                                      bool isThirdPerson);

// Whether third person should paste in the crosshair borrowed from first
// person, or leave what the lift brought alone.
//
// The first version of the borrowed crosshair pasted it unconditionally, on
// the belief that Oblivion draws nothing at all in the middle of the layer in
// third person. That belief was too broad, and the headset showed both ways it
// was wrong: the context icons vanished, and sneaking showed a crosshair where
// the eye belongs. The game draws no plain CROSSHAIR there - which is the gap
// this feature fills - but it does draw the icons, and it does draw the sneak
// eye.
//
// So the borrowed copy is a stand-in for the one thing that is missing, and it
// gets out of the way whenever the game is drawing something of its own. Both
// of those are known from elsewhere without any new reading: the reference
// under the crosshair is the same one the depth uses, and sneaking comes from
// the movement flags.
//
// Erring towards NOT pasting, on both counts. Something the game drew is
// always more right than the copy, and a missing plain crosshair is a smaller
// loss than a hand-over-icon that never appears.
bool BorrowedCrosshairWanted(bool thirdPerson, bool enabled, bool somethingAimedAt,
                             bool sneaking);

// Whether the HUD's target reference has a consumer this frame. Third-person
// borrowing belongs here as much as dynamic depth does: without the read, a
// context icon can be mistaken for the plain crosshair and then persisted.
bool CrosshairTargetReadWanted(bool dynamicDepth, bool onlyWhenNeeded,
                               bool probeEnabled, bool thirdPersonBorrowing);

// Whether a newly read target needs the crosshair depth immediately rather
// than after easing.  Zero means no target.  Acquiring a target and moving
// directly from one target to another both snap; holding the same target and
// losing it keep the calming ease.
bool CrosshairTargetNeedsImmediateDepth(UInt32 previousTarget, UInt32 currentTarget);

// Where the crosshair quad goes and how big it is there.
//
// Two numbers out rather than one, because they are not independent: the width
// is the size at one metre multiplied by the distance, which is what keeps the
// crosshair the same apparent size wherever it is placed. Get that wrong and
// moving the quad from two metres to ten shrinks it to a fifth on screen - the
// depth correct, the thing unusable.
struct CrosshairPlacement {
	float distanceMetres;
	float widthMetres;
};

// The limits are not taste. Below the near end a quad is closer than the eyes
// can converge on and doubles for a new reason; beyond the far end the depth
// stops changing anything, because the sight lines are already parallel. Both
// ends therefore make the value meaningless rather than merely extreme, which
// is what a clamp is for - and an INI is written by hand.
inline constexpr float kCrosshairNearestMetres = 0.3f;
inline constexpr float kCrosshairFarthestMetres = 100.0f;
inline constexpr float kCrosshairSmallestAtOneMetre = 0.002f;
inline constexpr float kCrosshairLargestAtOneMetre = 0.5f;

CrosshairPlacement PlaceCrosshair(float distanceMetres, float sizeAtOneMetre);

// How big a square to lift out of the 2D layer, in pixels of the size the game
// believes it drew in.
//
// A SHARE RATHER THAN A COUNT OF PIXELS, and that is a correction rather than a
// preference. The setting used to be an absolute 96 pixels, which was chosen
// while looking at an ordinary picture - and Oblivion's interface scales with
// the frame, so the same 96 covers less and less of the crosshair as the
// resolution rises. On a 5696x3164 layout it is three per cent of the height,
// where on a 1600x900 one it was over ten, and the crosshair the game draws has
// grown by the same factor the square has not. What was left over showed as
// fragments in the middle of the flat layer - reported from the headset while
// sneaking, where the icon is at its largest.
//
// So the square now follows the picture, and a value that is right stays right
// when the resolution changes.
//
// Measured against the HEIGHT, which on every aspect ratio Oblivion is run at
// is the smaller dimension - so the square stays inside the picture rather than
// growing past the top and bottom on a wide one.
inline constexpr float kCrosshairSourceSmallestShare = 1.0f;
inline constexpr float kCrosshairSourceLargestShare = 10.0f;

// Ten per cent is the upper end for a reason rather than for caution: the name
// of whatever is being looked at is drawn just below the crosshair, and a
// square much larger than this starts lifting that text along with it.
UInt32 CrosshairSourcePixels(UInt32 believedHeight, float sharePercent);

// How far away the thing under the crosshair is, in metres.
//
// WHY THIS IS NOT A DISTANCE. The obvious calculation - the length of
// targetPosition - cameraPosition - is wrong, and wrong in the near field,
// which is the only field this feature exists for. A reference's position is
// its ORIGIN, and an actor's origin is between its feet. Look someone in the
// face from a metre away and the straight line to their origin is close to two
// metres; place the crosshair there and the eyes are converged at one metre
// while the quad sits at two, which is most of the doubling back again.
//
// What is taken instead is the origin's depth ALONG THE VIEW AXIS: level at a
// face a metre away it answers 1.0, and looking down at that same actor's feet
// it answers the true 1.97. The vertical offset falls out of the projection on
// its own. A scene-bound sphere was tried as a surface estimate, but its broad
// near edge sat in front of doors and actors and changed discontinuously when
// the camera entered it: exactly the middle-distance doubling this avoids.
//
// EVERY WAY OUT IS THE FALLBACK. No target, a gaze vector too short to have a
// direction, nonsense units, or a target behind the camera - each answers
// fallbackMetres rather than a number derived from bad inputs. A crosshair at
// the wrong fixed depth is a known, tolerable fault; one placed on arithmetic
// over garbage is a new one.
struct CrosshairDepthInput {
	// Whether there is a target at all. False is the ordinary case, not an
	// error: most of what a player looks at cannot be activated, and Oblivion
	// only records a reference for things that can.
	bool haveTarget = false;

	// Both in Oblivion units, in the same space - which is what makes their
	// difference meaningful. The gaze is the camera's forward axis and need not
	// be of unit length; it is normalised here.
	NiPoint3 cameraPosition{};
	NiPoint3 gazeDirection{};
	NiPoint3 targetPosition{};

	float unitsPerMetre = 69.99125f;

	// Where the crosshair goes when there is nothing to measure against. Also
	// what a player sees for most of a session, so it is still worth setting
	// well rather than treating as an error value.
	float fallbackMetres = 3.0f;
};

float CrosshairDepth(const CrosshairDepthInput& input);

// Whether this frame runs a layout-probe measurement: one readback of the
// frame's 2D, its covered rectangle logged, and nothing else done with it.
//
// The question it answers: which screen size does each part of Oblivion's 2D
// lay out against, now that OBVR asks for a frame the game did not choose -
// the size the game believes from its INI, or the size the frame really is.
// The eye-sized frame's first run showed 2D symptoms that split exactly along
// that line, and the viewport cannot answer it, because the viewport is fully
// open either way.
//
// The gates: the probe was asked for, and the last measurement is at least
// the gap ago - a readback stalls the GPU, and one measurement every couple
// of seconds is evidence while one per frame is a slideshow. The counter
// comparison survives wrap-around by working on the difference.
bool LayoutProbeDue(bool probeEnabled, UInt32 presentedFrame, UInt32 lastProbeFrame);

// Frames between measurements: about two seconds of them, long enough to
// stay playable while the probe is on, short enough that a menu opened for a
// moment still gets measured.
inline constexpr UInt32 kLayoutProbeFrameGap = 120;

// Whether a menu can actually be delivered in the world, given the rest of the
// configuration.
//
// Menus=world without HudOverlay is a menu nobody can see. The eyes are
// captured before the 2D pass draws, so the only route a menu has into the
// headset is the overlay - take that away and the world is delivered in stereo
// with the menu neither in it nor in front of it. On the cinema screen the same
// combination is harmless, because there the menu is in the picture.
//
// So the two settings are asked together, at every point that acts on the
// choice, rather than being validated once at load: the INI is hot reloaded,
// and a rule enforced only at load is a rule that stops holding the moment
// somebody edits the file with the game running.
bool MenusCanReachTheWorld(bool menusInWorld, bool hudOverlay);

// Whether this frame's world render should run twice, once per eye.
//
// Three conditions, and all of them have to hold.
//
// frameOpen: the camera hook ran this frame and BeginFrame accepted it, so
// there is a pose the two passes were asked for and a compositor waiting for
// their pictures. Without it the second pass would be drawn for nobody.
//
// armed: the camera hook actually moved the camera to the left eye and worked
// out the shift to the right one. Not the same as frameOpen: the headset can
// be connected for tracking while stereo is off, and then there is no shift
// to apply and no second viewpoint to draw.
//
// menuIsUp and menusInWorld: a menu frame bound for the cinema screen ignores
// the captures whatever the passes do - DeliverFrame says so - and a second
// pass would be two renders for a picture that uses neither. A menu delivered
// in the world is a stereo frame like any other, and wants its second pass.
// The same question, asked of the same source, as the delivery decision: the
// two must agree on what kind of frame this is.
bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, bool menusInWorld);

// Debug.DualPassProbe = 9: walk the probe rungs on their own instead of
// asking for one run per rung.
//
// The dual pass is three things stacked - the second render itself, the
// camera move between the passes, and the two back buffer captures - and
// with it on, the 2D pass is entered every frame and draws nothing. Which
// of the three costs the HUD its draws is the whole question, and the probe
// answers it by cutting the stack down: 1 leaves only the second render, 2
// adds the camera move back, 0 is the whole mechanism. Rung 3 is the one
// the ladder was missing - it cuts the second render too, so nothing of the
// dual pass runs at all while everything around it stays as it is.
//
// The first sweep needed rung 3 and did not have it. It ran its bands from
// world render 260 onward, and the log then showed the HUD drawing its 22
// primitives up to the frame before the first dual pass and none after it.
// So the bands never saw a frame with the HUD alive, and cutting the
// captures and the camera move back off did not bring it back: whatever the
// dual pass does, it does once and it stays done.
//
// Hence these bands. Rung 3 first, from the first frame, so the sweep opens
// with the HUD drawing and the second render is the only thing that has not
// yet happened. Then rung 1 - the second render, alone. Then rung 3 again,
// which asks the question the first sweep could not: does the HUD come
// back, or is the switch one way? Then the whole mechanism.
//
// Any other configured value passes straight through, so the existing
// one-rung-per-run behaviour is untouched.
inline constexpr UInt32 kProbeSweep = 9;
inline constexpr UInt32 kProbeSinglePass = 3;
inline constexpr UInt32 kSweepFirstBand = 260;
inline constexpr UInt32 kSweepSecondBand = 320;
inline constexpr UInt32 kSweepThirdBand = 380;
UInt32 SweepProbeStage(UInt32 sceneCall, UInt32 configured);

// The same question as above, with the probe rung folded in: rung 3 refuses
// the second pass whatever the frame would otherwise have wanted. Kept as a
// separate overload so the plain decision stays what it was and the probe
// cannot quietly change it when it is not running.
bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, bool menusInWorld,
                          UInt32 probeRung);

// Whether this frame will hand the compositor two captured eyes.
//
// The dual submit reads pictures the two passes captured, so it may only be
// claimed on a frame that will actually run them. Two of the three reasons
// it might not are long-standing - stereo is not set to dual, or the scene
// render could not be hooked - and the third is the probe rung that cuts the
// second render.
//
// That third one is what makes rung 3 usable rather than merely quiet. With
// it folded in, a run on that rung falls back to the mono submit and the
// headset keeps a live picture instead of freezing on the last pair it
// captured, so the rung can be switched on and off while the game runs and
// what changes on screen is the second render alone. That is how a
// camera-dependent effect - water reflections resetting as the head moves -
// gets attributed to the dual pass or cleared of it, in one session, without
// leaving the spot being looked at.
bool DeliversDualEyes(bool stereoDual, bool sceneHooked, UInt32 probeRung);

// Whether this frame's 2D pass should be redirected to the HUD texture.
//
// Asked of the delivery rather than of the inputs, and deliberately so. The
// layer may leave the frame exactly when the frame itself is not what gets
// shown: the cinema screen shows the back buffer, so on those the layer has to
// stay in it, or the menu is taken out of the very picture that exists to show
// it. Stereo and held both show the overlay, so on those it should leave.
//
// This was its own decision over frameOpen, menuIsUp and menusInWorld once, and
// the two drifted apart the moment the redirect learned that menu frames never
// carry a camera pass. The main menu has no captured pair to hold, so it falls
// back to the cinema screen - while the redirect went on taking its layer away.
// The menu was then in neither place and the game looked hung at the title
// screen. Deriving it removes the second rule that could disagree.
bool WantsHudRedirect(FrameDelivery delivery);

// Whether the player's own pitch is made to follow the gaze on this frame.
//
// This is the other half of "the arrow does not go where I am looking", and
// the half nothing had been done about. OBVR takes the vertical look away
// from the mouse and hands the view to the head, but it does that by
// replacing the camera matrix after the engine computed it. The player's own
// rotation is never touched - and a projectile is a TESObjectREFR that leaves
// along that rotation, not along the camera. Measured rather than argued:
// with the head sweeping from a sine of -0.32 to +0.24, the player's rotX sat
// at exactly 0.0000 on every line of the probe.
//
// The gates:
//
// headsetConnected: without poses the head decides nothing and the mouse is
// still the only way to aim. Writing a pitch then would fight the player for
// their own aim, on a machine that never asked for VR.
//
// isThirdPerson and thirdPersonAllowed: in third person the pitch is written
// only with the switch on AND only while something is being aimed, where in
// first person it is written on every frame.
//
// The difference is measured, not assumed. The first person camera does not
// depend on the player's rotX at all, so writing it there moves nothing. The
// third person camera is BUILT from it: the engine keeps the camera on a
// sphere about a point above the player's feet and eases its angle towards
// rotX a few percent of what is left per physics step (fChaseDeltaMult, and
// the probe's curve and the headset's trace both agree). So a written pitch
// carries the viewpoint with it, a second later - and LookControl then reads
// that tilt back out and turns it into camera height on top. Writing it
// every frame would have the head's every nod moving the camera. Writing it only while
// aiming, with the share the camera has taken so far compensated the way the
// body's turn already is, keeps the picture still and sends the shot where
// the head points.
//
// menuIsUp: nothing is aimed while a menu is open, and a dialogue under
// Menus=world is still a menu over a live world - the same trap the crosshair
// fell into.
bool AimPitchWanted(bool enabled, bool headsetConnected, bool isThirdPerson,
                    bool thirdPersonAllowed, bool menuIsUp, bool attacking);

// How far from level the player may be aimed, in radians.
//
// 89 degrees rather than 90, and it is Oblivion's own limit rather than a
// choice: the Construction Set wiki gives GetAngle X a true in-game range of
// -89 to 89, "with -89 if the player is looking above himself, and 89 if
// player is looking at his feet" - you cannot look exactly up or exactly down
// in game. Writing past it would put the player somewhere the engine's own
// input can never reach, and there is nothing above the pole to aim at.
inline constexpr float kAimPitchLimitRadians = 1.55334303f;  // 89 degrees

// The player pitch, in radians, that sends a projectile along the view.
//
// viewSinPitch is the sine of how far the view is tilted, positive looking up
// - what SinPitchOf reports, and what LookControl already relies on for its
// sign ("the tilt is positive looking up").
//
// The result is negated, and that is not a guess. Two independent sources say
// Oblivion's rotX runs the other way: xOBSE's GameObjects.h has the triple at
// TESObjectREFR+0x20 in radians with rotX as pitch, and the Construction Set
// wiki's SetAngle page states it outright - "the values are counterintuitive:
// negative angles force the player look up, positive angles, down". So up is
// positive here and negative there, and the minus sign is the whole
// difference between aiming at the sky and aiming at the floor.
float PlayerPitchForGaze(float viewSinPitch);

// Whether the player is turned to face the gaze on this frame.
//
// Everything AimPitchWanted asks, and then one more thing: attacking. The
// sideways half is deliberately not the same deal as the vertical one.
//
// Turning the body to follow every glance was offered and refused - "character
// bleibt". Look left and you look left; the character keeps facing where it
// was pointed. But then an arrow cannot go where you are looking, because it
// leaves along the body's heading, and that is the complaint this answers:
// "head-based aiming only works up and down".
//
// So the body follows the gaze only while the attack control is held, which is
// exactly the drawing of a bow, the winding up of a swing, or the readying of
// a spell. Let go and the body is left facing where the shot went and stops
// following again.
//
// The attack control rather than the drawn weapon, and that is a choice with a
// reason on both sides. Reading whether a weapon is out means calling
// HighProcess::GetWeaponOut through a pointer at MobileObject+0x58 and a
// virtual table index of 0xBE - three unverified assumptions deep, and a wrong
// index is not a wrong answer but a crash. The held control needs no address
// at all, and it is the better question anyway: a weapon can be out for
// minutes while nothing is being aimed at.
// WHEN THE BODY IS TURNED, which is the whole difference between the two ways
// this can work.
//
// WhileAiming is the original: the body follows the gaze for as long as the
// attack control is held. It works - shots go where you look - and it has one
// fault that cannot be tuned away, because it is not a bug but the shape of the
// thing. Walking follows the body's heading, so a body turned to the gaze is a
// character walking sideways, and it stays turned until something puts it back.
// Putting it back is what caused nausea once.
//
// OnShot is Naragorn's suggestion and the better shape: "we decouple the aim
// camera from the character's heading; then I can keep choosing the walking
// direction with the mouse or the gamepad."
//
// The two cannot be separated in SPACE - a measurement, not a guess: the arrow
// leaves along rotZ and walking follows rotZ, one field, and turning it moves
// both. That is what the sideways-walking fault has been telling us all along.
//
// They can be separated in TIME. The arrow does not exist while the bow is
// being drawn; it is created when the shot is released. So the heading only has
// to be right for the handful of frames between letting go and the arrow
// leaving - and for all the rest of the time, including the whole draw, the
// body can be left exactly where the mouse put it.
//
// Which gives, in OnShot mode: aim anywhere, freely, for as long as you like,
// while still steering where you walk. The heading turns when the shot goes and
// comes back when the attack is over, and neither is a moment anyone spends
// walking. No standing offset, and nothing to reset.
//
// What it costs, stated rather than discovered later: during the draw the body
// is NOT turned, so the bow points where the character faces rather than where
// you are looking. Whether that reads badly in first person - where the hands
// are drawn against the camera rather than the body - is a question for the
// headset.
enum class AimTurnMode {
	WhileAiming,
	OnShot,
};

// Whether the body should be turned to the gaze on this frame, in either mode.
//
// In WhileAiming this is just the attack control. In OnShot it is the release
// and the attack that follows it - the window in which the arrow is made.
bool AimTurnDue(AimTurnMode mode, bool attackHeld, bool attackWasHeld, bool attackInProgress);

// thirdPersonAllowed is the switch for the third person - the gates are
// otherwise the same in both views, and the body is turned the same way. What
// differs is only how the camera answers, and that is the chase share below.
bool AimYawWanted(bool enabled, bool headsetConnected, bool isThirdPerson,
                  bool thirdPersonAllowed, bool menuIsUp, bool attacking);

// How much of what is left the third person camera closes in one of its
// steps, when the step cannot be measured.
//
// MEASURED twice, and the two measurements disagree in a way that is itself
// the finding. Without a headset the probe tilted the view with the mouse
// and read the camera back: the remaining angle shrank by 0.81 to 0.83 every
// four frames, which is 0.95 a frame, and Oblivion.ini carries
// fChaseDeltaMult=0.0500 under [Havok]. In the headset the shot trace then
// read the camera's heading frame by frame during a real turn, and the
// steps came in a pattern - two frames of movement, one frame of none, over
// and over - with each moving step closing about six percent of what was
// left. The camera is stepped by the PHYSICS, at its own sixty hertz, not by
// the renderer at the headset's ninety; the probe simply ran at a rate where
// the two coincided.
//
// So this is no longer what the compensation runs on. The rate is measured
// from the camera itself every frame (MeasuredChaseRate), and this constant
// is only the fallback for the frames where there is nothing to measure it
// from.
inline constexpr float kChaseDeltaMult = 0.05f;

// One step of the third person camera's easing, applied to OBVR's own record
// of the turn: how much of the body's turn the camera has taken so far.
//
// The camera eases its angle towards rotZ, and rotZ carries the body's turn.
// Easing is linear, so the camera's answer to (heading + turn) is its answer
// to the heading plus its answer to the turn - and the second is this value,
// stepped by the same rate the engine stepped the whole. It is what the base
// rotation has to be turned back by in third person, and what the camera's
// arc about the player has to be undone by: not the turn itself, which the
// camera has not yet taken, but the share of it that has arrived.
float ChaseStep(float chased, float target, float rate);

// The rate the engine actually applied to its chase camera THIS frame, read
// off the camera rather than assumed.
//
// The camera's angle eases towards the player's rotation - measured, in
// both axes, with the camera's heading sitting at minus rotZ once settled.
// So on any frame the engine moved it, the distance it moved divided by the
// distance it had left to go IS the rate, whatever the rate is and whether
// or not the physics stepped this frame: a frame with no step reads zero,
// and zero is exactly the share of the turn the camera took in it. The
// mouse turning at the same time changes nothing here, because the rate is
// a property of the easing and not of what is being eased towards - which
// is what lets the same rate be applied to the aim's share alone.
//
// Where the camera is already where it is going there is nothing to divide
// by, and the fallback rate stands in; the share it is applied to is then
// small as well, so what it gets wrong is small.
struct ChaseRateInput {
	// The camera's angle this frame and last frame, in the same convention
	// as target - positive the same way round.
	float cameraNow = 0.0f;
	float cameraBefore = 0.0f;
	bool haveBefore = false;

	// Where the camera is easing towards: the player's rotation as the engine
	// left it this frame, converted to the camera's convention.
	float target = 0.0f;

	// The rate to report when it cannot be measured.
	float fallbackRate = kChaseDeltaMult;
};

// Below this much left to go, the division is noise. Half a degree.
inline constexpr float kChaseMinRemainingRadians = 0.0087f;

float MeasuredChaseRate(const ChaseRateInput& input);

// The share of the body's turn the CAMERA currently carries, which is what
// the compensation has to undo.
//
// First person: the whole of it, at once. The engine builds that camera from
// the heading just written, so the offset is in the picture the same frame it
// goes into the player - measured, and it is why the compensation there is
// the raw offset. Third person: the eased share, because that camera swings
// round over a second and taking the whole turn out of it on the first frame
// would swing the picture the other way by everything the camera had not
// yet done.
float AimCameraShare(bool isThirdPerson, float bodyOffset, float chased);

// The heading to put into the player, given the one the engine left there and
// the step to turn the body by this frame.
//
// A difference rather than an absolute angle, and that is what makes it safe
// to write. An absolute heading would have to be converted out of OBVR's
// rotation matrix into Oblivion's own convention - zero at north, growing
// clockwise - and a mistake there points the character at a fixed compass
// bearing rather than merely off by a few degrees. Adding a turn to the
// heading the engine itself just wrote needs no such conversion: whatever the
// zero is, both sides share it.
//
// The sign follows from the two conventions and was then confirmed by
// measurement. OBVR reads a heading off column 0 of the rotation matrix, which
// runs opposite to rotZ, so the camera's heading is the negative of the
// player's; the aim probe found exactly that, with the two columns summing to
// zero across sixty frames whenever the head was centred, and to the head's own
// turn when it was not. So the step is subtracted.
//
// The result is brought into 0..2pi, which is the range Oblivion's own angles
// are reported in.
float PlayerYawForGaze(float engineYaw, float stepRadians);

// How far the body still has to come round before it faces the gaze.
//
// headYaw is where the head is pointing relative to the camera's base, and
// bodyOffset is how much of that OBVR has already handed to the body. The
// difference is what is left, and it is what the body is turned by - so with
// the head held still the body arrives and then stops, rather than turning for
// as long as the head is off centre.
//
// Wrapped, so a head either side of straight back does not read as most of a
// circle of remaining turn.
float AimYawRemaining(float headYaw, float bodyOffset);

// Whether the turn handed to the body should be given back this frame.
//
// The fault it fixes: after aiming to one side the body keeps that heading, so
// the character walks the way the shot went rather than the way the wearer is
// looking. Naragorn, after firing: "put the camera back to the middle so the VR
// is not disturbed."
//
// THIS IS THE CHANGE THAT CAUSED NAUSEA ONCE, and it is deliberately not the
// same change. Commit 7e57e69 unwound the body over about a quarter of a
// second, eased, with the view held still by the base-rotation compensation.
// It was reverted as a527686. The leading suspect was never the speed but the
// SHAPE: the compensation is applied from the next frame's base rotation, so
// during a multi-frame unwind the view can lag the body by one frame every
// frame - a small continuous drift, which is exactly the signature that makes
// people ill while being almost invisible to describe.
//
// So this gives the turn back in ONE frame. A single frame of lag is not a
// motion; a drift sustained over thirty frames is. That is a different fix
// rather than a slower one, which the task notes explicitly warn against
// re-landing.
//
// The view should not move at all: the body turns back by the same angle the
// compensation stops subtracting, so the two cancel. What moves is the
// character, which in first person cannot be seen and in third person is the
// point.
//
// AFTER THE SHOT, NOT ON THE RELEASE. This is the fault the first attempt at
// this had, and it is not a detail: letting go of the attack control STARTS the
// shot, and the arrow spawns several frames later at the end of the release
// animation. The heading in those frames is the heading the arrow leaves along,
// so a body straightened on the release frame sends the shot forwards instead
// of where it was aimed - which would have broken the aiming this exists to
// serve.
//
// So the wait ends when the game says the attack is over, or when the safety
// limit below runs out, whichever comes first. Two conditions because the first
// one rests on the least certain numbers in GameAddresses.h: if the action
// values are wrong the limit still ends the wait, and the log says which of the
// two did.
inline constexpr float kAimReturnLimitSeconds = 1.5f;

// secondsSinceRelease is negative while the control is held or has already been
// dealt with, so "not waiting for anything" has a value of its own rather than
// being confused with "released this very frame".
//
// aimStanding is whether there is anything to give back at all: a turn in the
// body, or the third person's borrowed pitch. Either alone is enough - a shot
// aimed with the head level sideways still holds the pitch, and still must not
// be straightened before it has gone.
bool AimReturnWanted(bool enabled, bool headsetConnected, bool menuIsUp, bool attackHeld,
                     float secondsSinceRelease, bool attackInProgress, bool aimStanding);

// Whether a heading OBVR wrote actually reached the player, judged one frame
// later against what is in the field before anything is written again.
//
// It was measured that a written heading does survive - the log line "a written
// heading SURVIVED the frame" is that measurement, and the whole compensation
// downstream is built on it. This is not that question being asked again. It is
// the case where something else - a load, a script, the engine turning the
// player itself - takes the write away, because then the camera would be
// corrected for a turn the body never made and the view would sit crooked.
//
// Below kYawLandingMinStep the two answers are not distinguishable from one
// frame to the next, and the measured behaviour stands.
bool YawWriteLanded(float wroteYaw, float engineYawNow, float stepTaken);

// The smallest step whose arrival can be told apart from the mouse's own
// movement within a frame: about half a degree.
inline constexpr float kYawLandingMinStep = 0.01f;

// How long the body stays turned to the gaze for a cast, once nothing else
// holds the window open.
//
// The same limit the return already uses for a bow shot, and for the same
// reason: it is not a duration anybody wants, it is the point past which
// something has gone wrong and the body must come back regardless. A window
// that never closed would leave the wearer walking sideways for good.
inline constexpr float kCastWindowLimitSeconds = 1.5f;

// Everything the cast window is decided from in one frame.
struct CastWindowInput {
	// Whether gaze aiming for spells is switched on at all.
	bool enabled = false;

	// The cast control, now and last frame. The window opens on the DOWN EDGE:
	// a spell leaves on the press, unlike an arrow which leaves on the release,
	// so there is nothing to wait for.
	bool castHeld = false;
	bool castWasHeld = false;

	// Whether the spell has still to leave: the engine's action field reading
	// Attack rather than AttackFollowThrough.
	//
	// This is the same signal the bow's window closes on, and that it works for
	// a cast at all was a measurement rather than a guess - see NextCastWindow.
	bool spellStillLeaving = false;

	float deltaSeconds = 0.0f;
};

// The cast window, carried from frame to frame.
struct CastWindow {
	bool open = false;
	float secondsOpen = 0.0f;
};

// The window, one frame on.
//
// A CAST DRIVES THE ACTION FIELD AFTER ALL, and finding that out is what this
// function is now built on.
//
// The enum has no cast in it - that reading was right, and it is why the first
// version of this went looking for a flag of its own instead. But the trace
// showed the field moving during a cast anyway, and moving the same way every
// time: 54 frames of Attack, then 24 to 26 of AttackFollowThrough, then None.
// Six casts in one session, that shape every time. A cast is simply reported as
// an attack.
//
// Which means the bow's own signal works here. IsShotUnreleased counts Attack
// and not AttackFollowThrough, and the whole reason it draws that line is that
// FollowThrough means the thing has GONE - the measurement that made the bow
// aim correctly in the first place. So the body can be let go 25 frames earlier
// than the casting flag would have let it go.
//
// That flag was measured too, and it is what the action field replaced: it read
// true for 79 frames, which is 54 plus 25 exactly. It covers Attack AND
// FollowThrough and cannot tell them apart, so it holds the body turned for
// four tenths of a second after the spell has already left. The action field
// says the same thing with more resolution, and it has two sources where the
// flag had one and a hedge.
//
// The press still opens the window, because that is the one moment nothing is
// unsure of, and the limit still closes it whatever else is true. The minimum
// now covers only the gap between the key going down and the action field
// catching up - a few frames - rather than standing in for a signal that was
// missing.
CastWindow NextCastWindow(const CastWindow& current, const CastWindowInput& input,
                          float minimumSeconds);

// Everything the release clock is stepped from in one frame.
struct ReleaseClockInput {
	bool attackHeld = false;
	bool attackWasHeld = false;

	// Whether the cast key's window is itself turning the body, and whether it
	// was standing at the start of this frame.
	bool castTurning = false;
	bool castWasOpen = false;

	// Whether a cast is allowed to start the clock at all.
	//
	// FALSE ONCE THE CAST HOOK IS DOING THE TURNING, and that is not a detail.
	// See NextReleaseClock.
	bool castFeedsClock = false;

	// A spell's turn is due this frame - the lead time after the cast is up
	// and the animation is still running. Starting the clock is what puts the
	// turn in place: from here the bow's own machinery holds it and gives it
	// back when the action field says the spell has left. See NextCastArm.
	bool castTurnDue = false;

	float deltaSeconds = 0.0f;
};

// Not counting. A negative clock means the control is held, or the last
// release has already been dealt with.
inline constexpr float kReleaseClockIdle = -1.0f;

// The clock that separates "let go" from "the shot has gone", one frame on.
//
// WHAT THIS INNOCENT-LOOKING NUMBER ACTUALLY CONTROLS. Starting it is what
// tells the rest of the aim that a shot is in flight: turningOnShot needs only
// a started clock, IsShotUnreleased then says whether the thing has left, and
// AimTurnDue turns the body on that alone. So whoever starts this clock hands
// the body to the bow's machinery.
//
// Which is how a cast kept turning the body for 0.88 seconds after a hook had
// been written specifically to stop it. A cast reads to the engine as an ATTACK
// - measured, 53 frames of Attack and then FollowThrough - so the cast window
// starting the clock was enough for the bow's logic to recognise a shot and
// hold the body turned for the whole animation. The window was never doing the
// turning; it was opening a door, and the fix that closed the wrong one changed
// nothing at all.
//
// So castFeedsClock exists to shut that door. With the hook in charge, a cast
// starts nothing and is invisible to all of it; the hook starts the clock
// itself at the one moment a return is owed.
float NextReleaseClock(float current, const ReleaseClockInput& input);

// When a spell's heading has to be turned, which is not when the spell is
// cast.
//
// MagicCaster::CastMagicItem is the START of a cast, and this was measured
// rather than assumed: the hook fires on a frame whose action field still
// reads None, and the very next frame the field turns to Attack and stays
// there for 53 frames before becoming AttackFollowThrough. Turning the
// heading inside that call is therefore about nine tenths of a second too
// early, and a spell turned there goes out along whatever heading the body
// happens to have when the animation ends - forwards, which is exactly what
// the headset showed.
//
// The moment that matters is the end of the animation. xOBSE names it: its
// OnMeleeRelease event is documented as the "release of bow/melee/staff
// attack, spell cast", and it is raised off changes in
// HighProcess::currentAction (obse/obse/EventManager.h). The engine's own
// notion of a spell leaving is that field going from Attack to something
// else.
//
// But the change cannot be waited for. OBVR's own reading of the bow says so
// in PlayerAim.h: on the frame the field first reads AttackFollowThrough the
// arrow HAS GONE. A turn made then is a turn made after the projectile has
// its direction.
//
// So the turn is armed by the cast and made a set time later, shortly before
// the animation is due to end - and from that moment the machinery the bow
// already has holds it and gives it back when the field says the spell has
// left. The lead time is the one number here that is a measurement rather
// than a mechanism, which is why it comes from the INI.
struct CastArmInput {
	// The hook saw the player's own cast begin this frame.
	bool castBegan = false;

	// The cast animation is still running - the action field reads Attack.
	bool actionIsAttack = false;

	float deltaSeconds = 0.0f;

	// How long after the cast begins the turn is made. The measured animation
	// is about 0.9 s, so a little under that puts the turn in place before the
	// spell leaves without holding the body for the whole cast.
	float turnAfterSeconds = 0.0f;

	// A cast that never ends - a spell interrupted, an animation that does not
	// report - must not leave this armed for the rest of the run.
	float limitSeconds = 0.0f;
};

// Nothing is being watched.
inline constexpr float kCastArmIdle = -1.0f;

struct CastArm {
	float seconds = kCastArmIdle;

	// The action field has read Attack at least once, so the animation is
	// running or has been. Without it the frames before the animation starts -
	// the field still reads None for one or two - would look like its end.
	bool sawAttack = false;

	// The turn for this cast has been made, so it is not made twice, and the
	// animation is watched to its end for the measurement rather than dropped.
	bool turned = false;
};

struct CastArmDecision {
	CastArm next;

	// Start the release clock now, which is what puts the turn in place and
	// hands it to the machinery that holds and returns it.
	bool turnNow = false;

	// The animation ended without the turn having been made, so the spell left
	// unaimed.
	bool missed = false;

	// How long this cast's animation actually ran, on the frame its end is
	// seen. Greater than zero only on that frame.
	//
	// THIS IS THE POINT OF WATCHING TO THE END. The lead time cannot be a
	// number written down once: 53 frames of animation is 0.88 s at 60 Hz and
	// 0.59 s at 90 Hz, and a headset picks the rate. A lead time of 0.70 s was
	// measured wrong for exactly that reason - the animation had ended before
	// it came up, five casts out of five, and the log said so. So the duration
	// is measured from the cast that just happened and the next one turns
	// shortly before that, whatever frame rate the machine is running at.
	float measuredSeconds = 0.0f;
};

CastArmDecision NextCastArm(const CastArm& current, const CastArmInput& input);

// What the body's turn did to where the eye stands, so it can be undone.
struct AimArcInput {
	// The camera as the engine placed it this frame, before OBVR has added
	// anything of its own to it.
	NiPoint3 cameraPosition{0.0f, 0.0f, 0.0f};

	// The point the body turns about: the player's own position.
	NiPoint3 turnCentre{0.0f, 0.0f, 0.0f};

	// Whether that centre could be read at all. Without it there is no arc and
	// no correction - see PlayerWorldPosition for why this is not signalled by
	// a zero vector.
	bool centreKnown = false;

	// How much turn the body is currently holding, in radians.
	float bodyOffset = 0.0f;
};

// How far to move the camera to put it back where it stood before the body
// turned underneath it.
//
// WHY THIS EXISTS, and it is the last piece of the aim's jump. Turning the body
// to the gaze moves TWO things about the view, and only one of them was ever
// put back. The rotation was: the compensation in the camera hook takes the
// body's turn back out of the base, and reading the result back proved it does
// so exactly, to a tenth of a degree, in every frame of every shot.
//
// The position was not. The first person camera does not stand on the axis the
// body turns about - it stands about 4.6 units off it, measured two ways that
// agree - so a turn walks it along an arc. The eye steps sideways while the
// view holds still, and a viewpoint that steps sideways cannot be told from one
// that turns. "es ist die ganze view. das ganze bild."
//
// MEASURED, not assumed, at both ends:
//   - the step itself: 1.7 units, eighteen times in nine shots, appearing the
//     frame the body takes the turn and reversing the frame it gives it back;
//   - the radius that produces it: 1.71 units of chord at 21.4 degrees needs an
//     arm of 4.60, and the arm read directly out of a later run - the camera's
//     placement minus the player's position - is 4.58.
// One number derived from motion, one read from geometry, agreeing to half a
// percent. That is the mechanism, not a story that fits.
//
// The direction the arm turns is read off the same two sources rather than
// recalled: PlayerYawForGaze writes rotZ = base - offset, and the view's
// heading was measured RISING by exactly the offset as the body turned, so a
// vector fixed to the body turns by +offset in the sense RotationFromHeading
// builds. Undoing it is therefore the very matrix the rotation compensation
// already uses, applied to the arm instead of composed onto the base.
//
// Horizontal only. The body's turn is about the vertical, so it cannot change
// how high the eye stands, and touching z here would fight the head tracking
// for the one axis it owns outright.
//
// The zero vector when there is nothing to undo, so the caller can add it
// unconditionally.
NiPoint3 AimArcCorrection(const AimArcInput& input);

// THE THIRD PERSON PITCH: OBVR holding the player's rotX while a shot is
// aimed, and giving it back afterwards.
//
// In first person rotX is simply written every frame, because nothing else
// depends on it. In third person the mouse's tilt is still a thing the
// player uses - it is what LookControl turns into camera height - so the
// field is borrowed rather than taken: written with the gaze while aiming,
// and set back to what the mouse would have made of it once the shot is gone.
//
// What "what the mouse would have made of it" is, measured: a written
// rotation SURVIVES the frame, and the engine then ADDS the mouse's movement
// to it. So while OBVR holds the field, the mouse's own contribution is the
// difference between what the engine leaves in the field and what OBVR last
// put there, and the tilt the mouse would be holding is that difference
// accumulated. While OBVR does not hold it, the field IS the mouse's tilt.
struct AimPitchHold {
	// Whether OBVR has written the field in third person and not yet given it
	// back.
	bool held = false;

	// The tilt the mouse would be holding on its own, in the engine's own
	// convention: radians, positive looking DOWN.
	float mouseTilt = 0.0f;

	// What the field holds after this frame - OBVR's write if there was one,
	// otherwise what the engine left there. The next frame's mouse movement
	// is measured against it.
	float fieldNow = 0.0f;
};

struct AimPitchHoldInput {
	// rotX as the engine left it this frame, before anything is written.
	float enginePitch = 0.0f;

	// Whether the gaze should be written this frame (AimPitchWanted, in third
	// person), and the value it would write.
	bool writeGaze = false;
	float gazePitch = 0.0f;

	// Whether a held field is to be given back this frame - the same moment
	// the body's turn is given back.
	bool returnDue = false;
};

struct AimPitchHoldDecision {
	AimPitchHold next;

	// Whether to write the field, and with what.
	bool write = false;
	float value = 0.0f;

	// How far the field now stands from the mouse's own tilt, in the engine's
	// convention. What the camera will ease towards on top of the mouse, and
	// therefore what the compensation has to take out of it once it has.
	float offset = 0.0f;
};

AimPitchHoldDecision NextAimPitchHold(const AimPitchHold& current, const AimPitchHoldInput& input);

// Undoing the third person camera's swing about its pivot for the share of
// OBVR's pitch it has taken - the vertical counterpart of AimArcCorrection.
//
// The geometry is measured, not assumed. The probe put the third person
// camera on a sphere about a point above the player's feet: at every tilt
// the camera's height below that point was the sphere's radius times the
// sine of the camera's own pitch, to a hundredth of a unit, and its distance
// from the player's axis the radius times the cosine. The camera always
// LOOKS AT the pivot, so its rotation and its position are one state, and
// the pivot can be recovered every frame from what is at hand: the camera's
// position, the player's feet and the camera's own pitch.
//
// The radius is read the same way rather than taken from a setting, because
// it is not a setting: a wall behind the player shortens it, and the run that
// measured it found 30 units where the game's default is far more.
struct AimTiltInput {
	// The camera as the engine placed it this frame, before OBVR has added
	// anything, and the player's feet - the axis the pivot sits on.
	NiPoint3 cameraPosition{0.0f, 0.0f, 0.0f};
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
	bool centreKnown = false;

	// The sine of the camera's own pitch as the engine built it, positive
	// looking up - SinPitchOf on the rotation before the look control levels
	// it.
	float cameraSinPitch = 0.0f;

	// The share of OBVR's pitch offset the camera has taken so far, in the
	// engine's convention (positive looking DOWN) - the eased value, the same
	// way the sideways share is.
	float pitchShare = 0.0f;
};

// Beyond this the sphere's geometry cannot be read back safely: the distance
// from the axis is the radius times the cosine of the pitch, and dividing by
// a cosine near zero would turn a tenth of a unit of noise into a radius of
// hundreds. The game itself stops at 89 degrees.
inline constexpr float kAimTiltMinCosine = 0.05f;

NiPoint3 AimTiltCorrection(const AimTiltInput& input);

// THE AIM SET AT THE SOURCE, and what it leaves the turn machinery.
//
// With AimAtSource on and its one hook in place, the heading a projectile
// leaves along and the heading a swing is tested against are set inside the
// engine call that reads them - the animation-key handler, see
// game/AimAtSource.h - so nothing above has to turn the body any more, and
// must not: a body turned for even one frame is a frame of walking pulled
// sideways and a turn the engine animates. With the source aim off, or its
// hook refused, the turn owns exactly what IsShotUnreleased always gave it.
//
// The action values are HighProcess's own: 2 Attack (melee and casts alike,
// measured), 4 AttackBow, 5 AttackBowArrowAttached.
bool AimTurnOwnsAction(bool aimAtSource, SInt32 action);

// Whether the source aim is wanted at all this frame - the gates the yaw and
// pitch share, and viewAimed is the third-person switch already decided
// upstream.
bool AimAtSourceWanted(bool enabled, bool aimAtSource, bool headsetConnected, bool menuIsUp,
                       bool viewAimed);

// Whether one invocation of the key handler gets the swap: wanted, the
// player's own, and an attack in flight - Attack for a swing or a cast, the
// arrow on the string for a bow. The handler also runs for keys that make
// no attack (a footstep, an equip), and those are left alone rather than
// written and unwritten for nothing.
bool AimSourceSwapDue(bool wanted, bool isPlayer, SInt32 action);

// THE VISIBLE THIRD-PERSON HALF OF SOURCE AIM.
//
// AimAtSource deliberately hides the temporary player rotation from every
// frame, which keeps walking free but also leaves the rendered attack pointing
// along the body. This state remembers the last live attack direction so the
// upper body can be turned after animation, without giving rotZ either job
// back. AttackFollowThrough keeps the last direction rather than following a
// new glance after the arrow, spell or hit has already gone.
struct ThirdPersonAimVisualState {
	bool active = false;
	float gazeYaw = 0.0f;
	float gazePitch = 0.0f;  // positive looking up
};

struct ThirdPersonAimVisualInput {
	bool enabled = false;
	bool aimAtSource = false;
	bool headsetConnected = false;
	bool isThirdPerson = false;
	bool thirdPersonAllowed = false;
	bool menuIsUp = false;
	// A drawn weapon is already an aiming pose. Keeping the visual alive from
	// ready to sheathe avoids straightening between bow shots and lets a sword
	// face the gaze before its short attack action begins.
	bool weaponDrawn = false;
	bool bodyWithoutWeapon = false;
	// Controls are included because the animation action field trails intent:
	// a bow does not report its useful action until the draw is under way, and
	// MagicCaster begins a cast before HighProcess changes to Attack.
	bool attackHeld = false;
	bool castActive = false;
	SInt32 action = -1;
	float percent = 0.0f;
	float gazeYaw = 0.0f;
	// The player's rotX convention, positive looking down. The visual result
	// below turns it into the scene graph convention, positive looking up.
	float playerPitch = 0.0f;
};

struct ThirdPersonAimVisualDecision {
	ThirdPersonAimVisualState next{};
	bool write = false;
	float yaw = 0.0f;
	float pitch = 0.0f;
};

// Every meaningful flow is value-only: all gates, weapon ready, the live
// attack groups, the held follow-through, reset, percentage scaling and its
// refusal/clamp. The acting half only applies the two returned angles to
// Bip01 Spine2.
ThirdPersonAimVisualDecision NextThirdPersonAimVisual(
	const ThirdPersonAimVisualState& current, const ThirdPersonAimVisualInput& input);

// The head is a continuous visual rather than an attack state. Kept as a pure
// gate so POV/menu/config transitions all release it deterministically.
bool ThirdPersonHeadVisualWanted(bool enabled, bool headFollowsGaze,
	                             bool headsetConnected, bool isThirdPerson,
	                             bool thirdPersonAllowed, bool menuIsUp);

// Whether a value found on the stack is plausibly a return address: inside
// the code section, and sitting right after a call instruction. preceding
// holds the six bytes before the address, preceding[5] being the last one.
//
// For the probes that walk the stack for the caller that makes a
// projectile. Every form a compiler emits a call in is accepted - rel32,
// through a register, through [reg+disp8], [reg+disp32], [sib] and [disp32]
// - and a number that merely lands in the code section is not, which is
// what keeps a probe's log short enough to read.
bool LooksLikeReturnAddress(UInt32 value, UInt32 textStart, UInt32 textEnd,
                            const UInt8 preceding[6]);

}  // namespace obvr::camera

#pragma once

#include "core/Types.h"

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

// Attempts per menu episode. A handful rather than one, because the first
// held frame after a menu opens may be special - the engine may still be
// mid-transition - and a probe that only ever measured that frame would
// mistake the transition for the answer.
inline constexpr UInt32 kMenuWorldProbeAttempts = 5;

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

}  // namespace obvr::camera

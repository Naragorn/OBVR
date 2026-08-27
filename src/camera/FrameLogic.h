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

// Whether this frame should reach the headset as a flat picture rather than as
// one eye of a stereo pair.
//
// Two reasons, and either one is enough.
//
// No camera pass. The camera hook is what supplies a viewpoint, so without it
// there is nothing to claim a picture was drawn from. Loading screens and the
// main menu are like this: the world is not being drawn at all.
//
// A menu is up. This is the one that was missing, and its absence is what made
// menus in game flicker. Oblivion keeps drawing the world behind an open menu,
// but not on every frame - so on the frames it did, the camera hook ran and
// OBVR treated the frame as a live eye, filling the headset with the world;
// on the frames it did not, the same menu came back as a small rectangle
// floating in black. Alternating between those two at frame rate is the menu
// snapping open and shut, and it is only visible in a headset, because on a
// monitor both look like the same picture.
//
// Asking the game whether a menu is open answers it once and for the whole
// time the menu is up, so nothing alternates.
bool FrameIsFlat(bool hadCameraPass, bool menuIsUp);

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
// menuIsUp: a frame with a menu open is delivered flat whatever the passes
// do - FrameIsFlat says so - so a second pass would be two renders for a
// picture that ignores both. The same question, asked of the same source, as
// the flat decision: the two must agree on what kind of frame this is.
bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp);

// Whether this frame's 2D pass should be redirected to the HUD texture.
//
// The same two questions the second scene pass asks, minus the arming - the
// redirect needs no camera shift, only a frame somebody will deliver.
//
// frameOpen: the compositor accepted this frame, so the captured layer has a
// way to reach the headset. Without it the redirect would strip the HUD from
// the monitor's frame and hand it to nobody.
//
// menuIsUp: a menu frame is delivered flat, and the flat picture is the back
// buffer - so the layer has to stay in it, or the menu being shown would be
// missing from the very picture that exists to show it.
bool WantsHudRedirect(bool frameOpen, bool menuIsUp);

}  // namespace obvr::camera

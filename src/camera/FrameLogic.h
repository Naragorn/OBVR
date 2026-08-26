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

}  // namespace obvr::camera

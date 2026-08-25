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

	// Records this frame's point of view and reports what changed. The caller
	// logs; keeping the log out of here is what makes it testable.
	PovEvent ObservePointOfView(bool isThirdPerson);
};

// Whether a periodic action is due on this frame.
//
// An interval of 0 means the action is switched off, which is why this is not
// simply a modulo at the call site: that would divide by zero. frameCount is
// counted from 1 upwards by the callback, so the first due frame of an
// interval of n is frame n rather than frame 0.
bool IsDue(UInt32 frameCount, UInt32 interval);

}  // namespace obvr::camera

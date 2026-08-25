#pragma once

#include "core/Types.h"

namespace obvr::render {

// What to do about what the compositor said last frame.
//
// Lifted out of the code that talks to OpenVR for the same reason
// camera/FrameLogic was lifted out of the camera hook: the calls themselves
// need a headset, the decisions made from their answers do not. A decision
// taken inside a call that cannot be run without SteamVR is a decision that
// never gets checked.
//
// The reason this exists at all is one specific way of failing badly.
// WaitGetPoses blocks until the compositor wants the next frame, so calling
// it from Oblivion's own thread puts the game on the compositor's clock -
// which is right, and is how the picture stays in step with the world. But
// when another application holds focus, WaitGetPoses throttles itself to
// 10 Hz. Blocking on that inside the game loop drags Oblivion to ten frames
// a second, and nothing on screen points at VR as the cause; it looks like a
// driver problem. So the answers have to be read, and rendering given up
// rather than the frame rate.

enum class SubmitDecision {
	// Everything is fine. Render and submit.
	Continue,

	// Skip this frame but try again. For conditions that pass on their own.
	Skip,

	// Stop rendering for this session, and stop calling the compositor.
	// Either the failure cannot pass, or waiting for it to pass costs more
	// than the picture is worth.
	StopRendering,
};

class SubmitPolicy {
public:
	// How many consecutive recoverable failures are tolerated before
	// rendering is given up.
	//
	// Ninety is around a second at headset frame rates, and about nine
	// seconds if the compositor has already throttled to 10 Hz - long enough
	// to ride out a headset being put down and picked up again, short enough
	// that nobody plays a whole scene at ten frames a second wondering what
	// broke.
	static constexpr UInt32 kMaxConsecutiveFailures = 90;

	// Reads one compositor error code and says what to do next. Call once per
	// frame with whatever WaitGetPoses or Submit returned.
	SubmitDecision Observe(int compositorError);

	// Forgets the run of failures. For the moments where continuity would be
	// misleading - a fresh start, a reconfiguration.
	void Reset();

	// Whether rendering has been given up. Once true it stays true: nothing
	// here turns it back on, because everything that gets here is either
	// permanent or has already been waited out.
	bool HasStopped() const { return m_stopped; }

	UInt32 GetConsecutiveFailures() const { return m_consecutiveFailures; }

	// The code that caused the stop, or 0 if rendering has not stopped. For
	// the log: "OBVR stopped rendering" is not a useful line on its own.
	int GetStopReason() const { return m_stopReason; }

private:
	UInt32 m_consecutiveFailures = 0;
	bool m_stopped = false;
	int m_stopReason = 0;
};

// Whether a compositor error can be expected to pass on its own.
//
// The distinction that matters is not severity but whether waiting helps.
// Losing focus passes when focus comes back. A texture in a format the
// compositor will not take is the same next frame and the frame after, and
// retrying it sixty times a second only fills the log.
bool IsRecoverable(int compositorError);

}  // namespace obvr::render

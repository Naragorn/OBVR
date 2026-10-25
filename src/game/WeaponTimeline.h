#pragma once

// How long a weapon is out of sight while it changes hands (Nadi, Windows,
// 2026-10-24: "eine Splitsekunde Pause zwischen Waffe schwebt an und wird
// ausgerüstet, same beim Loslassen"; free-weapons-spec 5.6).
//
// The take, as the code has it: a pulled weapon arrives and the engine's grab
// is let go of (game::StepWeaponPull), the activation takes it into the
// inventory - the floated model is gone from that frame - and the equip goes
// at once (game::StepWeaponTake); the frame the game shows it equipped and
// sheathed the ready click is set for the next frame's hand mode, the engine
// plays the Equip group, and the weapon hangs on the hand at the group's
// Attach key (onehandequip.kf: Attach at 0.300 s of 0.633; twohandequip.kf
// 0.367 of 0.933; bowequip.kf 0.500 of 0.800; handtohandequip.kf 0.100 of
// 0.200 - read from the game's files 2026-10-25), at WeaponDrawSpeed times
// the game's speed after the blend in. The release: the hold read open for
// WeaponReleaseSeconds, then the unequip and the drop, the dropped model and
// later its body.
//
// Which of these frames the eye sees was not measured. This counts them: a
// timeline begun at the first step, each later step marked in the frame it
// came in, ended once the marks it waits for are in (or after two seconds),
// and the log names the frames and the milliseconds. Pure, covered by
// weapon_timeline_test; CameraHook marks the steps.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kTimelineMarks = 4;
inline constexpr float kTimelineMostSeconds = 2.0f;

// The take's marks: begun at a pulled weapon's arrival, or at the take
// itself for a weapon taken at its grip.
inline constexpr UInt32 kTakeTaken = 0;     // the activation took it - the world's model is gone
inline constexpr UInt32 kTakeEquipped = 1;  // the game shows it equipped
inline constexpr UInt32 kTakeDrawn = 2;     // the game shows the weapon out
inline constexpr UInt32 kTakeInHand = 3;    // the weapon hangs on the hand's bone
// The release's marks: begun when the hold reads open.
inline constexpr UInt32 kReleaseLetGo = 0;      // the release time passed: sheathed, stowed or dropped
inline constexpr UInt32 kReleaseOutOfHand = 1;  // nothing hangs on the hand's bone any more
inline constexpr UInt32 kReleaseShown = 2;      // the dropped reference's model is in the world
inline constexpr UInt32 kReleaseBody = 3;       // and its body

inline constexpr UInt32 TimelineBit(UInt32 mark) { return mark < kTimelineMarks ? 1u << mark : 0u; }

struct WeaponTimeline {
	bool running = false;
	UInt32 frames = 0;   // frames stepped since the start
	float seconds = 0.0f;
	UInt32 wanted = 0;   // the marks that end it, as bits
	UInt32 marked = 0;   // the marks in, as bits
	SInt32 at[kTimelineMarks] = {-1, -1, -1, -1};  // the frame each mark came in
	float atSeconds[kTimelineMarks] = {0.0f, 0.0f, 0.0f, 0.0f};
};

enum class TimelineEnd : UInt8 {
	Idle,     // not running
	Running,
	Done,     // every wanted mark came in, this frame
	RanOut,   // kTimelineMostSeconds without them
};

inline void BeginTimeline(WeaponTimeline& t, UInt32 wanted) {
	t = WeaponTimeline{};
	t.running = true;
	t.wanted = wanted;
}

inline void CancelTimeline(WeaponTimeline& t) { t.running = false; }

// A mark ending the timeline from now on as well.
inline void WantTimeline(WeaponTimeline& t, UInt32 mark) {
	if (t.running) {
		t.wanted |= TimelineBit(mark);
	}
}

// The first time a mark comes, in the frame it came in.
inline void MarkTimeline(WeaponTimeline& t, UInt32 mark) {
	if (!t.running || mark >= kTimelineMarks || (t.marked & TimelineBit(mark)) != 0) {
		return;
	}
	t.marked |= TimelineBit(mark);
	t.at[mark] = static_cast<SInt32>(t.frames);
	t.atSeconds[mark] = t.seconds;
}

// Once a frame, after the frame's marks. A step that is not a number, goes
// back or is a pause (half a second or more) counts the frame, not the time.
inline TimelineEnd StepTimeline(WeaponTimeline& t, float dt) {
	if (!t.running) {
		return TimelineEnd::Idle;
	}
	if ((t.marked & t.wanted) == t.wanted) {
		t.running = false;
		return TimelineEnd::Done;
	}
	++t.frames;
	if (dt == dt && dt > 0.0f && dt < 0.5f) {
		t.seconds += dt;
	}
	if (t.seconds >= kTimelineMostSeconds) {
		t.running = false;
		return TimelineEnd::RanOut;
	}
	return TimelineEnd::Running;
}

// The frames and the seconds from one mark to a later one; false when either
// has not come or the second came first.
inline bool TimelineSpan(const WeaponTimeline& t, UInt32 from, UInt32 to, UInt32& frames, float& seconds) {
	if (from >= kTimelineMarks || to >= kTimelineMarks || t.at[from] < 0 || t.at[to] < t.at[from]) {
		return false;
	}
	frames = static_cast<UInt32>(t.at[to] - t.at[from]);
	seconds = t.atSeconds[to] - t.atSeconds[from];
	return true;
}

}  // namespace obvr::game

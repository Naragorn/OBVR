#pragma once

// The shield bash's approach (Nadi's headset run of 2026-11-28: "schildstoss
// ging nun paar mal nicht, paar mal schon. lag vll an der geschwindigkeit").
// The bash was judged by the controller's speed towards their middle in the
// one frame its shield's middle stood in their column (StepShieldBash). A jab
// ends at the target: by the frames it is in the column the hand is already
// slowing down. The log's refused attempts had the controller at 3.8-5.1 m/s
// during the attempt but 1.6-2.9 m/s towards them in the best frame in the
// column (OBVR.log 13:55, lines 45763, 45777, 47936, 47961, 47974, 51547).
//
// So a bash is judged by its approach: the fastest speed towards the target
// over the frames of the last kBashApproachSeconds, this one included, taken
// against the target the shield meets now - each frame's velocity into their
// body from that frame's shield disc (BashClosingSpeed, game/BashContactLogic.h:
// into their column's surface or at their middle, the more; until the 14:56
// run towards their middle alone). Towards only: a frame moving away ends the
// approach (an older fast frame before a pull back does not count), a sweep
// across counts nothing. The caller asks it only while the shield meets them.
//
// The trail is a fixed ring: nothing allocated, one sample a frame, a walk
// over at most the window's frames when asked. Pure, covered by
// bash_approach_test; the caller is camera/hands/ShieldBashApproach.cpp.

#include "core/Types.h"
#include "game/BashContactLogic.h"

namespace obvr::game {

// The window. The log's landed jabs took 6-10 frames at 90 Hz (67-111 ms)
// from 1.5 m/s to the contact, so the fastest frame of a jab lies within some
// 0.11 s before it; 0.15 s holds that with room. Derived from the frame counts
// of the attempt lines, the margin a guess: the lines do not carry the frame
// in which the speed peaked.
constexpr float kBashApproachSeconds = 0.15f;
// The frames kept: the window at up to 213 Hz.
constexpr UInt32 kBashApproachSamples = 32;

struct BashApproachSample {
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // the controller's, world axes, m/s
	BashDisc disc;                        // the shield's disc, world units (its middle alone without a shape)
	double time = 0.0;                    // seconds, the caller's clock
};

struct BashApproachTrail {
	BashApproachSample samples[kBashApproachSamples];
	UInt32 count = 0;
	UInt32 next = 0;  // where the next sample goes
};

inline void AddBashSample(BashApproachTrail& t, const NiPoint3& velocity, const BashDisc& disc, double now) {
	BashApproachSample& s = t.samples[t.next];
	s.velocity = velocity;
	s.disc = disc;
	s.time = now;
	t.next = (t.next + 1) % kBashApproachSamples;
	t.count = t.count < kBashApproachSamples ? t.count + 1 : kBashApproachSamples;
}

inline void ClearBashTrail(BashApproachTrail& t) {
	t.count = 0;
	t.next = 0;
}

// The fastest speed into the target (BashClosingSpeed), m/s, over the samples no older than
// windowSeconds before now (a window of zero, or not a number: the newest
// alone, if taken now), walking back from the newest and stopping at the first
// that moved away. Nothing kept, or a time not a number: 0.
inline float BashApproachSpeed(const BashApproachTrail& t, const BashColumn& target, double now, float windowSeconds) {
	const double window = windowSeconds > 0.0f ? static_cast<double>(windowSeconds) : 0.0;
	float most = 0.0f;
	for (UInt32 i = 0; i < t.count; ++i) {
		const BashApproachSample& s = t.samples[(t.next + kBashApproachSamples - 1 - i) % kBashApproachSamples];
		const double age = now - s.time;
		if (!(age >= 0.0 && age <= window + 1e-6)) {
			break;
		}
		const float towards = BashClosingSpeed(s.velocity, s.disc, target);
		if (towards < 0.0f) {
			break;  // it moved away: what came before is no approach
		}
		most = towards > most ? towards : most;
	}
	return most;
}

}  // namespace obvr::game

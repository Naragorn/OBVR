#pragma once

// Whether SteamVR has dropped the game to half rate.
//
// Read in the hall with seven people (2026-10-23 evening, compositor.csv):
// after a GPU spike of 14 to 47 ms the compositor fell into its half-rate
// fallback - interleaved reprojection: every frame shown twice, the frame
// index stepping by two, the poses handed at the vsync, the picture two
// vsyncs after the poses - and stayed there for 228, 231 and 784 frames (3
// to 10 s) although the frames had 3.5 ms of work. Valve: the compositor
// drops into half-time mode when the application gets too close to using
// up the frame time, "throttled after 4 dropped frames out of the past 10".
// Nothing on OBVR's side of WaitGetPoses shows it; the compositor's frame
// timing does (numFramePresents 2 on frame after frame). This watch turns
// that into two log lines, so a tester's "it stutters for seconds" has its
// name.
//
// Pure: fed the finished frame's index and present count every frame;
// render/HeadsetRenderer.cpp reads them for the submit pacing anyway.

#include "core/Types.h"

namespace obvr::render {

struct HalfRateWatch {
	// Consecutive finished frames shown twice or more before it is called
	// half rate (a single reprojected frame is not), and consecutive frames
	// shown once before it is called over.
	static constexpr UInt32 kEnterFrames = 10;
	static constexpr UInt32 kLeaveFrames = 10;

	enum class Event : UInt8 { None, Entered, Left };

	bool halfRate = false;
	UInt32 lastIndex = 0;
	bool haveIndex = false;
	UInt32 doubles = 0;         // the current streak of frames shown twice or more
	UInt32 singles = 0;         // the current streak of frames shown once
	UInt32 enteredAt = 0;       // the compositor frame index the episode began at
	UInt32 episodeFrames = 0;   // finished frames seen since the episode began
	UInt32 episodes = 0;
	UInt32 halfRateFrames = 0;  // over all episodes
	UInt32 lastEpisodeFrames = 0;

	// The compositor's finished frame, as GetFrameTiming reports it. The same
	// index twice is the same frame seen again and counts once.
	Event Feed(UInt32 frameIndex, UInt32 presents) {
		if (haveIndex && frameIndex == lastIndex) return Event::None;
		haveIndex = true;
		lastIndex = frameIndex;
		if (presents >= 2) {
			++doubles;
			singles = 0;
		} else {
			++singles;
			doubles = 0;
		}
		if (!halfRate) {
			if (doubles >= kEnterFrames) {
				halfRate = true;
				enteredAt = frameIndex - (kEnterFrames - 1) * 2;   // the index steps by two at half rate
				episodeFrames = kEnterFrames;
				++episodes;
				return Event::Entered;
			}
			return Event::None;
		}
		++episodeFrames;
		if (singles >= kLeaveFrames) {
			halfRate = false;
			lastEpisodeFrames = episodeFrames - kLeaveFrames;   // the single frames were the way out
			halfRateFrames += lastEpisodeFrames;
			episodeFrames = 0;
			return Event::Left;
		}
		return Event::None;
	}
};

}  // namespace obvr::render

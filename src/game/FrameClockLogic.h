#pragma once

// The engine's frame clock, read in Oblivion.exe 1.2.0.416 (2026-10-23):
// the main loop (0x0040D89F) calls GetTickCount and hands the tick to the
// time info's update, 0x0047D170, a thiscall on the TimeInfo at 0x00B33E90
// (xOBSE's g_timeInfo). That update takes the frame's length as an INTEGER
// number of milliseconds - the tick minus the previous frame's tick, a zero
// read as one, anything over 166.67 read as 166 - and stores it times 0.001
// and times the global time multiplier (0x00B06704) as the seconds at +0x0C,
// the delta every time-driven update in the frame advances by.
//
// Two coarsenesses follow. GetTickCount itself moves in steps of the system
// clock interrupt - 10 to 16 ms on Windows unless something raised the timer
// resolution (the "64 Hz bug" Oblivion Stutter Remover's bFix64Hertz and
// Oblivion Display Tweaks' iTickFix exist for): a frame of 11 ms then reads
// as 0 or 16. And even a millisecond-exact tick quantises a 13.33 ms frame to
// 13 or 14 - 3.7 % of speed, frame by frame, in every animation and every
// moving thing. A headset runs at 75, 90 or 120 Hz; none of those periods is
// a whole number of milliseconds.
//
// OBVR puts the precise length in: the update runs as the engine wrote it
// (its integer bookkeeping stays the engine's), then the seconds at +0x0C
// are replaced by the performance counter's delta since the previous
// replaced frame, held to the engine's own range and multiplied the same
// way. The pure half here; the detour in game/FrameClock.cpp.

#include <cstdio>

#include "core/Types.h"

namespace obvr::game {

// The TimeInfo at 0x00B33E90, as the update at 0x0047D170 uses it.
struct EngineTimeInfo {
	UInt8 frozen;          // +0x00: nonzero, the frame's seconds are written as zero
	UInt8 pad[3];
	float clampMs;         // +0x04: 1000 / iFPSClamp, or zero - the fixed step, when set
	float clampCarryMs;    // +0x08: the fixed step's carried fraction
	float frameSeconds;    // +0x0C: the frame's length, what everything advances by
	UInt32 lastMs;         // +0x10: the previous frame's milliseconds since the start
	UInt32 startTick;      // +0x14: GetTickCount at the start
};
static_assert(sizeof(EngineTimeInfo) == 0x18, "TimeInfo layout");

// The engine's own range for a frame: a zero reads as one millisecond, more
// than 166.67 (the float at 0x00A3D14C, 1000/6) reads as 166 (the 0xA6 it
// clamps to); 166.0 to 166.67 pass through as they are.
inline constexpr double kFrameClockMinMs = 1.0;
inline constexpr double kFrameClockCeilingMs = 1000.0 / 6.0;
inline constexpr double kFrameClockClampedMs = 166.0;

// Whether the precise length replaces the engine's this frame. Not while the
// clock is frozen (the engine writes zero and so must the frame read), not
// while iFPSClamp fixes the step (the engine's fixed-step bookkeeping is the
// frame's length then), and not on the first measured frame - there is no
// previous counter reading to take a delta from.
inline bool PreciseClockApplies(bool frozen, float clampMs, bool havePrevious) {
	return !frozen && clampMs == 0.0f && havePrevious;
}

// The frame's seconds from the counter delta: the milliseconds held to the
// engine's own range, times 0.001, times the engine's global multiplier -
// the same shape the engine gives its integer milliseconds. A frequency of
// zero or a negative delta (a counter that went backwards) gives the floor.
inline float PreciseFrameSeconds(long long counterDelta, long long counterFrequency,
                                 float multiplier) {
	double ms = kFrameClockMinMs;
	if (counterFrequency > 0 && counterDelta > 0) {
		ms = static_cast<double>(counterDelta) * 1000.0 / static_cast<double>(counterFrequency);
		if (ms < kFrameClockMinMs) {
			ms = kFrameClockMinMs;
		} else if (ms > kFrameClockCeilingMs) {
			ms = kFrameClockClampedMs;
		}
	}
	return static_cast<float>(ms * 0.001 * static_cast<double>(multiplier));
}

// What the engine's clock did and what the precise one does, counted over a
// stretch of frames so a log line can show the quantisation: the engine's
// values are kept as distinct milliseconds (at most a dozen distinct ones;
// an integer-millisecond clock has two or three) with their counts, the
// precise ones as a sum, a minimum and a maximum.
struct FrameClockTrace {
	static constexpr UInt32 kDistinct = 12;
	UInt32 frames = 0;
	UInt32 distinct = 0;
	float engineMs[kDistinct] = {};
	UInt32 engineCount[kDistinct] = {};
	UInt32 engineOverflow = 0;   // frames whose engine value found no free slot
	double preciseSumMs = 0.0;
	double preciseMinMs = 0.0;
	double preciseMaxMs = 0.0;

	// One frame: the engine's seconds as it computed them, and the seconds
	// the frame was given (the same value when the precise clock did not
	// apply). Engine values are matched to a tenth of a millisecond.
	void Add(float engineSeconds, float givenSeconds) {
		const float engine = engineSeconds * 1000.0f;
		const double given = static_cast<double>(givenSeconds) * 1000.0;
		if (frames == 0) {
			preciseMinMs = preciseMaxMs = given;
		} else {
			if (given < preciseMinMs) preciseMinMs = given;
			if (given > preciseMaxMs) preciseMaxMs = given;
		}
		preciseSumMs += given;
		++frames;
		for (UInt32 i = 0; i < distinct; ++i) {
			const float d = engineMs[i] - engine;
			if (d < 0.05f && d > -0.05f) {
				++engineCount[i];
				return;
			}
		}
		if (distinct < kDistinct) {
			engineMs[distinct] = engine;
			engineCount[distinct] = 1;
			++distinct;
			return;
		}
		++engineOverflow;
	}

	// The engine's distinct values and counts, most frequent first, then the
	// precise mean, minimum and maximum. False when the buffer was too small
	// or no frame was added.
	bool Summary(char* out, UInt32 capacity) const {
		if (out == nullptr || capacity == 0) return false;
		out[0] = 0;
		if (frames == 0) return false;
		UInt32 order[kDistinct];
		for (UInt32 i = 0; i < distinct; ++i) order[i] = i;
		for (UInt32 i = 1; i < distinct; ++i) {   // insertion sort by count, descending
			UInt32 j = i;
			while (j > 0 && engineCount[order[j - 1]] < engineCount[order[j]]) {
				const UInt32 t = order[j - 1];
				order[j - 1] = order[j];
				order[j] = t;
				--j;
			}
		}
		int used = std::snprintf(out, capacity, "%u frames, engine ms", frames);
		if (used < 0 || static_cast<UInt32>(used) >= capacity) return false;
		for (UInt32 k = 0; k < distinct; ++k) {
			const UInt32 i = order[k];
			const int n = std::snprintf(out + used, capacity - static_cast<UInt32>(used), " %.1f:%u",
			                            static_cast<double>(engineMs[i]), engineCount[i]);
			if (n < 0 || static_cast<UInt32>(used + n) >= capacity) return false;
			used += n;
		}
		if (engineOverflow != 0) {
			const int n = std::snprintf(out + used, capacity - static_cast<UInt32>(used), " other:%u",
			                            engineOverflow);
			if (n < 0 || static_cast<UInt32>(used + n) >= capacity) return false;
			used += n;
		}
		const int n = std::snprintf(out + used, capacity - static_cast<UInt32>(used),
		                            "; given ms mean %.3f min %.3f max %.3f",
		                            preciseSumMs / frames, preciseMinMs, preciseMaxMs);
		return n >= 0 && static_cast<UInt32>(used + n) < capacity;
	}

	void Reset() { *this = FrameClockTrace{}; }
};

}  // namespace obvr::game

#pragma once

// The recording mode ([Debug] Record): the right eye as a short film from
// each hand-script mark on, small enough to write while the game runs at its
// own pace - the proof of a change the way the headset shows it (Nadi,
// 2026-11-18: pictures and evidence for every fix).
//
// Debug.EyeDumpFrames, the instrument before it, read the whole eye back
// every frame (4036 x 3376 for the Dream Air, a new 54 MB system-memory
// surface each time, its rows written one WriteFile at a time) and slowed the
// game to about four frames a second - which changed what it filmed: a take's
// draw fell back on its 0.25 s wait, a throw's flight crawled (proof-arm and
// proof-throw, 2026-11-18). The recording instead
// - shrinks the eye on the GPU (halving blits, then one to the width asked),
//   optionally cut to a part of the eye first;
// - copies the small picture into one of a few system-memory surfaces and
//   reads it kRecordLagFrames later, when the GPU is done with it;
// - leaves the file to a thread of its own;
// - takes every `every`-th frame for `seconds` after a mark.
// A frame with no free slot or no free writer buffer is skipped and counted.
//
// Pure: the run's steps, the cut and the slots - covered by recording_test.

#include <cstdio>

#include "core/Types.h"

namespace obvr::test {

struct RecordSettings {
	bool enabled = false;        // [Debug] Record
	float seconds = 4.0f;        // [Debug] RecordSeconds: how long after each mark
	UInt32 every = 1;            // [Debug] RecordEvery: every n-th frame
	UInt32 width = 640;          // [Debug] RecordWidth: the film's width in pixels
	float cropLeft = 0.0f;       // [Debug] RecordCropLeft .. Bottom: the part of the eye,
	float cropTop = 0.0f;        // as fractions of its width and height
	float cropRight = 1.0f;
	float cropBottom = 1.0f;
};

inline constexpr UInt32 kRecordMinWidth = 64;
inline constexpr UInt32 kRecordMaxWidth = 2048;
inline constexpr float kRecordMaxSeconds = 60.0f;

// The running film: from its mark, how long it still runs, how many frames
// it has seen and how many pictures it asked for.
struct RecordRun {
	char mark[64] = "";
	float left = 0.0f;
	UInt32 frame = 0;
	UInt32 picture = 0;
};

// One frame of the run. True when this frame is to be filmed, its file name
// in nameOut ("OBVR-Rec-<mark>-<NNNN>.bmp", the number counting pictures,
// so the frames of a film are numbered without gaps). A mark starts a new
// film from its own frame.
inline bool StepRecordRun(RecordRun& run, const RecordSettings& s, bool marked, const char* markName, float dt,
                          char* nameOut, size_t nameSize) {
	if (!s.enabled) {
		run = RecordRun{};
		return false;
	}
	if (marked && markName != nullptr && markName[0] != '\0') {
		run = RecordRun{};
		std::snprintf(run.mark, sizeof(run.mark), "%s", markName);
		const float seconds = s.seconds == s.seconds ? s.seconds : 0.0f;
		run.left = seconds > kRecordMaxSeconds ? kRecordMaxSeconds : seconds;
	} else {
		run.left -= dt == dt && dt > 0.0f ? dt : 0.0f;
	}
	if (run.mark[0] == '\0' || run.left <= 0.0f) {
		return false;
	}
	const UInt32 every = s.every > 0 ? s.every : 1;
	const bool take = run.frame % every == 0;
	++run.frame;
	if (!take) {
		return false;
	}
	std::snprintf(nameOut, nameSize, "OBVR-Rec-%s-%04u.bmp", run.mark, run.picture);
	++run.picture;
	return true;
}

// The part of the eye filmed and the film's size: the cut in source pixels
// (fractions clamped to the eye, a cut thinner than 8 pixels taken whole),
// the film as wide as asked (never wider than the cut, within the limits),
// its height keeping the cut's shape, both even for the encoder.
struct RecordCut {
	bool valid = false;
	SInt32 left = 0;
	SInt32 top = 0;
	SInt32 right = 0;
	SInt32 bottom = 0;
	UInt32 outWidth = 0;
	UInt32 outHeight = 0;
};

inline float ClampUnit(float v, float fallback) {
	if (!(v == v)) {
		return fallback;
	}
	return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline RecordCut PlanRecordCut(UInt32 sourceWidth, UInt32 sourceHeight, const RecordSettings& s) {
	RecordCut c;
	if (sourceWidth < 16 || sourceHeight < 16) {
		return c;
	}
	float l = ClampUnit(s.cropLeft, 0.0f);
	float t = ClampUnit(s.cropTop, 0.0f);
	float r = ClampUnit(s.cropRight, 1.0f);
	float b = ClampUnit(s.cropBottom, 1.0f);
	c.left = static_cast<SInt32>(l * static_cast<float>(sourceWidth));
	c.right = static_cast<SInt32>(r * static_cast<float>(sourceWidth));
	c.top = static_cast<SInt32>(t * static_cast<float>(sourceHeight));
	c.bottom = static_cast<SInt32>(b * static_cast<float>(sourceHeight));
	if (c.right - c.left < 8) {
		c.left = 0;
		c.right = static_cast<SInt32>(sourceWidth);
	}
	if (c.bottom - c.top < 8) {
		c.top = 0;
		c.bottom = static_cast<SInt32>(sourceHeight);
	}
	const UInt32 cutWidth = static_cast<UInt32>(c.right - c.left);
	const UInt32 cutHeight = static_cast<UInt32>(c.bottom - c.top);
	UInt32 w = s.width < kRecordMinWidth ? kRecordMinWidth : (s.width > kRecordMaxWidth ? kRecordMaxWidth : s.width);
	if (w > cutWidth) {
		w = cutWidth;
	}
	w &= ~1u;
	UInt32 h = static_cast<UInt32>(static_cast<float>(w) * static_cast<float>(cutHeight) / static_cast<float>(cutWidth) + 0.5f);
	h &= ~1u;
	if (w < 2 || h < 2) {
		return RecordCut{};
	}
	c.outWidth = w;
	c.outHeight = h;
	c.valid = true;
	return c;
}

// The halving steps from the cut down to the film: each step half the one
// before while it is still more than twice the film, so every blit's
// bilinear filter averages its neighbours instead of skipping them. Writes
// up to kRecordMaxSteps sizes (widths and heights) and answers how many.
inline constexpr UInt32 kRecordMaxSteps = 6;

inline UInt32 PlanRecordSteps(UInt32 cutWidth, UInt32 cutHeight, UInt32 outWidth, UInt32 outHeight,
                              UInt32 (&widths)[kRecordMaxSteps], UInt32 (&heights)[kRecordMaxSteps]) {
	UInt32 count = 0;
	UInt32 w = cutWidth;
	UInt32 h = cutHeight;
	while (count + 1 < kRecordMaxSteps && w / 2 >= outWidth * 2 && h / 2 >= outHeight * 2) {
		w /= 2;
		h /= 2;
		widths[count] = w;
		heights[count] = h;
		++count;
	}
	widths[count] = outWidth;
	heights[count] = outHeight;
	return count + 1;
}

// The read-back slots: a picture copied in frame f is read in frame
// f + kRecordLagFrames, when the GPU is long done with it.
inline constexpr UInt32 kRecordSlots = 4;
inline constexpr UInt32 kRecordLagFrames = 2;

struct RecordSlots {
	bool busy[kRecordSlots] = {};
	UInt32 copiedAt[kRecordSlots] = {};
};

// A free slot for this frame's copy, or kRecordSlots with none.
inline UInt32 FreeRecordSlot(const RecordSlots& s) {
	for (UInt32 i = 0; i < kRecordSlots; ++i) {
		if (!s.busy[i]) {
			return i;
		}
	}
	return kRecordSlots;
}

// Whether a busy slot can be read in frame `now`.
inline bool RecordSlotReady(const RecordSlots& s, UInt32 slot, UInt32 now) {
	return slot < kRecordSlots && s.busy[slot] && now - s.copiedAt[slot] >= kRecordLagFrames;
}

}  // namespace obvr::test

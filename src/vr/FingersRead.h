#pragma once

// Which fingers' curl sensors have read anything at all, for the fist
// (vr::StepFist, game::HandBodyFist).
//
// A fist is the four fingers, index to little, all past the close limit
// (the thumb is left out: on an Index it rests on the stick and the
// buttons). The tester's left controller has a little finger that never
// curls: in his run of 2026-10-12 it read 0.00 in all 52 curl lines, while
// the same hand's middle and ring fingers closed to 1.00 - so the left hand
// was never a fist, and the off hand's blow was a slap every time ("gegner
// mit linker hand slappen geht ... aber mit faust hauen nicht (mein linker
// controller da geht der kleine finger nicht zu krümmen für die faust)").
// No close limit helps a sensor stuck at 0; the "Fist at" row (2026-09-28,
// "a hand whose little finger reads low never made a fist") was for one
// that reads low.
//
// So each finger's highest curl since the start is kept, and a finger that
// has never gone past kFingerReadCurl is left out of the fist - both ways,
// closing and opening - as long as kFistFingersAtLeast of the four have
// read; with fewer (the session's first frames, a hand never closed yet)
// all four count, as before. A working finger reads as soon as it closes
// once, and from then on the hand needs it again, so nothing changes for a
// hand whose sensors all work - the pointing hand and the middle finger
// stay no fist. A finger stuck closed is not handled: it reads, and a fist
// then never opens by it - not seen so far.
//
// Pure, covered by fist_test.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr float kFingerReadCurl = 0.1f;  // a finger has read once it went past this
inline constexpr int kFistFingersAtLeast = 3;   // fingers that must have read before one is left out

struct FingersRead {
	float most[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // thumb to little: the highest curl so far
};

// This frame's curls noted (no curls: nothing).
inline void NoteFingerCurls(FingersRead& read, bool curlValid, const float* curl) {
	if (!curlValid || curl == nullptr) {
		return;
	}
	for (int finger = 0; finger < 5; ++finger) {
		if (curl[finger] > read.most[finger]) {
			read.most[finger] = curl[finger];
		}
	}
}

inline bool FingerHasRead(const FingersRead& read, int finger) {
	return finger >= 0 && finger < 5 && read.most[finger] > kFingerReadCurl;
}

// How many of the four fingers (index to little) the fist goes by: all four
// unless kFistFingersAtLeast have read and some have not.
inline int FistFingerCount(const FingersRead* read) {
	if (read == nullptr) {
		return 4;
	}
	int count = 0;
	for (int finger = 1; finger < 5; ++finger) {
		count += FingerHasRead(*read, finger) ? 1 : 0;
	}
	return count >= kFistFingersAtLeast ? count : 4;
}

// The lowest and the highest curl of the fingers the fist goes by. `read`
// null: all four, index to little.
inline void FistCurlSpan(const float* curl, const FingersRead* read, float& lowest, float& highest) {
	const bool leaveOut = FistFingerCount(read) < 4;
	bool first = true;
	lowest = 0.0f;
	highest = 0.0f;
	for (int finger = 1; finger < 5; ++finger) {
		if (leaveOut && !FingerHasRead(*read, finger)) {
			continue;
		}
		if (first) {
			lowest = curl[finger];
			highest = curl[finger];
			first = false;
			continue;
		}
		lowest = curl[finger] < lowest ? curl[finger] : lowest;
		highest = curl[finger] > highest ? curl[finger] : highest;
	}
}

}  // namespace obvr::vr

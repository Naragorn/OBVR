#pragma once

// The people's bodies kept between collects (game/BladeBodies.cpp) and their
// capsule fits (game/FittedCapsules.h), as pure decisions
// (docs/mesh-fitted-capsules-spec.md, the open bug of 2026-11-09).
//
// Measured that day (OBNG's actor-count run, 105 actors, 20 within the
// hands' 130 units): the table held 16 people, so every collect evicted one
// it had just used - its bones read again by name, its fit abandoned and a
// new one started on a thread of its own. Fits that take 2-3 ms alone took
// 1.3-2.1 s each with dozens of them running at once, OBVR's frame-end work
// went from 0.9 ms to 16.1 ms a frame, and the game died of a failed
// allocation 20 s later (derived: threads' stacks and jobs filling the 32-bit
// address space; the vertex count is read as 16 bits, so no garbage count can
// ask for more than 65535 vertices).
//
// So: a slot is never taken from a person used in the last few collects (the
// one asked for then goes without a kept body this collect - the overflow -
// and the bound's column stands in), and a fit starts only while few are
// running, abandoned ones counted until they end.
//
// Pure, covered by body_keep_test.

#include "core/Types.h"

namespace obvr::game {

// One slot as the choice sees it: whose it is (0: empty), the root it was
// read from, and the collect it was last used at.
struct KeepSlotView {
	UInt32 actor = 0;
	UInt32 root = 0;
	UInt32 lastUse = 0;
};

enum class KeepAction : UInt8 {
	Found,     // this actor and root are kept in `index`
	Fresh,     // `index` is to be (re)read for them
	Overflow,  // no slot may be taken: none kept for them this collect
};

struct KeepChoice {
	KeepAction action = KeepAction::Overflow;
	UInt32 index = 0;
};

// The slot for `actor` with `root` at collect `now`: theirs when kept; else
// their own slot with an old root, an empty one, or the longest unused - but
// only one unused for at least `guardCollects` (the counter may wrap; ages
// are taken modulo 2^32).
inline KeepChoice ChooseKeepSlot(const KeepSlotView* slots, UInt32 count, UInt32 actor, UInt32 root, UInt32 now,
                                 UInt32 guardCollects) {
	KeepChoice none;
	if (slots == nullptr || count == 0 || actor == 0) {
		return none;
	}
	UInt32 own = count;
	UInt32 empty = count;
	UInt32 oldest = count;
	UInt32 oldestAge = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const KeepSlotView& s = slots[i];
		if (s.actor == actor) {
			if (s.root == root) {
				return KeepChoice{KeepAction::Found, i};
			}
			own = i;
			continue;
		}
		if (s.actor == 0) {
			if (empty == count) {
				empty = i;
			}
			continue;
		}
		const UInt32 age = now - s.lastUse;
		if (oldest == count || age > oldestAge) {
			oldest = i;
			oldestAge = age;
		}
	}
	if (own < count) {
		return KeepChoice{KeepAction::Fresh, own};
	}
	if (empty < count) {
		return KeepChoice{KeepAction::Fresh, empty};
	}
	if (oldest < count && oldestAge >= guardCollects && oldestAge < 0x80000000u) {
		return KeepChoice{KeepAction::Fresh, oldest};
	}
	return none;
}

// The spacing between fits started on the game's thread.
struct FitGate {
	bool started = false;
	double lastMs = 0.0;
};

// Whether a fit may start now: fewer than `mostRunning` running (abandoned
// ones included, they still hold their thread and memory) and `spacingMs`
// past the last start. A clock that went back starts the spacing anew.
inline bool FitMayStart(const FitGate& g, UInt32 running, UInt32 mostRunning, double nowMs, double spacingMs) {
	if (running >= mostRunning || !(nowMs == nowMs)) {
		return false;
	}
	if (!g.started || nowMs < g.lastMs) {
		return true;
	}
	return nowMs - g.lastMs >= spacingMs;
}

inline void NoteFitStarted(FitGate& g, double nowMs) {
	g.started = true;
	g.lastMs = nowMs;
}

// The bodies' collects' cost, a frame's worth summed, for one line every so
// many frames.
struct BodyCostWindow {
	UInt32 frames = 0;
	double sumMicros = 0.0;
	double maxMicros = 0.0;
	UInt32 skeletonReads = 0;   // slots read fresh (bones found by name)
	UInt32 overflows = 0;       // people left without a kept body
	UInt32 mostFitsRunning = 0;
};

// One frame added; true when the window is full and its line is due. A time
// that is no number or below zero is not counted; framesPerLine 0 is never
// due.
inline bool AddBodyCost(BodyCostWindow& w, double micros, UInt32 skeletonReads, UInt32 overflows, UInt32 fitsRunning,
                        UInt32 framesPerLine) {
	if (!(micros >= 0.0)) {
		return false;
	}
	++w.frames;
	w.sumMicros += micros;
	w.maxMicros = micros > w.maxMicros ? micros : w.maxMicros;
	w.skeletonReads += skeletonReads;
	w.overflows += overflows;
	w.mostFitsRunning = fitsRunning > w.mostFitsRunning ? fitsRunning : w.mostFitsRunning;
	return framesPerLine > 0 && w.frames >= framesPerLine;
}

inline double BodyCostMean(const BodyCostWindow& w) { return w.frames > 0 ? w.sumMicros / w.frames : 0.0; }

}  // namespace obvr::game

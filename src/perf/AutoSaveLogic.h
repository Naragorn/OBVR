#pragma once

// The ring saves itself at a slow stretch ([Performance] AutoSaveSlowSeconds,
// AutoSaveMax; OBNG and Nadi, 2026-11-14: Nadi's Windows run of 2026-11-13 had
// 315 half-rate episodes in Anvil, and its only capture was the quit's - two
// minutes indoors, the slow places missing). A stretch is SteamVR's half-rate
// fallback (render/HalfRateWatch.h: every frame shown twice). One that lasted
// slowSeconds is saved when it ends - the ring then holds it whole, with what
// led into it - or, if it goes on, holdSeconds into it, before the ring loses
// its start. At most maxSaves a session, none within cooldownSeconds of the
// last, one per stretch. Pure, covered by auto_save_test.

#include "core/Types.h"

namespace obvr::perf {

struct AutoSaveSettings {
	UInt32 slowSeconds = 3;      // 0: off
	UInt32 maxSaves = 5;         // a session's saves at most (each about 50 MB)
	double cooldownSeconds = 60.0;
	double holdSeconds = 30.0;   // a stretch still going is saved this far in
};

struct AutoSaveState {
	bool inStretch = false;
	double stretchStart = 0.0;
	bool savedThisStretch = false;
	UInt32 saves = 0;
	bool saved = false;          // any save yet (for the cooldown)
	double lastSave = 0.0;
};

enum class HalfRateEvent : UInt8 { None, Entered, Left };
enum class AutoSaveReason : UInt8 { None, StretchEnded, StretchLong };

struct AutoSaveStep {
	AutoSaveReason reason = AutoSaveReason::None;
	double stretchSeconds = 0.0;
};

// `ringRecords`: a ring is recording (Enabled and Ring). `now` in seconds on
// any steady clock.
inline AutoSaveStep StepAutoSave(AutoSaveState& state, const AutoSaveSettings& s, bool ringRecords,
                                 HalfRateEvent event, double now) {
	AutoSaveStep out;
	bool ended = false;
	if (event == HalfRateEvent::Entered) {
		state.inStretch = true;
		state.stretchStart = now;
		state.savedThisStretch = false;
	} else if (event == HalfRateEvent::Left && state.inStretch) {
		state.inStretch = false;
		ended = true;
	}
	if (!ended && !state.inStretch) {
		return out;
	}
	const double seconds = now - state.stretchStart;
	const bool open = ringRecords && s.slowSeconds > 0 && state.saves < s.maxSaves && !state.savedThisStretch &&
	                  (!state.saved || now - state.lastSave >= s.cooldownSeconds);
	if (!open) {
		return out;
	}
	if (ended && seconds >= static_cast<double>(s.slowSeconds)) {
		out.reason = AutoSaveReason::StretchEnded;
	} else if (!ended && seconds >= s.holdSeconds) {
		out.reason = AutoSaveReason::StretchLong;
	} else {
		return out;
	}
	out.stretchSeconds = seconds;
	state.savedThisStretch = true;
	state.saved = true;
	state.lastSave = now;
	++state.saves;
	return out;
}

}  // namespace obvr::perf

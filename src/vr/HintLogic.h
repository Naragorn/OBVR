#pragma once

// Hints in play, the first few times (docs/onboarding-spec.md 3.3; Nadi,
// 2026-11-14: what OBVR can do that the game cannot - the shield bash, the
// kick - is found by no one but through the controls card). A hint is a small
// sign over the hand that does the thing, shown when the moment fits, and gone
// for good once the player has done it. Pure: the host says each frame which
// hints the moment wants, which things the player did, and whether play is
// quiet (a menu, a load); StepHints says which hint shows. Covered by
// hint_test.

#include "core/Types.h"

namespace obvr::vr {

// In the order they win when two want the same frame.
enum class HintId : UInt32 { DrawAtBody, ShieldBash, Kick, Stow, EatAtMouth, DoorByGrip, Count };
inline constexpr UInt32 kHintCount = static_cast<UInt32>(HintId::Count);

struct HintSettings {
	bool enabled = true;          // [Onboarding] Hints
	UInt32 maxShows = 3;          // a hint ignored this often is not shown again
	float showSeconds = 6.0f;     // how long one showing stays up
	float gapSeconds = 90.0f;     // from one showing's start to the next one's, any hint
	float wantSeconds = 1.5f;     // how long the moment must hold before its hint shows
};

// What is kept between sessions (OBVR_Hints.ini): per hint, how often it was
// shown and whether the player has done the thing.
struct HintMemory {
	UInt32 shown[kHintCount] = {};
	bool learned[kHintCount] = {};
};

struct HintState {
	HintMemory memory;
	SInt32 active = -1;           // the hint up, or -1
	float activeLeft = 0.0f;
	float sinceLast = 1.0e6f;     // seconds since the last showing began; the first may come at once
	float wantHeld[kHintCount] = {};
};

struct HintFrame {
	bool wants[kHintCount] = {};  // the moment fits this hint
	bool did[kHintCount] = {};    // the player did the thing this frame
	bool quiet = false;           // a menu is up, a load, no world: nothing shows
	float dt = 0.0f;
};

struct HintStep {
	SInt32 show = -1;             // the hint to show this frame, or -1
	bool started = false;         // `show` began this frame
	bool learnedNow = false;      // a hint was learned this frame
	bool memoryChanged = false;   // HintMemory changed: write it
};

inline bool HintOpen(const HintSettings& s, const HintMemory& m, UInt32 i) {
	return !m.learned[i] && m.shown[i] < s.maxShows;
}

inline HintStep StepHints(HintState& state, const HintSettings& s, const HintFrame& f) {
	HintStep out;
	const float dt = f.dt == f.dt && f.dt > 0.0f ? f.dt : 0.0f;
	// Done once is learned, whatever else holds - even with hints off.
	for (UInt32 i = 0; i < kHintCount; ++i) {
		if (f.did[i] && !state.memory.learned[i]) {
			state.memory.learned[i] = true;
			out.learnedNow = true;
			out.memoryChanged = true;
		}
	}
	state.sinceLast += dt;
	if (!s.enabled || f.quiet) {
		state.active = -1;
		for (float& held : state.wantHeld) {
			held = 0.0f;
		}
		return out;
	}
	if (state.active >= 0) {
		const UInt32 a = static_cast<UInt32>(state.active);
		state.activeLeft -= dt;
		// Ends when its time is up, the moment is gone, or the thing was done.
		if (state.activeLeft <= 0.0f || !f.wants[a] || state.memory.learned[a]) {
			state.active = -1;
		} else {
			out.show = state.active;
			return out;
		}
	}
	SInt32 pick = -1;
	for (UInt32 i = 0; i < kHintCount; ++i) {
		state.wantHeld[i] = f.wants[i] ? state.wantHeld[i] + dt : 0.0f;
		if (pick < 0 && HintOpen(s, state.memory, i) && state.wantHeld[i] >= s.wantSeconds) {
			pick = static_cast<SInt32>(i);
		}
	}
	if (pick >= 0 && state.sinceLast >= s.gapSeconds) {
		state.active = pick;
		state.activeLeft = s.showSeconds;
		state.sinceLast = 0.0f;
		++state.memory.shown[pick];
		out.show = pick;
		out.started = true;
		out.memoryChanged = true;
	}
	return out;
}

// The INI keys of a hint: "<name>Shown", "<name>Learned".
inline const char* HintName(UInt32 i) {
	switch (static_cast<HintId>(i)) {
	case HintId::DrawAtBody:
		return "DrawAtBody";
	case HintId::ShieldBash:
		return "ShieldBash";
	case HintId::Kick:
		return "Kick";
	case HintId::Stow:
		return "Stow";
	case HintId::EatAtMouth:
		return "EatAtMouth";
	case HintId::DoorByGrip:
		return "DoorByGrip";
	default:
		return "?";
	}
}

// What the sign says. No side named: the sign hangs over the hand meant.
struct HintWords {
	const char* title;
	const char* line;
};

inline HintWords HintWordsFor(UInt32 i) {
	switch (static_cast<HintId>(i)) {
	case HintId::DrawAtBody:
		return {"Draw your weapon", "Grip it where it rests on your body"};
	case HintId::ShieldBash:
		return {"Shield bash", "Drive the shield into them"};
	case HintId::Kick:
		return {"Kick", "Press B on this hand"};
	case HintId::Stow:
		return {"Stow it", "Let go of it at your body"};
	case HintId::EatAtMouth:
		return {"Eat or drink", "Let go of it at your mouth"};
	case HintId::DoorByGrip:
		return {"Open the door", "Grip the handle, pull or push"};
	default:
		return {"", ""};
	}
}

// Which hand the sign hangs over when the host does not name one: true the
// weapon hand (by role), false the other hand. The held thing's hints (stow,
// eat) and the door's go over the hand that holds or reaches - the host's.
inline bool HintOnWeaponHand(UInt32 i) {
	const HintId id = static_cast<HintId>(i);
	return id == HintId::Kick || id == HintId::DrawAtBody;
}

}  // namespace obvr::vr

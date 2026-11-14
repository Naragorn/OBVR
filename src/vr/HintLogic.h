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
enum class HintId : UInt32 {
	Yield,
	DrawAtBody,
	ShieldBash,
	Parry,
	Kick,
	Stow,
	EatAtMouth,
	PotionBelt,
	Throw,
	DoorByGrip,
	Count
};
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
	case HintId::Yield:
		return "Yield";
	case HintId::PotionBelt:
		return "PotionBelt";
	case HintId::DrawAtBody:
		return "DrawAtBody";
	case HintId::ShieldBash:
		return "ShieldBash";
	case HintId::Parry:
		return "Parry";
	case HintId::Kick:
		return "Kick";
	case HintId::Stow:
		return "Stow";
	case HintId::Throw:
		return "Throw";
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
	case HintId::Yield:
		return {"Yield", "Both palms to them, rock them"};
	case HintId::PotionBelt:
		return {"Potion belt", "Keep potions at a weapon place, grip them there"};
	case HintId::DrawAtBody:
		return {"Draw your weapon", "Grip it where it rests on your body"};
	case HintId::ShieldBash:
		return {"Shield bash", "Drive the shield into them"};
	case HintId::Parry:
		return {"Parry", "Meet their blade with yours"};
	case HintId::Kick:
		return {"Kick", "Flick this stick up"};
	case HintId::Stow:
		return {"Stow it", "Let go of it at your body"};
	case HintId::Throw:
		return {"Throw it", "Swing and let go"};
	case HintId::EatAtMouth:
		return {"Eat or drink", "Let go of it at your mouth"};
	case HintId::DoorByGrip:
		return {"Open the door", "Grip the handle, pull or push"};
	default:
		return {"", ""};
	}
}

// What the frame holds that the hints' moments are read from.
struct HintMoment {
	bool fighting = false;        // in combat, or in the combat ring
	bool holsters = false;        // [Hands] Holsters
	bool weaponEquipped = false;
	bool weaponDrawn = false;
	bool shieldBash = false;      // [Hands] ShieldBash
	bool shieldEquipped = false;
	bool parries = false;         // [Hands] WeaponParries
	bool kick = false;            // [Hands] Kick
	bool stowAtBody = false;      // [Hands] StowAtBody
	bool eatAtMouth = false;      // [Hands] EatAtMouth
	bool holdingItem = false;
	bool holdingEdible = false;
	bool atHandle = false;
	bool yield = false;           // [Hands] Yield
	bool lawfulFoe = false;       // a guard or a lawful NPC fights the player (HintLawfulFoe)
	bool potionBelt = false;      // [Hands] PotionBelt
	bool holdingPotion = false;   // a potion held, no poison
	bool potionKept = false;      // a potion kept at a place
	bool lowHealth = false;       // HintLowHealth
};

// Whom the yield's hint is for (Nadi, 2026-11-14: only against the "good
// guys" - a high responsibility, or the guards). The game takes a yield by
// disposition alone and creatures never take one (UESP Oblivion:Aggression,
// Oblivion:Controls#Yield); a bandit's disposition is low, so the gesture
// is wasted on him, and a fight with a guard or a townsperson is where giving
// up is the way out. A guard is the NPC's class flag (xOBSE GameForms.h
// TESClass::kFlag_Guard); responsibility the actor value (0x24).
inline constexpr float kHintLawfulResponsibility = 50.0f;

inline bool HintLawfulFoe(bool isNpc, bool guardClass, float responsibility) {
	return isNpc && (guardClass || responsibility >= kHintLawfulResponsibility);
}

// The potion belt's hint ([Hands] PotionBelt) is learned by a potion drawn
// from a place, not by one kept there: it shows while a potion is held (keep
// it at a weapon place) and, once one is kept, when the player is hurt in a
// fight (reach for it). Hurt: health at this share of its base or below,
// alive.
inline constexpr float kHintLowHealthShare = 0.4f;

inline bool HintLowHealth(float now, float base) {
	return base > 0.0f && now > 0.0f && now <= base * kHintLowHealthShare;
}

inline void HintWantsFor(const HintMoment& m, bool wants[kHintCount]) {
	auto set = [&](HintId id, bool v) { wants[static_cast<UInt32>(id)] = v; };
	set(HintId::Yield, m.fighting && m.yield && m.lawfulFoe);
	set(HintId::PotionBelt,
	    m.potionBelt && (m.holdingPotion || (m.fighting && m.lowHealth && m.potionKept)));
	set(HintId::DrawAtBody, m.fighting && m.holsters && m.weaponEquipped && !m.weaponDrawn);
	set(HintId::ShieldBash, m.fighting && m.shieldBash && m.shieldEquipped);
	set(HintId::Parry, m.fighting && m.parries && m.weaponDrawn);
	set(HintId::Kick, m.fighting && m.kick);
	set(HintId::Stow, m.stowAtBody && m.holdingItem && !m.holdingEdible);
	set(HintId::EatAtMouth, m.eatAtMouth && m.holdingEdible);
	set(HintId::Throw, m.holdingItem);
	set(HintId::DoorByGrip, m.atHandle);
}

// A thing let go counts as the throw's deed from this speed on: a thing set
// down or dropped in passing leaves the hand slower, and would learn the hint
// by accident.
inline constexpr float kHintThrowMetresPerSecond = 2.0f;

inline bool HintThrowCounts(float metresPerSecond) { return metresPerSecond >= kHintThrowMetresPerSecond; }

// Which hand the sign hangs over when the host does not name one: true the
// weapon hand (by role), false the other hand. The held thing's hints (stow,
// throw, eat) and the door's go over the hand that holds or reaches - the host's.
inline bool HintOnWeaponHand(UInt32 i) {
	const HintId id = static_cast<HintId>(i);
	return id == HintId::Kick || id == HintId::DrawAtBody || id == HintId::Parry || id == HintId::Yield;
}

}  // namespace obvr::vr

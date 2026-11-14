#pragma once

// Which of the loaded actors game::CollectNecks (game/Dismember.h) asks for
// their bones. It asked the engine for "Bip01 Head" and "Bip01 Spine2" by
// name in the whole 3D of every loaded actor - up to 64 of them, every frame
// a weapon is drawn and for the hands near the dead - and looked only then
// how far the head was: up to 0.62 ms a scene call in a field of the dead
// (docs/performance-spec.md, "Nadi's Windows session of 2026-11-14").
//
// The rule: an actor's 3D has a world bound, the sphere the engine culls it
// by, and a bone of it lies in that sphere or near it. An actor whose bound,
// grown by the reach and a slack, does not hold the point asked about has no
// head in reach, and is not asked. A bound that says nothing - no radius, no
// number - leaves the actor asked as before.
//
// That a head stays within the slack of its bound is the engine's doing, not
// read in it: so one call in kNeckRuleCheckedEvery asks every actor as
// before and compares, and the caller's line in the log says how far outside
// its bound a taken head was at most and whether the rule ever left one out.
//
// Pure: neck_reach_test.

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Measured, 2026-11-14: the heads taken in four harness runs (2345 of them,
// the standing, the lying dead, the cut) were 16 units inside their bound
// or deeper. The slack is what a bound may lag behind its bones; how far
// outside its bound a head really is, the line in the log goes on saying.
inline constexpr float kNeckBoundSlackUnits = 60.0f;

inline constexpr UInt32 kNeckRuleCheckedEvery = 256;

// Whether the actor whose 3D's world bound is (`centre`, `radius`) can have a
// bone within `within` units of `around`. True as well when the bound or the
// question says nothing to go by.
inline bool BoundCanHoldNeck(const NiPoint3& centre, float radius, const NiPoint3& around, float within) {
	if (!(radius > 0.0f)) {
		return true;
	}
	const float reach = within + radius + kNeckBoundSlackUnits;
	if (!(reach > 0.0f)) {
		return true;
	}
	// Written so that anything that is no number answers true.
	return !((centre - around).LengthSquared() > reach * reach);
}

// Whether call number `call` (counted from 0) asks every actor, to check the
// rule against the names.
inline bool NeckRuleCheckDue(UInt32 call) { return call % kNeckRuleCheckedEvery == 0; }

enum class NeckAsk : UInt8 {
	No,          // out of reach by its bound
	Yes,         // in reach by its bound, or no bound to go by
	YesToCheck,  // out of reach by its bound, asked in a checking call
};

inline NeckAsk AsksBones(const NiPoint3& centre, float radius, const NiPoint3& around, float within, bool check) {
	if (BoundCanHoldNeck(centre, radius, around, within)) {
		return NeckAsk::Yes;
	}
	return check ? NeckAsk::YesToCheck : NeckAsk::No;
}

// What the rule did, counted for the caller's lines in the log.
struct NeckRule {
	UInt32 calls = 0;  // since the start: the checking calls' clock
	UInt32 said = 0;   // the calls at the last line
	// Since the last line:
	UInt32 checks = 0;
	UInt32 loaded = 0;
	UInt32 asked = 0;
	UInt32 taken = 0;
	UInt32 missed = 0;         // left out by the bound with a head in reach
	float mostOutside = 0.0f;  // of the taken heads, HeadOutsideBound
};

// The first line early, so a short session's log has one, then every few
// minutes of a drawn weapon.
inline constexpr UInt32 kNeckRuleFirstLineAfter = 512;
inline constexpr UInt32 kNeckRuleLineEvery = 16384;

// A call of CollectNecks begun with `loaded` actors: whether it checks the
// rule.
inline bool BeginNeckCall(NeckRule& rule, UInt32 loaded) {
	const bool check = NeckRuleCheckDue(rule.calls++);
	rule.checks += check ? 1u : 0u;
	rule.loaded += loaded;
	return check;
}

// An actor whose head is in reach, `outside` its bound by that much
// (HeadOutsideBound); `leftOutByBound` in a checking call for one the bound
// had left out - the rule was wrong there.
inline void NoteNeckTaken(NeckRule& rule, float outside, bool leftOutByBound) {
	rule.mostOutside = rule.taken == 0 || outside > rule.mostOutside ? outside : rule.mostOutside;
	++rule.taken;
	rule.missed += leftOutByBound ? 1u : 0u;
}

inline bool NeckRuleLineDue(const NeckRule& rule) {
	return rule.calls - rule.said >= (rule.said == 0 ? kNeckRuleFirstLineAfter : kNeckRuleLineEvery);
}

inline void NeckRuleLineSaid(NeckRule& rule) {
	rule.said = rule.calls;
	rule.checks = rule.loaded = rule.asked = rule.taken = rule.missed = 0;
	rule.mostOutside = 0.0f;
}

// The necks of a frame are collected once: the blade's step collects them,
// and the end of the frame keeps them for the next frame's sweeps instead of
// collecting them again. A count of this value says "not collected in this
// frame" - none in reach is a count of 0, and collected.
inline constexpr UInt32 kNecksNotCollected = 0xFFFFFFFFu;

inline bool NecksStillToCollect(UInt32 countNow) { return countNow == kNecksNotCollected; }

// How far outside its 3D's bound a head is, in units; negative inside it. 0
// for a bound that says nothing.
inline float HeadOutsideBound(const NiPoint3& centre, float radius, const NiPoint3& head) {
	if (!(radius > 0.0f)) {
		return 0.0f;
	}
	const float squared = (head - centre).LengthSquared();
	if (!(squared >= 0.0f) || !(squared < 1.0e30f)) {
		return 0.0f;
	}
	return std::sqrt(squared) - radius;
}

}  // namespace obvr::game

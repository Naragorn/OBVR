#pragma once

#include "core/Types.h"

namespace obvr::game {

// The weapon skill a strike by motion trains (Nadi via Walkers, 2026-10-27:
// "VR-Schläge sollen Waffenskill trainieren, mit einer Ausnahmeliste, die ein
// anderes Plugin setzen kann").
//
// The engine splits it in two (read 2026-10-27, Oblivion.exe):
// AttackHandling 0x005FEBF0, on a landed hit, counts the hit and notes the
// attack's skill on the attacker (0x005FF53D: vtable +0x214 the count, +1
// through +0x210; 0x005FF55C: the skill through +0x218 - for the player
// +0x648 and +0x64C). The attack's update 0x005FCAB0 later credits them
// (0x005FD7E0..0x005FD85B): for each counted hit, unless the skill reads
// 0x48, UseSkill (vtable +0x39C, 0x00668C30) with that skill, use 0 and
// 0.0 times, then the count back to 0. A strike by motion calls
// AttackHandling straight (game/MeleeHits.cpp) and the update never credits,
// so the count only grew. Now the strike does the update's part itself, at
// once, through the same vtable slots so another plugin hooked there sees it.
// A target whose base is in the exception list trains nothing; its count is
// cleared all the same, so it is not credited later by a vanilla attack.
// Pure: game/SkillCredit.cpp reads and calls.

// What the update skips (the skill noted when there was none).
inline constexpr UInt32 kNoAttackSkill = 0x48;
// More pending hits than a strike can have landed means a count that is not
// one - at most this many uses are credited.
inline constexpr UInt32 kMaxSkillUsesPerStrike = 8;
// The exception list's room: Oblivion Walkers sends its 64 walker bases.
inline constexpr UInt32 kMaxSkillExceptions = 256;

struct SkillCreditInput {
	bool enabled = false;          // [Hands] StrikesTrainSkills
	UInt32 pendingHits = 0;        // the attacker's counted hits (vtable +0x214)
	UInt32 skill = 0;              // the attack's skill (vtable +0x21C)
	bool targetExcepted = false;   // the target's base in the exception list
};

struct SkillCreditPlan {
	UInt32 uses = 0;            // UseSkill calls
	bool clearPending = false;  // the count back to 0 (vtable +0x210)
};

inline SkillCreditPlan PlanSkillCredit(const SkillCreditInput& in) {
	SkillCreditPlan plan;
	if (!in.enabled || in.pendingHits == 0) {
		return plan;
	}
	plan.clearPending = true;
	if (in.targetExcepted || in.skill == kNoAttackSkill) {
		return plan;
	}
	plan.uses = in.pendingHits < kMaxSkillUsesPerStrike ? in.pendingHits : kMaxSkillUsesPerStrike;
	return plan;
}

// Whether a slot of the player's table may be called: the engine's own
// function, or a function outside the game's code - another plugin's hook on
// the slot (Oblivion Walkers holds +0x210 and +0x39C since 2026-10-27), which
// passes on to the engine's. Some other function inside the game's code means
// another build or a patch nobody read: refused, as is no address at all.
inline bool SlotUsable(UInt32 entry, UInt32 engines, UInt32 textStart, UInt32 textEnd) {
	if (entry == engines) {
		return true;
	}
	// Below 64 KB nothing is mapped (core/AddressSpace.h); code needs no
	// alignment, so none is asked.
	if (entry < 0x00010000u) {
		return false;
	}
	return entry < textStart || entry >= textEnd;
}

// Base form IDs whose strikes train nothing, replaced whole by each message
// (an empty one clears it).
class SkillExceptions {
public:
	// `bytes` long, a whole number of UInt32 ids; false (unchanged) otherwise.
	// Beyond kMaxSkillExceptions the rest is dropped.
	bool Replace(const UInt32* ids, UInt32 bytes) {
		if (bytes % sizeof(UInt32) != 0 || (ids == nullptr && bytes != 0)) {
			return false;
		}
		const UInt32 count = bytes / sizeof(UInt32);
		m_count = count < kMaxSkillExceptions ? count : kMaxSkillExceptions;
		for (UInt32 i = 0; i < m_count; ++i) {
			m_ids[i] = ids[i];
		}
		return true;
	}

	bool Contains(UInt32 id) const {
		if (id == 0) {
			return false;
		}
		for (UInt32 i = 0; i < m_count; ++i) {
			if (m_ids[i] == id) {
				return true;
			}
		}
		return false;
	}

	UInt32 Count() const { return m_count; }

private:
	UInt32 m_ids[kMaxSkillExceptions] = {};
	UInt32 m_count = 0;
};

}  // namespace obvr::game

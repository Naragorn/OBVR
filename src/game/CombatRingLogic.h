#pragma once

// The fighters' ring and N at a time (docs/combat-comfort-spec.md 4, items 3
// and 4; the tester, 2026-10-10: "gegner umkreisen den spieler auf einer
// distanz und geben ihm x meter abstand (default 5m) ... aus diesem kreis an
// gegner greift dann immer nur die anzahl an die der spieler in der anderen
// setting angegeben hat. ähnlich zu blade&sorcery" and "immer nur N gegner
// ... n einstellbar 1-5 default 1").
//
// Of those fighting the player, N have a turn: they fight as the game has
// them. The others wait at the ring - kept out of reach, so no blow of
// theirs can land: inside the ring their step forward becomes a step back,
// at the ring a step to the side (combat keeps them facing the player, so a
// side step walks them round the player), beyond it they walk in as they
// will. A turn ends after an attack (once the blow is through) or after a
// spell of not attacking at all, when somebody else is waiting; the one who
// has waited longest steps in next.
//
// The movement goes through the same flags and setters as the slow approach
// (game/SlowApproach.h; xOBSE GameProcess.h's movement flags: forward 0x1,
// backward 0x2, left 0x4, right 0x8, walk 0x100, run 0x200).
//
// Pure: the decisions here, the engine side in game/CombatRing.cpp;
// combat_ring_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt16 kMoveForward = 0x0001;
inline constexpr UInt16 kMoveBackward = 0x0002;
inline constexpr UInt16 kMoveLeft = 0x0004;
inline constexpr UInt16 kMoveRight = 0x0008;
inline constexpr UInt16 kMoveDirections = 0x000F;
inline constexpr UInt16 kMoveWalk = 0x0100;
inline constexpr UInt16 kMoveRun = 0x0200;

struct CombatRingSettings {
	bool enabled = true;
	float ringUnits = 350.0f;          // 5 m at 70 units a metre
	UInt32 attackersAtOnce = 1;        // 1-5
	float bandUnits = 50.0f;           // the ring's width either side: no back-and-forth at its edge
	float idleTurnSeconds = 8.0f;      // a turn without an attack ends after this, when others wait
};

// The settings from the INI's: metres 0 (off) to 10, attackers 1-5.
inline CombatRingSettings CombatRingFrom(bool enabled, float metres, int attackers, float unitsPerMetre) {
	CombatRingSettings s;
	const float perMetre = unitsPerMetre > 0.0f ? unitsPerMetre : 70.0f;
	const float m = metres == metres ? (metres > 10.0f ? 10.0f : metres) : 0.0f;
	s.enabled = enabled && m > 0.0f;
	s.ringUnits = (m > 0.0f ? m : 0.0f) * perMetre;
	s.attackersAtOnce = attackers < 1 ? 1u : attackers > 5 ? 5u : static_cast<UInt32>(attackers);
	// Never wider than a third of the ring: a small ring still has an inside.
	const float band = 0.7f * perMetre;
	s.bandUnits = band < s.ringUnits / 3.0f ? band : s.ringUnits / 3.0f;
	return s;
}

// What a fighter's legs do this frame.
enum class RingOrder : UInt8 {
	Free = 0,  // their turn, or outside the ring walking in: as the game has them
	Back,      // waiting, inside the ring: away from the player
	Circle,    // waiting, at the ring: round the player
};

// The order for a waiting fighter at this distance.
inline RingOrder WaitingOrder(float distanceUnits, const CombatRingSettings& s) {
	if (!(distanceUnits >= 0.0f)) {
		return RingOrder::Free;
	}
	if (distanceUnits < s.ringUnits - s.bandUnits) {
		return RingOrder::Back;
	}
	if (distanceUnits <= s.ringUnits + s.bandUnits) {
		return RingOrder::Circle;
	}
	return RingOrder::Free;
}

// The flags the game asked for, as the order wants them: a step back or to
// the side at a walk; nothing else touched. `circleLeft` picks the side.
inline UInt16 RingFlags(UInt16 flags, RingOrder order, bool circleLeft) {
	if (order == RingOrder::Free) {
		return flags;
	}
	const UInt16 rest = static_cast<UInt16>(flags & ~(kMoveDirections | kMoveRun));
	const UInt16 step = order == RingOrder::Back ? kMoveBackward : (circleLeft ? kMoveLeft : kMoveRight);
	return static_cast<UInt16>(rest | step | kMoveWalk);
}

// --------------------------------------------------------------- The turns

inline constexpr UInt32 kRingMembersMax = 16;

// One fighter as seen this frame.
struct RingFighter {
	UInt32 actor = 0;
	float distanceUnits = 0.0f;
	bool attacking = false;  // their process's action an attack (ParryLogic.h IsAttackAction)
};

struct RingMember {
	UInt32 actor = 0;
	bool turn = false;           // fighting as the game has them
	bool wasAttacking = false;
	bool attacked = false;       // an attack in this turn
	float turnSeconds = 0.0f;    // in this turn
	float waitingSeconds = 0.0f; // since the turn ended (or since they came)
	bool seen = false;           // in this frame's list
	float distanceUnits = 0.0f;
	RingOrder order = RingOrder::Free;
};

struct RingState {
	UInt32 count = 0;
	RingMember member[kRingMembersMax];
};

// What the step decided, for the log.
struct RingVerdict {
	UInt32 turns = 0;
	UInt32 waiting = 0;
	UInt32 turnsEnded = 0;   // this frame
	UInt32 turnsGiven = 0;   // this frame
};

// One frame: `fighters` are those fighting the player now. Members gone
// from the list are forgotten; new ones wait; turns end and are given as
// above. Off: everybody Free and forgotten.
inline RingVerdict StepCombatRing(RingState& state, const CombatRingSettings& s, const RingFighter* fighters,
                                  UInt32 fighterCount, float dtSeconds) {
	RingVerdict v;
	if (!s.enabled || s.ringUnits <= 0.0f) {
		state = RingState{};
		return v;
	}
	const float dt = dtSeconds > 0.0f && dtSeconds < 0.25f ? dtSeconds : 0.0f;
	for (UInt32 i = 0; i < state.count; ++i) {
		state.member[i].seen = false;
	}
	// Who is here: the known updated, the new added waiting.
	for (UInt32 f = 0; f < fighterCount; ++f) {
		const RingFighter& fighter = fighters[f];
		if (fighter.actor == 0) {
			continue;
		}
		RingMember* m = nullptr;
		for (UInt32 i = 0; i < state.count; ++i) {
			if (state.member[i].actor == fighter.actor) {
				m = &state.member[i];
				break;
			}
		}
		if (m == nullptr) {
			if (state.count >= kRingMembersMax) {
				continue;  // past the table: left to the game
			}
			m = &state.member[state.count++];
			*m = RingMember{};
			m->actor = fighter.actor;
		}
		m->seen = true;
		m->distanceUnits = fighter.distanceUnits;
		if (m->turn) {
			m->turnSeconds += dt;
			if (fighter.attacking) {
				m->attacked = true;
			}
		} else {
			m->waitingSeconds += dt;
		}
		m->order = fighter.attacking ? RingOrder::Free : WaitingOrder(fighter.distanceUnits, s);
		m->wasAttacking = fighter.attacking;
	}
	// The gone forgotten.
	UInt32 kept = 0;
	for (UInt32 i = 0; i < state.count; ++i) {
		if (state.member[i].seen) {
			state.member[kept++] = state.member[i];
		}
	}
	state.count = kept;

	UInt32 turns = 0;
	UInt32 waiting = 0;
	for (UInt32 i = 0; i < state.count; ++i) {
		(state.member[i].turn ? turns : waiting) += 1;
	}
	// Turns end - only while somebody waits, and never mid-blow.
	for (UInt32 i = 0; i < state.count && waiting > 0; ++i) {
		RingMember& m = state.member[i];
		if (!m.turn || m.wasAttacking) {
			continue;
		}
		const bool done = m.attacked || m.turnSeconds >= s.idleTurnSeconds;
		if (done) {
			m.turn = false;
			m.attacked = false;
			m.turnSeconds = 0.0f;
			m.waitingSeconds = 0.0f;
			--turns;
			++v.turnsEnded;
		}
	}
	// Free turns to the longest waiting (the nearest first among equals).
	while (turns < s.attackersAtOnce) {
		RingMember* next = nullptr;
		for (UInt32 i = 0; i < state.count; ++i) {
			RingMember& m = state.member[i];
			if (m.turn) {
				continue;
			}
			if (next == nullptr || m.waitingSeconds > next->waitingSeconds ||
			    (m.waitingSeconds == next->waitingSeconds && m.distanceUnits < next->distanceUnits)) {
				next = &m;
			}
		}
		if (next == nullptr) {
			break;
		}
		next->turn = true;
		next->attacked = false;
		next->turnSeconds = 0.0f;
		++turns;
		++v.turnsGiven;
	}
	// A turn is Free; the waiting keep their order.
	v.turns = 0;
	v.waiting = 0;
	for (UInt32 i = 0; i < state.count; ++i) {
		RingMember& m = state.member[i];
		if (m.turn) {
			m.order = RingOrder::Free;
			++v.turns;
		} else {
			++v.waiting;
		}
	}
	return v;
}

// The order the step gave an actor (Free for one it does not know).
inline RingOrder RingOrderOf(const RingState& state, UInt32 actor) {
	for (UInt32 i = 0; i < state.count; ++i) {
		if (state.member[i].actor == actor) {
			return state.member[i].order;
		}
	}
	return RingOrder::Free;
}

}  // namespace obvr::game

#pragma once

// The fighters' ring and N at a time (docs/combat-comfort-spec.md 4, items 3
// and 4; the tester, 2026-10-10: "gegner umkreisen den spieler auf einer
// distanz und geben ihm x meter abstand (default 5m) ... aus diesem kreis an
// gegner greift dann immer nur die anzahl an die der spieler in der anderen
// setting angegeben hat. ähnlich zu blade&sorcery" and "immer nur N gegner
// ... n einstellbar 1-5 default 1").
//
// Of those fighting the player, N have a turn: they fight as the game has
// them. The others wait at the ring: inside it and at it their step becomes
// a step to the side (combat keeps them facing the player, so a side step
// walks them round the player), and the ring adds a drift outwards inside
// it and a pull to its line at it, only while their legs walk (since
// 2026-10-12, "The legs walk; the ring only adds to a walk" below); beyond
// it they walk in as they will. A turn ends after an attack (once the blow
// is through - a blow is an attack sequence animating, not the process's
// lingering action) or
// after a spell of not attacking at all, when somebody else is waiting; the
// one who has waited longest steps in next.
//
// The movement goes through the same flags and setters as the slow approach
// (game/SlowApproach.h; xOBSE GameProcess.h's movement flags: forward 0x1,
// backward 0x2, left 0x4, right 0x8, walk 0x100, run 0x200).
//
// Pure: the decisions here, the engine side in game/CombatRing.cpp;
// combat_ring_test.

#include "core/MathFns.h"
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
	float turnSeconds = 6.0f;          // a turn lasts at least this long (the tester: "zu wenig zeit zum angreifen")
	float turnMaxSeconds = 12.0f;      // and at most this: each turn's length is drawn between the two
	float idleTurnSeconds = 10.0f;     // a turn without an attack ends after this, when others wait
	bool byLevel = true;               // the next turn to the lowest level waiting, the strongest last
};

// The settings from the INI's: metres 0 (off) to 10, attackers 1-5, a turn
// 2-30 s at least and at most that to 60 s (the tester, 2026-10-11: "random
// time ... von random(5s, 15s)"); the order by level (the tester: "erstmal
// kommen nur alle low levels an und dann die high levels").
inline CombatRingSettings CombatRingFrom(bool enabled, float metres, int attackers, float unitsPerMetre,
                                         float turnSeconds = 6.0f, float turnMaxSeconds = 12.0f, bool byLevel = true) {
	CombatRingSettings s;
	s.turnSeconds = turnSeconds >= 2.0f && turnSeconds <= 30.0f ? turnSeconds : 6.0f;
	const float longest = turnMaxSeconds == turnMaxSeconds ? (turnMaxSeconds > 60.0f ? 60.0f : turnMaxSeconds)
	                                                       : (s.turnSeconds > 12.0f ? s.turnSeconds : 12.0f);
	s.turnMaxSeconds = longest < s.turnSeconds ? s.turnSeconds : longest;
	s.idleTurnSeconds = s.turnMaxSeconds > 10.0f ? s.turnMaxSeconds : 10.0f;
	s.byLevel = byLevel;
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

// The legs walk; the ring only adds to a walk (the tester, 2026-10-12: "in
// pausen menüs scheints du die gegner weiter zu pushen ... du pusht die
// gegner auch weg wenn sie eig stehen oder? das wirkt unrealistisch, die
// sollten eig gehen").
//
// Until then the ring placed each waiting fighter's whole step the legs
// fell short of, and inside the ring that was most of it: the walk
// backwards does not move an NPC the AI wants near the player (harness,
// 2026-10-10: one given it stood at 1.9 m for five seconds; ring-wall,
// 2026-10-11: sent back, the actor moved 54 units a second TOWARDS the player
// while the ring placed up to 130 a second away) - a fighter slid backwards
// across the ground while it stood or walked at the player. The walk to
// the side does move them: going round, the legs carried them 67-88 units a
// second and the ring added 0-12 (ring-wall's state lines).
//
// So now every waiting fighter's legs walk to the side - round the player,
// at the ring and inside it alike (RingFlags) - and the ring places only the
// step's radial part: inside, a drift outwards (kRingDriftUnitsPerSecond),
// so the walk round spirals out; at the ring, the pull to its line. And it
// places that only while the legs play a walk whose way does not fight it
// (RingMayPlace, from the walk group animating in the actor's slots): legs
// standing, striking, staggering get nothing placed, and nothing is slid
// away from the player while its legs walk at him.
//
// The flags the game asked for, as the order wants them: a step to the side
// at a walk; nothing else touched. `circleLeft` is the ring's sense,
// counter-clockwise seen from above (RingTangentStep), and the legs face
// the player: that is a step to the actor's RIGHT, and the ring's right a
// step to its left. With the sides the other way round (until 2026-10-11)
// the legs walked against the placed step - the harness had them going round
// the wrong way at 20 units a second.
inline UInt16 RingFlags(UInt16 flags, RingOrder order, bool circleLeft) {
	if (order == RingOrder::Free) {
		return flags;
	}
	const UInt16 rest = static_cast<UInt16>(flags & ~(kMoveDirections | kMoveRun));
	const UInt16 step = circleLeft ? kMoveRight : kMoveLeft;
	return static_cast<UInt16>(rest | step | kMoveWalk);
}

inline constexpr float kRingWalkUnitsPerSecond = 65.0f;  // under a metre a second (100 looked hurried, the tester 2026-10-10)
inline constexpr float kRingDriftUnitsPerSecond = 40.0f; // inside the ring, outwards while the legs walk round

// The legs' walk round the player this frame, a walk's step along the
// tangent from where the fighter stands relative to the player (rx, ry, game
// units): what the side step's flags ask of the legs. Not placed - asked of
// the world (ProbeRingStep), so a wall beside the ring turns the walk round
// (RingStepRefused) before the legs walk into it.
inline bool RingTangentStep(float rx, float ry, bool circleLeft, float dtSeconds, float& ox, float& oy) {
	ox = 0.0f;
	oy = 0.0f;
	const float distance2 = rx * rx + ry * ry;
	if (!(dtSeconds > 0.0f) || dtSeconds > 0.25f || !(distance2 > 1.0f)) {
		return false;
	}
	const float distance = math::Sqrt(distance2);
	const float ux = rx / distance;  // away from the player
	const float uy = ry / distance;
	const float step = kRingWalkUnitsPerSecond * dtSeconds;
	// Left turns counter-clockwise seen from above.
	ox = (circleLeft ? -uy : uy) * step;
	oy = (circleLeft ? ux : -ux) * step;
	return true;
}

// The ring's own step this frame, along the line from the player (units,
// positive away from them): inside the ring a drift outwards, at the ring
// the pull to its line - half the way there, at most half a walk's step.
// Free, no time, a long stall, or a fighter on the player: none.
inline float RingRadialStep(float distanceUnits, RingOrder order, const CombatRingSettings& s, float dtSeconds) {
	if (order == RingOrder::Free || !(dtSeconds > 0.0f) || dtSeconds > 0.25f || !(distanceUnits > 1.0f)) {
		return 0.0f;
	}
	if (order == RingOrder::Back) {
		return kRingDriftUnitsPerSecond * dtSeconds;
	}
	const float half = kRingWalkUnitsPerSecond * dtSeconds * 0.5f;
	const float pull = (s.ringUnits - distanceUnits) * 0.5f;
	return pull > half ? half : (pull < -half ? -half : pull);
}

// The walk animating in an actor's slots, as TESAnimGroup codes (the
// TESReloaded framework's Oblivion Game.h, AnimGroup: Forward 3, Backward 4,
// Left 5, Right 6, FastForward 7, FastBackward 8, FastLeft 9, FastRight 10 -
// the same list the attack watch takes 19-26 from). kRingLegsStill: none.
inline constexpr UInt8 kRingLegsStill = 0xFF;
inline bool RingLegsWalking(UInt8 group) { return group >= 3 && group <= 10; }
inline bool RingLegsForward(UInt8 group) { return group == 3 || group == 7; }
inline bool RingLegsBackward(UInt8 group) { return group == 4 || group == 8; }

// Whether the ring may place its radial step on legs walking `group`: only
// on legs that walk, and not against them - nothing slid away from the player
// while the legs walk at him (a fighter faces the player in combat, so their
// forward is inwards), nor pulled in while they walk back.
inline bool RingMayPlace(UInt8 group, float outwardUnits) {
	if (!RingLegsWalking(group) || !(outwardUnits == outwardUnits) || outwardUnits == 0.0f) {
		return false;
	}
	if (outwardUnits > 0.0f && RingLegsForward(group)) {
		return false;
	}
	if (outwardUnits < 0.0f && RingLegsBackward(group)) {
		return false;
	}
	return true;
}

// ------------------------------------------------------------ The step probed
//
// A step is placed, not walked, so the world has to be asked first (the
// tester, 2026-10-11: "teilweise pusht du die gegner in oder auf wände oder
// zäune"): a ray at knee height and one at chest height along the step and
// the body's half width past it, each stopped by anything fixed (a wall, a
// fence, a tree; clutter and the living are passed), and the ground under
// the new spot against the ground under the old - a stair's step is
// climbed, a fence's top or a ledge is not. game/CombatRing.cpp asks the
// world; this decides.
struct RingStepProbe {
	bool blockedLow = false;   // something fixed at knee height along the step
	bool blockedHigh = false;  // at chest height
	bool groundFound = true;   // ground under both spots
	float riseUnits = 0.0f;    // the ground under the new spot above the old
};

inline constexpr float kRingProbeLowUnits = 30.0f;    // knee height above the feet
inline constexpr float kRingProbeHighUnits = 95.0f;   // chest
inline constexpr float kRingProbeAheadUnits = 22.0f;  // the body's half width past the step
inline constexpr float kRingGroundUpUnits = 60.0f;    // the ground is looked for from this far up
inline constexpr float kRingGroundDownUnits = 90.0f;  // to this far down
inline constexpr float kRingStepRiseMaxUnits = 24.0f; // a stair's step; a fence's top is more

inline bool RingStepClear(const RingStepProbe& p) {
	return !p.blockedLow && !p.blockedHigh && p.groundFound && p.riseUnits == p.riseUnits &&
	       p.riseUnits <= kRingStepRiseMaxUnits && p.riseUnits >= -kRingStepRiseMaxUnits;
}

inline const char* RingStepRefusal(const RingStepProbe& p) {
	if (p.blockedLow) {
		return "something fixed at knee height";
	}
	if (p.blockedHigh) {
		return "something fixed at chest height";
	}
	if (!p.groundFound) {
		return "no ground there";
	}
	return p.riseUnits > 0.0f ? "a rise too high (a fence, a ledge)" : "a drop too deep";
}

// A refused step round the player turns the walk round the other way (a
// wall or a fence beside the ring); not again within this long, so one
// blocked both ways stands rather than flickers.
inline constexpr float kRingFlipCooldownSeconds = 1.0f;

// --------------------------------------------------------------- The turns

inline constexpr UInt32 kRingMembersMax = 16;

// One fighter as seen this frame.
struct RingFighter {
	UInt32 actor = 0;
	float distanceUnits = 0.0f;
	// A blow of theirs in the air: an attack sequence animating
	// (game/AttackTempo.cpp AttackSwinging). Not the process's action - that
	// stays "attack" across a whole chain of blows (the harness, 2026-10-11:
	// one action of 12.7 s over 11 swings), and read from it the "never
	// mid-blow" below held turns of 6 s open for 15, and showed the waiting
	// "attacking" at 3 m for half a minute.
	bool attacking = false;
	UInt16 level = 0;        // the actor's level (LeveledLevel), for the order by level
};

struct RingMember {
	UInt32 actor = 0;
	bool turn = false;           // fighting as the game has them
	bool wasAttacking = false;
	bool attacked = false;       // an attack in this turn
	float turnSeconds = 0.0f;    // in this turn
	float turnLength = 6.0f;     // this turn lasts at least this long (drawn when it was given)
	float waitingSeconds = 0.0f; // since the turn ended (or since they came)
	bool seen = false;           // in this frame's list
	float distanceUnits = 0.0f;
	UInt16 level = 0;
	RingOrder order = RingOrder::Free;
	bool sideSet = false;        // which way round they walk, once chosen
	bool circleLeft = false;
	float flipCooldown = 0.0f;   // seconds until the side may be turned again
	float refusedQuiet = 0.0f;   // seconds until a refused step of theirs is logged again
	bool placedKnown = false;    // where the step before left them (the legs' walk since is measured against it)
	float placedX = 0.0f;
	float placedY = 0.0f;
	UInt32 stepsPlaced = 0;      // since the last state line, for the log
	UInt32 stepsRefused = 0;
	UInt32 stepsHeld = 0;        // ring steps not placed: the legs not walking, or walking against it
	float legsUnits = 0.0f;      // the legs' own walk (its length) since the last state line
	float placedUnits = 0.0f;    // and what the ring placed
	float steppedSeconds = 0.0f; // the time with a step wanted, for the rates
	UInt8 legsGroup = 0xFF;      // the walk animating in their slots this frame (kRingLegsStill: none)
};

// The legs' walk since the step before: the actor's position now against
// where that step left them. Unknown for a first step, and after a frame
// without one.
inline bool RingLegsWalked(const RingMember& m, float x, float y, float& ex, float& ey) {
	ex = m.placedKnown ? x - m.placedX : 0.0f;
	ey = m.placedKnown ? y - m.placedY : 0.0f;
	return m.placedKnown;
}

// Where this frame's step left them (placed, or left where they stand), and
// the books for the state line: the legs' own walk since the step before
// (its length), what was placed, the time.
inline void RingStepTaken(RingMember& m, float leftAtX, float leftAtY, float legsUnits, float placedUnits,
                          float dtSeconds) {
	m.placedKnown = true;
	m.placedX = leftAtX;
	m.placedY = leftAtY;
	m.legsUnits += legsUnits == legsUnits && legsUnits > 0.0f ? legsUnits : 0.0f;
	m.placedUnits += placedUnits;
	m.steppedSeconds += dtSeconds > 0.0f ? dtSeconds : 0.0f;
}

// A frame without a step of the ring's (their turn, or walking in): the
// walk since is not the legs' alone.
inline void RingStepNone(RingMember& m) { m.placedKnown = false; }

// An actor's level as the game plays it: the base form's level, or with the
// "PC level offset" flag the player's level plus it, held between the
// base's min and max (0 for no bound). xOBSE GameForms.h TESActorBaseData:
// level 00E "absolute level, or level offset if PCLevelOffset flag set",
// minLevel 010, maxLevel 012.
inline UInt16 LeveledLevel(SInt16 level, bool pcOffset, UInt16 minLevel, UInt16 maxLevel, SInt32 playerLevel) {
	if (!pcOffset) {
		return level < 0 ? 0 : static_cast<UInt16>(level);
	}
	SInt32 l = playerLevel + level;
	if (minLevel > 0 && l < minLevel) {
		l = minLevel;
	}
	if (maxLevel > 0 && l > maxLevel) {
		l = maxLevel;
	}
	return l < 1 ? 1 : static_cast<UInt16>(l);
}

// A turn's length: between the shortest and the longest by `draw` (0..1,
// the caller's random number; not a number: the middle).
inline float TurnLengthFor(const CombatRingSettings& s, float draw) {
	const float d = draw == draw ? (draw < 0.0f ? 0.0f : (draw > 1.0f ? 1.0f : draw)) : 0.5f;
	return s.turnSeconds + (s.turnMaxSeconds - s.turnSeconds) * d;
}

// One waiting this long goes next whatever their level: the strict order by
// level would hold the strongest back for the whole fight while weaker ones
// that never die take turn after turn (the tester asked for the weak ones
// first; this is the safety behind it).
inline constexpr float kRingStarvedSeconds = 60.0f;

// Whether `m` goes before `best` for the next turn: the starved first, then
// the rested before one whose turn just ended (rested: waited at least a
// turn's shortest length - else a lone weak fighter would take every turn
// while a strong one watches, since it is the lowest level again the moment
// its turn ends), then (by level) the lowest level, then the longest
// waiting, then the nearest.
inline bool RingGoesBefore(const RingMember& m, const RingMember& best, bool byLevel, float restSeconds) {
	const bool mStarved = m.waitingSeconds >= kRingStarvedSeconds;
	const bool bStarved = best.waitingSeconds >= kRingStarvedSeconds;
	if (mStarved != bStarved) {
		return mStarved;
	}
	const bool mRested = m.waitingSeconds >= restSeconds;
	const bool bRested = best.waitingSeconds >= restSeconds;
	if (mRested != bRested) {
		return mRested;
	}
	if (byLevel && !mStarved && m.level != best.level) {
		return m.level < best.level;
	}
	if (m.waitingSeconds != best.waitingSeconds) {
		return m.waitingSeconds > best.waitingSeconds;
	}
	return m.distanceUnits < best.distanceUnits;
}

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
// above, each turn given this frame a length drawn by `draw` (0..1, the
// caller's random number). Off: everybody Free and forgotten.
inline RingVerdict StepCombatRing(RingState& state, const CombatRingSettings& s, const RingFighter* fighters,
                                  UInt32 fighterCount, float dtSeconds, float draw = 0.5f) {
	RingVerdict v;
	if (!s.enabled || s.ringUnits <= 0.0f) {
		state = RingState{};
		return v;
	}
	const float dt = dtSeconds > 0.0f && dtSeconds < 0.25f ? dtSeconds : 0.0f;
	for (UInt32 i = 0; i < state.count; ++i) {
		RingMember& m = state.member[i];
		m.seen = false;
		m.flipCooldown = m.flipCooldown > dt ? m.flipCooldown - dt : 0.0f;
		m.refusedQuiet = m.refusedQuiet > dt ? m.refusedQuiet - dt : 0.0f;
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
		m->level = fighter.level;
		if (m->turn) {
			m->turnSeconds += dt;
			if (fighter.attacking) {
				m->attacked = true;
			}
		} else {
			m->waitingSeconds += dt;
		}
		// A waiting one is kept off even mid-blow: the harness saw the waiting
		// swing again and again at 1.3-2.5 m (2026-10-10), never backing off.
		m->order = WaitingOrder(fighter.distanceUnits, s);
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
		const bool done = m.turnSeconds >= m.turnLength && (m.attacked || m.turnSeconds >= s.idleTurnSeconds);
		if (done) {
			m.turn = false;
			m.attacked = false;
			m.turnSeconds = 0.0f;
			m.waitingSeconds = 0.0f;
			--turns;
			++v.turnsEnded;
		}
	}
	// Free turns: the starved first, then by level the lowest, then the
	// longest waiting, the nearest among equals (RingGoesBefore). Each turn
	// given its own length, the draws of one frame spread apart.
	UInt32 given = 0;
	while (turns < s.attackersAtOnce) {
		RingMember* next = nullptr;
		for (UInt32 i = 0; i < state.count; ++i) {
			RingMember& m = state.member[i];
			if (m.turn) {
				continue;
			}
			if (next == nullptr || RingGoesBefore(m, *next, s.byLevel, s.turnSeconds)) {
				next = &m;
			}
		}
		if (next == nullptr) {
			break;
		}
		float spread = draw + 0.618034f * static_cast<float>(given);
		spread = spread - static_cast<float>(static_cast<int>(spread));
		next->turn = true;
		next->attacked = false;
		next->turnSeconds = 0.0f;
		next->turnLength = TurnLengthFor(s, spread);
		++given;
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

inline RingMember* RingMemberOf(RingState& state, UInt32 actor) {
	for (UInt32 i = 0; i < state.count; ++i) {
		if (state.member[i].actor == actor) {
			return &state.member[i];
		}
	}
	return nullptr;
}

// Which way round an actor walks: as chosen before, or `initial` the first
// time (the caller spreads the fighters half and half).
inline bool RingCircleLeft(RingState& state, UInt32 actor, bool initial) {
	RingMember* const m = RingMemberOf(state, actor);
	if (m == nullptr) {
		return initial;
	}
	if (!m->sideSet) {
		m->sideSet = true;
		m->circleLeft = initial;
	}
	return m->circleLeft;
}

// A step round the player refused by the world: the walk turns round the
// other way, once per cooldown. Answers whether it turned.
inline bool RingStepRefused(RingState& state, UInt32 actor) {
	RingMember* const m = RingMemberOf(state, actor);
	if (m == nullptr || m->flipCooldown > 0.0f) {
		return false;
	}
	m->sideSet = true;
	m->circleLeft = !m->circleLeft;
	m->flipCooldown = kRingFlipCooldownSeconds;
	return true;
}

// Whether a refused step of this actor's is logged now: once a second per
// actor (a fighter against a wall is refused every frame - the first run's
// forty lines went on one in a second, 2026-10-11).
inline constexpr float kRingRefusalLogSeconds = 1.0f;

inline bool RingRefusalLogged(RingState& state, UInt32 actor) {
	RingMember* const m = RingMemberOf(state, actor);
	if (m == nullptr || m->refusedQuiet > 0.0f) {
		return false;
	}
	m->refusedQuiet = kRingRefusalLogSeconds;
	return true;
}

}  // namespace obvr::game

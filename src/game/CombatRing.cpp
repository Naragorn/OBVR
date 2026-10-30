#include "game/CombatRing.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/AttackTempo.h"
#include "game/BladeContactLogic.h"
#include "game/ActorExceptions.h"
#include "game/GameAddresses.h"
#include "game/NiMath.h"
#include "game/ParryLogic.h"
#include "game/PlayerTeleport.h"
#include "game/SlowApproach.h"
#include "game/WorldPush.h"

namespace obvr::game {
namespace {

constexpr UInt32 kProcessLevelSlot = 0x08;
constexpr UInt32 kIsInCombatSlot = 0x334;      // Actor, as the slow approach calls it
constexpr UInt32 kCombatTargetSlot = 0x338;    // Actor::GetCombatTarget, 0x005E0AF0

// The actor base's level (xOBSE GameForms.h: TESActorBase's actorBaseData at
// +0x24, TESActorBaseData's flags at +0x04, level at +0x0E, minLevel +0x10,
// maxLevel +0x12; kFlag_PCLevelOffset 0x80) - for NPCs and creatures alike.
constexpr UInt32 kActorBaseFlagsOffset = 0x24 + 0x04;
constexpr UInt32 kActorBaseLevelOffset = 0x24 + 0x0E;
constexpr UInt32 kActorBaseMinLevelOffset = 0x24 + 0x10;
constexpr UInt32 kActorBaseMaxLevelOffset = 0x24 + 0x12;
constexpr UInt32 kActorBaseFlagPCLevelOffset = 0x80;

RingState g_ring;
UInt32 g_lines = 40;
UInt32 g_refusedLines = 40;
UInt32 g_pausedLines = 20;    // the marks' paused state lines
UInt32 g_pauseLogLines = 20;  // "the world paused behind a menu"
bool g_wasPaused = false;
UInt32 g_wasTurns = 0;
UInt32 g_wasWaiting = 0;
UInt32 g_seed = 0x2545F491u;
bool g_holdsBlows = true;  // the last step's CombatRingHoldsBlows
bool g_rangedShoot = true;  // and its CombatRingRangedShoot

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
bool LooksLikeObject(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

// A number 0..1 for the turns' lengths: a linear congruential draw, enough
// for a fight's variety.
float Draw() {
	g_seed = g_seed * 1664525u + 1013904223u;
	return static_cast<float>(g_seed >> 8) / 16777216.0f;
}

// The level the game plays an actor at (LeveledLevel), 0 when the base form
// cannot be read. The player's own level is the player base's absolute one.
UInt16 LevelOf(UInt32 actor, SInt32 playerLevel) {
	const UInt32 base = LooksLikeObject(actor) ? Read(actor + addr::kRefBaseFormOffset) : 0;
	if (!LooksLikeObject(base)) {
		return 0;
	}
	const UInt32 flags = Read(base + kActorBaseFlagsOffset);
	const SInt16 level = *reinterpret_cast<const SInt16*>(base + kActorBaseLevelOffset);
	const UInt16 minLevel = *reinterpret_cast<const UInt16*>(base + kActorBaseMinLevelOffset);
	const UInt16 maxLevel = *reinterpret_cast<const UInt16*>(base + kActorBaseMaxLevelOffset);
	return LeveledLevel(level, (flags & kActorBaseFlagPCLevelOffset) != 0, minLevel, maxLevel, playerLevel);
}

SInt32 PlayerLevel(UInt32 player) {
	const UInt32 base = LooksLikeObject(player) ? Read(player + addr::kRefBaseFormOffset) : 0;
	if (!LooksLikeObject(base)) {
		return 1;
	}
	const SInt16 level = *reinterpret_cast<const SInt16*>(base + kActorBaseLevelOffset);
	return level < 1 ? 1 : level;
}

// Whether a pick met something fixed - a wall, a fence, the ground - by the
// blade's own rule (ContactKindOf: fixed and keyframed bodies; clutter, the
// living and water are passed).
bool PickedFixed(const WorldPick& pick) {
	UInt32 body = 0;
	UInt32 motion = 0;
	UInt32 layer = 0;
	return pick.hit && ReadPickBody(pick.collidable, body, motion, layer) &&
	       ContactKindOf(true, motion, layer) == ContactKind::Fixed;
}

bool RayBlocked(const NiPoint3& from, const NiPoint3& to) {
	WorldPick pick;
	return PickWorldSegment(from, to, pick, kHandBodyQuietLayer) && PickedFixed(pick);
}

// The ground under a spot: looked for from above the feet to below them.
bool GroundUnder(float x, float y, float feetZ, float& groundZ) {
	WorldPick pick;
	const NiPoint3 from{x, y, feetZ + kRingGroundUpUnits};
	const NiPoint3 to{x, y, feetZ - kRingGroundDownUnits};
	if (!PickWorldSegment(from, to, pick, kHandBodyQuietLayer) || !PickedFixed(pick)) {
		return false;
	}
	groundZ = pick.point.z;
	return true;
}

// The world asked about a step from the feet at `ap` by (ox, oy)
// (CombatRingLogic.h, "The step probed").
RingStepProbe ProbeRingStep(const float* ap, float ox, float oy) {
	RingStepProbe p;
	const float length = math::Sqrt(ox * ox + oy * oy);
	if (!(length > 1e-3f)) {
		return p;
	}
	const float reach = length + kRingProbeAheadUnits;
	const float ux = ox / length * reach;
	const float uy = oy / length * reach;
	p.blockedLow = RayBlocked(NiPoint3{ap[0], ap[1], ap[2] + kRingProbeLowUnits},
	                          NiPoint3{ap[0] + ux, ap[1] + uy, ap[2] + kRingProbeLowUnits});
	p.blockedHigh = !p.blockedLow && RayBlocked(NiPoint3{ap[0], ap[1], ap[2] + kRingProbeHighUnits},
	                                            NiPoint3{ap[0] + ux, ap[1] + uy, ap[2] + kRingProbeHighUnits});
	float here = 0.0f;
	float there = 0.0f;
	const bool foundHere = GroundUnder(ap[0], ap[1], ap[2], here);
	const bool foundThere = GroundUnder(ap[0] + ox, ap[1] + oy, ap[2], there);
	p.groundFound = foundHere && foundThere;
	p.riseUnits = p.groundFound ? there - here : 0.0f;
	return p;
}

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

UInt32 HighProcessOf(UInt32 actor) {
	const UInt32 process = LooksLikeObject(actor) ? Read(actor + addr::kMobileProcessOffset) : 0;
	const UInt32 levelFn = Slot(process, kProcessLevelSlot);
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	if (levelFn == 0 || reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process), nullptr) != 0) {
		return 0;
	}
	return process;
}

// The walk animating in an actor's five animation slots (CombatRingLogic.h,
// RingLegsWalking), read as the attack watch reads them (game/AttackTempo.cpp;
// xOBSE GameProcess.h and NiNodes.h): the process's anim data at +0x17C, its
// sequences from +0xA0, a sequence's state at +0x44 (1 animating,
// game/WeaponDrawSpeed.h) and its group at +0x68, the group's code at +0x08.
constexpr UInt32 kProcessAnimDataOffset = 0x17C;
constexpr UInt32 kAnimDataSequencesOffset = 0xA0;
constexpr UInt32 kAnimDataSequenceCount = 5;
constexpr UInt32 kSequenceStateOffset = 0x44;
constexpr UInt32 kSequenceAnimGroupOffset = 0x68;
constexpr UInt32 kAnimGroupCodeOffset = 0x08;
constexpr UInt32 kSequenceStateAnimating = 1;

UInt8 LegsGroupOf(UInt32 process) {
	const UInt32 animData = LooksLikeObject(process) ? Read(process + kProcessAnimDataOffset) : 0;
	if (!LooksLikeObject(animData)) {
		return kRingLegsStill;
	}
	for (UInt32 i = 0; i < kAnimDataSequenceCount; ++i) {
		const UInt32 sequence = Read(animData + kAnimDataSequencesOffset + 4 * i);
		if (!LooksLikeObject(sequence) || Read(sequence + kSequenceStateOffset) != kSequenceStateAnimating) {
			continue;
		}
		const UInt32 group = Read(sequence + kSequenceAnimGroupOffset);
		if (!LooksLikeObject(group)) {
			continue;
		}
		const UInt8 code = *reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset);
		if (RingLegsWalking(code)) {
			return code;
		}
	}
	return kRingLegsStill;
}

struct ListNode {
	void* data;
	ListNode* next;
};

struct Seen {
	UInt32 actor = 0;
	UInt32 process = 0;
};

}  // namespace

void StepCombatRing(const CombatRingSettings& settings, float dtSeconds, bool logState, bool worldPaused) {
	g_holdsBlows = settings.holdsBlows;
	g_rangedShoot = settings.rangedShoot;
	if (!settings.enabled) {
		if (g_ring.count != 0) {
			g_ring = RingState{};
			SetRingLegs(nullptr, 0);
		}
		return;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		SetRingLegs(nullptr, 0);
		return;
	}
	// The world paused behind a menu: nothing steps, nothing is placed, no
	// turn runs out (the tester, 2026-10-12: "in pausen menüs scheints du die
	// gegner weiter zu pushen obwohl eig pause sein soll"). The ring placed
	// its steps every frame the camera ran, and behind the pause menu no leg
	// walked, so the whole step was placed - the waiting slid across the
	// ground with the game stopped. A hand script's mark still logs where
	// each one stands, for the harness to see that they held still.
	if (worldPaused) {
		if (logState && g_pausedLines > 0) {
			--g_pausedLines;
			for (UInt32 i = 0; i < g_ring.count; ++i) {
				const RingMember& m = g_ring.member[i];
				if (!LooksLikeObject(m.actor)) {
					continue;
				}
				const auto* const ap = reinterpret_cast<const float*>(m.actor + addr::kRefPositionOffset);
				OBVR_LOG("Combat ring: paused - %08X at %.1f %.1f, %s", m.actor, static_cast<double>(ap[0]),
				         static_cast<double>(ap[1]), m.turn ? "its turn" : "waits");
			}
		}
		if (!g_wasPaused && g_pauseLogLines > 0) {
			--g_pauseLogLines;
			OBVR_LOG("Combat ring: the world paused behind a menu - %u fighter(s) held where they stand", g_ring.count);
		}
		g_wasPaused = true;
		return;
	}
	g_wasPaused = false;
	const auto* const pp = reinterpret_cast<const float*>(player + addr::kRefPositionOffset);
	const NiPoint3 playerAt{pp[0], pp[1], pp[2]};
	const SInt32 playerLevel = PlayerLevel(player);

	RingFighter fighters[kRingMembersMax];
	Seen seen[kRingMembersMax];
	UInt32 count = 0;
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	for (UInt32 visited = 0; node != nullptr && visited < 512 && count < kRingMembersMax; ++visited) {
		if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
			break;
		}
		const UInt32 actor = reinterpret_cast<UInt32>(node->data);
		node = node->next;
		if (actor == 0 || actor == player || !LooksLikeObject(actor)) {
			continue;
		}
		const UInt32 vtable = Read(actor);
		if (vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) {
			continue;
		}
		const UInt32 process = HighProcessOf(actor);
		if (process == 0 || !ActorFightsPlayer(actor, player)) {
			continue;
		}
		// On [Hands] CombatRingExceptions: no ring, no turn, not counted.
		if (ActorOnExceptionList(ExceptionList::CombatRing, actor)) {
			continue;
		}
		const auto* const ap = reinterpret_cast<const float*>(actor + addr::kRefPositionOffset);
		const float dx = ap[0] - playerAt.x;
		const float dy = ap[1] - playerAt.y;
		// A blow in the air by the attack sequences (AttackSwinging), not the
		// process's action: that stays "attack" across a chain of blows.
		fighters[count].actor = actor;
		fighters[count].distanceUnits = math::Sqrt(dx * dx + dy * dy);
		fighters[count].attacking = AttackSwinging(actor);
		fighters[count].level = LevelOf(actor, playerLevel);
		seen[count].actor = actor;
		seen[count].process = process;
		++count;
	}

	const RingVerdict v = StepCombatRing(g_ring, settings, fighters, count, dtSeconds, Draw());

	RingLegs legs[kRingMembersMax];
	UInt32 legCount = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const RingOrder order = RingOrderOf(g_ring, seen[i].actor);
		RingMember* const member = RingMemberOf(g_ring, seen[i].actor);
		if (order == RingOrder::Free) {
			if (member != nullptr) {
				RingStepNone(*member);
			}
			continue;
		}
		legs[legCount].process = seen[i].process;
		legs[legCount].order = static_cast<UInt8>(order);
		// Half go round one way, half the other to begin with: the circle
		// spreads; a wall turns one round (RingStepRefused).
		legs[legCount].left = RingCircleLeft(g_ring, seen[i].actor, ((seen[i].actor >> 4) & 1) != 0);
		// The legs walk round the player by the flags (RingFlags); the ring
		// places only the radial part of the step - a drift out inside the
		// ring, the pull to its line at it - and only on legs that walk
		// (RingMayPlace; "The legs walk; the ring only adds to a walk",
		// CombatRingLogic.h). The world is asked first (ProbeRingStep): the walk
		// round turns the other way before a wall, and a radial step into a
		// wall or a fence, or up onto one, is not taken.
		const auto* const ap = reinterpret_cast<const float*>(seen[i].actor + addr::kRefPositionOffset);
		const float rx = ap[0] - playerAt.x;
		const float ry = ap[1] - playerAt.y;
		const float distance = math::Sqrt(rx * rx + ry * ry);
		float ex = 0.0f;
		float ey = 0.0f;
		const bool walkedKnown = member != nullptr && RingLegsWalked(*member, ap[0], ap[1], ex, ey);
		const float legsUnits = walkedKnown ? math::Sqrt(ex * ex + ey * ey) : 0.0f;
		const UInt8 legsGroup = LegsGroupOf(seen[i].process);
		float leftAtX = ap[0];
		float leftAtY = ap[1];
		float placedUnits = 0.0f;
		bool stepping = false;
		float tx = 0.0f;
		float ty = 0.0f;
		if (RingTangentStep(rx, ry, legs[legCount].left, dtSeconds, tx, ty)) {
			stepping = true;
			// The way round at knee height, a body's half width past the
			// step: a wall or a fence there turns the walk round.
			const float length = math::Sqrt(tx * tx + ty * ty);
			const float reach = length + kRingProbeAheadUnits;
			const bool blocked =
				length > 1e-3f && RayBlocked(NiPoint3{ap[0], ap[1], ap[2] + kRingProbeLowUnits},
				                             NiPoint3{ap[0] + tx / length * reach, ap[1] + ty / length * reach,
				                                      ap[2] + kRingProbeLowUnits});
			if (blocked) {
				if (member != nullptr) {
					++member->stepsRefused;
				}
				const bool turned = RingStepRefused(g_ring, seen[i].actor);
				if (turned) {
					legs[legCount].left = !legs[legCount].left;
				}
				if (g_refusedLines > 0 && RingRefusalLogged(g_ring, seen[i].actor)) {
					--g_refusedLines;
					OBVR_LOG("Combat ring: %08X's walk round meets something fixed at knee height%s", seen[i].actor,
					         turned ? "; it goes round the other way" : "");
				}
			}
		}
		const float radial = distance > 1.0f ? RingRadialStep(distance, order, settings, dtSeconds) : 0.0f;
		if (radial != 0.0f) {
			stepping = true;
			if (!RingMayPlace(legsGroup, radial)) {
				if (member != nullptr) {
					++member->stepsHeld;
				}
			} else {
				const float sx = rx / distance * radial;
				const float sy = ry / distance * radial;
				const RingStepProbe probe = ProbeRingStep(ap, sx, sy);
				if (RingStepClear(probe)) {
					leftAtX = ap[0] + sx;
					leftAtY = ap[1] + sy;
					placedUnits = radial > 0.0f ? radial : -radial;
					PlaceActorAt(reinterpret_cast<void*>(seen[i].actor), NiPoint3{leftAtX, leftAtY, ap[2] + probe.riseUnits});
					if (member != nullptr) {
						++member->stepsPlaced;
					}
				} else {
					if (member != nullptr) {
						++member->stepsRefused;
					}
					if (g_refusedLines > 0 && RingRefusalLogged(g_ring, seen[i].actor)) {
						--g_refusedLines;
						OBVR_LOG("Combat ring: %08X's step %s refused - %s (rise %.0f units)", seen[i].actor,
						         radial > 0.0f ? "out" : "in", RingStepRefusal(probe),
						         static_cast<double>(probe.riseUnits));
					}
				}
			}
		}
		if (member != nullptr) {
			member->legsGroup = legsGroup;
			if (stepping) {
				RingStepTaken(*member, leftAtX, leftAtY, legsUnits, placedUnits, dtSeconds);
			} else {
				RingStepNone(*member);
			}
		}
		++legCount;
	}
	SetRingLegs(legs, legCount);

	if ((v.turnsEnded != 0 || v.turnsGiven != 0 || v.turns != g_wasTurns || v.waiting != g_wasWaiting) &&
	    g_lines > 0) {
		--g_lines;
		// Who got a turn this frame, with their level and the length drawn.
		char given[96] = "";
		UInt32 at = 0;
		for (UInt32 i = 0; v.turnsGiven != 0 && i < g_ring.count; ++i) {
			const RingMember& m = g_ring.member[i];
			if (m.turn && m.turnSeconds == 0.0f && at < sizeof(given) - 32) {
				at += static_cast<UInt32>(std::snprintf(given + at, sizeof(given) - at, " %08X (level %u, %.1f s)", m.actor,
				                                        static_cast<unsigned>(m.level), static_cast<double>(m.turnLength)));
			}
		}
		OBVR_LOG("Combat ring: %u fighting the player - %u with a turn, %u waiting at %.1f m (%u turn(s) ended, %u "
		         "given this frame%s; at once %u, %s)",
		         count, v.turns, v.waiting, static_cast<double>(settings.ringUnits / 70.0f), v.turnsEnded,
		         v.turnsGiven, given, settings.attackersAtOnce, settings.byLevel ? "the weakest first" : "the longest waiting first");
	}
	g_wasTurns = v.turns;
	g_wasWaiting = v.waiting;
	if (logState) {
		// The walls round the player, for a hand script to stand him by one:
		// the nearest fixed thing in the four directions at knee height.
		float walls[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		const float dirs[4][2] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};
		const float reach = 3000.0f;
		for (UInt32 d = 0; d < 4; ++d) {
			WorldPick pick;
			const NiPoint3 from{playerAt.x, playerAt.y, playerAt.z + kRingProbeLowUnits};
			const NiPoint3 to{playerAt.x + dirs[d][0] * reach, playerAt.y + dirs[d][1] * reach, from.z};
			if (PickWorldSegment(from, to, pick, kHandBodyQuietLayer) && PickedFixed(pick)) {
				walls[d] = pick.fraction * reach;
			}
		}
		OBVR_LOG("Combat ring: the walls - +x %.0f, -x %.0f, +y %.0f, -y %.0f units from the player at %.0f %.0f (0: none "
		         "within 3000)",
		         static_cast<double>(walls[0]), static_cast<double>(walls[1]), static_cast<double>(walls[2]),
		         static_cast<double>(walls[3]), static_cast<double>(playerAt.x), static_cast<double>(playerAt.y));
		for (UInt32 i = 0; i < g_ring.count; ++i) {
			RingMember& m = g_ring.member[i];
			UInt32 flags = 0;
			for (UInt32 k = 0; k < count; ++k) {
				if (seen[k].actor == m.actor) {
					flags = *reinterpret_cast<const UInt16*>(seen[k].process + addr::kProcessMovementFlagsOffset);
				}
			}
			float at[2] = {0.0f, 0.0f};
			if (LooksLikeObject(m.actor)) {
				const auto* const ap = reinterpret_cast<const float*>(m.actor + addr::kRefPositionOffset);
				at[0] = ap[0];
				at[1] = ap[1];
			}
			// The legs' own walk and what the ring placed, units a second over
			// the time with a step wanted; the walk animating now (a TESAnimGroup
			// code, 255 none: CombatRingLogic.h).
			const float stepped = m.steppedSeconds > 0.0f ? m.steppedSeconds : 1.0f;
			OBVR_LOG("Combat ring: state - %08X level %u, %.1f m off at %.0f %.0f, %s, order %u (%s), flags %04X, "
			         "legs group %u, attacking %d, %.1f s in turn of %.1f, %.1f s waiting; since the last mark steps "
			         "placed %u, held %u, refused %u, %.1f s stepping - the legs %.0f and the ring %.0f units a second",
			         m.actor, static_cast<unsigned>(m.level), static_cast<double>(m.distanceUnits / 70.0f),
			         static_cast<double>(at[0]), static_cast<double>(at[1]), m.turn ? "its turn" : "waits",
			         static_cast<unsigned>(m.order), m.circleLeft ? "left" : "right", flags,
			         static_cast<unsigned>(m.legsGroup), m.wasAttacking ? 1 : 0, static_cast<double>(m.turnSeconds),
			         static_cast<double>(m.turnLength), static_cast<double>(m.waitingSeconds), m.stepsPlaced,
			         m.stepsHeld, m.stepsRefused, static_cast<double>(m.steppedSeconds),
			         static_cast<double>(m.legsUnits / stepped), static_cast<double>(m.placedUnits / stepped));
			m.stepsPlaced = 0;
			m.stepsHeld = 0;
			m.stepsRefused = 0;
			m.legsUnits = 0.0f;
			m.placedUnits = 0.0f;
			m.steppedSeconds = 0.0f;
		}
	}
}

UInt32 CombatRingFighting() { return g_ring.count; }

bool CombatRingHoldsBlows(UInt32 actor) { return RingHoldsBlows(g_ring, g_holdsBlows, actor); }

bool CombatRingHoldsAttack(UInt32 actor, UInt32 group) {
	return RingHoldsAttack(g_ring, g_holdsBlows, g_rangedShoot, actor, group);
}

bool CombatRingWaiting(UInt32 actor) { return RingHoldsBlows(g_ring, true, actor); }

bool ActorFightsPlayer(UInt32 actor, UInt32 player) {
	const UInt32 combatFn = Slot(actor, kIsInCombatSlot);
	const UInt32 targetFn = Slot(actor, kCombatTargetSlot);
	if (combatFn == 0 || targetFn == 0) {
		return false;
	}
	using CombatFn = UInt8(__fastcall*)(void* actor, void* edx, UInt32 unk);
	using TargetFn = UInt32(__fastcall*)(void* actor, void* edx);
	return (reinterpret_cast<CombatFn>(combatFn)(reinterpret_cast<void*>(actor), nullptr, 1) & 1) != 0 &&
	       reinterpret_cast<TargetFn>(targetFn)(reinterpret_cast<void*>(actor), nullptr) == player;
}

}  // namespace obvr::game

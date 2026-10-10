#include "game/SlowApproach.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/NiMath.h"

namespace obvr::game {
namespace {

constexpr UInt32 kVtableSetFlag = 0x00A71AD8;   // HighProcess vtable +0x2C4
constexpr UInt32 kVtableSetFlags = 0x00A71ADC;  // HighProcess vtable +0x2C8
constexpr UInt32 kSetFlag = 0x00631B90;
constexpr UInt32 kSetFlags = 0x00631B50;
constexpr UInt8 kSetFlagBytes[] = {0x80, 0x7C, 0x24, 0x08, 0x00, 0x8B, 0x44, 0x24, 0x04};
constexpr UInt8 kSetFlagsBytes[] = {0x66, 0x8B, 0x44, 0x24, 0x04, 0xA8, 0x30};
constexpr UInt32 kProcessLevelSlot = 0x08;
constexpr UInt32 kIsInCombatSlot = 0x334;  // Actor, as leading by the hand calls it

bool g_installed = false;

// The processes of the fighters near the player this frame.
constexpr UInt32 kSlowMax = 32;
UInt32 g_slow[kSlowMax] = {};
UInt32 g_slowCount = 0;

// Who was near the frame before (the hysteresis), by actor, and how fast
// each moves (the log's proof of the walk).
constexpr UInt32 kKeptMax = 64;
struct Kept {
	UInt32 actor = 0;
	bool slow = false;
	bool haveLast = false;
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	float speed = 0.0f;  // units a second across the ground, smoothed
};
Kept g_kept[kKeptMax];
UInt32 g_keptCount = 0;
UInt32 g_lines = 24;
UInt32 g_turned = 0;  // runs turned to walks, for the log

// The process of the one held still (HoldStill), 0 nobody; and its actor.
UInt32 g_heldProcess = 0;
UInt32 g_heldActor = 0;

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
bool LooksLikeObject(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

bool Slowed(const void* process) {
	const UInt32 p = reinterpret_cast<UInt32>(process);
	for (UInt32 i = 0; i < g_slowCount; ++i) {
		if (g_slow[i] == p) {
			return true;
		}
	}
	return false;
}

using SetFlagFn = void(__fastcall*)(void* process, void* edx, UInt32 flag, UInt32 on);
using SetFlagsFn = void(__fastcall*)(void* process, void* edx, UInt32 flags);

void __fastcall OnSetFlag(void* process, void* edx, UInt32 flag, UInt32 on) {
	if ((on & 0xFF) != 0 && g_heldProcess != 0 && reinterpret_cast<UInt32>(process) == g_heldProcess) {
		flag = StandStill(static_cast<UInt16>(flag));
	} else if ((on & 0xFF) != 0 && Slowed(process) && (flag & kMovementRun) != 0) {
		flag = WalkInsteadOfRun(static_cast<UInt16>(flag));
		++g_turned;
	}
	reinterpret_cast<SetFlagFn>(kSetFlag)(process, edx, flag, on);
}

void __fastcall OnSetFlags(void* process, void* edx, UInt32 flags) {
	if (g_heldProcess != 0 && reinterpret_cast<UInt32>(process) == g_heldProcess) {
		flags = (flags & 0xFFFF0000u) | StandStill(static_cast<UInt16>(flags));
	} else if (Slowed(process) && (flags & kMovementRun) != 0) {
		flags = (flags & 0xFFFF0000u) | WalkInsteadOfRun(static_cast<UInt16>(flags));
		++g_turned;
	}
	reinterpret_cast<SetFlagsFn>(kSetFlags)(process, edx, flags);
}

Kept& KeptFor(UInt32 actor) {
	for (UInt32 i = 0; i < g_keptCount; ++i) {
		if (g_kept[i].actor == actor) {
			return g_kept[i];
		}
	}
	const UInt32 at = g_keptCount < kKeptMax ? g_keptCount++ : (actor % kKeptMax);
	g_kept[at] = Kept{};
	g_kept[at].actor = actor;
	return g_kept[at];
}

UInt32 HighProcessOf(UInt32 actor) {
	const UInt32 process = LooksLikeObject(actor) ? Read(actor + addr::kMobileProcessOffset) : 0;
	const UInt32 levelFn = Slot(process, kProcessLevelSlot);
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	if (levelFn == 0 || reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process), nullptr) != 0) {
		return 0;  // only a high process has the flags
	}
	return process;
}

struct ListNode {
	void* data;
	ListNode* next;
};

}  // namespace

void InstallSlowApproach() {
	if (Read(kVtableSetFlag) != kSetFlag || Read(kVtableSetFlags) != kSetFlags ||
	    !mem::Verify(kSetFlag, kSetFlagBytes, sizeof(kSetFlagBytes)) ||
	    !mem::Verify(kSetFlags, kSetFlagsBytes, sizeof(kSetFlagsBytes))) {
		OBVR_LOG("Slow approach: HighProcess's movement flag setters are not as read (%08X, %08X) - fighters run as "
		         "in vanilla",
		         Read(kVtableSetFlag), Read(kVtableSetFlags));
		return;
	}
	const UInt32 setFlag = reinterpret_cast<UInt32>(&OnSetFlag);
	const UInt32 setFlags = reinterpret_cast<UInt32>(&OnSetFlags);
	g_installed = mem::SafeWrite(kVtableSetFlag, &setFlag, sizeof(setFlag)) &&
	              mem::SafeWrite(kVtableSetFlags, &setFlags, sizeof(setFlags));
	OBVR_LOG("Slow approach: %s", g_installed ? "HighProcess's movement flag setters rerouted (00A71AD8, 00A71ADC)"
	                                          : "rerouting the movement flag setters failed");
}

void HoldStill(UInt32 actor) {
	if (actor == g_heldActor) {
		return;
	}
	g_heldActor = actor;
	g_heldProcess = g_installed && actor != 0 ? HighProcessOf(actor) : 0;
	if (actor != 0) {
		OBVR_LOG("Slow approach: %08X's legs held%s", actor,
		         g_heldProcess != 0 ? "" : " - not possible, no high process or the setters not rerouted");
	}
}

void StepSlowApproach(bool enabled, float radiusUnits, float dtSeconds, bool logState) {
	g_slowCount = 0;
	// The one held stands, slow approach or not - its process read again
	// each frame (a process can change level).
	if (g_heldActor != 0) {
		const UInt32 vtable = LooksLikeObject(g_heldActor) ? Read(g_heldActor) : 0;
		g_heldProcess = g_installed && (vtable == addr::kVtblCharacter || vtable == addr::kVtblCreature)
		                    ? HighProcessOf(g_heldActor)
		                    : 0;
	}
	if (g_heldProcess != 0) {
		UInt16* const flags = reinterpret_cast<UInt16*>(g_heldProcess + addr::kProcessMovementFlagsOffset);
		*flags = StandStill(*flags);
	}
	if (!g_installed || !enabled) {
		g_keptCount = 0;
		return;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return;
	}
	const auto* const pp = reinterpret_cast<const float*>(player + addr::kRefPositionOffset);
	const NiPoint3 playerAt{pp[0], pp[1], pp[2]};
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
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
		if (process == 0) {
			continue;
		}
		const UInt32 combatFn = Slot(actor, kIsInCombatSlot);
		using CombatFn = UInt8(__fastcall*)(void* actor, void* edx, UInt32 unk);
		const bool inCombat =
			combatFn != 0 && (reinterpret_cast<CombatFn>(combatFn)(reinterpret_cast<void*>(actor), nullptr, 1) & 1) != 0;
		const auto* const ap = reinterpret_cast<const float*>(actor + addr::kRefPositionOffset);
		const NiPoint3 at{ap[0], ap[1], ap[2]};
		const NiPoint3 off = at - playerAt;
		const float distance = math::Sqrt(off.LengthSquared());
		Kept& kept = KeptFor(actor);
		if (kept.haveLast) {
			const NiPoint3 moved = at - kept.last;
			kept.speed = SmoothedGroundSpeed(kept.speed, math::Sqrt(moved.x * moved.x + moved.y * moved.y), dtSeconds);
		}
		kept.last = at;
		kept.haveLast = true;
		const bool was = kept.slow;
		const bool slow = SlowApproachNear(was, inCombat, distance, radiusUnits);
		UInt16* const flags = reinterpret_cast<UInt16*>(process + addr::kProcessMovementFlagsOffset);
		if (slow != was && g_lines > 0) {
			--g_lines;
			OBVR_LOG("Slow approach: %08X %s - %.1f m off, in combat %d, its movement flags %04X, moving %.0f units/s",
			         actor, slow ? "is near - walks" : "is no longer near - runs as it will",
			         static_cast<double>(distance / 70.0f), inCombat ? 1 : 0, *flags, static_cast<double>(kept.speed));
		}
		kept.slow = slow;
		if (logState && inCombat) {
			OBVR_LOG("Slow approach: state - %08X %.1f m off, walks %d, flags %04X (run %d), moving %.0f units/s, %u runs "
			         "turned so far",
			         actor, static_cast<double>(distance / 70.0f), slow ? 1 : 0, *flags,
			         (*flags & kMovementRun) != 0 ? 1 : 0, static_cast<double>(kept.speed), g_turned);
		}
		if (!slow) {
			continue;
		}
		if (g_slowCount < kSlowMax) {
			g_slow[g_slowCount++] = process;
		}
		// One already running before it came near: walking from now.
		if ((*flags & kMovementRun) != 0) {
			*flags = WalkInsteadOfRun(*flags);
			++g_turned;
		}
	}
}

}  // namespace obvr::game

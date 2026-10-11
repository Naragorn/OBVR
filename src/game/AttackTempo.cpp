#include "game/AttackTempo.h"

#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/ParryLogic.h"
#include "game/WeaponDrawSpeed.h"

namespace obvr::game {
namespace {

// Offsets, xOBSE GameProcess.h and NiNodes.h, as the draw speed reads them.
constexpr UInt32 kProcessAnimDataOffset = 0x17C;
constexpr UInt32 kAnimDataClockOffset = 0x94;
constexpr UInt32 kAnimDataSequencesOffset = 0xA0;
constexpr UInt32 kAnimDataSequenceCount = 5;
constexpr UInt32 kSequenceOffsetOffset = 0x48;
constexpr UInt32 kSequenceStateOffset = 0x44;
constexpr UInt32 kSequenceAnimGroupOffset = 0x68;
constexpr UInt32 kAnimGroupCodeOffset = 0x08;
constexpr UInt32 kProcessLevelSlot = 0x08;
constexpr UInt32 kCombatTargetSlot = 0x338;  // Actor::GetCombatTarget, as the ring reads it

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
float& FloatAt(UInt32 a) { return *reinterpret_cast<float*>(a); }
bool Looks(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = Looks(object) ? Read(object) : 0;
	if (!Looks(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

UInt32 HighProcessOf(UInt32 actor) {
	const UInt32 process = Looks(actor) ? Read(actor + addr::kMobileProcessOffset) : 0;
	const UInt32 levelFn = Slot(process, kProcessLevelSlot);
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	if (levelFn == 0 || reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process), nullptr) != 0) {
		return 0;
	}
	return process;
}

struct ListNode {
	void* data;
	ListNode* next;
};

// Fighting the player: a Character or Creature whose combat target is the
// player (Actor vtable +0x338, the ring's reading).
bool FightsThePlayer(UInt32 actor, UInt32 player) {
	if (actor == 0 || actor == player || !Looks(actor) || !Looks(player)) {
		return false;
	}
	const UInt32 vtable = Read(actor);
	if (vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) {
		return false;
	}
	const UInt32 targetFn = Slot(actor, kCombatTargetSlot);
	using TargetFn = UInt32(__fastcall*)(void* actor, void* edx);
	return targetFn != 0 && reinterpret_cast<TargetFn>(targetFn)(reinterpret_cast<void*>(actor), nullptr) == player;
}

float SequenceTime(UInt32 sequence, float clock) {
	using TimeFn = float(__fastcall*)(UInt32 self, void* edx, float clock);
	return reinterpret_cast<TimeFn>(addr::kSequenceTimeAt)(sequence, nullptr, clock);
}

// An attack sequence being followed, with the anim data whose clock it runs on.
struct Followed {
	UInt32 sequence = 0;
	UInt32 animData = 0;
	UInt32 actor = 0;
	UInt8 group = 0;
	bool seen = false;        // in a slot, playing, this frame
	bool haveLastTo = false;  // a key window's end is known
	float lastTo = 0.0f;      // where the last key window fired for it ended
	float lastClock = 0.0f;
	// Its own rate: how far the engine ran its time over the first frame
	// followed, per second of the clock, before the offset was touched -
	// the weapon's speed (MeasuredSpeed). Known after that frame; the
	// measure runs from there.
	float rate = 1.0f;
	bool rateKnown = false;
	float startTime = 0.0f;   // its time when the measure began
	float lastTime = 0.0f;    // its time as last read
	float clockRun = 0.0f;    // the clock that passed while measured
};
constexpr UInt32 kFollowedMax = 16;
Followed g_followed[kFollowedMax];
UInt32 g_followedCount = 0;

// Someone fighting the player, watched for the measure the log gives
// whatever the settings: how long each attack action of theirs lasts on
// their own clock (the process action, as the parry reads it - the swing
// and its follow-through), and how many they begin.
struct Watched {
	UInt32 actor = 0;
	bool seen = false;
	bool attacking = false;
	float startClock = 0.0f;
	float lastClock = 0.0f;
	bool haveLastClock = false;
	UInt32 attacks = 0;
	UInt32 swings = 0;
	// Per slot: the attack sequence playing there last frame, its time as
	// last read, and the swing's run so far - the clock that passed and the
	// sequence's time that ran, which at the game's own speed is the
	// weapon's speed times the clock.
	UInt32 slotSequence[kAnimDataSequenceCount] = {};
	float slotTime[kAnimDataSequenceCount] = {};
	UInt8 slotGroup[kAnimDataSequenceCount] = {};
	float slotClockRun[kAnimDataSequenceCount] = {};
	float slotTimeRun[kAnimDataSequenceCount] = {};
};
constexpr UInt32 kWatchedMax = 16;
Watched g_watched[kWatchedMax];
UInt32 g_watchedCount = 0;
UInt32 g_attackActions = 0;  // attack actions begun by those fighting the player, in all
UInt32 g_swings = 0;         // attack sequences begun by them, in all
UInt32 g_swingsOver = 0;     // of them, over - with their clock and their time summed
float g_swingClock = 0.0f;
float g_swingTime = 0.0f;
UInt32 g_actionLines = 60;

AttackTempoSettings g_settings;
// The style last fetched was for an actor fighting the player: the chance
// multipliers and the hold draws that follow are theirs.
bool g_scope = false;
UInt32 g_attacks = 0;  // attack sequences followed, in all
UInt32 g_holds = 0;    // hold timers drawn in scope, in all
float g_lastHoldMin = 0.0f;
float g_lastHoldMax = 0.0f;
float g_lastHoldDrawn = 0.0f;
UInt32 g_lines = 60;

using KeysFn = void(__fastcall*)(UInt32 group, void* edx, UInt32 actor, float from, float to, UInt32 sequence);
using StyleFloatFn = float(__fastcall*)(UInt32 style, void* edx);
using ActorStyleFn = UInt32(__fastcall*)(UInt32 actor, void* edx);
using RandomFn = float(__cdecl*)(float a, float b);

// The originals, through their trampolines.
StyleFloatFn g_underAttackMult = nullptr;
StyleFloatFn g_notUnderAttackMult = nullptr;
ActorStyleFn g_getCombatStyle = nullptr;
bool g_keysWrapped = false;
UInt32 g_holdDrawsWrapped = 0;
bool g_installTried = false;

Followed* Find(UInt32 sequence) {
	for (UInt32 i = 0; i < g_followedCount; ++i) {
		if (g_followed[i].sequence == sequence) {
			return &g_followed[i];
		}
	}
	return nullptr;
}

Watched* WatchedFor(UInt32 actor) {
	for (UInt32 i = 0; i < g_watchedCount; ++i) {
		if (g_watched[i].actor == actor) {
			return &g_watched[i];
		}
	}
	if (g_watchedCount >= kWatchedMax) {
		return nullptr;
	}
	Watched* w = &g_watched[g_watchedCount++];
	*w = Watched{};
	w->actor = actor;
	return w;
}

// The actor's attack action followed on its own clock - begun, and over -
// and its swings counted: each attack sequence begun in one of its slots.
void WatchAction(UInt32 actor, UInt32 process, UInt32 animData, float clock, bool haveClock) {
	Watched* w = WatchedFor(actor);
	if (w == nullptr) {
		return;
	}
	w->seen = true;
	const SInt32 action = *reinterpret_cast<const SInt16*>(process + addr::kProcessCurrentActionOffset);
	const bool attacking = IsAttackAction(action);
	if (attacking && !w->attacking) {
		w->startClock = haveClock ? clock : 0.0f;
		++w->attacks;
		++g_attackActions;
	} else if (!attacking && w->attacking && haveClock && g_actionLines > 0) {
		--g_actionLines;
		OBVR_LOG("Attack tempo: %08X's attack action lasted %.2f s of its clock (its %u., %u swings so far)", actor,
		         static_cast<double>(clock - w->startClock), w->attacks, w->swings);
	}
	w->attacking = attacking;
	if (!haveClock) {
		w->haveLastClock = false;
		return;
	}
	const float clockStep = w->haveLastClock ? ClockStep(clock, w->lastClock) : 0.0f;
	w->lastClock = clock;
	w->haveLastClock = true;
	for (UInt32 i = 0; i < kAnimDataSequenceCount; ++i) {
		UInt32 sequence = Read(animData + kAnimDataSequencesOffset + 4 * i);
		const UInt32 group = Looks(sequence) ? Read(sequence + kSequenceAnimGroupOffset) : 0;
		const UInt8 code = Looks(group) ? *reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset) : 0;
		if (!Looks(group) || !IsAttackGroup(code) || Read(sequence + kSequenceStateOffset) != kSequenceAnimating) {
			sequence = 0;
		}
		// The same sequence in two slots is one swing.
		for (UInt32 j = 0; j < i && sequence != 0; ++j) {
			if (w->slotSequence[j] == sequence) {
				sequence = 0;
			}
		}
		const float time = sequence != 0 ? SequenceTime(sequence, clock) : 0.0f;
		const bool begins = SwingBegins(w->slotSequence[i], sequence, w->slotTime[i], time);
		if (w->slotSequence[i] != 0 && (begins || sequence == 0)) {
			// The swing that was in the slot is over.
			++g_swingsOver;
			g_swingClock += w->slotClockRun[i];
			g_swingTime += w->slotTimeRun[i];
			if (g_actionLines > 0 && w->slotClockRun[i] > 0.0f) {
				--g_actionLines;
				OBVR_LOG("Attack tempo: %08X's swing (group %u) over - %.2f s of its animation in %.2f s of its clock, "
				         "its time ran x%.2f of the clock",
				         actor, static_cast<unsigned>(w->slotGroup[i]), static_cast<double>(w->slotTimeRun[i]),
				         static_cast<double>(w->slotClockRun[i]),
				         static_cast<double>(MeasuredSpeed(w->slotTimeRun[i], w->slotClockRun[i], 1.0f)));
			}
		}
		if (begins) {
			++w->swings;
			++g_swings;
			w->slotGroup[i] = code;
			w->slotClockRun[i] = 0.0f;
			w->slotTimeRun[i] = 0.0f;
		} else if (sequence != 0 && clockStep > 0.0f) {
			w->slotClockRun[i] += clockStep;
			w->slotTimeRun[i] += time - w->slotTime[i];
		}
		w->slotSequence[i] = sequence;
		w->slotTime[i] = time;
	}
}

// The anim update's key-firing call: a followed sequence's window starts
// where its last one ended.
void __fastcall HookedHandleKeys(UInt32 group, void* edx, UInt32 actor, float from, float to, UInt32 sequence) {
	float start = from;
	Followed* f = Find(sequence);
	if (f != nullptr) {
		start = ContinuousKeyStart(from, to, f->lastTo, f->haveLastTo);
		f->lastTo = to;
		f->haveLastTo = true;
	}
	reinterpret_cast<KeysFn>(addr::kAnimGroupHandleKeys)(group, edx, actor, start, to, sequence);
}

float __fastcall HookedUnderAttackMult(UInt32 style, void* edx) {
	return ScaledChanceMult(g_underAttackMult(style, edx), g_settings.chance, g_scope);
}

float __fastcall HookedNotUnderAttackMult(UInt32 style, void* edx) {
	return ScaledChanceMult(g_notUnderAttackMult(style, edx), g_settings.chance, g_scope);
}

// The scope whenever the mode runs, so the hold draws of those fighting
// the player are counted at the game's own frequency as well; with nothing
// to change the factors are 1 and the draws and multipliers pass through.
UInt32 __fastcall HookedGetCombatStyle(UInt32 actor, void* edx) {
	g_scope = g_settings.active && FightsThePlayer(actor, Read(addr::kPlayerPointer));
	return g_getCombatStyle(actor, edx);
}

float __cdecl HookedHoldDraw(float a, float b) {
	const float drawn = reinterpret_cast<RandomFn>(addr::kRandomBetween)(a, b);
	if (!g_scope) {
		return drawn;
	}
	const float scaled = ScaledHold(drawn, g_settings.hold);
	++g_holds;
	g_lastHoldMin = a;
	g_lastHoldMax = b;
	g_lastHoldDrawn = scaled;
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Attack tempo: a hold of %.2f s drawn between %.2f and %.2f, held for %.2f s (x%.2f)",
		         static_cast<double>(drawn), static_cast<double>(a), static_cast<double>(b),
		         static_cast<double>(scaled), static_cast<double>(g_settings.hold));
	}
	return scaled;
}

// An entry detoured: the bytes checked, the trampoline built into the
// buffer given, the patch written. The original is then the trampoline.
bool InstallEntry(const char* what, UInt32 entry, const UInt8* bytes, UInt32 length, UInt32 replacement,
                  UInt8* trampoline, UInt32 capacity, void*& original) {
	if (!mem::Verify(entry, bytes, length)) {
		OBVR_LOG("Attack tempo: the bytes at %08X differ - %s is left as it is", entry, what);
		mem::ReportForeignCode("Attack tempo", entry);
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, capacity, trampolineAddress, entry, bytes, length) == 0) {
		OBVR_LOG("Attack tempo: the trampoline for %s does not fit", what);
		return false;
	}
	UInt8 patch[16];
	if (mem::BuildEntryPatch(patch, sizeof(patch), entry, replacement, length) != length) {
		OBVR_LOG("Attack tempo: the patch for %s has an unexpected length", what);
		return false;
	}
	original = trampoline;
	if (!mem::SafeWrite(entry, patch, length)) {
		original = nullptr;
		OBVR_LOG("Attack tempo: could not write the entry at %08X for %s", entry, what);
		return false;
	}
	OBVR_LOG("Attack tempo: %s detoured at %08X (trampoline at %08X)", what, entry, trampolineAddress);
	return true;
}

// A direct call re-pointed: the five bytes checked to be `call target`, then
// made `call replacement`.
bool InstallCall(const char* what, UInt32 site, UInt32 target, UInt32 replacement) {
	const UInt32 displacement = mem::CallRelativeDisplacement(site, target);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF),
	                           static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(site, expected, sizeof(expected))) {
		OBVR_LOG("Attack tempo: the call at %08X is not `call %08X` - %s is left as it is", site, target, what);
		mem::ReportForeignCode("Attack tempo", site);
		return false;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), site, replacement) != sizeof(patch) ||
	    !mem::SafeWrite(site, patch, sizeof(patch))) {
		OBVR_LOG("Attack tempo: could not patch the call at %08X for %s", site, what);
		return false;
	}
	OBVR_LOG("Attack tempo: %s - the call at %08X goes through %08X around %08X", what, site, replacement, target);
	return true;
}

void Release(Followed& f, const char* why) {
	if (g_lines > 0 && f.clockRun > 0.0f) {
		--g_lines;
		OBVR_LOG("Attack tempo: %08X's attack (group %u) %s - %.2f s of its animation in %.2f s, its own rate %.2f "
		         "(x%.2f asked, x%.2f read back)",
		         f.actor, static_cast<unsigned>(f.group), why, static_cast<double>(f.lastTime - f.startTime),
		         static_cast<double>(f.clockRun), static_cast<double>(f.rate), static_cast<double>(g_settings.speed),
		         static_cast<double>(MeasuredSpeed(f.lastTime - f.startTime, f.clockRun, f.rate)));
	}
	f = g_followed[--g_followedCount];
}

}  // namespace

void InstallAttackTempo() {
	if (g_installTried) {
		return;
	}
	g_installTried = true;
	// The entry bytes: push ecx / push ebx, push esi, mov esi,ecx, mov eax,[esi]
	// - six bytes, nothing relative in them.
	static const UInt8 kMultEntry[6] = {0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x06};
	static const UInt8 kStyleEntry[6] = {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x06};
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampolines = static_cast<UInt8*>(mem::AllocExecutable(3 * kTrampolineCapacity));
	if (trampolines == nullptr) {
		OBVR_LOG("Attack tempo: no executable memory for the trampolines - the frequency is the game's own");
	} else {
		void* original = nullptr;
		if (InstallEntry("the attack-while-under-attack multiplier", addr::kCombatStyleAttackUnderAttackMult, kMultEntry,
		                 sizeof(kMultEntry), reinterpret_cast<UInt32>(&HookedUnderAttackMult), trampolines,
		                 kTrampolineCapacity, original)) {
			g_underAttackMult = reinterpret_cast<StyleFloatFn>(original);
		}
		original = nullptr;
		if (InstallEntry("the attack-not-under-attack multiplier", addr::kCombatStyleAttackNotUnderAttackMult,
		                 kMultEntry, sizeof(kMultEntry), reinterpret_cast<UInt32>(&HookedNotUnderAttackMult),
		                 trampolines + kTrampolineCapacity, kTrampolineCapacity, original)) {
			g_notUnderAttackMult = reinterpret_cast<StyleFloatFn>(original);
		}
		original = nullptr;
		if (InstallEntry("Actor::GetCombatStyle", addr::kActorGetCombatStyle, kStyleEntry, sizeof(kStyleEntry),
		                 reinterpret_cast<UInt32>(&HookedGetCombatStyle), trampolines + 2 * kTrampolineCapacity,
		                 kTrampolineCapacity, original)) {
			g_getCombatStyle = reinterpret_cast<ActorStyleFn>(original);
		}
	}
	// Without the scope, nothing may be scaled: the multipliers and the hold
	// draws would reach every fight in the game.
	if (g_getCombatStyle == nullptr) {
		OBVR_LOG("Attack tempo: without the scope the frequency is the game's own");
		g_settings.chance = 1.0f;
		g_settings.hold = 1.0f;
	} else {
		if (InstallCall("the hold timer's draw (melee decision A)", addr::kCallHoldTimerDrawA, addr::kRandomBetween,
		                reinterpret_cast<UInt32>(&HookedHoldDraw))) {
			++g_holdDrawsWrapped;
		}
		if (InstallCall("the hold timer's draw (melee decision B)", addr::kCallHoldTimerDrawB, addr::kRandomBetween,
		                reinterpret_cast<UInt32>(&HookedHoldDraw))) {
			++g_holdDrawsWrapped;
		}
	}
	g_keysWrapped = InstallCall("the anim update's key firing", addr::kCallAnimGroupHandleKeys,
	                            addr::kAnimGroupHandleKeys, reinterpret_cast<UInt32>(&HookedHandleKeys));
	if (!g_keysWrapped) {
		OBVR_LOG("Attack tempo: without the key wrapper no attack is slowed - a key could fire twice");
	}
}

void StepAttackTempo(const AttackTempoSettings& settings, bool logState) {
	g_settings = settings;
	if (g_getCombatStyle == nullptr) {
		g_settings.chance = 1.0f;
		g_settings.hold = 1.0f;
	}
	for (UInt32 i = 0; i < g_followedCount; ++i) {
		g_followed[i].seen = false;
	}
	for (UInt32 i = 0; i < g_watchedCount; ++i) {
		g_watched[i].seen = false;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	const bool watch = settings.active && Looks(player);
	const bool follow = watch && settings.enabled && settings.speed != 1.0f && g_keysWrapped;
	if (watch) {
		auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
		using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
		auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
		for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
			if (!Looks(reinterpret_cast<UInt32>(node))) {
				break;
			}
			const UInt32 actor = reinterpret_cast<UInt32>(node->data);
			node = node->next;
			if (!FightsThePlayer(actor, player)) {
				continue;
			}
			const UInt32 process = HighProcessOf(actor);
			if (process == 0) {
				continue;
			}
			const UInt32 animData = Read(process + kProcessAnimDataOffset);
			const bool haveClock = Looks(animData);
			const float clock = haveClock ? FloatAt(animData + kAnimDataClockOffset) : 0.0f;
			WatchAction(actor, process, animData, clock, haveClock);
			if (!follow || !haveClock) {
				continue;
			}
			for (UInt32 i = 0; i < kAnimDataSequenceCount; ++i) {
				const UInt32 sequence = Read(animData + kAnimDataSequencesOffset + 4 * i);
				if (!Looks(sequence)) {
					continue;
				}
				const UInt32 group = Read(sequence + kSequenceAnimGroupOffset);
				if (!Looks(group)) {
					continue;
				}
				const UInt8 code = *reinterpret_cast<const UInt8*>(group + kAnimGroupCodeOffset);
				if (!IsAttackGroup(code) || Read(sequence + kSequenceStateOffset) != kSequenceAnimating) {
					continue;
				}
				Followed* f = Find(sequence);
				if (f == nullptr) {
					if (g_followedCount >= kFollowedMax) {
						continue;
					}
					f = &g_followed[g_followedCount++];
					*f = Followed{};
					f->sequence = sequence;
					f->animData = animData;
					f->actor = actor;
					f->group = code;
					f->lastClock = clock;
					// Its time now is where the last key window ended: the
					// engine's own window reaches exactly this far.
					f->startTime = SequenceTime(sequence, clock);
					f->lastTime = f->startTime;
					f->lastTo = f->startTime;
					f->haveLastTo = true;
					++g_attacks;
					if (g_lines > 0) {
						--g_lines;
						OBVR_LOG("Attack tempo: %08X's attack (group %u, sequence %08X) followed from %.2f s - its time "
						         "runs x%.2f",
						         actor, static_cast<unsigned>(code), sequence, static_cast<double>(f->startTime),
						         static_cast<double>(settings.speed));
					}
				}
				f->seen = true;
			}
		}
	}
	for (UInt32 i = 0; i < g_watchedCount;) {
		if (!g_watched[i].seen) {
			g_watched[i] = g_watched[--g_watchedCount];
			continue;
		}
		++i;
	}
	for (UInt32 i = 0; i < g_followedCount;) {
		Followed& f = g_followed[i];
		if (!follow) {
			Release(f, "let go, the tempo off");
			continue;
		}
		if (!f.seen) {
			Release(f, "over");
			continue;
		}
		const float clock = FloatAt(f.animData + kAnimDataClockOffset);
		const float step = ClockStep(clock, f.lastClock);
		f.lastClock = clock;
		if (step > 0.0f) {
			if (!f.rateKnown) {
				// The engine's own run over this first step, the offset
				// untouched so far: the sequence's rate. The measure begins
				// here.
				const float ran = SequenceTime(f.sequence, clock);
				f.rate = SequenceRate((ran - f.lastTime) / step);
				f.rateKnown = true;
				f.startTime = ran;
			} else {
				f.clockRun += step;
			}
			float& offset = FloatAt(f.sequence + kSequenceOffsetOffset);
			offset = TempoOffset(offset, step, settings.speed, f.rate);
			f.lastTime = SequenceTime(f.sequence, clock);
		}
		++i;
	}
	if (logState) {
		OBVR_LOG("Attack tempo: state - speed x%.2f, chance x%.2f, hold x%.2f (%s); %u followed now, %u attacks "
		         "followed, %u holds drawn in scope, the last %.2f s of %.2f-%.2f; %u watched, %u attack actions "
		         "begun, %u swings begun, %u over in %.1f s of the clock with their time run x%.2f of it; keys "
		         "wrapped %d, hold draws %u",
		         static_cast<double>(g_settings.speed), static_cast<double>(g_settings.chance),
		         static_cast<double>(g_settings.hold), g_settings.enabled ? "on" : "off", g_followedCount, g_attacks,
		         g_holds, static_cast<double>(g_lastHoldDrawn), static_cast<double>(g_lastHoldMin),
		         static_cast<double>(g_lastHoldMax), g_watchedCount, g_attackActions, g_swings, g_swingsOver,
		         static_cast<double>(g_swingClock), static_cast<double>(MeasuredSpeed(g_swingTime, g_swingClock, 1.0f)),
		         g_keysWrapped ? 1 : 0, g_holdDrawsWrapped);
		for (UInt32 i = 0; i < g_followedCount; ++i) {
			const Followed& f = g_followed[i];
			OBVR_LOG("Attack tempo: state - %08X's attack (group %u) at %.2f s, %.2f s of its animation in %.2f s so far",
			         f.actor, static_cast<unsigned>(f.group), static_cast<double>(f.lastTime),
			         static_cast<double>(f.lastTime - f.startTime), static_cast<double>(f.clockRun));
		}
	}
}

}  // namespace obvr::game

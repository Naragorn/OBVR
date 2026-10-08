#include "game/Shove.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/AddressSpace.h"
#include "game/ConsoleLine.h"
#include "game/GameSound.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/PlayerStagger.h"
#include "game/PlayerTeleport.h"

namespace obvr::game {
namespace {

constexpr UInt32 kKnockback = 0x00654420;       // thiscall(process, actor, x, y, z, force), ret 14h
constexpr UInt32 kModDisposition = 0x005E2070;  // thiscall(actor, toward, delta), ret 8
// The hit's knockback through the character proxy (the hit handler at
// 0x0060008A..0x006000AF): 0x008907A0 thiscall(proxy, const NiPoint3*
// distance in game units, float seconds), ret 8 - Havok-scaled and divided by
// the time into the proxy's velocity.
constexpr UInt32 kProxyKnockback = 0x008907A0;
constexpr UInt32 kActorProcessOffset = 0x58;
constexpr UInt32 kProcessKnockedState = 0x11C;  // 0 standing, 2 knocked down
constexpr UInt32 kProcessLevelSlot = 0x08;      // vtable: the process level, 0 high

constexpr UInt8 kStaggerBytes[] = {0x53, 0x56, 0x8B, 0xF1, 0xE8};
constexpr UInt8 kKnockbackBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x48};
constexpr UInt8 kDispositionBytes[] = {0x53, 0x55, 0x56, 0x8B, 0xD9, 0x8B, 0x03, 0x8B, 0x50, 0x40};
constexpr UInt8 kProxyKnockbackBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x40};

bool g_verified = false;
UInt32 g_lines = 20;
// A slap for the mod to finish: the slapped one and where they stand, until
// the camera pass takes it (TakeSlapGrabTap).
UInt32 g_slapTapActor = 0;
// The slap's noise waited for with the mod on (ShoveLogic.h, SlapNoiseWait).
SlapNoiseWait g_slapNoiseWait;
bool g_slapNoiseHaveStart = false;
UInt32 g_slapNoiseActor = 0;
NiPoint3 g_slapNoiseCentre{0.0f, 0.0f, 0.0f};
UInt8 g_slapNoiseModIndex = 0;
UInt32 g_slapNoiseLines = 12;
NiPoint3 g_slapTapCentre{0.0f, 0.0f, 0.0f};
// The victim's reaction to a hit: vtable +0x3A8, 0x005FE380 on
// PlayerCharacter, Character and Creature alike, thiscall(victim, Actor*
// attacker, UInt32 0) ret 8. The hit handler calls it after every landed
// blow (read at 0x006005B1..0x006005BE: `mov eax, [edx+3A8h]; push 0; push
// edi; mov ecx, esi; call eax`, esi the target, edi the attacker). It weighs
// disposition and aggression, forgives a friend the first few hits and
// otherwise raises the alarm (vtable +0x240: an Attack crime, the player's
// assault count, the witnesses) or fights back. Called only when the slot
// holds it and its first bytes are the ones read.
constexpr UInt32 kHitReactionSlot = 0x3A8;
constexpr UInt32 kHitReaction = 0x005FE380;
constexpr UInt8 kHitReactionBytes[] = {0x83, 0xEC, 0x14, 0x56, 0x8B, 0xF1, 0x8B, 0x06};

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

void ReactAsToAHit(UInt32 victim, UInt32 player) {
	const UInt32 vtable = Read(victim);
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + kHitReactionSlot) : 0;
	if (fn != kHitReaction || !mem::Verify(kHitReaction, kHitReactionBytes, sizeof(kHitReactionBytes))) {
		OBVR_LOG("Shove: %08X not told of a hit - its reaction slot holds %08X", victim, fn);
		return;
	}
	using ReactFn = void(__thiscall*)(void* victim, void* attacker, UInt32 zero);
	reinterpret_cast<ReactFn>(fn)(reinterpret_cast<void*>(victim), reinterpret_cast<void*>(player), 0);
}

}  // namespace

bool VerifyShoveAddresses() {
	g_verified = mem::Verify(kStaggerStart, kStaggerBytes, sizeof(kStaggerBytes)) &&
	             mem::Verify(kKnockback, kKnockbackBytes, sizeof(kKnockbackBytes)) &&
	             mem::Verify(kModDisposition, kDispositionBytes, sizeof(kDispositionBytes)) &&
	             mem::Verify(kProxyKnockback, kProxyKnockbackBytes, sizeof(kProxyKnockbackBytes));
	if (!g_verified) {
		OBVR_LOG("Shove: the stagger, knockback or disposition function is not the bytes read - no shoves");
	}
	return g_verified;
}

bool ShoveActor(void* actor, ShoveKind kind, const NiPoint3& fromWorld, const NiPoint3& centre,
                const ShoveSettings& settings, bool byHand) {
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!g_verified || kind == ShoveKind::None || !LooksLikeObject(a) || !LooksLikeObject(player)) {
		return false;
	}
	const char* done = "";
	// A slap in the face with the mod loaded (ShoveLogic.h, SlapTriggersMod):
	// OBVR's own slap below, the mod's slap noise at them by the engine (its
	// SOUN form; the script lines with its form id answered 0), and the mod
	// set off on top by a grab tap with the pick on them, which the camera
	// pass reads (TakeSlapGrabTap) - whatever its own sequence then adds.
	const bool inTheFace = byHand && SlapInTheFace(fromWorld.z, centre.z);
	const UInt8 modIndex = PutItInItsPlaceIndex();
	// The mod's part only with the mod on (ShoveLogic.h, SlapModOn: its
	// master switch and its slap feature, both from its INI).
	double modEnabled = 0.0;
	double modSlapper = 0.0;
	if (modIndex != 0) {
		ReadModQuestVar(modIndex, kPiiiPVarsQuestLow, kPiiiPVarEnabled, modEnabled);
		ReadModQuestVar(modIndex, kPiiiPVarsQuestLow, kPiiiPVarSlapper, modSlapper);
	}
	const bool modOn = SlapModOn(modIndex, modEnabled, modSlapper);
	const bool modsTap = SlapTriggersMod(modOn, kind, inTheFace);
	if (modsTap) {
		g_slapTapActor = a;
		g_slapTapCentre = centre;
		// The noise is the mod's, with its sequence; OBVR's only if that
		// does not start (ShoveLogic.h, StepSlapNoiseWait, stepped by the
		// camera pass through the tap's watch).
		g_slapNoiseWait.pending = true;
		g_slapNoiseWait.encumbranceAtSlap = 0.0f;
		g_slapNoiseHaveStart = ReadPlayerEncumbrance(g_slapNoiseWait.encumbranceAtSlap);
		g_slapNoiseActor = a;
		g_slapNoiseCentre = centre;
		g_slapNoiseModIndex = modIndex;
	}
	if (kind == ShoveKind::Hard) {
		const UInt32 process = Read(a + kActorProcessOffset);
		const UInt32 vtable = LooksLikeObject(process) ? Read(process) : 0;
		const UInt32 levelFn = LooksLikeObject(vtable) ? Read(vtable + kProcessLevelSlot) : 0;
		if (!LooksLikeObject(levelFn)) {
			return false;
		}
		using LevelFn = UInt32(__thiscall*)(void* process);
		const UInt32 level = reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process));
		// One byte (xOBSE GameProcess.h, HighProcess: "SInt8 knockedState;
		// // 11C", sleepState beside it at 11D). Read as a whole word it took
		// its neighbours along - 0xD6140000, 0x65680000 in the tester's log of
		// 2026-09-29 - and refused nearly every hard shove on a standing NPC.
		const UInt32 knocked = *reinterpret_cast<const UInt8*>(process + kProcessKnockedState);
		if (level != 0 || knocked != 0) {
			static UInt32 refusedLines = 10;
			if (refusedLines > 0) {
				--refusedLines;
				OBVR_LOG("Shove: the hard shove refused for %08X - its process level %u, its knocked state %u (both must "
				         "be 0) - a light one instead",
				         a, level, knocked);
			}
			// Too far from the player's attention (not a high process) or
			// already down: a stagger instead.
			kind = ShoveKind::Light;
		} else {
			using KnockFn = void(__thiscall*)(void* process, void* actor, float x, float y, float z, float force);
			reinterpret_cast<KnockFn>(kKnockback)(reinterpret_cast<void*>(process), actor, fromWorld.x, fromWorld.y,
			                                      fromWorld.z, settings.hardForce);
			done = "knocked down";
		}
	}
	if (kind == ShoveKind::Light) {
		using StaggerFn = void(__thiscall*)(void* actor);
		reinterpret_cast<StaggerFn>(kStaggerStart)(actor);
		done = "staggered";
		// And pushed back, as a hit's knockback moves its target.
		using ProxyOfFn = void*(__thiscall*)(void* actor);
		void* const proxy = reinterpret_cast<ProxyOfFn>(kCharacterProxyOf)(actor);
		// Not for a slap in the face: the stagger is its reaction, the push
		// "schubst den npc leider auch bisschen" (the tester, 2026-10-07).
		const NiPoint3 push = inTheFace ? NiPoint3{0.0f, 0.0f, 0.0f} : ShovePush(fromWorld, centre, settings.distance);
		if (LooksLikeObject(reinterpret_cast<UInt32>(proxy)) && push.LengthSquared() > 0.0f &&
		    settings.pushSeconds > 0.0f) {
			using PushFn = void(__thiscall*)(void* proxy, const NiPoint3* distance, float seconds);
			reinterpret_cast<PushFn>(kProxyKnockback)(proxy, &push, settings.pushSeconds);
			done = "staggered and pushed back";
		}
	}
	// A thrown thing (byHand false) costs the player nothing more: the throw
	// was the effort.
	const float fatigue = !byHand ? 0.0f : kind == ShoveKind::Hard ? settings.fatigueHard : settings.fatigueLight;
	const float disposition = ShoveDisposition(settings, kind, inTheFace);
	if (fatigue > 0.0f) {
		SpendPlayerFatigue(fatigue);
	}
	using DispositionFn = void(__thiscall*)(void* actor, void* toward, float delta);
	reinterpret_cast<DispositionFn>(kModDisposition)(actor, reinterpret_cast<void*>(player), -disposition);
	// And the engine hears of it as of a blow.
	const bool asHit = ShoveCountsAsHit(settings, kind);
	if (asHit) {
		ReactAsToAHit(a, player);
	}
	// The noise: without the mod OBVR's own and the game's gasp, run as them
	// (ShoveLogic.h, SlapLines; game/ConsoleLine.h); with the mod loaded
	// neither - the mod's own noise comes with its sequence, and the tester
	// wants that one ("seinen sound nehmen statt unseren").
	UInt32 slapLines = 0;
	bool ownWave = false;
	if (inTheFace && kind == ShoveKind::Light && !modsTap) {
		char lines[4][kSlapLineChars] = {};
		const UInt32 count = SlapLines(0, SlapSound::Own, lines);
		for (UInt32 i = 0; i < count; ++i) {
			if (RequestConsoleLineAs(a, lines[i])) {
				++slapLines;
			}
		}
		ownWave = PlayPluginWave(kSlapWave);
		if (!ownWave) {
			OBVR_LOG("Shove: OBVR's own slap (%s) could not be played - missing next to OBVR.dll?", kSlapWave);
		}
	}
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Shove: %08X %s (from %.0f %.0f %.0f), the player's fatigue -%.0f, its disposition -%.0f%s%s, %u slap "
		         "line(s) queued%s%s",
		         a, done, static_cast<double>(fromWorld.x), static_cast<double>(fromWorld.y),
		         static_cast<double>(fromWorld.z), static_cast<double>(fatigue), static_cast<double>(disposition),
		         asHit ? ", taken as a hit" : "", inTheFace ? ", in the face" : "", slapLines,
		         ownWave ? ", OBVR's own slap played"
		                 : (modsTap ? ", the mod set off by a grab tap - its noise waited for" : ""),
		         modIndex != 0 && !modOn && inTheFace && kind == ShoveKind::Light
		             ? " (the mod loaded but off - sEnabled or sSlapper 0 - so OBVR's own slap)"
		             : "");
	}
	return true;
}

void StepSlapNoise(bool watchOver) {
	if (!g_slapNoiseWait.pending) {
		return;
	}
	float now = 0.0f;
	const bool have = g_slapNoiseHaveStart && ReadPlayerEncumbrance(now);
	const SlapNoiseStep step = StepSlapNoiseWait(g_slapNoiseWait, have, now, watchOver);
	if (step == SlapNoiseStep::ModStarted) {
		if (g_slapNoiseLines > 0) {
			--g_slapNoiseLines;
			OBVR_LOG("Shove: the mod's slap sequence started (the player's encumbrance %.0f -> %.0f) - its noise is "
			         "its own, OBVR plays none",
			         static_cast<double>(g_slapNoiseWait.encumbranceAtSlap), static_cast<double>(now));
		}
	} else if (step == SlapNoiseStep::OurTurn) {
		const bool played =
			PlaySoundFormAt((static_cast<UInt32>(g_slapNoiseModIndex) << 24) | kPiiiPSlapNoiseLow, g_slapNoiseActor,
		                    g_slapNoiseCentre);
		if (g_slapNoiseLines > 0) {
			--g_slapNoiseLines;
			OBVR_LOG("Shove: the mod's slap sequence did not start within the tap's watch (the player's encumbrance "
			         "%.0f -> %.0f%s) - the mod's noise %s by OBVR for the slap dealt",
			         static_cast<double>(g_slapNoiseWait.encumbranceAtSlap), static_cast<double>(now),
			         have ? "" : ", not readable", played ? "played" : "could not be played");
		}
	}
}

bool TakeSlapGrabTap(UInt32& actor, NiPoint3& centre) {
	if (g_slapTapActor == 0) {
		return false;
	}
	actor = g_slapTapActor;
	centre = g_slapTapCentre;
	g_slapTapActor = 0;
	return true;
}

UInt8 PutItInItsPlaceIndex() {
	// The mod's load index: the one load index under which its slap noise
	// (a SOUN at the low id kPiiiPSlapNoiseLow) and its slapped idle token (a
	// CLOT at kPiiiPSlappedTokenLow) both exist, asked of the game's own form
	// table (0x0046B250, cdecl(formId), as Lead.cpp uses it). Read once per
	// session; 0 when no index has them (the mod not loaded).
	constexpr UInt32 kLookupFormById = 0x0046B250;
	// xOBSE GameForms.h's FormType: SOUN 0x0A, CLOT 0x16 (the same table
	// IsHandItemType in NearbyItems.h is written against: APPA 0x13, ARMO
	// 0x14, BOOK 0x15, CLOT 0x16, INGR 0x19 ...).
	constexpr UInt8 kFormTypeSound = 0x0A;
	constexpr UInt8 kFormTypeClothing = 0x16;
	static int s_index = -1;
	if (s_index >= 0) {
		return static_cast<UInt8>(s_index);
	}
	s_index = 0;
	using LookupFn = void*(__cdecl*)(UInt32 formId);
	const auto lookup = reinterpret_cast<LookupFn>(kLookupFormById);
	for (UInt32 index = 1; index < 0xFF; ++index) {
		const UInt32 sound = reinterpret_cast<UInt32>(lookup((index << 24) | kPiiiPSlapNoiseLow));
		const UInt32 token = reinterpret_cast<UInt32>(lookup((index << 24) | kPiiiPSlappedTokenLow));
		if (!LooksLikeObject(sound) || !LooksLikeObject(token)) {
			continue;
		}
		const UInt8 soundType = *reinterpret_cast<const UInt8*>(sound + addr::kFormTypeOffset);
		const UInt8 tokenType = *reinterpret_cast<const UInt8*>(token + addr::kFormTypeOffset);
		if (soundType != kFormTypeSound || tokenType != kFormTypeClothing) {
			continue;
		}
		s_index = static_cast<int>(index);
		OBVR_LOG("Shove: Put it in its Place - Enhanced Grabbing is loaded at index %02X - a slap takes turns with "
		         "its noise and runs its slapped idle",
		         index);
		return static_cast<UInt8>(index);
	}
	return 0;
}

}  // namespace obvr::game

namespace obvr::game {

// Put it in its Place's own state, read from its quest scripts' variables
// (xOBSE GameForms.h: TESQuest, the ScriptEventList* at +0x58, its flags
// at +0x3C with bit 0 active; GameAPI.h: ScriptEventList::m_vars at +0x0C,
// a list of VarEntry {Var*, next}, Var {id, nextEntry, double data}; a
// reference variable holds the form id in the double's low 32 bits). The
// quests and variable ids are from its ESP (SCPT records' SLSD/SCVR).
namespace {
constexpr UInt32 kLookupFormByIdFn = 0x0046B250;
constexpr UInt8 kFormTypeQuest = 0x3B;
constexpr UInt32 kQuestFlagsOffset = 0x3C;
constexpr UInt32 kQuestEventListOffset = 0x58;
constexpr UInt32 kEventListVarsOffset = 0x0C;

UInt32 ModQuest(UInt8 modIndex, UInt32 questLow) {
	if (modIndex == 0) {
		return 0;
	}
	using LookupFn = void*(__cdecl*)(UInt32 formId);
	const UInt32 quest = reinterpret_cast<UInt32>(
		reinterpret_cast<LookupFn>(kLookupFormByIdFn)((static_cast<UInt32>(modIndex) << 24) | questLow));
	if (!LooksLikeObject(quest) || *reinterpret_cast<const UInt8*>(quest + addr::kFormTypeOffset) != kFormTypeQuest) {
		return 0;
	}
	return quest;
}
}  // namespace

bool ModQuestActive(UInt8 modIndex, UInt32 questLow) {
	const UInt32 quest = ModQuest(modIndex, questLow);
	return quest != 0 && (*reinterpret_cast<const UInt8*>(quest + kQuestFlagsOffset) & 1) != 0;
}

bool ReadModQuestVar(UInt8 modIndex, UInt32 questLow, UInt32 varId, double& out) {
	const UInt32 quest = ModQuest(modIndex, questLow);
	if (quest == 0) {
		return false;
	}
	const UInt32 events = Read(quest + kQuestEventListOffset);
	if (!LooksLikeObject(events)) {
		return false;
	}
	UInt32 entry = Read(events + kEventListVarsOffset);
	for (UInt32 guard = 0; guard < 512 && LooksLikeObject(entry); ++guard) {
		const UInt32 var = Read(entry);
		if (LooksLikeObject(var) && Read(var) == varId) {
			out = *reinterpret_cast<const double*>(var + 8);
			return true;
		}
		entry = Read(entry + 4);
	}
	return false;
}

}  // namespace obvr::game

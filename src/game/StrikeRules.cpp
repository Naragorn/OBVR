#include "game/StrikeRules.h"

#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// Read 2026-11-03 in Oblivion.exe (dumpbin /disasm), AttackHandling
// 0x005FEBF0's hand-to-hand branch:
//   0x005FF40C mov edx,[eax+284h]; push 11h; mov ecx,edi; call edx  - the
//              attacker's hand-to-hand, an integer
//   0x005FF418 push eax; call 00547280; add esp,1Ch - the damage, cdecl,
//              seven dwords, the skill's value first, the results written
//              through two of them, nothing on the FPU stack at its ret
//   0x005FF446 push 11h ... call edx; push eax; call 00546BA0; add esp,8 -
//              the power attack's factor of the same skill, cdecl, st0
//   0x006005B1 mov edx,[esi]; mov eax,[edx+3A8h]; push 0; push edi;
//              mov ecx,esi; call eax - the struck one (esi) told of the
//              attacker (edi): 0x005FE380 in the three actor tables,
//              thiscall, ret 8. Jumped to from 0x005FF1BE, 0x005FF1E1,
//              0x00600581, 0x006005A2 at its first byte; nothing jumps inside.
constexpr UInt32 kCallDamage = 0x005FF419;
constexpr UInt32 kDamage = 0x00547280;
constexpr UInt32 kCallPowerFactor = 0x005FF44D;
constexpr UInt32 kPowerFactor = 0x00546BA0;
constexpr UInt32 kReactionSite = 0x006005B1;
constexpr UInt8 kReactionBytes[] = {0x8B, 0x16, 0x8B, 0x82, 0xA8, 0x03, 0x00, 0x00,
                                    0x6A, 0x00, 0x57, 0x8B, 0xCE, 0xFF, 0xD0};
constexpr UInt32 kReactionSlot = 0x3A8;
constexpr UInt32 kActorValueSlot = 0x284;

// The engine's weapon sound (0x006AF880, cdecl, void, nine dwords, the caller
// cleans up; game/MeleeHits.cpp PlaySwingSwish): passed through as raw words.
using WeaponSoundFn = void(__cdecl*)(UInt32, UInt32, UInt32, UInt32, UInt32, UInt32, UInt32, UInt32, UInt32);

struct StrikeHooks {
	bool tried = false;
	bool damageRouted = false;
	bool factorRouted = false;
	bool reactionRouted = false;
	// The one strike being handed over (the engine runs it at once, on this
	// thread): the skill's value, or -1 for the engine's own; the target
	// not to be told.
	SInt32 skillValue = -1;
	UInt32 quietTarget = 0;
	// The thread a slap's engine hit runs on (StrikeRule::quietHit), 0 when
	// none: the weapon sound's detour skips its sound for that thread alone.
	UInt32 quietHitThread = 0;
	WeaponSoundFn weaponSound = nullptr;  // the original, through its trampoline
	UInt32 soundsSkipped = 0;             // during the strike running now
	UInt32 skipLines = 12;
};

StrikeHooks& Hooks() {
	static StrikeHooks s_hooks;
	return s_hooks;
}

using DamageFn = void(__cdecl*)(UInt32, UInt32, UInt32, UInt32, UInt32, UInt32, UInt32);
using FactorFn = float(__cdecl*)(UInt32 skill, UInt32 direction);
using ReactFn = void(__fastcall*)(void* self, void* edx, void* attacker, UInt32 flag);
using ActorValueFn = SInt32(__fastcall*)(void* self, void* edx, UInt32 av);

UInt32 SkillArgument(UInt32 engines) {
	return Hooks().skillValue >= 0 ? static_cast<UInt32>(Hooks().skillValue) : engines;
}

void __cdecl OnDamage(UInt32 skill, UInt32 a1, UInt32 a2, UInt32 a3, UInt32 a4, UInt32 a5, UInt32 a6) {
	reinterpret_cast<DamageFn>(kDamage)(SkillArgument(skill), a1, a2, a3, a4, a5, a6);
}

float __cdecl OnPowerFactor(UInt32 skill, UInt32 direction) {
	return reinterpret_cast<FactorFn>(kPowerFactor)(SkillArgument(skill), direction);
}

// Called as the original was, ecx the struck one and two stack arguments;
// __fastcall with the edx dummy pops the same 8 bytes the thiscall did.
void __fastcall OnReaction(void* target, void* edx, void* attacker, UInt32 flag) {
	if (Hooks().quietTarget != 0 && reinterpret_cast<UInt32>(target) == Hooks().quietTarget) {
		static UInt32 lines = 8;
		if (lines > 0) {
			--lines;
			OBVR_LOG("Strike: %08X not told of the attack - a shove", Hooks().quietTarget);
		}
		return;
	}
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(target);
	const UInt32 react = *reinterpret_cast<const UInt32*>(vtable + kReactionSlot);
	reinterpret_cast<ReactFn>(react)(target, edx, attacker, flag);
}

bool RerouteCall(UInt32 site, UInt32 original, UInt32 replacement, const char* what) {
	const UInt32 displacement = mem::CallRelativeDisplacement(site, original);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF), static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(site, expected, sizeof(expected))) {
		OBVR_LOG("Strike: the call at %08X is not `call %08X` - %s left to hand to hand", site, original, what);
		mem::ReportForeignCode("Strike", site);
		return false;
	}
	UInt8 patch[5];
	return mem::BuildCallSitePatch(patch, sizeof(patch), site, replacement) == sizeof(patch) &&
	       mem::SafeWrite(site, patch, sizeof(patch));
}

bool RerouteReaction() {
	if (!mem::Verify(kReactionSite, kReactionBytes, sizeof(kReactionBytes))) {
		OBVR_LOG("Strike: the hit's reaction at %08X is not as read - a shove gets no engine hit", kReactionSite);
		mem::ReportForeignCode("Strike", kReactionSite);
		return false;
	}
	// push 0; push edi; mov ecx,esi; call OnReaction; five nops - fifteen
	// bytes, as many as the original's.
	UInt8 patch[sizeof(kReactionBytes)] = {0x6A, 0x00, 0x57, 0x8B, 0xCE};
	if (mem::BuildCallSitePatch(patch + 5, 5, kReactionSite + 5, reinterpret_cast<UInt32>(&OnReaction)) != 5) {
		return false;
	}
	for (UInt32 i = 10; i < sizeof(patch); ++i) {
		patch[i] = 0x90;
	}
	return mem::SafeWrite(kReactionSite, patch, sizeof(patch));
}

}  // namespace

void InstallStrikeRules() {
	if (Hooks().tried) {
		return;
	}
	Hooks().tried = true;
	Hooks().damageRouted = RerouteCall(kCallDamage, kDamage, reinterpret_cast<UInt32>(&OnDamage), "the damage");
	Hooks().factorRouted =
		RerouteCall(kCallPowerFactor, kPowerFactor, reinterpret_cast<UInt32>(&OnPowerFactor), "the power factor");
	Hooks().reactionRouted = RerouteReaction();
	OBVR_LOG("Strike: the fist blow's damage %s, its power factor %s, the reaction %s", Hooks().damageRouted ? "rerouted" : "NOT",
	         Hooks().factorRouted ? "rerouted" : "NOT", Hooks().reactionRouted ? "rerouted" : "NOT");
}

bool StrikeCanStayQuiet() { return Hooks().reactionRouted; }

namespace {

// The engine's hit without its sound (StrikeRule::quietHit, a slap):
// AttackHandling plays the hit on flesh through the weapon sound 0x006AF880
// (OBNG's decompile 0x5febf0.c, the calls after the damage). One detour on its
// entry, installed once at the start and never taken out, skips the sound
// while a slap's hit runs on the calling thread (WeaponSoundSkipped) and
// plays it for every other call. Its first seven bytes (`mov eax,[esp+4]`,
// `sub esp,24h`) move whole into the trampoline; the ten read are checked
// first, and anything else there - another plugin's hook - leaves the
// function alone and the slap with the hit's sound.
constexpr UInt32 kWeaponSound = 0x006AF880;
constexpr UInt8 kWeaponSoundBytes[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0xEC, 0x24, 0x53, 0x33, 0xDB};
constexpr UInt32 kWeaponSoundDisplaced = 7;

void __cdecl HookedWeaponSound(UInt32 a, UInt32 b, UInt32 c, UInt32 d, UInt32 e, UInt32 f, UInt32 g, UInt32 h,
                               UInt32 i) {
	if (WeaponSoundSkipped(Hooks().quietHitThread, GetCurrentThreadId())) {
		++Hooks().soundsSkipped;
		return;
	}
	Hooks().weaponSound(a, b, c, d, e, f, g, h, i);
}

}  // namespace

bool WeaponSoundCallable() {
	return Hooks().weaponSound != nullptr || mem::Verify(kWeaponSound, kWeaponSoundBytes, sizeof(kWeaponSoundBytes));
}

bool InstallQuietHitSound() {
	if (Hooks().weaponSound != nullptr) {
		return true;
	}
	if (!mem::Verify(kWeaponSound, kWeaponSoundBytes, sizeof(kWeaponSoundBytes))) {
		OBVR_LOG("Strike: the weapon sound at %08X is not as read (hooked by another plugin?) - a slap keeps the "
		         "fist's hit sound",
		         kWeaponSound);
		mem::ReportForeignCode("Strike: the weapon sound", kWeaponSound);
		return false;
	}
	constexpr UInt32 kCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kCapacity));
	UInt8 patch[8];
	const bool built =
		trampoline != nullptr &&
		mem::BuildEntryTrampoline(trampoline, kCapacity, reinterpret_cast<UInt32>(trampoline), kWeaponSound,
		                          kWeaponSoundBytes, kWeaponSoundDisplaced) != 0 &&
		mem::BuildEntryPatch(patch, sizeof(patch), kWeaponSound, reinterpret_cast<UInt32>(&HookedWeaponSound),
		                     kWeaponSoundDisplaced) == kWeaponSoundDisplaced;
	if (!built) {
		OBVR_LOG("Strike: no trampoline for the weapon sound - a slap keeps the fist's hit sound");
		return false;
	}
	Hooks().weaponSound = reinterpret_cast<WeaponSoundFn>(trampoline);
	if (!mem::SafeWrite(kWeaponSound, patch, kWeaponSoundDisplaced)) {
		Hooks().weaponSound = nullptr;
		OBVR_LOG("Strike: could not write the weapon sound's entry at %08X - a slap keeps the fist's hit sound",
		         kWeaponSound);
		return false;
	}
	OBVR_LOG("Strike: the weapon sound at %08X detoured (trampoline at %08X) - a slap's engine hit plays no sound "
	         "of its own",
	         kWeaponSound, reinterpret_cast<UInt32>(trampoline));
	return true;
}

void BeginStrike(void* player, void* target, const StrikeRule& rule) {
	Hooks().quietHitThread = rule.quietHit && Hooks().weaponSound != nullptr ? GetCurrentThreadId() : 0;
	Hooks().skillValue = -1;
	Hooks().quietTarget = rule.reacts ? 0 : reinterpret_cast<UInt32>(target);
	if (rule.skill == kSkillHandToHand || !Hooks().damageRouted || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(player))) {
		return;
	}
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(player);
	const UInt32 slot = mem::LooksLikeObjectAddress(vtable) ? *reinterpret_cast<const UInt32*>(vtable + kActorValueSlot) : 0;
	if (slot < addr::kTextStart || slot >= addr::kTextEnd) {
		return;
	}
	const SInt32 value = reinterpret_cast<ActorValueFn>(slot)(player, nullptr, rule.skill);
	Hooks().skillValue = value >= 0 ? value : 0;
}

void EndStrike() {
	if (Hooks().quietHitThread != 0 && Hooks().skipLines > 0) {
		--Hooks().skipLines;
		OBVR_LOG("Strike: a slap's engine hit - %u hit sound(s) of the engine left out", Hooks().soundsSkipped);
	}
	Hooks().soundsSkipped = 0;
	Hooks().quietHitThread = 0;
	Hooks().skillValue = -1;
	Hooks().quietTarget = 0;
}

}  // namespace obvr::game

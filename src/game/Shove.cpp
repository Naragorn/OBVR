#include "game/Shove.h"

#include "core/AddressSpace.h"
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
ShoveTally g_tally;

// The assault: the victim's vtable +0x240, thiscall(victim, criminal, UInt8
// flag) ret 8 - what the hit handler's reaction 0x005FE380 calls on a hit it
// takes as an attack (read at 0x005FE789: `push 1; push edi; mov ecx, esi;
// call [edx+240h]`). It is 0x00610930 on Character and PlayerCharacter,
// 0x0060CF60 on Creature; it registers crime type 3, Attack (cs.uesp.net/
// wiki/Crime_Types), with the victim and the criminal, counts the player's
// assaults (player+0x6D4, xOBSE's kMiscStat_Assaults) and lets the witnesses
// react. Called only when the slot holds one of the two, checked each time.
constexpr UInt32 kAlarmAttackSlot = 0x240;
constexpr UInt32 kAlarmAttackCharacter = 0x00610930;
constexpr UInt32 kAlarmAttackCreature = 0x0060CF60;

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

void CommitAssault(UInt32 victim, UInt32 player) {
	const UInt32 vtable = Read(victim);
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + kAlarmAttackSlot) : 0;
	if (fn != kAlarmAttackCharacter && fn != kAlarmAttackCreature) {
		OBVR_LOG("Shove: no assault for %08X - its alarm slot holds %08X, not the attack's", victim, fn);
		return;
	}
	using AlarmFn = void(__thiscall*)(void* victim, void* criminal, UInt32 flag);
	reinterpret_cast<AlarmFn>(fn)(reinterpret_cast<void*>(victim), reinterpret_cast<void*>(player), 1);
	OBVR_LOG("Shove: %08X shoved down too often - an assault (crime type 3) reported", victim);
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

void StepShoveCrime(float dt) { StepShoveTally(g_tally, dt); }

bool ShoveActor(void* actor, ShoveKind kind, const NiPoint3& fromWorld, const NiPoint3& centre,
                const ShoveSettings& settings) {
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!g_verified || kind == ShoveKind::None || !LooksLikeObject(a) || !LooksLikeObject(player)) {
		return false;
	}
	const char* done = "";
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
			if (CountHardShove(g_tally, actor, settings.crimeAfter, settings.crimeWindowSeconds)) {
				CommitAssault(a, player);
			}
		}
	}
	if (kind == ShoveKind::Light) {
		using StaggerFn = void(__thiscall*)(void* actor);
		reinterpret_cast<StaggerFn>(kStaggerStart)(actor);
		done = "staggered";
		// And pushed back, as a hit's knockback moves its target.
		using ProxyOfFn = void*(__thiscall*)(void* actor);
		void* const proxy = reinterpret_cast<ProxyOfFn>(kCharacterProxyOf)(actor);
		const NiPoint3 push = ShovePush(fromWorld, centre, settings.distance);
		if (LooksLikeObject(reinterpret_cast<UInt32>(proxy)) && push.LengthSquared() > 0.0f &&
		    settings.pushSeconds > 0.0f) {
			using PushFn = void(__thiscall*)(void* proxy, const NiPoint3* distance, float seconds);
			reinterpret_cast<PushFn>(kProxyKnockback)(proxy, &push, settings.pushSeconds);
			done = "staggered and pushed back";
		}
	}
	const float fatigue = kind == ShoveKind::Hard ? settings.fatigueHard : settings.fatigueLight;
	const float disposition = kind == ShoveKind::Hard ? settings.dispositionHard : settings.dispositionLight;
	SpendPlayerFatigue(fatigue);
	using DispositionFn = void(__thiscall*)(void* actor, void* toward, float delta);
	reinterpret_cast<DispositionFn>(kModDisposition)(actor, reinterpret_cast<void*>(player), -disposition);
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Shove: %08X %s (from %.0f %.0f %.0f), the player's fatigue -%.0f, its disposition -%.0f", a, done,
		         static_cast<double>(fromWorld.x), static_cast<double>(fromWorld.y), static_cast<double>(fromWorld.z),
		         static_cast<double>(fatigue), static_cast<double>(disposition));
	}
	return true;
}

}  // namespace obvr::game

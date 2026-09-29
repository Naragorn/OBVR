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
constexpr UInt32 kActorProcessOffset = 0x58;
constexpr UInt32 kProcessKnockedState = 0x11C;  // 0 standing, 2 knocked down
constexpr UInt32 kProcessLevelSlot = 0x08;      // vtable: the process level, 0 high

constexpr UInt8 kStaggerBytes[] = {0x53, 0x56, 0x8B, 0xF1, 0xE8};
constexpr UInt8 kKnockbackBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x48};
constexpr UInt8 kDispositionBytes[] = {0x53, 0x55, 0x56, 0x8B, 0xD9, 0x8B, 0x03, 0x8B, 0x50, 0x40};

bool g_verified = false;
UInt32 g_lines = 20;

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

}  // namespace

bool VerifyShoveAddresses() {
	g_verified = mem::Verify(kStaggerStart, kStaggerBytes, sizeof(kStaggerBytes)) &&
	             mem::Verify(kKnockback, kKnockbackBytes, sizeof(kKnockbackBytes)) &&
	             mem::Verify(kModDisposition, kDispositionBytes, sizeof(kDispositionBytes));
	if (!g_verified) {
		OBVR_LOG("Shove: the stagger, knockback or disposition function is not the bytes read - no shoves");
	}
	return g_verified;
}

bool ShoveActor(void* actor, ShoveKind kind, const NiPoint3& fromWorld, const ShoveSettings& settings) {
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
		if (level != 0 || Read(process + kProcessKnockedState) != 0) {
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

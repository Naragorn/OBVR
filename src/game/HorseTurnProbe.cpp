#include "game/HorseTurnProbe.h"

#include <intrin.h>

#include "core/AddressSpace.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/PlayerTeleport.h"

namespace obvr::game {
namespace {

constexpr UInt32 kTurnBy = 0x00659B90;
// push esi; mov esi,ecx; push edi; mov edi,[esi] - whole instructions, nothing relative.
const UInt8 kEntry[] = {0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x3E};

using TurnByFn = void(__fastcall*)(void* actor, void* edx, float delta);
TurnByFn g_original = nullptr;
UInt32 g_lines = 60;

void __fastcall OnTurnBy(void* actor, void* edx, float delta) {
	const UInt32 horse = PlayerMountedHorse();
	if (horse != 0 && reinterpret_cast<UInt32>(actor) == horse && g_lines > 0) {
		--g_lines;
		OBVR_LOG("Horse turn probe: the horse %08X turned by %.4f rad (%.1f deg) from %08X", horse,
		         static_cast<double>(delta), static_cast<double>(delta * 57.2957795f),
		         reinterpret_cast<UInt32>(_ReturnAddress()));
	}
	g_original(actor, edx, delta);
}

}  // namespace

void InstallHorseTurnProbe() {
	if (g_original != nullptr) {
		return;
	}
	if (!mem::Verify(kTurnBy, kEntry, sizeof(kEntry))) {
		OBVR_LOG("Horse turn probe: the turn-by entry at %08X is not as read - no probe", kTurnBy);
		return;
	}
	UInt8* const trampoline = static_cast<UInt8*>(mem::AllocExecutable(16));
	if (trampoline == nullptr) {
		return;
	}
	const UInt32 trampolineSize =
		mem::BuildEntryTrampoline(trampoline, 16, reinterpret_cast<UInt32>(trampoline), kTurnBy, kEntry, sizeof(kEntry));
	UInt8 patch[sizeof(kEntry)]{};
	const UInt32 patchSize =
		mem::BuildEntryPatch(patch, sizeof(patch), kTurnBy, reinterpret_cast<UInt32>(&OnTurnBy), sizeof(kEntry));
	if (trampolineSize == 0 || patchSize != sizeof(patch)) {
		return;
	}
	g_original = reinterpret_cast<TurnByFn>(trampoline);
	if (!mem::SafeWrite(kTurnBy, patch, sizeof(patch))) {
		g_original = nullptr;
		return;
	}
	OBVR_LOG("Horse turn probe: the actors' turn-by at %08X goes through OBVR while the player is mounted", kTurnBy);
}

}  // namespace obvr::game

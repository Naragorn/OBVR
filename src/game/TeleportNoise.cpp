#include "game/TeleportNoise.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kSetMovementFlagsSlot = 0x00A71ADC;  // HighProcess vtable 0x00A71814 + 0x2C8
constexpr UInt32 kSetMovementFlags = 0x00631B50;
constexpr UInt32 kWalkForward = 0x0101;  // forward (1) and walk (0x100), as HandleInput sets them

using SetFlagsFn = void(__fastcall*)(void* process, void* edx, UInt32 flags);

volatile bool g_on = false;
bool g_installed = false;
bool g_saidAdded = false;

void __fastcall SetFlagsWithNoise(void* process, void* edx, UInt32 flags) {
	if (g_on) {
		const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
		if (mem::LooksLikeObjectAddress(player) &&
		    process == *reinterpret_cast<void* const*>(player + 0x58)) {
			flags |= kWalkForward;
			if (!g_saidAdded) {
				g_saidAdded = true;
				OBVR_LOG("Teleport: heard like walking - the movement flags read %04X while it moves",
				         flags & 0xFFFF);
			}
		}
	}
	reinterpret_cast<SetFlagsFn>(kSetMovementFlags)(process, edx, flags);
}

}  // namespace

void InstallTeleportNoise() {
	const UInt8 expected[4] = {0x50, 0x1B, 0x63, 0x00};
	if (!mem::Verify(kSetMovementFlagsSlot, expected, sizeof(expected))) {
		OBVR_LOG("Teleport: the movement-flags slot at %08X is not the game's own - a teleport "
		         "stays silent",
		         kSetMovementFlagsSlot);
		return;
	}
	const UInt32 thunk = reinterpret_cast<UInt32>(&SetFlagsWithNoise);
	if (!mem::SafeWrite(kSetMovementFlagsSlot, &thunk, sizeof(thunk))) {
		OBVR_LOG("Teleport: could not write the movement-flags slot - a teleport stays silent");
		return;
	}
	g_installed = true;
	OBVR_LOG("Teleport: the movement-flags setter goes through OBVR (for TeleportMakesNoise)");
}

void SetTeleportNoise(bool on) {
	g_on = on && g_installed;
	// Once, the first time it is asked for: which table the player's process
	// really has, and what its setter slot holds.
	static bool s_described = false;
	if (on && !s_described) {
		s_described = true;
		const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
		const UInt32 process =
			mem::LooksLikeObjectAddress(player) ? *reinterpret_cast<const UInt32*>(player + 0x58) : 0;
		const UInt32 vtable =
			mem::LooksLikeObjectAddress(process) ? *reinterpret_cast<const UInt32*>(process) : 0;
		const UInt32 slot = vtable != 0 ? *reinterpret_cast<const UInt32*>(vtable + 0x2C8) : 0;
		OBVR_LOG("Teleport: noise asked for - the player's process %08X, its table %08X, setter "
		         "slot %08X (%s)",
		         process, vtable, slot,
		         slot == reinterpret_cast<UInt32>(&SetFlagsWithNoise) ? "OBVR's" : "not OBVR's");
	}
}

}  // namespace obvr::game

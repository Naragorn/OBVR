#include "game/TeleportNoise.h"

#include <intrin.h>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kGetMovementFlagsSlot = 0x00A71AD4;  // HighProcess vtable 0x00A71814 + 0x2C0
constexpr UInt32 kGetMovementFlags = 0x006285A0;      // mov ax,[ecx+1FCh]; ret
constexpr UInt32 kWalkForward = 0x0101;  // forward (1) and walk (0x100), as HandleInput sets them

using GetFlagsFn = UInt32(__fastcall*)(void* process, void* edx);

volatile bool g_on = false;
bool g_installed = false;
bool g_saidAdded = false;

UInt32 __fastcall GetFlagsWithNoise(void* process, void* edx) {
	UInt32 flags = reinterpret_cast<GetFlagsFn>(kGetMovementFlags)(process, edx);
	if (g_on && TeleportNoiseReadsFlags(reinterpret_cast<UInt32>(_ReturnAddress()))) {
		const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
		if (mem::LooksLikeObjectAddress(player) &&
		    process == *reinterpret_cast<void* const*>(player + 0x58)) {
			flags = (flags & 0xFFFF0000u) | ((flags | kWalkForward) & 0xFFFFu);
			if (!g_saidAdded) {
				g_saidAdded = true;
				OBVR_LOG("Teleport: heard like walking - the detection reads the movement flags as %04X "
				         "while it moves; the player's own flags, which move it, are left alone",
				         flags & 0xFFFF);
			}
		}
	}
	return flags;
}

}  // namespace

void InstallTeleportNoise() {
	const UInt8 expected[4] = {0xA0, 0x85, 0x62, 0x00};
	if (!mem::Verify(kGetMovementFlagsSlot, expected, sizeof(expected))) {
		OBVR_LOG("Teleport: the movement-flags getter slot at %08X is not the game's own - a teleport "
		         "stays silent",
		         kGetMovementFlagsSlot);
		return;
	}
	const UInt32 thunk = reinterpret_cast<UInt32>(&GetFlagsWithNoise);
	if (!mem::SafeWrite(kGetMovementFlagsSlot, &thunk, sizeof(thunk))) {
		OBVR_LOG("Teleport: could not write the movement-flags getter slot - a teleport stays silent");
		return;
	}
	g_installed = true;
	OBVR_LOG("Teleport: the movement-flags getter goes through OBVR for the detection's two reads "
	         "(for TeleportMakesNoise)");
}

void SetTeleportNoise(bool on) {
	g_on = on && g_installed;
	// Once, the first time it is asked for: which table the player's process
	// really has, and what its getter slot holds.
	static bool s_described = false;
	if (on && !s_described) {
		s_described = true;
		const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
		const UInt32 process =
			mem::LooksLikeObjectAddress(player) ? *reinterpret_cast<const UInt32*>(player + 0x58) : 0;
		const UInt32 vtable =
			mem::LooksLikeObjectAddress(process) ? *reinterpret_cast<const UInt32*>(process) : 0;
		const UInt32 slot = vtable != 0 ? *reinterpret_cast<const UInt32*>(vtable + 0x2C0) : 0;
		OBVR_LOG("Teleport: noise asked for - the player's process %08X, its table %08X, getter "
		         "slot %08X (%s)",
		         process, vtable, slot,
		         slot == reinterpret_cast<UInt32>(&GetFlagsWithNoise) ? "OBVR's" : "not OBVR's");
	}
}

}  // namespace obvr::game

#pragma once

#include "core/Types.h"

namespace obvr::game {

// A teleport heard like walking ([Locomotion] TeleportMakesNoise).
//
// The engine's detection hears a moving player through the process's
// movement flags: the caller of the detection core (0x005463F0) reads them
// through HighProcess vtable +0x2C0 (0x006285A0, `mov ax,[ecx+1FCh]; ret`)
// and counts the player moving on bits 0x0F (the call returning to
// 0x005F66DD), running on 0x200 (returning to 0x005F66FD). A teleport sets
// none of them, so the engine hears nothing (research 2026-09-27).
//
// The first version added "forward" and "walk" to the flags themselves,
// through the setter PlayerCharacter::HandleInput writes them with. But the
// same flags move the player: for the half second the noise lasted after
// the landing, the character walked forward on its own (the tester,
// 2026-09-28: "kommt der teleport aber dann noch ein zusätzlicher schritt
// des chars nach vorne ... das erzeugt nausea").
//
// So now only the detection's reading is changed: while asked to, the
// getter's slot in HighProcess's table (0x00A71AD4) goes through OBVR,
// which adds "forward" and "walk" (0x0101) to what it answers - for the
// player's process, and only when called from those two reads (the return
// address). The flags themselves, and everything that moves the player by
// them, are left alone.
void InstallTeleportNoise();
void SetTeleportNoise(bool on);

// The return addresses of the detection's two reads of the flags.
inline constexpr UInt32 kDetectionReadsMoving = 0x005F66DD;
inline constexpr UInt32 kDetectionReadsRunning = 0x005F66FD;

// Whether a read of the flags returning to `returnAddress` is the
// detection's. Pure, for teleport_noise_test.
inline bool TeleportNoiseReadsFlags(UInt32 returnAddress) {
	return returnAddress == kDetectionReadsMoving || returnAddress == kDetectionReadsRunning;
}

}  // namespace obvr::game

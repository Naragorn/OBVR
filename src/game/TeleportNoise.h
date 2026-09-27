#pragma once

namespace obvr::game {

// A teleport heard like walking ([Locomotion] TeleportMakesNoise).
//
// The engine's detection hears a moving player through the process's
// movement flags: the caller of the detection core (0x005463F0) reads them
// through HighProcess vtable +0x2C0 and counts the player moving on bits
// 0x0F, running on 0x200 (0x5F66CA-0x5F66FD). PlayerCharacter::HandleInput
// (0x00671620) rebuilds them from the controls every frame and writes them
// through the setter at vtable +0x2C8 (0x00631B50, `mov [ecx+1FCh],ax; ret
// 4`, called at 0x006725A5). A teleport sets none of them, so the engine
// hears nothing (research 2026-09-27).
//
// While asked to, the setter's slot in HighProcess's table (0x00A71ADC)
// goes through OBVR, which adds "forward" and "walk" (0x0101) to what the
// player's own controls set - the sneak bit stays as it is. Only the
// player's process is touched; every other actor passes straight through.
void InstallTeleportNoise();
void SetTeleportNoise(bool on);

}  // namespace obvr::game

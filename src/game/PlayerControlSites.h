#pragma once

// The sites the player's controls are held to the walk through while they
// run under a menu (game/MenuPause.h; docs/inventory-tablet-spec.md 4.6),
// beside the gate and the wheel's read in game/GameAddresses.h
// (kPlayerControlsIsMenuModeSite, kPlayerWheelReadSite). Read in the
// disassembly of 1.2.0.416 (objdump, 2026-11-18).

#include "core/Types.h"

namespace obvr::addr {

// The same reader for the mouse's x and y (axes 1 and 2), two calls before
// the wheel's, in the player's input (objdump of 1.2.0.416, 2026-11-18):
//
//   00671953  push 1; mov ecx,edi
//   0067195B  call 00403190            ; the mouse's x - the body's turn
//   00671960  push 2; mov ecx,edi
//   00671966  call 00403190            ; the mouse's y - the look up and down
//
// Under the tablet the laser's cursor steps go on while the sticks walk, so
// these answer 0 there (PlayerControlsReadAxis): the steps are the menu's.
inline constexpr UInt32 kPlayerMouseXReadSite = 0x0067195B;
inline constexpr UInt32 kPlayerMouseYReadSite = 0x00671966;

// OSInputGlobals::GetControlState (thiscall: the control's index and the
// kind of read on the stack, ret 8; the control map at +0x1B7E, three
// devices of 29 bytes; read in the disassembly 2026-11-18). The player's
// input asks it for every control - the attack (4) at 0066E02E and in
// 0065E9xx, activate, block, cast, jump, the menu key. Its first seven bytes
// (mov eax,[esp+4]; cmp eax,1Dh) are whole and hold nothing relative: the
// entry is detoured so that, while the player's controls run under a menu
// (between the gate kPlayerControlsIsMenuModeSite and
// kPlayerControlsScopeEndSite), only the walk is read (PlayerControlReadUnderMenu).
inline constexpr UInt32 kGetControlState = 0x00403520;

// Where every path from the controls gate meets again, the player's update
// done (the gate's menu branch jumps here, 0040DC75; so do the update's own
// exits 0040DCCB/DCDA/DCEA/DD17/DD4A):
//
//   0040DD53  mov ecx,[00B333CC]
//   0040DD59  mov esi,[ecx+DC]
//   0040DD5F  E8 FC B1 16 00   call 00578F60   ; IsMenuMode, for 0055FA50
//
// This IsMenuMode call is pointed at OBVR's, which ends the span in which
// the player's control reads are held back and gives vanilla's answer.
inline constexpr UInt32 kPlayerControlsScopeEndSite = 0x0040DD5F;

// The player's anim data while the F1-F4 menus show their figure:
// PlayerCharacter::GetAnimData (its vtable 0x00A73A0C, slot 0x164,
// 0x0065D720) answers the pointer at +0x5DC when it is set, else the first-
// or third-person one:
//
//   0065D720  mov eax,[ecx+5DC]; test eax,eax; jne ret
//   0065D72A  cmp byte [ecx+588],0 ... mov eax,[ecx+5CC] / jmp 004D8370
//
// The menus set +0x5DC to their figure's anim data (0x00664C40 steps it).
// The player's movement animation (0x005FCAB0, called from the player's input
// at 0x00672E0D) and the process's UpdateMovement (0x0063C730, vtable slot
// 0x2CC, called at 0x006737EE) both take the anim data from that getter, so
// under the menus the walk was played on the figure, which stays idle, and
// the player's Move got a zero step (measured 2026-11-19: the walk group
// played on 22496FE0, not the third-person 1DA2F75C; the Move trace's step
// 0.000 with the walking flags). Both calls lie inside the player's update,
// between the controls gate and kPlayerControlsScopeEndSite.
inline constexpr UInt32 kPlayerMenuAnimDataOffset = 0x5DC;

// The anim data's play of a sequence (0x00474530, thiscall: the sequence,
// its group, its slot; ret 0Ch) asks IsMenuMode at three sites that, in a
// menu, treat the player's third-person skeleton - the anim data whose root
// (+4) is the player's own node, PlayerCharacter 0x00660110(0) -> 0x00612220
// -> the reference's +0x3C - as a picture of one pose (objdump of 1.2.0.416,
// 2026-11-26):
//
//   004745FE  call 00578F60   ; a sequence already playing in the slot: in
//                             ;   a menu it is started again, else kept
//   0047462C  call 00578F60   ; in a menu: slots 4, 0, 1, 2 and 3 cleared
//                             ;   (0x00470FC0 sets each group to 0xFF), then
//                             ;   the one played is set
//   0047478A  call 00578F60   ; in a menu: no blend from the slot's last
//
// The player's update never runs in a vanilla menu. Under a walked menu it
// does, and with a torch in the off hand its arm's group (0x21, slot 2) is
// played after the walk's (slot 0) and clears it. UpdateMovement (0x0063C730)
// takes the step from the first-person anim data only while its slot-0 group
// equals the third-person one's (0x0063C7D6..0x0063C7EE: 0x00470720(0) of
// each, +0x3C), else from the third-person anim data - so the walk read a
// zero step (measured 2026-11-26: first-person 0x0007 with 4.8 units of root
// motion a frame, third-person 0x00FF and none; without the torch both
// 0x0007). The fourth site (0047488D) is about the F1-F4 figure's own node
// (+0x5D8) and stays vanilla's.
inline constexpr UInt32 kPlayerSkeletonMenuModeSites[] = {0x004745FE, 0x0047462C, 0x0047478A};
}  // namespace obvr::addr

#pragma once

#include "core/Types.h"

// Fixed addresses in Oblivion.exe 1.2.0.416 (image base 0x00400000).
//
// Every address here was disassembled from the actual binary and is
// documented below with the evidence. None of it is guessed, and nothing was
// taken from another source without checking it first.

namespace obvr::addr {

// End of the vanilla camera calculation.
//
// The relevant block reads:
//
//   0066BE1B  mov  ebx, [esp+0x14]              ; camera object
//   0066BE1F  cmp  word ptr [ebx+0xB6], 0       ; node list empty?
//   0066BE27  ja   0066BE2D
//   0066BE29  xor  eax, eax
//   0066BE2B  jmp  0066BE35
//   0066BE2D  mov  edx, [ebx+0xB0]              ; node list
//   0066BE33  mov  eax, [edx]                   ; eax = CameraNode (NiAVObject*)
//   0066BE35  mov  ecx, [esp+0x38]
//   0066BE39  mov  edx, [esp+0x3C]
//   0066BE3D  mov  [eax+0x54], ecx              ; localTransform.pos.x
//   0066BE40  mov  ecx, [esp+0x40]
//   0066BE44  mov  [eax+0x58], edx              ; localTransform.pos.y
//   0066BE47  mov  [eax+0x5C], ecx              ; localTransform.pos.z
//   0066BE4A  cmp  word ptr [ebx+0xB6], 0
//   0066BE52  ja   0066BE58
//   0066BE54  xor  eax, eax
//   0066BE56  jmp  0066BE60
//   0066BE58  mov  edx, [ebx+0xB0]
//   0066BE5E  mov  eax, [edx]                   ; eax = CameraNode
//   0066BE60  lea  edi, [eax+0x30]              ; localTransform.rot
//   0066BE63  mov  ecx, 9
//   0066BE68  lea  esi, [esp+0x60]
//   0066BE6C  rep  movsd                        ; 9 DWORDs = NiMatrix33
//   0066BE6E  <-- kHookCameraUpdate
//
// From 0x0066BE6E onwards the camera's position and rotation are settled and
// eax still holds the CameraNode: exactly the point where OBVR wants to lay
// the head rotation on top.
//
// The write targets [eax+0x54] and [eax+0x30] also establish the NiAVObject
// layout in GameTypes.h.
inline constexpr UInt32 kHookCameraUpdate = 0x0066BE6E;

// The original instruction overwritten at kHookCameraUpdate:
//
//   0066BE6E  66 83 BB B6 00 00 00 00   cmp word ptr [ebx+0xB6], 0
//
// Eight bytes, so there is room for a 5-byte jmp. The control flow that
// follows is faithfully rebuilt inside the trampoline:
//
//   0066BE76  ja   0066BE7C     -> kHookCameraUpdateResumeTaken
//   0066BE78  xor  ecx, ecx
//   0066BE7A  jmp  0066BE84     -> kHookCameraUpdateResumeEmpty
inline constexpr UInt32 kHookCameraUpdatePatchSize = 8;
inline constexpr UInt32 kHookCameraUpdateResumeTaken = 0x0066BE7C;
inline constexpr UInt32 kHookCameraUpdateResumeEmpty = 0x0066BE84;

// Shortly after the hook the game calls, on the CameraNode:
//
//   0066BE84  fldz
//   0066BE86  push 0
//   0066BE88  push ecx                          ; ecx = CameraNode
//   0066BE89  fstp [esp]
//   0066BE8C  call 00707370
//
// and 0x00707370 dispatches through vtable slot 0x64:
//
//   007073A7  mov  eax, [esi]
//   007073A9  mov  edx, [eax+0x64]
//   007073B0  call edx
//
// Slot 0x64 / 4 = index 25 = NiAVObject::UpdateSelectedDownwardPass.
//
// Which yields the decisive point for OBVR: a scene graph update pass still
// runs after the hook. OBVR therefore has to modify localTransform rather
// than worldTransform - the world transform gets recomputed from
// parent * local regardless.
inline constexpr UInt32 kUpdateSelectedDownwardPass = 0x00707370;

// Pointer to the player. Established by 0x0066C580 (ToggleCamera), which
// writes to [ecx+0x588] - the isThirdPerson flag in PlayerCharacter.
inline constexpr UInt32 kPlayerPointer = 0x00B333C4;
inline constexpr UInt32 kPlayerIsThirdPersonOffset = 0x588;

// Expected game version. OBSE reports it as oblivionVersion.
inline constexpr UInt32 kOblivionVersion_1_2_416 = 0x010201A0;

}  // namespace obvr::addr

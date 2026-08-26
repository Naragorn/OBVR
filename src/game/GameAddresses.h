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

// Pointer to NiDX9Renderer, the object that owns Oblivion's Direct3D 9
// device. This is where 0.1.0 has to start: the camera hook works on the
// scene graph and has never touched the renderer, but OpenVR takes a texture
// and only the device can produce one.
//
// Two independent sources, which is the standard this file holds addresses
// to. The address comes from OBGEv2's Nodes/NiDX9Renderer.cpp, inside a
// namespace named v1_2_416 - the same build OBVR targets:
//
//   mov eax,0x00B3F928
//   mov eax,[eax]
//
// The offset comes from xOBSE's obse/obse/NiRenderer.h, which lays out
// NiDX9Renderer with "IDirect3DDevice9 * device; // 280" and asserts the
// struct's size and two of its offsets at compile time.
//
// Checked against the binary before adoption, as everything here is: the
// encoding of "mov eax, [0x00B3F928]" - A1 28 F9 B3 00 - appears 41 times in
// Oblivion.exe. A five byte sequence does not occur 41 times by chance, and
// a global read that often is one the renderer is genuinely reached through.
inline constexpr UInt32 kRendererPointer = 0x00B3F928;
inline constexpr UInt32 kRendererDeviceOffset = 0x280;

// Expected game version. OBSE reports it as oblivionVersion.
inline constexpr UInt32 kOblivionVersion_1_2_416 = 0x010201A0;

// Whether a menu is up: the main menu, a loading screen, an inventory, the
// ESC menu, a dialogue. A function rather than a flag, and nullary.
//
// Two sources, as everything here has. xOBSE names the address in GameAPI.cpp
// as the target of its _IsMenuMode function pointer for 1.2.0.416. The bytes
// at that address in Oblivion.exe say the same thing independently:
//
//   00578F60  push 1; push 0; call 00582160    <- InterfaceManager singleton,
//   00578F6C  add esp,8; test eax,eax; jz +2A     the address xOBSE also names
//   00578F70  push 1; push 0; call 00582160
//   00578F7C  add esp,8; cmp dword [eax+1C],0; jz +18
//   00578F82  push 1; push 0; call 00582160
//   00578F8B  xor ecx,ecx; add esp,8
//   00578F90  cmp byte [eax+8],1; setne cl; mov al,cl; ret
//   00578F9A  xor al,al; ret
//
// A function that reaches the interface manager three times and returns a
// byte is the one being described. The ret takes no argument, so it is
// nullary and the calling convention does not matter.
//
// Why OBVR wants it: without it, "is a menu up" has to be guessed from
// whether the camera hook ran this frame - and in a menu Oblivion still draws
// the world behind the menu on some frames and not others. The guess
// therefore flips back and forth, and with it the whole presentation: one
// frame the world fills the headset, the next a small flat rectangle hangs in
// black. That is the flicker seen when opening the ESC menu.
inline constexpr UInt32 kIsMenuMode = 0x00578F60;

// Where Oblivion keeps d3d9.dll and the Direct3DCreate9 it looked up in it.
//
// This exists because Oblivion.exe does not import d3d9.dll at all - the
// import table lists d3dx9_27.dll and thirteen others, and no d3d9. It loads
// it by hand, at 00761DF0:
//
//   00761DF0  push esi; xor esi,esi
//   00761DF3  cmp [00B42154],esi; jnz +5E
//   00761DFB  mov eax,[00B42158]           <- already resolved? then done
//   00761E00  test eax,eax; jnz +29
//   00761E04  push "D3D9.DLL"
//   00761E09  call [00A28118]              <- LoadLibraryA, from the IAT
//   00761E11  mov [00B42150],eax           <- the module handle
//   00761E18  push "Direct3DCreate9"
//   00761E1D  push eax
//   00761E1E  call [00A2811C]              <- GetProcAddress, from the IAT
//   00761E26  mov [00B42158],eax           <- the function, cached here
//
// The two IAT slots agree with the import table read separately, which is
// what makes this two sources rather than one reading.
//
// So an import hook on Direct3DCreate9 can never fire: there is no import to
// replace. GetProcAddress is imported, and is hooked instead. The cached
// pointer is the second way in, for the case where the lookup has already
// happened by the time OBVR loads - and it is validated before being written,
// by resolving Direct3DCreate9 independently and requiring the same value.
inline constexpr UInt32 kD3D9Module = 0x00B42150;
inline constexpr UInt32 kDirect3DCreate9Pointer = 0x00B42158;

}  // namespace obvr::addr

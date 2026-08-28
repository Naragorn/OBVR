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

// The per-frame clock. xOBSE's g_timeInfo points at a TimeInfo structure at
// 0x00B33E90 whose float at +0x0C is the seconds the last frame took - the
// delta every time-driven update in the frame advances by. The second world
// render of a dual frame must not advance anything: animation controllers
// ticking twice per frame were the player's long-standing stutter, and the
// NPC head-aim re-running each walk is the sideways helmets. The scene hook
// zeroes this for the second render and restores it after - guarded by a
// plausibility check at runtime (a frame delta reads between zero and one),
// which is the second source for this address.
inline constexpr UInt32 kFrameSecondsAddress = 0x00B33E9C;

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

// PlayerCharacter::SetDialogCamera - the function behind the dialogue zoom,
// called when a conversation starts (with the NPC) and again when it ends
// (with null), each time starting the camera transition whose distance
// fDlgFocus sets. In a headset the zoom itself never shows, because OBVR owns
// the camera - but the transition is still spent, and it arrives as a dead
// pause on the way out of every conversation.
//
// Two sources, as everything here has. TESReloaded (llde/TESReloaded10,
// Framework/Oblivion/Base.h) names Hooks::SetDialogCamera = 0x0066C6F0 as
// __thiscall (PlayerCharacter*, Actor*, float, UInt8), and its camera mode
// detours it without ever calling the original - which is exactly the "no
// zoom at all" that mod is known for. The bytes in this machine's 1.2.0.416
// say the same thing independently:
//
//   0066C6F0  sub esp,18; push ebp
//   0066C6F4  mov ebp,[esp+20]        <- the first stack argument (the Actor)
//   0066C6F8  test ebp,ebp
//   0066C6FA  push esi; mov esi,ecx   <- __thiscall
//   0066C6FD  jz +527                 <- null Actor takes the ending path
//   0066C703  fld1; fcomp [esp+28]    <- the float argument against 1.0
//   ...       and at +1AD the body reads dword [00B14F10] - the fDlgFocus
//             setting's value slot, found from its name string: 00B14F14
//             holds the pointer to "fDlgFocus" at 00A7409C, and the float
//             before it holds 2.1, the setting's documented default.
//
// A function with that signature, split on a null Actor, reading fDlgFocus,
// at the very address TESReloaded names, is the one being described. OBVR
// patches its first bytes to ret 0Ch - three dword arguments, callee-cleaned
// under __thiscall - so neither transition ever starts.
inline constexpr UInt32 kSetDialogCamera = 0x0066C6F0;

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

// The function that draws one frame of the world: culling, both scene graph
// passes, water, and the image space shaders, but not the 2D layer - menus
// and HUD are drawn later, on a different path. __thiscall, one argument (a
// BSRenderedTexture*, null on the ordinary world pass).
//
// Two sources. Oblivion Reloaded's RenderHook.cpp names it kRender for this
// exact build, and detours it. The bytes agree independently, in three ways:
//
//   * inside it, at 0040CCD3 and 0040CE48, are the only two calls in the
//     whole binary to 0070C0B0 that render a scene graph on the world path -
//     the function Oblivion Reloaded names RenderObject, and which begins
//     with mov ecx,[00B3F928], the renderer global this project has already
//     verified twice over
//   * at 0040CF6E is the single call in the whole binary to 007B48E0, the
//     image space shaders (HDR) - so the picture is finished, tone mapping
//     included, when this function returns
//   * at 0040C95F it reads the scene graph at 00B333CC and walks the same
//     node list ([eax+0xB6] count, [eax+0xB0] list, first entry) that the
//     camera hook site walks - the CameraNode - and copies its position
//     (+0x54) into two follower nodes before drawing
//
// The third point matters beyond verification: Render re-reads the camera
// node's position itself, each call, to place the sky and LOD roots. A second
// call with the camera moved therefore keeps everything consistent without
// further help.
//
// Called from three places: 0040D41B (a menu wants the world in a texture),
// 0040D658 (the ordinary world pass, texture null), 00411CBF (the save game
// screenshot). Only the ordinary pass is drawn twice; the argument and a
// once-per-frame guard tell them apart.
//
// The entry reads
//
//   0040C830  push -1              6A FF
//   0040C832  push 0x9AA163        68 63 A1 9A 00
//   0040C837  mov eax,fs:[0]       (the SEH frame; not moved)
//
// so the first seven bytes are two whole instructions with nothing relative
// in them, which is what the entry detour relocates.
inline constexpr UInt32 kRenderScene = 0x0040C830;
inline constexpr UInt32 kRenderSceneEntryLength = 7;

// NiAVObject::UpdateSelectedDownwardPass - recomputes world transforms from
// parent * local, downward from the given node. __thiscall on the node, two
// arguments: a float time and an int flags, both observed as zero.
//
// Two sources. The camera hook site itself: immediately after the hooked
// instruction the game calls it on the CameraNode it has just written -
//
//   0066BE84  fldz
//   0066BE86  push 0
//   0066BE88  push ecx
//   0066BE89  fstp dword ptr [esp]        <- (0.0f, 0)
//   0066BE8C  call 00707370               <- ecx = CameraNode
//
// and Render at 0040C9A5/0040C9F0 makes the identical call, with identical
// arguments, on the two follower nodes it has just repositioned. HANDOFF
// section 3 records the same address a third way, as the vtable dispatch that
// overwrites worldTransform after the hook - which is why the hook writes
// localTransform.
//
// Why OBVR calls it: moving the camera to the second eye between the two
// render passes edits localTransform, exactly as the camera hook does, and
// this is the call the game itself uses to make the world transform follow.
inline constexpr UInt32 kUpdateNodeTransforms = 0x00707370;

// The function that draws the 2D layer: HUD, menus, dialogues and loading
// screens. __thiscall on the InterfaceManager singleton, one argument (a
// rendered texture, null on the ordinary path), ret 4.
//
// Two sources, twice over. Oblivion Reloaded's RenderHook.cpp hooks a call
// site inside it (its kRenderInterface, 0x0057F3F3) for this exact build.
// The bytes agree, and they agree through addresses this project has already
// verified independently:
//
//   * its callers fetch `this` through 0x00582160 - the InterfaceManager
//     singleton getter that kIsMenuMode calls three times - and the wrapper
//     at 0x00579260 checks the same [manager+0x1C] field IsMenuMode checks
//   * at 0057F2C3 it draws the menu scene graph through 0x0070C0B0, the same
//     RenderObject the world passes use, with the camera at [scenegraph+0xDC]
//     - the offset kSceneGraphCameraOffset already confirmed in the game
//   * at 0057F3A0 it calls 0x00701970, the SetCameraViewProj OBGEv2 names
//
// Every route to the 2D layer in the whole binary funnels through this one
// function - four call sites, all wrappers deciding when. Full walk in
// HANDOFF, "The 2D pass is located".
//
// One property that matters to the redirect: it begins the *default* render
// target group from inside itself (clear flags 6 - depth and stencil, not
// colour, which is why menus sit on the world instead of on black). So a
// wrapper cannot simply set a target first; the substitution happens at the
// device's SetRenderTarget while the pass runs.
//
// The entry reads
//
//   0057F170  push -1              6A FF
//   0057F172  push 0x9BEAE6        68 E6 EA 9B 00
//   0057F177  mov eax,fs:[0]       (the SEH frame; not moved)
//
// - the same seven-byte relocatable shape as kRenderScene.
inline constexpr UInt32 kRenderInterface = 0x0057F170;
inline constexpr UInt32 kRenderInterfaceEntryLength = 7;

// The handle of the loading thread Oblivion runs while a cell streams in,
// and the reason a frame can end with no 2D layer on it at all.
//
// The wrapper that owns the interface pass, 00579260, guards it three ways:
// the interface manager must exist, its [+1Ch] must be set, and 0040FDA0
// must answer false. That last one reads this global, and when it is not
// null asks GetExitCodeThread whether the thread is still 0x103 -
// STILL_ACTIVE:
//
//   0040FDA1  mov eax,ds:[00B33434]
//   0040FDA6  test eax,eax
//   0040FDA8  jne 0040FDAE          ; null -> false, the interface draws
//   0040FDB3  call ds:[00A280E8]    ; GetExitCodeThread(handle, &code)
//   0040FDBB  cmp dword ptr [esp],103h
//   0040FDC2  sete al               ; still running -> true
//
// and 00579289 turns that true into a jump straight past the interface
// pass. So while this handle names a living thread, kRenderInterface is
// never called - which is what a pass with no draws and no clears in it
// looks like from the outside. Read only, and only to log: the value is a
// diagnosis, never something OBVR writes.
inline constexpr UInt32 kLoadingThreadHandle = 0x00B33434;


// The menu stack the interface pass branches on at 0057F358, and the object
// whose [+18h] decides which of the two draw calls it makes:
//
//   0057F358  cmp word ptr ds:[00B1397A],6
//   0057F360  jbe 0057F376          ; -> 005903E0
//   0057F362  mov eax,ds:[00B13974]
//   0057F367  mov ecx,[eax+18h]
//   0057F36A  cmp ecx,ebx
//   0057F36C  je 0057F376           ; -> 005903E0
//   0057F36F  call 0058FBA0
//
// Both arms draw, so this branch cannot be why nothing is drawn - it is
// logged to say which of the two paths the pass took, so a gate that closed
// can be looked for in the right one.
inline constexpr UInt32 kMenuStackCount = 0x00B1397A;
inline constexpr UInt32 kMenuStackRoot = 0x00B13974;

}  // namespace obvr::addr

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

// MobileObject::process, from xOBSE's GameObjects.h where it is commented
// "BaseProcess * process; // 058". PlayerCharacter inherits it through Actor,
// so this counts from the player pointer above - which is the same anchoring
// the rotation at 0x20 and isThirdPerson at 0x588 already rest on, both of
// them right in the running game for weeks.
inline constexpr UInt32 kMobileProcessOffset = 0x058;

// The byte behind BaseProcess::GetWeaponOut - whether the actor is in combat
// stance, weapon or spell readied.
//
// READ DIRECTLY RATHER THAN CALLED, and that is the whole point of this
// constant existing. GetWeaponOut is a virtual at vtable index 0xBE, and
// calling it would mean trusting that index: a wrong one calls some other
// virtual, and the neighbouring entries take arguments and set things. That
// fails by corrupting the game. Reading a byte at a wrong offset fails by
// returning a wrong byte, which shows up as a crosshair that appears at the
// wrong moment - visible, harmless, and easy to correct.
//
// Both the offset and what sits behind the call come from the same file read
// two independent ways. xOBSE's GameProcess.h declares
//
//   virtual UInt8 GetWeaponOut(void) = 0;    // 0xBE
//   virtual UInt8 SetWeaponOut(UInt8 out) = 0;
//
// and the vtable analysis table at the top of that same file, which was built
// from disassembly rather than from the declarations, has for that index:
//
//   // 0BE  0  8  retn0  <-  <-  get unk114  <-
//   // 0BF  1  x  null   <-  <-  set unk114  <-
//
// Zero arguments, an 8-bit return, and a plain read of unk114 on
// MiddleHighProcess - which HighProcess inherits, and the player always has a
// high process. The getter and the field agree, and so do the two halves of
// the file.
//
// It is still checked before it is believed: the field is a boolean, so
// anything but 0 or 1 means this offset is not what this build has, and the
// answer is then "no idea" rather than a number.
inline constexpr UInt32 kProcessWeaponOutOffset = 0x114;

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

// The working copy of the screen size that Oblivion's whole 2D reads, found
// by disassembling this very binary rather than quoted from anyone.
//
// (The NiDX9Renderer's own width/height at +0xA58/+0xA5C were the first
// suspect and were measured in game already holding the believed size - real
// fields, wrong lever.)
//
// The evidence, all from dumpbin /disasm of Oblivion.exe 1.2.0.416:
//
//   * The UI normalization UESP describes - height fixed at 960, or width at
//     1280 in portrait - exists at 0x57D7A0/0x57D7F0: fild [0x00B06C4C],
//     fild [0x00B06C50], compare, divide, multiply by 960.0. The float
//     constants 960.0f/1280.0f each occur exactly once in .rdata, which is
//     what made the functions findable.
//   * Those two integers are written in exactly ONE place in the whole
//     executable, 0x4983AC/0x4983B2, inside the window-creation function:
//     copied from [0x00B06C5C]/[0x00B06C64] - and those are the iSize
//     SettingInfo objects, proven by the name pointers beside them reading
//     "iSize W:Display" and "iSize H:Display" in the image.
//   * Some ninety reads spread over the interface code (0x57xxxx around the
//     2D pass), the window/display code (0x498xxx) and the input side
//     (0x682xxx, 0x5DF1B7's cursor-range compares).
//
// So: settings -> copied once at window creation -> read everywhere. The INI
// is saved from the settings themselves, never from this copy, which is what
// makes the copy patchable where the settings are not (see the ban in
// IniSettings.h). Rewritten under Render.UiFollowsFrameSize, after the
// window and display-mode decisions have consumed the game's own numbers,
// and only when it still reads exactly the asked-for size.
inline constexpr UInt32 kUiScreenWidthCopy = 0x00B06C4C;
inline constexpr UInt32 kUiScreenHeightCopy = 0x00B06C50;

// A dead end, recorded so it is not walked twice: the cursor-movement
// function at 0x57E7C0 calls a renderer getter (0x403190, returning
// [this+0x1B20/0x1B24/0x1B28] for axis 1/2/3) inside its position math,
// which read like a size-based conversion of the sprite position. It is not.
// Measured in game: the getter answers 0 for both axes - the three fields
// are set together in one place (0x40433C, same value into all three) and
// stay zero here - so the whole term multiplies to nothing in vanilla.
// Redirecting those calls to the screen-size copy (commit 40d7e13) armed the
// inert term instead: the cursor node walked off screen and the sprite
// vanished. The mouse offset between sprite and hit test under a raised copy
// comes from somewhere else, still unfound.

// The InterfaceManager singleton pointer, for the read-only cursor probe
// that hunts that unfound source. Two sources: xOBSE's GameAPI.cpp calls a
// GetSingleton at 0x582160, and this binary's disassembly of that function
// reads the pointer from [0x00B3A6E0] (creating a 0x134-byte object into it
// when null). Field offsets, from xOBSE's GameAPI.h (STATIC_ASSERTed there)
// and this binary's cursor-movement function 0x57E7C0: +0x1C the cursor
// Tile*, +0x88 altActiveTile, +0x98 activeTile; the movement function writes
// the cursor position as floats at +0x20/+0x24/+0x28 and a derived triple at
// +0x2C/+0x30/+0x34, and reaches the cursor tile's render node through
// tile+0x24, whose NiAVObject translation sits at +0x54. The probe only ever
// reads, and only behind null checks.
// The tile-under-cursor search - what decides the hover highlight, and with
// it what a click activates. Called from the InterfaceManager update
// (0x582406, right after GetSingleton at 0x582160) with the cursor position
// it reads from the manager's own pixel fields (+0x2C/+0x34, clamped
// against the screen-size copy inside), and answered by a scene-graph pick
// (0x70D300 on the ui scene at [manager+0xDC]). The pick maps the pixels
// through the renderer's camera geometry, not through the copy - measured
// with a cursor ladder: identical spacing to the drawn items, the centre
// half the height difference lower, and re-asserting the believed viewport
// around the search is what moves the zones. Detoured at entry so the
// search runs under the believed viewport and everything downstream of it -
// highlight and click alike - answers in the drawn space.
inline constexpr UInt32 kFindTileAtCursor = 0x00581390;

// Where that pick turns pixels into camera coordinates - and the one place
// the wrong size enters. 0x70D325 (the pick's only normalization call, and
// this function's only caller) passes the cursor pixels here; the function
// divides x by the renderer's width getter and y by its height getter
// (vtable calls through [[0x00B3F928]], slots 0x4C/0x50 on the size source
// its [renderer+0x20C] flag selects) and hands back 0..1 for the camera's
// port and frustum test. The pixels live in the screen-size copy's space,
// so with the copy raised the division is by the wrong height - the
// measured hover offset. Detoured at entry to divide by the believed size
// instead; inert while belief and frame agree.
inline constexpr UInt32 kPickNormalizePoint = 0x00701540;

inline constexpr UInt32 kInterfaceManagerPointer = 0x00B3A6E0;
inline constexpr UInt32 kInterfaceCursorTileOffset = 0x1C;
inline constexpr UInt32 kInterfaceCursorPosOffset = 0x20;
inline constexpr UInt32 kInterfaceCursorDerivedOffset = 0x2C;
inline constexpr UInt32 kInterfaceAltActiveTileOffset = 0x88;
inline constexpr UInt32 kInterfaceActiveTileOffset = 0x98;
inline constexpr UInt32 kTileRenderNodeOffset = 0x24;
inline constexpr UInt32 kNiTranslateOffset = 0x54;

// Expected game version. OBSE reports it as oblivionVersion.
inline constexpr UInt32 kOblivionVersion_1_2_416 = 0x010201A0;

// The engine's INI settings, alive in memory: the IniSettingCollection
// singleton object - the object itself at this address, not a pointer to it.
// Its layout, from xOBSE's obse/GameAPI.h: vtable at +0, the INI file's path
// at +4, and the setting list starting inline at +0x10C as {SettingInfo*,
// next*} entries whose SettingInfo is {value union, name pointer}.
//
// One documented source, from xOBSE's obse/GameAPI.cpp:
//
//   static const UInt32 g_IniSettingCollection = 0x00B07BF0;
//
// and its GetIniSetting walks exactly the list described above - code this
// installation runs, since xOBSE 22.13 is what loads OBVR. The second source
// is the same runtime validation the cached Direct3DCreate9 pointer gets: the
// object is only trusted after its vtable points into the executable image
// and its path field reads as an .ini path, and a setting is only written
// after its name matches exactly and its current value is the very number the
// game is asking CreateDevice for. A wrong address cannot pass those checks
// except by holding the right structure, in which case it is not wrong.
inline constexpr UInt32 kIniSettingCollection = 0x00B07BF0;
inline constexpr UInt32 kIniSettingListOffset = 0x10C;

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
// fDlgFocus sets. OBVR patches its first bytes to ret 0Ch - three dword
// arguments, callee-cleaned under __thiscall - so neither transition ever
// starts.
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
// at the very address TESReloaded names, is the one being described.
//
// The intervention here went through three shapes, each correcting the last:
// a bare ret 0Ch (cut everything), then holding fDlgFocus at 15 in memory
// (the transition ran, going nowhere), then the ret again - and the ret
// turned out to cut one thing too many. SetDialogCamera is ALSO what flips a
// third-person player into first person for the conversation and back after
// it, and that half was wanted. So the entry now jumps to a shim of OBVR's
// own that does the flip through ToggleCamera below and nothing else: no
// transition, no zoom, the vanilla point-of-view dance kept.
inline constexpr UInt32 kSetDialogCamera = 0x0066C6F0;

// PlayerCharacter::ToggleCamera - the game's own "put the player in first or
// third person", one byte argument, 1 meaning first person.
//
// Three sources for once. TESReloaded (Framework/Oblivion/Base.h) names
// ToggleCamera = 0x0066C580 and calls it __thiscall with a byte; xOBSE
// (obse/GameObjects.cpp) implements PlayerCharacter::TogglePOV(bool
// bFirstPerson) as ThisStdCall(0x0066C580, this, bFirstPerson), and its
// command documentation fixes the meaning: "Passing 1 enables first person
// view, 0 enables third person". And this file already leaned on the
// function once: the kPlayerPointer comment below records that 0x0066C580
// writes the isThirdPerson flag at +0x588, which is how that offset was
// established.
inline constexpr UInt32 kToggleCamera = 0x0066C580;

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

// g_worldSceneGraph - the pointer to the world's SceneGraph (a NiNode
// subclass), with its camera and its culling process beside it.
//
// Two sources. xOBSE's headers name the global and the two offsets
// (SceneGraph::camera at 0xDC, SceneGraph::cullingProcess at 0xE4). The
// second is this machine's runtime: across a whole session the three read
// back as stable, plausible heap pointers - scene 1812BBA8, camera 187E424C,
// culling 187E4EE8 - identical on world frames and menu frames alike, and the
// culling process's first word is a vtable in the executable's read-only data
// (00A7E610), which a wrong offset would not produce.
//
// What they were read to answer, and the answer: whether Oblivion takes the
// world away while a pause menu is up. It does not. Every one of these fields
// is unchanged between a frame the engine renders and a frame it refuses to,
// so a live background is not blocked by a missing scene - see the dead end
// below for where the emptiness actually comes from.
inline constexpr UInt32 kWorldSceneGraphPointer = 0x00B333CC;
inline constexpr UInt32 kSceneGraphCameraOffset = 0xDC;
inline constexpr UInt32 kSceneGraphCullingOffset = 0xE4;

// DEAD END, measured 2026-08-30: NiCullingProcess + 0x08 is NOT a pointer to
// the culled-geometry list the renderer consumes. xOBSE's headers put a
// NiCulledGeoList there, and the reasoning that follows from it - "the list
// is empty while a menu is up, fill it and the world draws" - is the obvious
// next step and it is wrong. The field reads null on EVERY frame, including
// the world frames where the render provably draws the whole scene. Whatever
// the renderer walks, it is not reached through there.
//
// The two facts that stand instead, both from the menu-world probe: a
// self-initiated render on a menu frame runs to completion, issuing ~344
// vertex setup calls and not one draw; and the scene graph it walks is intact
// while it does so. So the geometry is registered somewhere the per-frame
// update fills - BSShaderAccumulator is the documented candidate - and the
// list, wherever it is, is not this one.
inline constexpr UInt32 kCullingProcessListOffsetDeadEnd = 0x08;

// The fields the emptiness has to come from, read from the disassembly of the
// render path on 2026-08-30.
//
// The chain, whole: kRenderScene calls 0x0070C0B0, which reads the culling
// process's visible set at +0x08, finds the null above, and so calls
// NiCullingProcess::Process (0x0070E0A0) - which fetches the renderer's
// accumulator itself, brackets the walk with StartAccumulating and
// FinishAccumulating (vtable +0x4C and +0x50), and walks the graph through
// NiAVObject::Cull at 0x007073D0. That walk is four instructions:
//
//   007073D0  test byte ptr [ecx+18h],1
//   007073D4  jne 007073E7            <- set: turn back, register nothing
//   007073E2  mov eax,[edx+4]         <- clear: NiCullingProcess::Cull
//   007073E7  ret 4
//
// So a render that cannot return early - kRenderScene has exactly one ret,
// at 0x0040D150 - still comes away with nothing whenever that bit is set,
// or whenever the world bound at +0x20 fails the frustum planes the walk
// rebuilds from the camera each time. Those, and a missing accumulator, are
// the only ways the measured shape happens: full vertex setup, no draws.
//
// The flag offset is derived rather than documented: 0x0040C830 sets bit 0 at
// [node+0x18] on the first-person node at 0x0040C95A and clears it again at
// 0x0040CDA5, using it as its own visibility switch, and 0x007073D0 tests the
// same bit on any NiAVObject. Second source is the probe that reads them.
inline constexpr UInt32 kNiFlagsOffset = 0x18;
inline constexpr UInt32 kNiWorldBoundOffset = 0x20;
inline constexpr UInt32 kNiChildrenOffset = 0xB0;
inline constexpr UInt32 kNiChildCountOffset = 0xB6;
inline constexpr UInt32 kNiCameraFrustumOffset = 0xEC;

// The engine's own switch for a live world behind menus, and the reason it
// is normally still.
//
// Oblivion does not simply stop rendering while a menu is up. It renders the
// world ONCE into a texture (0x0040D160), sets a "the snapshot is valid" byte
// at 0x00B33397, and from then on blits that texture instead of rendering -
// which is why the scene counter stands still, measured, across every menu
// frame. The guard is at
//
//   0040D5FE  cmp byte ptr ds:[00B33397h],bl
//   0040D604  jne 0040D662              <- snapshot valid: skip the render
//   ...
//   0040D658  call 0040C830             <- otherwise, render the world live
//
// and whether the snapshot is ever taken hangs on one byte earlier:
//
//   0040DA54  cmp byte ptr ds:[00B33396h],bl
//   0040DA5A  je  0040DB37              <- zero: never take one
//
// kStaticMenuBackground is that byte. It is a copy of Oblivion's own display
// setting bStaticMenuBackground, written once during start-up by
//
//   0040713D  mov dl,byte ptr ds:[00B06DC4h]
//   00407143  mov byte ptr ds:[00B33396h],dl
//
// from a call site that runs a single time in WinMain. Clear it and the
// engine renders the world live behind every menu, on its own, through its
// own call - no patched bytes, no self-initiated render.
//
// The copy is deliberately the thing OBVR touches rather than the setting at
// 0x00B06DC4. Writing the setting is the shape that made iSize dangerous:
// settings can be written back to the user's INI, and a value the game
// persists is a value that outlives the session. Nothing persists this copy.
//
// The simulation is not affected, which is the whole point. Every pause in
// the update step is guarded by its own fresh IsMenuMode call (0x00578F60 at
// 0x0040DC61 and seven other sites in 0x0040D800), not by this byte - so the
// world stays frozen while the picture comes alive. The engine itself already
// drives this path: with SleepWait open (menu id 0x3F4) it retakes the
// snapshot every frame because world time is running.
inline constexpr UInt32 kStaticMenuBackground = 0x00B33396;
inline constexpr UInt32 kMenuSnapshotValid = 0x00B33397;

// The accumulator hanging off the renderer singleton (kRendererPointer, well
// above) at +0x08 - the one the walk registers geometry with. 0x0040CE1B
// swaps a second accumulator in there for the first-person pass and
// 0x0040CE79 swaps it back, which is the second source for the offset.
inline constexpr UInt32 kRendererAccumulatorOffset = 0x08;

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
//
// WHAT THIS ACTUALLY IS, established later and from the other direction. It
// was read here as "the menu stack", which was a guess from the shape of the
// branch and was wrong. xOBSE's GameMenus.cpp declares
//
//   NiTArray<TileMenu*> * g_TileMenuArray = (NiTArray<TileMenu*> *)0x00B13970;
//
// and the two addresses above are that object's own fields: the data pointer
// at +0x04 and the UInt16 count at +0x0A. A decompiled branch in this binary
// and a modding SDK's source, arrived at years and methods apart, describing
// the same object - which is the two-source standard this file holds itself
// to, met without anyone setting out to meet it.
//
// It is an array indexed by menu type, not a stack: entry n belongs to the
// menu with id kMenuIdFirst + n, loaded or not. That is what makes it a second
// route to any menu by id, and the crosshair depth uses it as the check on the
// direct pointer below.
inline constexpr UInt32 kTileMenuArray = 0x00B13970;
inline constexpr UInt32 kTileMenuArrayData = 0x00B13974;
inline constexpr UInt32 kTileMenuArrayCount = 0x00B1397A;

// Pointer to the pointer to HUDInfoMenu - the menu that owns crosshairRef,
// the reference whatever the player is aiming at. From xOBSE's GameMenus.cpp:
//
//   HUDInfoMenu ** g_HUDInfoMenu = (HUDInfoMenu**)0x00B3B33C;
//
// A double pointer, which matters: reading this address gives the global, and
// the global holds the menu.
//
// HOW IT IS CHECKED, since one document is not the standard this file holds
// itself to. Not by a second route to the same pointer - reaching the menu
// through the tile menu array would need TileMenu's layout, which is not
// recorded here, and a guessed offset into a live pointer crashes rather than
// answers. Instead the object is asked what it is: every Menu carries its own
// id at kMenuIdOffset, and the one this address leads to has to answer
// kMenuIdHudInfo before a single further byte is read from it.
//
// That is the stronger check anyway. A second pointer route would only show
// that two addresses agree; the id shows that the address leads to the RIGHT
// menu, and it does so using an offset that has been read in the running game
// for weeks rather than a new one taken on faith.
inline constexpr UInt32 kHudInfoMenuPointer = 0x00B3B33C;

// Where a Menu keeps its own type id.
//
// Already relied on by game::ActiveMenuId, which reads it through the
// InterfaceManager's activeMenu and has been reporting menu types correctly
// since the persuasion work. Named here because the crosshair depth uses it
// for something stricter than logging: as the proof that kHudInfoMenuPointer
// leads where it claims.
inline constexpr UInt32 kMenuIdOffset = 0x20;

// TESObjectREFR::crosshairRef inside HUDInfoMenu, from the class layout in
// xOBSE's GameMenus.h: name 028, valueText 02C ... actionIcon 050,
// crosshairRef 054, unk058, class size 05C.
inline constexpr UInt32 kHudInfoCrosshairRefOffset = 0x54;

// TESObjectREFR's world position.
//
// Two sources, the second being OBVR's own working code: xOBSE's GameObjects.h
// puts posX/posY/posZ directly after the rotation triple, and PlayerAim.cpp
// has been reading that rotation at +0x20 - and writing to it - in the running
// game for weeks. A rotation at 0x20/0x24/0x28 puts the position at 0x2C.
inline constexpr UInt32 kRefPositionOffset = 0x2C;

}  // namespace obvr::addr

# Engine behaviour and reverse-engineering knowledge

What is known about Oblivion 1.2.0.416 and the Gamebryo engine underneath it, how it was
established, and what of it is expected to transfer to other games. The address authority
is `src/game/GameAddresses.h`; this document explains and classifies, it does not
replace the evidence there.

## Method: two sources for every address

`Scope: Legacy games`

An address goes into `GameAddresses.h` only when the disassembly of the actual binary
(`objdump -d --start-address=... -M intel` or `dumpbin /disasm`; the Steam exe's `.text`
is not encrypted by SteamStub) and an independent source agree. Independent sources used:
xOBSE (`llde/xOBSE`: `GameAPI.cpp`, `GameObjects.h`, `GameProcess.h`, `GameMenus.h`,
`NiRenderer.h`, `EventManager.cpp`), Oblivion Reloaded / TESReloaded (`RenderHook.cpp`,
`Base.h`), OBGEv2 (`NiDX9Renderer.cpp`), NorthernUI (`PlayerCharacter.h`, notes on
`0x00671620`), JRoush's COEF exports, Real Time Menus, the Construction Set wiki, UESP,
CommonLibSSE (for NiCamera layout, a different game of the same lineage), and - the best
second source - a second code path in the same binary or a runtime read that must agree
with something already proven.

Runtime validation is the standard before writing anything: an object must identify
itself (a menu answers its id, a node has a readable name, the settings collection's
vtable lies in the image and its path field is an `.ini` path, a frustum read must match
the projection matrix's angles, a cached function pointer must equal the independently
resolved one). A wrong address then reports instead of crashing.

Semantic identification beats raw addresses: `Present` is vtable index 17 in every D3D9
runtime including DXVK, and needs no game address at all; the same for `SetRenderTarget`
37, `CreateTexture` 23, `StretchRect` 34, `ColorFill` 35. Those indices were counted from
the SDK header and are pinned by `d3d9_types_test` against a fake vtable, because a wrong
slot calls a different method with this method's arguments and unbalances the stack.

## Engine structures

`CONFIRMED` unless marked, `Scope: Oblivion` layouts, `Scope: Gamebryo` shapes

### NiAVObject (32-bit)

```text
0x00 vtable    0x04 refCount   0x08 name (char*)   0x0C controller
0x10 extraDataList  0x14/0x16 count/capacity (u16)  0x18 flags (u16; bit 0 = culled/hidden)
0x1C parent    0x20 worldBound (NiBound, centre + radius)
0x30 localTransform (NiTransform 0x34: rot 3x3 at 0x30, pos at 0x54, scale)
0x64 worldTransform
0xB0 children list, 0xB6 child count (u16)   (NiNode)
```
Established by the camera write site (`[eax+0x30]`, `[eax+0x54]`), the render function's
node walk and the culling walk's flag test. `GetObject(name)` is vtable +0x58;
`UpdateSelectedDownwardPass` is slot index 25 (+0x64) and the free function `0x00707370`
dispatches to it.

### NiCamera

Frustum `m_kViewFrustum` at +0xEC (`NiFrustum`: l, r, t, b, n, f, ortho flag; l/r/t/b are
tangents of half-angles, not near-plane distances - r/t = 1.7778 at 16:9 settles the
reading), viewport at +0x110 (0xEC + 0x24, agreeing with CommonLibSSE's layout). Read and
written by `game/GameCamera`.

### Scene graph and renderer

- `g_worldSceneGraph` at `0x00B333CC`; camera at +0xDC, culling process at +0xE4. The
  culling process's vtable `0x00A7E610` slot 2 is `Process` (`0x0070E0A0`), slot 1 `Cull`.
- `NiDX9Renderer*` at `0x00B3F928` (41 occurrences of the `mov eax,[0x00B3F928]` encoding
  in the exe); device at +0x280; the accumulator at +0x08 (swapped for the first-person
  pass at `0x0040CE1B`/`0x0040CE79`); a screen size pair at +0xA58/+0xA5C that is *not*
  the lever for 2D layout (measured).
- `NiCullingProcess+0x08` is **not** the culled-geometry list (reads null on every frame,
  world frames included). `DEAD END`, recorded at `kCullingProcessListOffsetDeadEnd`.
- `RenderObject` `0x0070C0B0` (cdecl: camera, scene, culling process, visible array) is
  called exactly twice on the world path inside `kRenderScene` (world and first-person
  node) and once by the 2D pass for the menu scene graph.
- `NiDX9Renderer::SetCameraViewProj` `0x00701970`.

### Player and actors

- `PlayerCharacter*` at `0x00B333C4`. Offsets: rotation `+0x20` (rotX, rotY, rotZ,
  radians), position `+0x2C`, scale `+0x38`, `niNode` `+0x3C`, `parentCell` `+0x40`,
  `process` `+0x58`, MagicCaster base `+0x5C`, MagicTarget `+0x68`, grab spring `+0x574`,
  grabbed ref `+0x578`, grab distance `+0x584`, `isThirdPerson` `+0x588`, first-person
  anim data `+0x5CC`, `firstPersonNiNode` `+0x5D0` (one source; validated by name at
  runtime), mounted yaw accumulator `+0x61C`.
- Process fields read as bytes rather than through virtuals: `weaponOut` at
  `MiddleHighProcess+0x114` (virtual 0xBE), movement flags at `HighProcess+0x1FC`
  (virtual 0xAF; sneaking = 0x0400), `currentAction` at `HighProcess+0x1F4` (virtual 0xB3).
  A wrong offset misplaces a crosshair; a wrong vtable index crashes.
- `HighProcess::kAction`: None -1, EquipWeapon 0, UnequipWeapon 1, Attack 2,
  AttackFollowThrough 3, AttackBow 4, AttackBowArrowAttached 5, Block 6, Recoil 7,
  Stagger 8, Dodge 9, LowerBodyAnim 10, SpecialIdle 11, ScriptAnimation 12. **A cast is
  reported as an attack** (54 frames Attack, 24-26 FollowThrough, measured over six casts);
  the "casting" flag at `MiddleHighProcess+0x14C` covers both phases and was dropped.
- A bow shot: frames 0-6 action 5 (arrow on the string), frame 7 action 3 (the arrow has
  gone). The release starts an animation; the arrow spawns several frames later along
  the heading at *that* moment.
- Actor vtables: PlayerCharacter `0x00A73A0C`, Character `0x00A6FC9C`, Creature
  `0x00A710F4`; `AttackHandling` at slot byte 0x3AC = `0x005FEBF0` in all three, taking
  (flag, arrowRef, target), a non-null target taken as the hit target; `IsDead` +0x198,
  `GetNiNode` +0x154, `GetHandReachDistance` +0x26C, `GetScale` +0xEC; the process's
  `GetEquippedWeaponData` +0xEC; weapon type at +0x90 and reach at +0x98 of the weapon
  form; reach in units through `0x00547540` times `fCombatDistance` at `0x00B36F20`.
- `ActorProcessManager` at `0x00B3BD00`; `0x00673A50(mgr, level)` returns the tList for
  the high (0), middle-high, middle-low, low processing levels.
- `ToggleCamera` `0x0066C580` (byte, 1 = first person); `SetDialogCamera` `0x0066C6F0`
  (also flips a third-person player into first person for the conversation and back;
  reads `fDlgFocus` at `0x00B14F10`).

### Attack, projectile and magic sites

- Attack update `0x005FCAB0`, called from the input handler at `0x00672E0D` with the
  player flipped into third person around the call, and from `0x0066CB49`, `0x006758C6`
  with 1.0, 1.0; inside it, at the animation's moments: arrow construction (`0x0060C940`
  writing vtable `0x00A6F08C`), `UseActiveMagicItem` (`0x0069BEC0`), `AttackHandling`.
- Animation-key handler `0x005FC890`: the NPC wrapper; the player's vtable slot for it is
  `0x00A739D4`.
- `MagicCaster::CastMagicItem` `0x00699190` (thiscall, three args); `ApplyActiveMagicItem`
  `0x0069AF30`; projectile factory `0x0069A060` called at `0x0069B996`; `FindTouchTarget`
  `0x00699500`; hit cone `0x006131D0` with `fCombatHitConeAngle`.
- Heading virtual +0x1E0: `0x0065ABB0` for NPCs (rotZ), `0x0065DA60` for the player
  (rotZ plus `+0x61C`); rotation makers `MakeZRotation` `0x0070FDD0`, `MakeXRotation`
  `0x0070FD30`; walking: `HighProcess::Move` `0x0063C730` -> `MobileObject::Move`
  `0x0065AF30`.
- Grab: handler `0x00671170`, update `0x0066D930` called at `0x0067125E`, target built
  by `0x005F11F0`.

### Interface

- InterfaceManager singleton pointer `0x00B3A6E0` (getter `0x00582160`); cursor tile +0x1C,
  cursor position floats +0x20/24/28, derived +0x2C/30/34, altActiveTile +0x88,
  activeTile +0x98, activeMenu +0x9C, active menu id stack at +0xE0 (ten dwords).
- `IsMenuMode` `0x00578F60`; `GetTopVisibleMenuID` `0x0057CF60`; the 2D pass `0x0057F170`
  (wrapper `0x00579260`, master frame call at `0x0040D6F7`, loading paths `0x00579CF0`);
  `Tile::UpdateFloat` `0x0058CEB0`; HUDReticle update `0x005A82D0` called at `0x00582251`
  (fades the reticle to zero in third person by testing `player+0x588`); the world-pick
  ray assembly at `0x0058080C`; tile search `0x00581390`; pick normalisation `0x00701540`.
- `g_TileMenuArray` `0x00B13970` (data +4, count +0xA); `g_HUDInfoMenu` `0x00B3B33C`
  (double pointer); `Menu::id` at +0x20; `HUDInfoMenu::crosshairRef` at +0x54.
- Menu ids from `0x3E9` (`MenuType.h`).

### Globals and settings

- Update step `0x0040D800` with fourteen `IsMenuMode` calls; the seven redirected:
  `0x0040D809` (Havok pause), `0x0040DB5B` (animations), `0x0040DBAB` (sound),
  `0x0040DBFB` (actors), `0x0040DE3F` (physics step), `0x0040DE76` (weather),
  `0x00663176` (scripts). `SetHavokPaused` `0x00889A30`.
- Frame render `0x0040C830` (single `ret` at `0x0040D150`); menu-texture render
  `0x0040D160`; snapshot-valid byte `0x00B33397`; static-menu-background copy `0x00B33396`
  (written once in WinMain from the setting at `0x00B06DC4`); the SleepWait menu retakes
  the snapshot every frame because world time runs.
- `TimeInfo` at `0x00B33E90`, frame delta float at +0x0C (`0x00B33E9C`).
- `IniSettingCollection` object at `0x00B07BF0`, list at +0x10C, entries {SettingInfo*,
  next}, `SettingInfo` {value union, name}; names carry their section ("iSize W:Display").
  **Writing these persists to the user's INI**; see [failed-approaches.md](failed-approaches.md).
- 2D screen-size copy `0x00B06C4C/50`; safe-zone settings at `0xB135F8` are a mislead.
- d3d9 module handle `0x00B42150`, cached `Direct3DCreate9` `0x00B42158` (Oblivion does not
  import d3d9; it loads it by hand at `0x00761DF0`). Import table: fourteen DLLs, among
  them `d3dx9_27.dll` (which is why OBVR can assemble a pixel shader through it).
- Loading thread handle `0x00B33434`.
- Code section `0x00401000..0x00A27C39`; `.rdata` at VA `0x628000`.
- Version resource check: `0x010201A0`.

## Engine behaviour established by measurement

`CONFIRMED`, `Scope: Oblivion` unless marked

| Behaviour | How it was measured |
| --- | --- |
| The world transform of the camera is recomputed from parent x local after the camera write; only `localTransform` writes survive. | Disassembly at `0x0066BE84`; a `worldTransform` write would be overwritten in the same frame. `Scope: Gamebryo` |
| The camera node's local and world positions differ only by what OBVR itself added the frame before: the parent neither rotates nor moves it. | Probe columns LOCAL vs EYE across whole sweeps, first and third person. |
| The render function re-reads the camera position each call to place sky and LOD roots. | Disassembly `0x0040C95F`. `Scope: Gamebryo` pattern |
| Rendering does not advance the simulation; the render walk advances time-driven controllers by the frame delta. | Menu trace (scene counter vs frame counter); the stutter and helmet offset cured by zeroing the delta. `Scope: Gamebryo` |
| Behind a pause menu the engine renders the world once into a texture and blits it; it does not call the frame render at all. Dialogues and the persuasion minigame keep rendering. | Scene counter across menus; the snapshot byte. |
| A written player rotation survives the frame; the engine adds mouse movement to it rather than replacing it. | Probe: rotZ standing at 1.8708 then 3.0148 across dozens of frames. |
| The third-person camera sits on a sphere about a pivot above the feet, always looks at the pivot, eases towards rotX/rotZ by `fChaseDeltaMult` (0.05) per **physics step at 60 Hz**, not per rendered frame. | `third-person-run.ps1` probe (height = r sin pitch to 0.01 units); headset trace (two moving frames, one still, at 90 Hz). |
| The first-person camera stands 4.6 units off the body's yaw axis. | 1.71 units of chord at 21.4 degrees -> arm 4.60; direct read 4.58. |
| The player is flipped into third person for the duration of the attack update call. | `mov byte ptr [ebx+588h],1` before `call 005FCAB0` at `0x00672E0D`. |
| `fDefaultFOV` is a 4:3 horizontal angle; the frustum widens with the frame. | Frustum tangents 1.0231 x 0.5755 at 75 degrees, 16:9. |
| The frustum read at the camera update (1.0231) differs from the one at Present (1.1188). | `FrustumWatcher`. Which pass the wider one belongs to: `UNKNOWN`. |
| The activation reference is only set for activatable things within `iActivatePickLength` (150 units). | HUDInfoMenu reads; the crosshair probe's "no reference" share. |
| Software-skinned actors upload bone rows as three float4 constants into three register classes; palettes are camera-relative. | The bone-lock investigation. |
| The 2D pass draws nothing after a second world render in the same frame. | DualPassProbe sweep. Cause `UNKNOWN`. |
| Oblivion tolerates being windowed at an arbitrary back-buffer size, provided the window is resized to match; it does not tolerate a D3D9Ex device. | Resolution hook runs; `OBVR-d3d9ex-*.log`. |
| The vertical look is asymmetric by geometry (the camera starts at head height). | 0.0.4 report; 60 up / 120 down. |
| Water reflections reset as the camera moves, in mono as well. | `DualPassProbe=3` versus 0 at a pond. |
| The game pauses when its window loses focus (frame counter stands still). | Linux harness notes, `HANDOFF.md` section 12. `Scope: Bethesda legacy` (`LIKELY` for others) |
| `iSize W/H` written in memory are persisted to `Oblivion.ini` by the engine on its own, during the run. | The poisoned-INI incident (`f577b1c`). |

## Reverse-engineering workflow that worked here

`Scope: Legacy games`

1. Start from a published reference (xOBSE, TESReloaded, NorthernUI) for a *name*; never
   adopt its address without reading the bytes.
2. Read the bytes with `dumpbin /disasm` or `objdump` on the file; map VA to file offset
   through the section table (`.text` at `0x00401000`, raw `0x400`; `.rdata` at VA
   `0x628000`, raw `0x627400`).
3. Find call sites of a candidate with a byte search for `E8 rel32` (or the `A1 disp32`
   encoding for a global read) and count them; a global read occurring 41 times is real.
4. Prefer functions with **relocatable entry bytes** (`push -1; push imm32` is the SEH
   prologue of every MSVC function with a try block, seven bytes with nothing relative)
   for entry detours; prefer instruction boundaries of 5+ bytes for mid-function patches.
5. Confirm at runtime before writing: read a value that must agree with something known
   (a frustum against the projection matrix, a pointer that answers `QueryInterface`, an
   object that names itself).
6. Write a probe first, act second (`Debug.*Probe` keys, budgeted log lines, hot-reloaded).
7. Put the disassembly excerpt into `GameAddresses.h` beside the constant.

## Calling conventions met

- `__thiscall` with stack arguments and callee cleanup: the render, culling and interface
  functions; called from C++ as `__fastcall` with a dead `edx`.
- `__cdecl` for OpenVR's global `VR_*` exports; `__stdcall` for the FnTable pointers -
  two conventions in one interface (openvr.h line 2299, openvr_capi.h line 21).
- `__stdcall` for every D3D9 method (COM).
- The `AroundCall` stub keeps `__thiscall` intact by popping the return address aside.

## Version sensitivity

- Every address above is 1.2.0.416 only; GOG's exe is the same version but untested.
- The 4GB patch does not move code; it moves *allocations* above 2 GB.
- xOBSE's PlayerCharacter/Process layouts are the same headers other plugins rely on;
  NorthernUI and TESReloaded agree where they overlap.
- Oblivion Reloaded's detours sit on `0x0040C830` in every version and on `0x0066BE6E`
  and `0x0066C6F0` in 8.x-derived builds (E3, ORC).

## Notes for other Bethesda / Gamebryo games

`HYPOTHESIS` unless marked; nothing here has been tried on another game.

What is expected to transfer (the engine lineage NetImmerse -> Gamebryo -> Creation is
one scene-graph family; xOBSE, FOSE, NVSE and SKSE headers show the same `NiAVObject`
shape):

- The scene graph with `NiAVObject` local/world transforms recomputed downward, a camera
  node whose `localTransform` is the thing to write, an `UpdateSelectedDownwardPass`-like
  call after the camera write. `LIKELY` for Morrowind (NetImmerse), Oblivion, Fallout 3,
  Fallout: New Vegas; Skyrim's `NiCamera` layout matched here (CommonLibSSE).
- One frame-render function that walks culling, both scene passes and post-processing
  and leaves the 2D layer to a later pass; OBGE/OBSE-era mods hook it in Oblivion, and
  FNV's "Fallout Reloaded"/NVR-style renderers hook the equivalent. `LIKELY`.
- The player rotation triple at `TESObjectREFR+0x20` and a heading that both walking and
  projectiles read. The "one field, two jobs" problem is a Bethesda gameplay-layer fact,
  and the "set it inside the call that reads it" cure should apply wherever an attack
  update exists. `LIKELY` for FO3/FNV (Actor/Projectile classes are documented by
  NVSE/JIP); Skyrim's `PlayerCharacter::rotation` and its aim are structured differently
  (SKSE), `UNKNOWN`.
- Software skinning with camera-relative bone palettes uploaded as constants: an Oblivion
  shader-package fact (register classes c14/c31/c42). FO3/FNV share the shader package
  lineage (`LIKELY` similar), Skyrim's BSLightingShader skins differently (`UNKNOWN`).
- The static menu background snapshot and `IsMenuMode` gating the update step: Real Time
  Menus exists for Oblivion; FNV's engine also snapshots (`UNVERIFIED`).
- The frame delta living in a `TimeInfo` global read by controllers: `LIKELY` for the
  Gamebryo titles (xOBSE's `g_timeInfo`; NVSE has `g_timeGlobal`), `UNVERIFIED`.
- D3D9 through DXVK interop for the compositor handoff: Morrowind, Oblivion, FO3, FNV are
  all 32-bit D3D9 and the route is the same. Skyrim LE is 32-bit D3D9 too; Skyrim SE/AE
  and Fallout 4 are 64-bit D3D11 and have official VR editions.

What is known to differ:

- Morrowind: Direct3D 8 originally (NetImmerse 4), no script extender of xOBSE's shape;
  OpenMW-VR is the realistic path (see [ecosystem-and-prior-art.md](ecosystem-and-prior-art.md)).
- Fallout 3 / New Vegas: 32-bit D3D9 Gamebryo with a deferred-ish lighting path and
  different shader packages; every address differs; NVSE and JIP LN provide the headers.
- Skyrim LE: 32-bit D3D9, Creation Engine's first renderer; Skyrim VR exists as an
  official 64-bit product, so the reason to port LE is thin.
- Every game's 2D system differs in how the screen size is copied and normalised; the
  three-coordinate-space split should be expected, its addresses not.

What requires verification before any transfer: the frame-render entry and whether it
re-reads the camera; where the frame delta lives; whether the 2D pass begins its own
target group; whether the engine tolerates a windowed arbitrary back-buffer size; whether
the update step gates on a menu-mode function with per-subsystem calls.

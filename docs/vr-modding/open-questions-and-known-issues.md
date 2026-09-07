# Open questions and known issues

Current bugs first (what a player sees today), then the unverified claims, then the
research questions a future agent could take. Historical dead ends are in
[failed-approaches.md](failed-approaches.md); do not confuse the two.

## Known issues (0.1.1)

| Issue | Symptom | Reproduction | Subsystem | Evidence | Workaround | Severity | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Recenter dead during intro films | The flat picture rides the head for the first seconds; `Del` does nothing until the main menu | Start the game with the head tilted | `HeadsetRenderer` flat anchor, `WaitGetPoses` validity | log "Poses: WaitGetPoses now returns ..." (`72df027`); task #25 | Wait for the main menu | low | open |
| FaceGen head parts one eye off in the second pass | Eyes, teeth, some hair sit beside the face in one eye, scaling with `EyeSeparationScale` | Stand close to an NPC | Second render / FaceGen time-gated build | `ceb2389`, `664b56d` | none; `EyeSeparationScale` lower reduces it | medium | open, cause located, fix not built |
| Edge-of-view follower collapse | A body straddling one eye's frustum can collapse; shared culling mitigates | Followers at the edge of view | bone lock pairing | `e4342b7` reopened #21; `e0fc88c` culling sync | none | low since culling sync | mitigated, hypothesis open |
| Text entry needs the keyboard | Character name | new game | UI | README | keyboard | low | by design so far |
| Oblivion Reloaded and derivatives incompatible | No stereo (scene render refused) or no camera | load both | detour sites | log names the DLL | none | n/a | by design |
| Enhanced Camera incompatible | OBVR stays inactive, the game runs flat | load both (tested 2026-09-07) | camera update site `0x0066BE6E` | log "Camera: at 0066BE6E another plugin's jump to 1001D580 ... inside OBSE_EnhancedCamera.dll" | remove Enhanced Camera | n/a | by design; chaining through its detour not attempted |
| The visible body moves the view | With `[Body] Visible=1` the view lurches on every look: left or right turns it up, up and down go too far; nauseating | first person, look around | `PlayerBody.cpp` | two headset runs 2026-09-07: with the player root moved (`7c915b0`) and with `Bip01` alone moved (`41363f5`, log "Bip01 ... under Player"), same symptom; the collapse of `Bip01 Head` to scale zero is the remaining suspect if the camera node hangs under it (the first camera pass now logs the camera's ancestry) | `Visible=0`, now the default | high while on | open, off by default |
| NorthernUI takes the HUDReticle update site | Third-person crosshair tooltips keep vanilla state; everything else works (tested 2.0.3, 2026-09-07) | load both | `WorldPickHook` at `0x00582251` | log "Crosshair tooltip: at 00582251 another plugin's call to ... inside NorthernUI.dll" | none needed | low | by design |
| The freeze fix unconfirmed | Freezes after view switches / loads may or may not be gone | play | `WaitGetPoses` under the queue lock | `6f4ccde` | none | high if present | fix in, confirmation open |
| Mono path magnified and stretched | `Stereo=none` shows the old fixed-bounds picture | set `Stereo=none` | `HeadsetRenderer::SubmitMono` | HANDOFF "The mono path still uses the old fixed bounds" | use `dual` or `aer` | low | deliberate |
| Foliage billboards | SpeedTree cards may face one eye's camera | outdoors, trees | render | HANDOFF "Foliage billboards" | none | `LIKELY` low | unmeasured |
| Water reflection resets with head movement | reflection jumps as the head moves, in mono too | any pond | engine | `DualPassProbe=3` vs 0 | none | cosmetic | engine behaviour, not a bug of OBVR |
| Built-in defaults diverge from the shipped INI | A hand-trimmed INI falls back to flat, mono, overlay off, `HeadMovementScale` 3.0 | delete keys | `Config`, `HeadTracker.h` | this KB, architecture.md | keep the shipped INI | low | open, cosmetic |
| Flat picture overflowed the eye texture on a Quest 3 (Air Link) | Menus and loading screens visible, then SteamVR's "loading" view for good once the world appeared; the game kept running on the monitor; log "linear stretching was refused" then "Submit returned 105 (left 0, right 105)" | Quest 3 through Air Link, 2064x2272 eye textures, view axes at 62% / 38% of the width, `MenuScale=0.9` | `EyeMirror::Create` flat placement, `HeadsetRenderer` flat path | Nexus report 2026-09-06 (three logs, `dual`/`aer`/`none` identical); DXVK `StretchRect` returns `D3DERR_INVALIDCALL` for a rectangle outside the surface | none in 0.1.1 (`MenuScale` around 0.75 would have fitted) | high on that headset | fixed after 0.1.1: `FitFlatPicture` shrinks the flat picture for both eyes alike, and a failed flat copy falls back to mono instead of the test pattern |
| Test-pattern submit answered 105 for the right eye only | Same report: after the flat copy failed the D3D11 test pattern went to the compositor and the right eye's `Submit` returned `TextureUsesUnsupportedFormat` while the left returned 0 | Quest 3 / Air Link, SteamVR 2.16, RTX 4090 driver 616.64; not seen on the Beyond or Dream Air | `HeadsetRenderer::EndFrame` pattern fallback, `EyeTextures` (D3D11, `R8G8B8A8_UNORM`, no shared flag) | the three logs above, and the antialiasing log of 2026-09-07 with the same pair | the fallback is no longer reached from a failed flat copy or a multisampled back buffer | low now | `LIKELY`: the pair "left 0, right 105" is what the compositor answers whenever it refuses the frame's textures, the refusal surfacing on the eye that completes the frame - the same pair appeared for a multisampled Vulkan back buffer; why it refuses the D3D11 pattern on that setup is still `UNKNOWN` |
| Antialiasing (launcher AA) stopped the headset at the first world frame | Menus and loading screen in the headset, then nothing once the world loaded; monitor plays on; log "back buffer ... samples=8 ... NOT submittable as it stands" then "Submit returned 105 (left 0, right 105)" | Oblivion launcher antialiasing 8x, any headset (reported on a Quest 3, 0.1.2) | `GameFrame::Acquire` on the mono path (first world frame, `Stereo=none`) | Nexus log "3. AAx8" 2026-09-07 | turn the launcher's antialiasing off | high with AA on | fixed after 0.1.2: `FrameResolve` resolves a multisampled back buffer into a single-sample texture before the mono submit; the eye copies always resolved through DXVK's `StretchRect`. `CONFIRMED` in the developer's headset 2026-09-07 with 8x antialiasing (4036x3376, "resolved into a ... single-sample copy", then "dual pass is live", no 105) |

Built but never seen in a headset (`EXPERIMENTAL`), each a potential issue: the
first-start walkthrough, `LiveMenuBackground` with `MenuStandIn` on pause menus,
`MirrorMenusToMonitor`, the watchdog, everything under `[Hands]`, `ControllerMenus`.

## Unverified claims worth checking before relying on them

| Claim | Where it lives | Status | What would settle it |
| --- | --- | --- | --- |
| A readied spell raises `weaponOut` (`MiddleHighProcess+0x114`) like a weapon | `PlayerAim.h` | `UNVERIFIED` (stated as assumed) | ready a spell with `CrosshairProbe=1` and read the state line |
| `wineopenxr` is not built for i386 under Proton | HANDOFF section 10 (2026-08-25) | `UNVERIFIED` against 2026 Proton | read `Makefile.in` in the current Proton tree |
| SteamVR 32-bit OpenXR is beta-only | HANDOFF, `HeadTracker.h` (2026-06) | true as of 2026-07-21 beta 2.17.6; stable `UNKNOWN` | the SteamVR release notes after 2026-07 |
| RenderDoc can capture this process (DXVK, 32-bit, interop) | debugging.md | `UNVERIFIED` | try it |
| The `+0xA58/+0xA5C` renderer pair is a screen size | `bd17287` | `LIKELY` (it read the believed size) but not the 2D's lever | n/a |
| `LiveMenuBackground` shows the world correctly offset (the shelved attempt's offset had a suspect: base rotation) | `498a1e7`, `PlaceMenuCamera` | fixed by reusing the look control's base; `EXPERIMENTAL` | one headset session in an Esc menu, leaning |
| The seated/standing symmetry of the eye separation sign is right for every headset | `BoneRebase.h` calibration | `CONFIRMED` for one headset | any second headset log's "Bone lock" sign line |
| `fCombatDistance` is the setting at `0x00B36F20` | `GameAddresses.h` (`kCombatDistanceSetting`) | `LIKELY`; the strike logs the name it finds | a hand-mode run's "strikes by motion armed" line |
| Every edition with 1.2.0.416 (GOG, disc) matches the Steam exe byte for byte at the patched sites | README | `UNVERIFIED` | one GOG log (the verify lines) |
| DXVK 3.1 behaves like 3.0.2 with OBVR | architecture.md | `UNVERIFIED` | a run on 3.1 |

## Open research questions

### Why does the interface pass draw nothing after a second world render?

Status: UNKNOWN. Priority: medium.

- **Why it matters:** the between-pass HUD draw is a workaround; if the cause is a state
  the second render leaves behind, it may bite other late-frame work (probes, mirrors).
- **Evidence:** entered once per frame, every gate in the wrapper and the manager open,
  zero draws and zero clears; recovers when the second render is cut.
- **Hypotheses:** an accumulator or render-target-group state the second render leaves
  closed; a per-frame "already drawn" flag inside the interface manager set by the walk;
  the depth clear with flags 6 failing against a state the second pass leaves.
- **Suggested experiment:** with `HudBetweenPasses=0`, trace the pass's `SetRenderTarget`,
  `Clear` and viewport calls (the counters exist) on a dual frame versus a mono frame and
  diff; read `[manager+0x1C]` and `[manager+4]+0x2C` on both.
- **Relevant code:** `InterfaceRenderHook.cpp` (`HookedRenderInterface`, `LogInterfaceGates`).

### Why does a self-initiated world render from Present draw nothing?

Status: UNKNOWN. Priority: low (superseded by the engine's own live background).

- **Hypotheses:** no `BeginScene` bracket; the renderer's accumulator not started; the
  "snapshot valid" byte; a per-frame visible-set already consumed.
- **Experiment:** call it from inside the engine's frame at other points (after the
  update step, before the 2D pass) and count draws.

### Where exactly does FaceGen build the head parts, and how to make the second pass rebuild them?

Status: HYPOTHESIS (a lazily packed face mesh built once per frame with the first
render's camera). Priority: medium (visible on every close NPC).

- **Experiment:** find the FaceGen render-data build (the time-gated update the microsecond
  experiment reopened), and either invalidate its cache between the passes or replay its
  camera-dependent output shifted like the bone rows.
- **Relevant:** `ceb2389`, `664b56d`, the head-part bone class (c14) in the lock.

### Do particles advance or diverge between the two passes?

Status: UNKNOWN. Priority: low.

- **Why it matters:** per-eye particle divergence is a stereo artefact the eyes cannot
  fuse.
- **Evidence:** none; the frame clock is zeroed, so controller-driven emitters should not
  advance; whether particle vertex data is rebuilt per walk is unknown.
- **Experiment:** stand in a torch's smoke with `DualPassProbe` 0 versus 3; count the
  dynamic buffer locks per pass attributable to particle systems.

### Which pass owns the wider frustum read at Present (1.1188 vs 1.0231)?

Status: UNKNOWN. Priority: low.

- **Hypotheses:** a widened culling frustum; the water/shadow pass; a menu camera.
- **Experiment:** `FrustumWatcher` reads at several points inside `kRenderScene`.

### Would a per-eye asymmetric frustum write remove the last geometric compensation?

Status: HYPOTHESIS. Priority: medium (image quality: no black margin arithmetic, exact
optical centre).

- **Experiment:** write each eye's own `l r t b` into `NiCamera+0xEC` before its pass
  (between the passes for the second), submit with no bounds; check that culling
  (`CullingHook` replays positions only, not frustums) still agrees.

### Can the engine's water-reflection path serve as a second "view" (Plan B stereo)?

Status: HYPOTHESIS (`HANDOFF.md` section 15). Priority: low while the dual pass works.

- **Why it matters:** "draw again, move nothing" from the engine itself, with no clock
  games and no palette locks.
- **Experiment:** locate the reflection render-to-texture pass, its camera construction,
  and whether it re-evaluates skeletons.

### Does 32-bit OpenXR through SteamVR (2.17 beta) work for this process?

Status: UNKNOWN. Priority: low-medium (a second backend, and the standing experience's
`IVRInput` alternative would go through OpenXR actions).

- **Experiment:** `TrackerSource::OpenXR` is declared and unimplemented; an OpenXR loader
  for Win32 exists in the `OpenXR.Loader` NuGet package (HANDOFF section 10); the texture
  path would need `XR_KHR_vulkan_enable2` against DXVK's instance.

### Does the legacy controller input path still deliver buttons?

Status: UNKNOWN. Priority: high for the standing experience, none for the seated one.

- **Experiment:** `docs/hand-tracked-mode.md`, "The first headset session", steps 1 and 2.

### Is the culling-sync sliver visible at large `EyeSeparationScale`?

Status: UNKNOWN. Priority: low.

### Frame-time cost of the dual pass on weaker GPUs

Status: UNKNOWN. Priority: medium for the public build.

- **Experiment:** the per-frame log line at `LogEveryFrames=60` on a mid-range GPU, dual
  versus `DualPassProbe=3`.

### The vanity and dialogue cameras

Status: partly answered (dialogue: the zoom is cut, the flip kept; the world keeps
rendering). The vanity (idle) camera: `UNKNOWN`.

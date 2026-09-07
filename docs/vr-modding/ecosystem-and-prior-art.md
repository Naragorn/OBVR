# Ecosystem and prior art

**Landscape reviewed on 2026-09-05.** Everything here can go stale; the date is the
first thing to check before relying on a runtime or version claim. Sources are primary
where one could be found (official docs, repositories, authors' own writeups) and are
linked after the claim. Where only a secondary source was found it is marked.

## Runtime and API facts

`Scope: OpenVR` / `Scope: OpenXR` / `Scope: DXVK`

| Fact | Status | Source |
| --- | --- | --- |
| OpenVR supports 32-bit applications; SteamVR's OpenXR did not (2022) | `CONFIRMED` | Valve staff on https://github.com/ValveSoftware/openvr/issues/1687 |
| SteamVR added 32-bit OpenXR applications in beta 2.17.2 (2026-06-11); a prompt sets up the 32-bit runtime beside the 64-bit one | `CONFIRMED` | https://steamcommunity.com/app/250820/announcements/ ("SteamVR Beta Updated - 2.17.2") |
| SteamVR stable 2.16 released 2026-06-03; beta 2.17.6 on 2026-07-21; whether 32-bit OpenXR reached stable | `UNKNOWN` as of the review | same announcements page; https://www.gamingonlinux.com/2026/06/steamvr-2-16-arrives-with-a-number-of-linux-fixes-for-vr-fans/ |
| OpenVR is still maintained: SDK 2.15.6 (2026-03-27) added `Submit_TextureWithMotion` (motion vectors to SteamVR) | `CONFIRMED` | https://github.com/ValveSoftware/openvr/commit/0924064316de3effbcd1acf1e309182a2deb1c05 |
| Valve steers new development to OpenXR ("We continue to focus on OpenXR as our preferred API for new games and applications") | `CONFIRMED` | every SteamVR release note above |
| The compositor reprojects against the `WaitGetPoses` pose and assumes the app rendered with it | `CONFIRMED` | https://github.com/ValveSoftware/openvr/issues/518 |
| `WaitGetPoses` may access the Vulkan queue in the default timing mode; one thread per `VkQueue`; explicit timing mode exists to avoid it | `CONFIRMED` | https://github.com/ValveSoftware/openvr/wiki/Vulkan |
| DXVK's D3D9 interop interfaces (`ID3D9VkInteropInterface`, `ID3D9VkInteropTexture`, `ID3D9VkInteropDevice`) are upstream; `IsAttachmentOnly` controls the sampled bit | `CONFIRMED` | https://github.com/doitsujin/dxvk/blob/master/src/d3d9/d3d9_interfaces.h |
| DXVK latest release 3.1 (2026-08-28); 3.0 added `DXVK_DEBUG=hang` | `CONFIRMED` | https://github.com/doitsujin/dxvk/releases |
| DXVK asked for OpenVR's required Vulkan extensions itself from its first VR interop work (2018) | `CONFIRMED` | https://github.com/doitsujin/dxvk/issues/27 |
| Plain D3D9 cannot share surfaces; D3D9Ex can (Microsoft, "Surface sharing between Windows graphics APIs") | `CONFIRMED` in `HANDOFF.md` section 13 (Microsoft page not re-fetched in this review) |
| OpenXR 1.1 with the extensions relevant to a future backend: `XR_KHR_vulkan_enable`/`enable2`, `XR_KHR_D3D11_enable`, `XR_KHR_composition_layer_depth`, `XR_FB_space_warp` | `CONFIRMED` they exist | https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html |
| Proton had no 32-bit OpenXR (feature request open since 2023-05); OpenComposite and xrizer are OpenVR-on-OpenXR layers used on Linux | `CONFIRMED` for the request; current state `UNKNOWN` | https://github.com/ValveSoftware/Proton/issues/6808, https://github.com/ValveSoftware/steam-runtime/issues/758 |

## The approaches, project by project

### UEVR (praydog) - Unreal Engine 4/5

- **What:** a generic injector for UE4/UE5 games with three rendering modes: Native
  Stereo (uses UE's own stereo pipeline through a hooked `IStereoRendering` /
  `FFakeStereoRendering` vtable: `CalculateStereoViewOffset`,
  `CalculateStereoProjectionMatrix`, `AdjustViewRect` to a double-wide target),
  Synchronized Sequential ("renders two frames sequentially in a synchronized fashion on
  the same engine tick"; Skip Draw or Skip Tick variants; TAA ghosts), and Alternating/AFR
  ("the game world advancing time in between frames. Causes eye desyncs and usually
  nausea"). OpenVR and OpenXR backends, D3D11 and D3D12.
- **Sources:** https://docs.uevr.io/ , https://github.com/praydog/uevr ,
  https://deepwiki.com/praydog/UEVR/4-rendering-pipeline
- **Taxonomy:** native engine stereo first; sequential same-tick second; AER last.
- **Relevance:** UEVR's ordering (native > synchronized sequential > AFR) is the same
  ranking OBVR arrived at from below (dual > aer > none), and its "Skip Tick" documents
  the same simulation-versus-render problem (particles not advancing correctly when a
  tick is skipped). Not applicable to Gamebryo directly; Oblivion Remastered (UE5) is a
  UEVR target (https://roadtovr.com/oblivion-remastered-vr-mod-release/), which is what
  makes OBVR's target the *original* game only.

### praydog/FEAR2VR - LithTech Jupiter EX, 32-bit D3D9

- **What:** a 32-bit DLL in the game hooking `CLTRenderer_SetupPassPerspective` ("the one
  call that decides what the next DrawScene sees": camera transform, FOV, fractional
  viewport), repeating the setup/draw/end group per eye within one frame, with a
  **64-bit OpenXR host process (`xr64.exe`)** fed through a shared-memory frame publisher
  (seqlock, triple-buffered) because 32-bit OpenXR runtimes were unreliable; DXVK
  required for frame pacing.
- **Source:** https://deepwiki.com/praydog/FEAR2VR (2026-08), which cites the repository's
  own `ENGINE_NOTES.md` and `FRAME_LOOP.md`.
- **Taxonomy:** engine-hook geometry stereo, same tick, sequential.
- **Relevance:** the closest architectural sibling in method ("hook the engine's own view
  setup, not Direct3D's matrix state"), and the worked example of the 64-bit helper route
  OBVR declined. Its early viewport-composition bug (the rect halving twice) is the kind of
  fault OBVR's `PlacePicture` arithmetic pre-empts.

### daniel-lynch/bo1-vr - Call of Duty: Black Ops (2010), 32-bit D3D9

- **What:** in-process mod, "two scene renders per frame, per-eye projection with the
  headset's own FOV", head tracking hooked into `R_SetViewParms`, frames submitted "via
  DXVK's `ID3D9VkInterop*` interfaces", DXVK's 32-bit `d3d9.dll` required. Built for
  Linux/Proton through xrizer on any OpenXR runtime; Windows/SteamVR untested by its
  author.
- **Source:** https://github.com/daniel-lynch/bo1-vr
- **Taxonomy:** engine-hook geometry stereo, same tick, DXVK interop handoff.
- **Relevance:** exactly OBVR's route, found after OBVR had chosen it; the two projects
  are independent confirmation of the DXVK interop path for 32-bit D3D9.

### sd805/l4d2vr - Left 4 Dead 2 (Source), 32-bit D3D9

- **What:** ships a **modified DXVK fork** as the game's `d3d9.dll` with VR-specific
  changes, per-eye render targets (RGBA8, shared D24S8 depth, a UI overlay target)
  submitted to OpenVR.
- **Source:** https://deepwiki.com/sd805/l4d2vr/3.4-graphics-pipeline-and-dxvk
- **Relevance:** the precedent that made the DXVK route look like it cost a fork; it does
  not (upstream interop suffices), which `HANDOFF.md` records as the correction.

### Detegr/openRBRVR - Richard Burns Rally (2004), 32-bit D3D9

- **What:** in-process, OpenVR *and* OpenXR through a backend switch, rendering through a
  DXVK fork with VR support. Not indexed on DeepWiki at review time; details from the
  repository README as read in `HANDOFF.md` section 10.
- **Source:** https://github.com/Detegr/openRBRVR (`UNVERIFIED` beyond the README summary)
- **Relevance:** the closest relative by starting position; its backend switch is the
  shape `TrackerSource` was designed for.

### Hochgeschwindigkeitsrennfahrer/Grand-Theft-Auto-IV-VR-Mod - GTA IV, 32-bit D3D9

- **What (WIP, 2026):** stock DXVK 3.0.2 `d3d9.dll` + a Win32 ASI + OpenVR; head tracking
  done, "AER and TrueStereo 3D paths in different stereo modes - both work for depth,
  with look-around blur as a known side effect".
- **Source:** https://github.com/Hochgeschwindigkeitsrennfahrer/Grand-Theft-Auto-IV-VR-Mod
- **Relevance:** a third independent stock-DXVK-interop project; its "look-around blur"
  reads like the pose-mismatch fault OBVR fixed with the `WaitGetPoses` pose (`3f0d557`).

### mohamad-balouza/bioshock-vr - BioShock (UE2.5), 32-bit

- **What:** sequential stereo with the engine's `CalcView` run twice per frame; the second
  pass re-evaluates the skeleton over the mod's bone writes, so cached bone transforms are
  reapplied (`bones::reapply()`), with a dirty-flag discipline.
- **Source:** https://github.com/mohamad-balouza/bioshock-vr/blob/2372e221/src/game/bioshock1r/bones.cpp ,
  https://deepwiki.com/mohamad-balouza/bioshock-vr/6.2-bones-module:-skeletal-drive-and-render-lock
- **Relevance:** the same failure class as OBVR's bone lock, named independently (cited in
  `a1d7791`). Reusable rule: sequential stereo re-evaluates skeletons; replay pass one.

### DR-89/fear-vr - F.E.A.R. (2005)

- **What:** a two-process architecture with shared memory (`HANDOFF.md` section 10).
- **Source:** https://github.com/DR-89/fear-vr (`UNVERIFIED` in this review; not fetched)

### Luke Ross R.E.A.L. VR (GTA V, RDR2, Cyberpunk 2077, Elden Ring, ...)

- **What:** closed-source in-process mods using **alternate-eye rendering**: "render just
  one eye per frame, and display the previous frame for the other eye"; one eye always
  carries 11 ms of extra latency; reprojection hides it for head rotation, not for
  objects moving relative to the camera. The author's stated reason: modern titles cannot
  render 180 fps, and "GTA V doesn't allow you to render the world twice without some
  time passing between frames (and without every moving object/NPC in the world
  animating)". AER v2 (2023) reduced ghosting (secondary source).
- **Sources:** https://github.com/lukeross00/gta5-real-mod (FAQ),
  https://www.theverge.com/23190201/luke-ross-vr-real-mod-gta-v-elden-ring-horizon-red-dead
  (secondary), https://mixed-news.com/en/real-vr-mod-dlss-ray-reconstruction/ (secondary).
- **Taxonomy:** AER. Also documents "camera rotation compensation" (smooth turning) and
  "dynamic stereo" versus "virtual screen" for cutscenes - the same two menu shapes OBVR
  calls world and cinema.
- **Relevance:** OBVR's `Stereo=aer` is this technique; OBVR's dual pass is the answer to
  the FAQ's own open question ("has anybody found a way to render the *same* frame twice,
  without any change to the world?") for one 2006 engine.

### VorpX

- **What:** commercial injector with two modes: **Z3D** (Z-buffer reconstruction: "takes
  the flat image, calculates the Z distance of objects on screen, and modifies the image
  slightly different for each eye ... a slight transparent outline around objects
  nearest the screen"; "Very fast (almost twice as fast)"; "usually worse depth
  perception") and **Geometry 3D** ("renders two distinct views", "roughly half the
  speed", "often annoying artifacts (misplaced shaders/reflections, effects/ui at wrong
  depth)"; DX12 titles Z3D only). "DirectVR" adds in-engine head tracking and FOV setup
  for supported games.
- **Sources:** https://www.vorpx.com/more-headtracking-z-buffer-vs-geometry-3d/ ,
  https://www.vorpx.com/forums/topic/what-the-difference-in-3d-reconstruction/ ,
  https://www.vorpx.com/forums/topic/how-to-find-out-if-a-game-support-z3d-or-g3d/
- **Oblivion under VorpX:** a profile exists with Direct VR head tracking; the FOV resets
  to 75 after every conversation (the same `SetDialogCamera` path OBVR shims), the
  correct FOV for VorpX's default was given as 112, tree shadows and torch depth needed
  profile fixes, HUD/menu scaling was added, and a 4:3/5:4 resolution is what the game
  "fully supports" (https://www.vorpx.com/forums/topic/oblivion-problems-distorted-headtracking-in-directvr/ ,
  https://www.vorpx.com/forums/topic/oblivion-error-and-ctd/). The Rift-DK1 era used
  VorpX Geometry mode and TriDef with the FOV Modifier OBSE mod
  (https://communityforums.atmeta.com/t5/VR-Experiences/Oblivion-Configuration/td-p/113201).
- **Relevance:** the prior art for "Oblivion in a headset", and the reason OBVR exists:
  no prior project rendered the original game's world per eye through its own engine.

### Depth3D / SuperDepth3D (BlueSkyDefender, ReShade)

- **What:** depth-map-based stereoscopic shader with a VR companion app (OpenXR); the
  shader's own options document the artefact class: "Occlusion Masking" view modes for
  filling disoccluded sections, "Halo Priority/Reduction", "De-Artifacting" for hair and
  fur, TAA/DLSS/FSR compatibility offsets, and the author's stated goals (limit window
  violations, hide disocclusion near the screen, avoid depth conflicts such as a
  crosshair at the wrong depth).
- **Sources:** https://github.com/BlueSkyDefender/Depth3D ,
  https://github.com/BlueSkyDefender/Depth3D/blob/master/Shaders/SuperDepth3D.fx ,
  https://www.reshade.me/forum/shader-presentation/2128-3d-depth-map-based-stereoscopic-shader?start=1340
- **Taxonomy:** depth reconstruction.
- **Relevance:** the last resort in OBVR's original quality ladder; never needed.

### geo-11 (davegl1234 / bo3b, from 3Dmigoto) - DX11 geometry stereo

- **What:** a 3D Vision replacement that "stereoize[s] all vertices in VertexShaders"
  through the 3Dmigoto hooking layer, with SBS/TAB/anaglyph/`katanga_vr` (HelixVision
  VR app) outputs; deferred rendering, shadows and screen-space effects need per-game
  shader fixes ("shadows in particular need to *not* be stereoized"); geo-12 for DX12 in
  progress.
- **Sources:** https://helixmod.blogspot.com/2022/06/announcing-new-geo-11-3d-driver.html ,
  https://github.com/bo3b/3Dmigoto/issues/354 (bo3b's explanation of the deferred-effects
  limit)
- **Taxonomy:** generic injector, shader-level geometry stereo.
- **Relevance:** the explanation of *why* screen-space effects break in any stereo that
  is not the engine's own; OBVR's engine-hook approach sidesteps it because every pass
  really runs per eye.

### OpenMW-VR (Morrowind, engine source port)

- **What:** OpenXR VR in OpenMW; stereo managed by a stereo manager with per-view render
  targets; `GL_OVR_multiview`/`multiview2` when the OSG build supports it, else the OSG
  horizontal split ("brute-force ... rendering a separate copy for each eye"); shared
  shadow maps for both eyes as an option.
- **Sources:** https://openmw-vr.readthedocs.io/en/latest/reference/modding/settings/stereo.html ,
  https://gitlab.com/OpenMW/openmw/-/merge_requests/1757 ,
  https://registry.khronos.org/OpenGL/extensions/OVR/OVR_multiview.txt
- **Taxonomy:** engine source port with native stereo (multiview or brute force).
- **Relevance:** the Morrowind answer; the multiview route is not available to a D3D9
  engine, and "one set of shadow maps for both eyes" is the same shadow observation as
  OBVR's unshifted sub-pass rows.

### Skyrim VR / Fallout 4 VR and Community Shaders

- **Official products** (2017); no first-party technical description of their stereo path
  was found beyond a producer interview about 90 Hz job budgets
  (https://www.gamedeveloper.com/design/q-a-bethesda-opens-up-about-i-fallout-4-vr-i-and-the-plunge-into-vr-game-dev).
  How the Creation Engine renders its two eyes: `UNVERIFIED`.
- **Community Shaders (doodlum, jiayev forks)** document the mod-side reality: Skyrim VR
  renders "both eyes into a single combined texture with a side-by-side stereo layout",
  Community Shaders use `SV_INSTANCEID` instancing with `[2]` arrays of view/projection
  matrices, per-eye stereo-sync passes for screen-space effects (SSGI, SSS: bilateral
  cross-eye reprojection after "Stereo-consistent screen-space ambient occlusion", Shi,
  Billeter, Eisemann 2022), stencil culling of pixels reprojectable from the other eye,
  per-eye DLSS/FSR contexts, and an OpenVR overlay for their menu.
- **Sources:** https://deepwiki.com/doodlum/skyrim-community-shaders/8.1-vr-support-implementation ,
  https://deepwiki.com/jiayev/skyrim-community-shaders/11.2-vr-stereo-consistency ,
  https://deepwiki.com/jiayev/skyrim-community-shaders/9.3-vr-upscaling-and-per-eye-processing
- **Relevance:** the catalogue of what a *native* stereo Creation Engine title still has
  to fix (screen-space effects, upscalers, hidden-area masks) - the effects Oblivion does
  not have, which is part of why a 2006 engine is the easier port.

### Injectors and toolkits on top of a VR app

- **OpenXR Toolkit (mbucchia):** an OpenXR API layer for upscaling, foveation, hand
  tracking to controller, image adjustments (https://github.com/mbucchia/OpenXR-Toolkit).
- **PrimaShock's modded toolkit DLL:** "3D Depth Boost" (a stronger stereo separation
  applied by the layer) and FOV cropping; the author advises against SteamVR as the
  runtime (https://primashock.com/openxr-toolkit-mod-by-primashock-vr/ ,
  https://eu.pimax.com/blogs/blogs/a-guide-to-primashock-s-openxr-toolkit-mod). Users
  report 7-8 % as the usable range and warping at 8+
  (https://forum.openmr.com/t/enhanced-vr-depth-with-openxr-3d-boost-mod/43004).
  OBVR's `EyeSeparationScale` is the same idea applied before rendering (no warping).
- **vrperfkit (fholger):** upscaling (FSR/NIS/CAS), fixed foveated rendering via VRS,
  hidden-area-mask forcing, for D3D11 OpenVR/OpenXR games
  (https://github.com/fholger/vrperfkit). Not applicable to D3D9.
- **OpenComposite / xrizer:** OpenVR-on-OpenXR translation layers (Linux-centric);
  relevant if OBVR's OpenVR client ever has to run on a non-SteamVR runtime.

### Engine source ports with VR

Quake, Doom 3 BFG, Half-Life 2 (the 32-bit HL2 VR mod is the reason the Proton 32-bit
OpenXR request exists) - not reviewed in depth; source ports are the model when source
exists and irrelevant to OBVR's constraint (no source).

### Enhanced Camera 1.4b (LogicDragon) - the first-person body in Oblivion itself

`Scope: Oblivion`. Read on 2026-09-07 from the source that ships on the Nexus files tab
(mod 44337, "Enhanced Camera Source-44337-1-4b", mirrored at
https://github.com/figlinafik/OblivionEnhagedCam). One file, `main.cpp`, 1832 lines, plus
a 3x3 matrix helper. Licence text on the Nexus page: "You can do whatever you want with
this mod, just give me credits if you use any part of the mod." Not VR, and no inverse
kinematics anywhere in it, but it is the only published answer to "show the player's
own body in first person", which a standing mode needs before any arm tracking.

- **The trick:** Oblivion keeps two skeletons for the player, the first-person one
  (`PlayerCharacter::firstPersonNiNode`, arms only) and the third-person one
  (`niNode`, the whole body). Enhanced Camera leaves the camera in first person but
  clears the hide flag (`m_flags & 1`) on the third-person root, hides the
  first-person root, scales `Bip01 Head` and both `Bip01 * Clavicle` nodes to zero
  (`UpdateSkeletonNodes`) so the head and the animated third-person arms do not sit in
  the picture, and moves the third-person root each frame so its head lands under the
  first-person camera (`TranslateThirdPerson`: root += camera1st.world - (head3rd.world +
  R_root * fCameraPos)). `RotateArms` optionally pitches the clavicles with `rotX` when
  a spell, attack, block, or torch is active, working in root-relative bone space
  (`RotateNode`: node.local = inv(parent.world) * Rz(rad) * node.world, both without
  the root).
- **Its hook sites (Oblivion 1.2.0.416), none of which OBVR touches except the first:**
  `0x0066BE6E` camera update (the site OBVR needs; this is the whole incompatibility,
  see [open-questions-and-known-issues.md](open-questions-and-known-issues.md));
  `0x006650C9` after the POV switch call `0x005E5480` (`UpdateSwitchPOV`, root flags);
  `0x006043DC` the animation apply, wrapping `0x00471F20` (`ApplyAnimData`, "applies
  animData to skeleton", called with `ActorAnimData*`) for the player only
  (`UpdateActor`); `0x00603AAA` head-tracking IK, disabled for the player in first
  person; `0x0065F4E2` player fade-out; `0x0070C159` collision apply and `0x007492BE`
  particle update, each with a global flag to skip one call; `0x0040C91B` inside the
  scene render around the shadow call `0x004073D0`; `0x0066C63D`, `0x0066CEEE`,
  `0x0066CF5D`, `0x0066CCBD`, `0x00600BCC` the forced third-person switches (chair,
  mount, dismount, vampire feed, death) turned into a "fake first person"; and byte
  patches at `0x00664FC6`/`0x00664FFB` (load the third-person body in first person),
  `0x00407519` (first-person shadows), `0x009E8192`/`0x009E8162`/`0x009E7DB2`
  (looking-down and vanity-distance constants redirected to its own floats).
- **Globals it reads:** `0x00B3BB0C` first-person camera node pointer, `0x00B3BB04`
  vanity-mode byte, `0x00B3BB24` third-person zoom, `0x00B13FCC` dialogue zoom percent,
  `0x00601B80` "evp on the player"; `HighProcess` bytes `unk114` combat mode, `unk11C`
  knocked, `unk11D` sit/sleep state; `PlayerCharacter+0x71C/0x71D` sit-down and get-up
  animation flags. All from the 2013 OBSE headers; OBVR verifies its own offsets from
  the binary (see [engine-behavior.md](engine-behavior.md)) and would have to do the
  same for these before use.
- **What OBVR would take and what it would not:** the body part (root flags, node
  scaling, per-frame root translation under the camera, the ApplyAnimData replay after
  a skeleton edit, the head-tracking disable) is exactly the fundament for a standing
  mode and for task #43. The camera part is not needed: OBVR already owns the camera
  site and places it from the headset. `RotateArms` is a monitor-era substitute for arm
  tracking and would be replaced by controller poses. The magic-node fix (`magicNode`
  moved to the first-person hand) matters because OBVR's spell aim already turns the
  caster ([camera-tracking-and-aiming.md](camera-tracking-and-aiming.md)).
- **Known coexistence trap:** its shadow hook at `0x0040C91B` collides with Oblivion
  Reloaded's, which is why XJDHDR's fork made it conditional on `bFirstPersonShadows`
  (https://github.com/XJDHDR/game-mods, readme in "Enhanced Camera - no shadow hook").
  OBVR's scene-render hook is at the function entry `0x0040C830`, so that one would not
  collide, but the ApplyAnimData replay runs inside the scene render and would meet
  OBVR's dual pass; whether the second pass needs the replay too is `UNKNOWN`.

Oblivion Reloaded's "Immersive Camera / Enhanced Camera" feature (TESReloaded10,
GPL-3.0-or-later with a section 7 naming clause, https://github.com/llde/TESReloaded10)
covers the same ground inside a much larger plugin; not read in depth.

## Comparison

`Scope: General VR`. Ratings are qualitative and from the sources above plus OBVR's own
measurements.

| Approach | Visual quality | Geometric correctness | Temporal correctness | Latency | Performance cost | Compatibility | Engine access | Complexity |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Native engine stereo (UEVR native, OpenMW-VR) | best | exact | exact; TAA intact | lowest | 1x-1.5x (multiview shares work) | engine must have the concept | high (engine API) | medium |
| Engine-hook geometry stereo, same tick (OBVR dual, FEAR2VR, bo1-vr, BioShock VR) | full per-eye render | exact | exact if per-frame caches are replayed (skinning, culling) | low | ~2x render | any engine with a callable view setup | deep reverse engineering | high |
| Alternate-eye rendering (R.E.A.L., OBVR aer, UEVR AFR) | full resolution per eye | exact per eye | one eye a frame old: doubling on relative motion, TAA smear | +1 frame for one eye | ~1x | almost any engine | camera write only | low-medium |
| Depth reconstruction (VorpX Z3D, Depth3D) | warped; halos; transparencies and UI at wrong depth | approximate | exact | low | ~1.1x | any game with a readable depth buffer | none | low |
| Shader-level injector geometry stereo (3D Vision, geo-11) | good where fixed; deferred effects break | exact for vertex-transformed geometry | exact | low | ~1.5-2x | DX9/DX11 with per-game fixes | shaders only | high per game |
| Engine source port | best | exact | exact | lowest | 1x-2x | that engine | source | very high |

Where OBVR's own numbers sit: dual pass at no measurable frame-time cost on an RTX 4090
for a 2006 game (not measured on weaker GPUs); AER at one full-screen copy per frame;
mono (the fallback) magnified by design.

## Ideas worth borrowing for future mods

- UEVR's **Skip Tick / Skip Draw** vocabulary for what a sequential second render must
  skip; OBVR's version is "zero the frame delta and replay the caches".
- Enhanced Camera's **third-person body under a first-person camera** (root flags, head
  and clavicle scaled to zero, root translated so the head sits under the camera,
  ApplyAnimData replayed after each edit) as the starting point for a standing mode with
  a visible body; licence is attribution only.
- FEAR2VR's **64-bit host over shared memory** when a 32-bit runtime is not available.
- BioShock VR's **dirty-flag discipline** for skeleton writes (write, then clear the
  dirty flag so the engine does not re-evaluate over it) - a possible alternative to
  OBVR's palette replay if Gamebryo's skin instances expose such a flag (`UNKNOWN`).
- Community Shaders' **stereo-consistency passes** if a target engine has screen-space
  effects.
- OpenMW-VR's **shared shadow maps** as the cheap half of "sub-passes are camera-free".
- Luke Ross's **dynamic stereo versus virtual screen** for cutscenes, which OBVR already
  mirrors as world versus cinema.
- OpenVR SDK 2.15.6's `Submit_TextureWithMotion` if an engine can produce motion vectors
  (Oblivion cannot without new work).

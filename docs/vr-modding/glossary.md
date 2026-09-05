# Glossary

OBVR's own words first, then the general ones. Scope labels follow
[README.md](README.md#scope-labels).

- **AER, alternate eye rendering (also AFR):** one eye drawn per display frame, the other
  eye shown its last image; time-disparity artefacts. OBVR's `Stereo=aer`.
- **Around-call stub (`core/AroundCall`):** a generated stub that runs OBVR code before and
  after one engine call while keeping the engine's calling convention intact; not
  reentrant. Used for the aim at the source.
- **Base rotation:** the camera rotation as the engine built it, levelled by the look
  control, before the head rotation is composed onto it.
- **Believed size:** the screen size the game's 2D lays out and maps the mouse against
  (the window-creation copy at `0x00B06C4C/50`), as opposed to the frame's real size.
- **Bone lock:** replaying the first world render's skinning palette rows onto the second,
  shifted by the eye step. See rendering-and-stereo.md.
- **Bridge (worldless streak):** holding the last stereo pair across up to three frames
  that have neither a world render nor a menu.
- **Camera pass / camera hook:** the per-frame callback at `0x0066BE6E`, after the engine
  wrote the player camera.
- **Cinema (delivery):** the whole back buffer as one flat picture to both eyes, on a
  levelled anchored pose; films, loading screens, the main menu, and menus under
  `Menus=cinema`.
- **Culling sync:** the second pass culls from the first pass's camera positions.
- **Depth boost:** hyperstereo; `EyeSeparationScale` above 1.
- **Delivery (`FrameDelivery`):** Stereo, Cinema or HeldStereo; decided per frame by
  `DeliverFrame`.
- **Dual pass:** the world rendered twice per tick, once per eye; `Stereo=dual`.
- **Entry detour (`core/EntryDetour`):** replacing the first seven relocatable bytes of a
  function with a jump and running them from a trampoline.
- **Eye step:** the camera's move from the centre to an eye and from one eye to the other
  (`StereoEyeStep`).
- **FrameLogic pattern:** every decision a hook makes lifted into a pure function over
  plain values so it can be tested without the game.
- **Frame clock / frame delta:** the seconds the last frame took, at `TimeInfo+0x0C`;
  zeroed across the second render.
- **Held pair, HeldStereo:** the last captured eye pair submitted again with the pose it
  was drawn from, for menu frames the engine does not redraw.
- **HUD layer / overlay:** the redirected 2D layer texture and the `IVROverlay` it hangs on.
- **Interop bracket:** flush, lock DXVK's queue, transition images, submit, transition
  back, release; one RAII owner.
- **Live menu background:** the engine rendering the world behind pause menus because its
  static-background snapshot is switched off; with `MenuStandIn` supplying the camera.
- **Menu stand-in:** OBVR arming a VR camera from inside the scene render on menu frames
  the engine draws the world on without a camera update.
- **Mixup (bone):** a second-render bone row evaluated against the wrong same-posed
  instance, a body length off.
- **Optical axis / centre:** where the eye's view axis lands in its texture; off-centre
  and mirrored between the eyes.
- **Pool timeline:** a one-frame record of every dynamic-buffer lock, unlock and skinned
  draw, for the standalone replay.
- **Probe:** a `[Debug]` switch that measures and logs without acting, budgeted.
- **Recenter:** taking the current head pose (yaw only, and position) as the new zero.
- **Redirect (2D):** answering the interface pass's `SetRenderTarget(0)` with OBVR's
  surface while the pass runs.
- **Ring (bone log):** the first render's rows in upload order, searched forward from a
  running position for a fingerprint match.
- **Rung:** one level of a diagnostic ladder (`DualPassProbe`, the hand-tracked mode).
- **Seated experience / standing experience:** OBVR's two shapes; head-tracked with
  keyboard/mouse/gamepad versus motion controllers (under construction).
- **Source aim (`AimAtSource`):** the player's rotation set to the gaze only inside the
  engine call that reads it for a shot, swing or spell.
- **Stale row:** a bone row the second render did not re-evaluate, still at the first eye.
- **Tracking space / universe:** OpenVR's seated or standing origin; OBVR declares seated
  for poses, compositor and overlays alike.
- **Two sources:** the standard for an address: the binary's bytes plus an independent
  source, or a runtime check that must agree with something proven.
- **Vergence error:** the angle between where the eyes converge (the crosshair quad) and
  where the target is; IPD (1/d - 1/c).
- **Worldless frame:** no camera pass and no menu.

General:

- **Asymmetric frustum:** left/right and top/bottom tangents of different magnitude;
  every headset eye has one.
- **Compositor:** the runtime process that takes eye textures, reprojects and displays
  them (SteamVR's vrcompositor).
- **Depth reconstruction (Z3D):** stereo from one image plus depth; disocclusion holes.
- **DXVK:** the D3D9/10/11 to Vulkan translation layer; `d3d9.dll` beside the exe.
- **Engine-hook geometry stereo:** a mod making the engine render each eye's geometry.
- **Gamebryo / NetImmerse:** the scene-graph engine under Morrowind (NetImmerse),
  Oblivion, Fallout 3 and New Vegas; Creation Engine descends from it.
- **IPD:** interpupillary distance; read from the runtime, never assumed.
- **LAA, 4GB patch:** LargeAddressAware; allocations above 2 GB in a 32-bit process.
- **OpenVR:** Valve's SteamVR client API; 32-bit capable; `IVRSystem`, `IVRCompositor`,
  `IVROverlay`, `IVRInput`.
- **OpenXR:** the Khronos standard; 32-bit support depends on the runtime.
- **Reprojection (ATW/ASW):** the compositor warping the last frame for the head motion
  since it was rendered; correct only against the pose the app rendered with.
- **Same-tick stereo:** both eyes from one simulation state.
- **Sequential stereo:** both eyes rendered one after the other in one frame.
- **xOBSE:** the maintained Oblivion Script Extender that loads OBVR.

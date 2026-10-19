# Water shimmer - a plugin of its own

Status: spec, nothing built (2026-10-19). Scope: a separate xOBSE plugin in the
FoliageAA mould (github.com/Naragorn/FoliageAA: hooks the Direct3D 9 device
Oblivion made, touches no game file, works flat and in VR), not a part of OBVR -
the shimmer is the flat game's too.

## What is seen

The waves and the reflections on them flicker: Nadi in the headset, 2026-10-19
evening, right after the stable capture started turning with the head
(controls-spec, "Beta report of 2026-10-17", water). With the reflection now
whole wherever one looks, the high-frequency sparkle on the water surface is
what remains. No picture or measurement of it exists yet.

No earlier attempt at this exists (Nadi, 2026-10-19: "wir haben das nie
angegangen zu fixen, sondern nur die Reprojektion gefixt"). The water commits of
2026-09-18/20 (1d94a9e, c3e0ddb, 9f11bb4) and of 2026-10-19 (2f6f9b5) are the
stable capture against the reflection resetting with the head and its seam - a
different fault, fixed. Nothing here is ruled out yet.

## What is known of the engine's water

Read in OBVR's own water work (render/WaterReprojection.h, tools/water_blend_ps.asm,
render/WaterReflectionHook.cpp):

- The water is drawn by the engine's own vertex and pixel shaders out of the
  shader packages; OBVR selects a patched vertex shader for its stable capture
  and leaves the pixel shader the game's ("waves and reflection sampling remain
  byte-for-byte the game's path", render/InterfaceRenderHook.cpp).
- The pixel path samples three textures with projective coordinates: the
  reflection target, the refraction and the wave normal map; OBVR's blend
  shader (tools/water_blend_ps.asm) mirrors that topology - `texldp` on s0,
  s1, s2, a Fresnel from the view vector (`pow` with c8.x), a specular term
  from a light direction (c2) and a depth fade.
- The reflection target is a small texture the engine renders once per frame
  (the capture, 3x/1.5x the eye's frustum since 2026-10-19), sampled through
  the projective matrix with a per-pixel offset from the wave normal.
- Under DXVK the live dxvk.conf sets `d3d9.samplerAnisotropy = 16` (since
  2026-10-09, for the parallax shimmer); the engine's own sampler states for
  the water samplers (mip filter, LOD bias) are unread.

## Hypotheses (not measured)

- **H1 Normal-map aliasing.** The wave normal map is sampled at a frequency the
  pixel grid cannot carry at distance - no mipmap, a mip filter of none, or a
  negative LOD bias - and every frame the sample lands elsewhere on the waves:
  the normal flips, and with it the reflection offset and the specular. MSAA
  does nothing for it (shading aliasing, one shade per pixel).
- **H2 Specular sparkle.** The specular `pow` of a high-frequency normal against
  the sun: single bright pixels that come and go. A subset of H1 in cause,
  separate in the fix (clamp or fade the specular by distance).
- **H3 Reflection sampling.** The small reflection target, sampled with the
  wave offset, magnified and point- or bilinear-filtered: the reflected tower
  shimmers because its texels are large and the offset moves them every frame.
  In OBVR the reflection target is the engine's own size; its filtering is
  unread.
- **H4 Two eyes.** In VR each eye samples the same waves at a different place;
  what shimmers in time on a monitor also differs between the eyes, which the
  brain reads as sparkle. Would make the headset worse than the monitor, not
  cause it.

## Measurement first

1. **The shaders and states.** OBVR's `WaterShaderDumpLedger` already writes the
   water pixel shaders to OBVR-WaterPS-NN.bin under the VR test replay; the
   plugin's first tool dumps the same (FoliageAA's `DumpShaders=1` pattern) and
   logs the sampler states set for the water draw's three samplers - mip filter,
   LOD bias, anisotropy - and the normal map's mip count (GetLevelCount). That
   settles H1 as a reading, not a guess. Cost: cheap, one run.
2. **A number for the shimmer.** A harness scenario at the lake
   (`cow Tamriel 1 15`, the head still) takes pictures 0.11 s apart; the water
   region's frame-to-frame difference (tools/pgmdiff.py exists) is the shimmer
   energy. Vanilla against each fix, on the monitor; the same pair in the headset
   is Nadi's judgement. Cost: cheap, the scenario and a crop script.
3. **The eyes.** The same at `Stereo=none` against `dual`, for H4. Cost: cheap.

## Options

| | What | Cost | Why |
|---|---|---|---|
| A | Sampler-state override while the water draws: mip filter linear, a positive LOD bias on the normal map, anisotropy on | cheap | FoliageAA's draw-window hooks exist (SetPixelShader to recognise the water shader, SetSamplerState around the draw); nothing new but the recognition; risk: if the normal map has no mip chain the override does nothing, then B |
| B | A replacement water pixel shader: the normal's strength faded with distance (the depth fade the shader already has), the specular clamped | medium | the shader has to be read and reassembled per package (OBVR did the same for the vertex shader; d3dx9_27 assembles ps_2_0 at runtime); risk: Oblivion Reloaded and the like replace the water shaders, the plugin must step aside as FoliageAA does for leaf shaders |
| C | Supersample the water draw as FoliageAA supersamples leaves | large | leaves are alpha-tested, one decision a sample; water is blended, and a pass per sample would blend eight times - a different mechanism (per-sample shading through a resolve) that D3D9 does not offer |
| D | Temporal filtering of the water | huge | needs history buffers and reprojection the engine has nowhere for |

Order: 1 and 2 first (an afternoon), then A, then B if A's reading says the
normal map cannot be filtered. C and D are written down to say why not.

## Open questions

- Whether the shimmer is the same on the monitor (H4): the first measurement
  run says.

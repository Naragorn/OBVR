# Leaf batch orientation fix — 2026-09-16

## Restoration to main — 2026-09-20

The implementation, audit helpers and batch tests below were recovered from
commit `ecaedb4` on `dlss5`; they were absent from `main`. The three original
hook sites and facing calculation are restored unchanged. The camera-position
accessor now reads main's existing pre-render cyclopean camera snapshot.
Build registration, scene-hook installation, per-eye audit calls and the
`StableLeafBillboards` configuration setting are restored as well.

Current validation: Release Win32 DLL built successfully; all 60 CTest targets
passed, including leaf batching and configuration default/on/off/missing-key
checks. No new in-game or headset acceptance run was performed for this
restoration. The runtime captures described below are historical evidence,
not a test of this new build. The separate live water-toggle regression remains
open.

## Cause and intervention

The existing character-position facing math was correct for an individual tree,
but its hook at 007F16CF runs only during the initial leaf geometry setup.
The optimized leaf renderer reuses that first basis for later trees. A basis
derived from a tree at the edge of the view can therefore turn every other
tree's cards edge-on. This is a shared orientation error, not evidence that
all tree nodes were culled.

The fix retains the initial hook and also wraps the two per-tree setup calls
at 007F8B51 and 007F8C66. Both pass the CURRENT tree world transform as their
sixth stack argument. Each wrapper calls the original setup, restores the
original camera basis (including zero fourth components), then applies the
same existing character-position facing math. The return value is preserved.
The later engine constant-map upload remains responsible for the GPU update.

No visibility/frustum bypass, HMD-facing rotation, shader replacement, tree
LOD change or wind change was added. The two added sites cover both branches:
different geometry data and reused geometry data.

## Local executable evidence

Evidence read from build-ci/oblivion-disasm.txt and the installed
D:/SteamLibrary/steamapps/common/Oblivion/Oblivion.exe:

- 007F15E8 loads the world-transform argument into ESI.
- 007F16CF calls 007F1170: bytes E8 9C FA FF FF.
- 007F1170 copies camera UP/RIGHT to B46768/B46758.
- 007F86C0 is the optimized leaf rendering loop. Initial setup calls shader
  vtable +34 at 007F8946; later iterations do not revisit that initial call.
- 007F8AB5..007F8AC1 copies the current geometry's world transform (+64)
  into stack storage reused across iterations.
- 007F8AEB branches according to shared geometry data.
- 007F8B51 and 007F8C66 call 007F0BC0 with seven stack arguments and the
  copied transform in argument six. Bytes E8 6A 80 FF FF / E8 55 7F FF FF.
- 007F8D39 calls 007F6BF0 for the current tree; 007F6F34 uploads the vertex
  constant map before the geometry draw at 007F6F9D.
- 007F0964..007F0984 binds the six-vector block starting at B46738.
  The orientation values are inside that block.

Old runtime evidence: build-ci/vr-tests/20260912-170834-479/OBVR.log
reported one setup call and one transform identity per eye. A transform
identity was NOT a tree identity: the loop reuses the same stack address.

Exa did not locate the exact vanilla batch implementation. DeepWiki reported
TESReloaded10 unavailable; Context7 had no matching library. No external
documentation claim is used to justify these binary addresses.

## Validation

- New standalone leaf_batch test: 1,768 checks pass. Covers all 8 partial
  installation states, all 8 signature mismatch masks, every write failure
  position, retries, literal executable call signatures, all facing gates,
  four tree quadrants in every order and both eyes using reused transform
  storage, and degenerate/nonfinite fallback inputs.
- Existing facing tests continue to cover translation/elevation invariance,
  orthonormality, gates and rejected input.
- 59/59 CTest targets pass after the final replay changes.
- Release Win32 DLL built. The hook compilation itself reports no warnings;
  recompiling existing replay dependencies emits existing warnings.
- 20260916-081246-026: real-engine run with a COPY of Save 2 / Amy /
  Hawkhaven. All three hook signatures installed; six setup calls versus
  the previous one, including five distinct subsequent geometries per eye.
  In other views 31 subsequent geometries were processed per eye.
  64 leaf images captured and visually inspected as a contact sheet: no
  whole-scene leaf disappearance observed in those views.
- The ten render/hand cases and ten control roundtrips pass. Overall suite
  FAIL: live controller bindings were unavailable. Do not label this as
  full VR acceptance or a complete flow-coverage proof of the engine.

The diagnostic replay now captures 16 views (eight headings 45 degrees apart,
each at two horizontal positions), four time samples and both eyes: 128 images.
The sweep ends before the existing reload/menu tests.

Physical HMD acceptance, arbitrary outdoor locations, modded leaf shaders,
and every possible LOD/wind/shadow state remain unverified. Pure tests cannot
prove the engine's final pixels; the user's reported edge case still needs
the headset check.

## Matched old/new engine comparison

- Fixed sweep: build-ci/vr-tests/20260916-081646-160 (128 leaf images).
  This run used the fixed hook, as confirmed by the per-tree install/batch logs.
  Its runner filename contains "baseline" because the first attempt to switch
  source versions preserved an older timestamp and did not rebuild the hook.
  The subsequent builds explicitly updated timestamps and compiled the hook.
- Actual old-hook sweep: build-ci/vr-tests/20260916-082006-473 (128 leaf images).
  The log confirms the original single setup hook and one setup call per eye.
- Same copied save, replay and camera positions. Contact sheets
  build-ci/leaf-compare-0.jpg and leaf-compare-1.jpg compare all 16 settled
  views in both eyes. Views 4/5 and 14/15 show absent foliage under the old
  hook and visible foliage under the fixed hook. View 14 clearly shows bare
  branches before and a leafy crown after, in both eyes.
- User-facing comparison: build-ci/leaf-before-after.jpg.
- These two extra runs were deliberately time-bounded to 110/100 seconds
  after collecting all leaf captures; their full-suite reports fail because
  later menu tests are incomplete. This does not negate the captured A/B
  result and must not be presented as two full-suite passes.
- All runner reports confirm game/mod INIs restored byte-for-byte.

## Open bug, 2026-10-22: distant trees turn with the head

Nadi in the headset: "weit entfernte bäume drehen sich mit dem headset view, was nervt" - far trees
turn as the head turns. Not looked at. The fix above covers the near trees' leaf cards (SpeedTree,
faced by the character's position). The far trees are another renderer: the distant land's tree
LOD, flat billboards (Textures\Trees\Billboards) the engine faces to the camera. Hypothesis, not
read: they are faced by the camera's ROTATION (screen-aligned), which in a headset is the head's, so
every turn of the head turns them; facing them by the camera's POSITION (a turn about the vertical
only, as the leaf fix does for the near trees), or by the body's heading rather than the head's,
would hold them still. What to read first: where the distant tree billboards' facing is computed
(the LOD tree draw, its vertex shader constants or a CPU basis like the leaf batch's), and whether
it reads the camera node's world rotation. Cost unknown until that is read; the leaf fix's hook
pattern (wrap the setup, put a position-faced basis back) is the likely shape - small to medium.

**Read and measured 2026-10-22, not fixed.** Nadi: "denke da können wir den selben fix applien wie
beim gras und nahen bäumen". Three candidates read in the disassembly, two of them ruled out:

- **The distant land's LOD trees** ("DTree distant 3d billboard", built at 004BA984): a
  NiBillboardNode (vtable 00A45134) given the billboard bits 9, then `& ~2 | 5` - mode 5, whose
  facing (its jump table at 00722308, the case at 00722170) turns the node about its up axis toward
  the camera's POSITION (camera world translation minus the node's). Not turned by a head's rotation;
  only by the eyes' place, which a head turn moves by centimetres - nothing at a hundred metres.
- **The trees' per-frame setup 007F0100** (the leaves' neighbour): its vector at B46738 is
  normalize(TransformNormal(-B465A8, matrix)), and the probe (Debug.TreeBillboardProbe, a build kept
  aside in artifacts/scratch/tree-billboard-attempt) read B465A8 as (-0.045, 0.242, -0.969) under a
  high sun with the head level - the LIGHT's direction, not the view's. Writing a facing there would
  have broken the trees' lighting; it was taken out before anything shipped.
- **SpeedTree's own billboards:** CSpeedTreeRT::SetCamera (0078D140; its name string at 00A8C390)
  keeps the camera's position at 00B429AC and its direction at 00B2B6D8; the per-frame caller
  0055FB05 hands it the camera's world position and its view direction (rotation column 0) times -1.
  The billboard geometry inside GetGeometry (0078C6F0's call of 007887A0 at 0078C77B, geometry bit 8)
  turns by atan2 of that direction - direction-based, so it would turn with the head. A wrapper giving
  each tree the direction from it to the camera was built and run (tree-billboards.txt at the lake,
  a 4-degree yaw sweep): it was never called - Oblivion draws no SpeedTree CPU billboards there - and
  it was taken out again.
So the trees that turn are none of the three as read; the pictures of a 52-pixel-a-step sweep do not
show it either (the pan-aligned difference is the same over trees, land and mountains). Next: what
Nadi sees - which trees (near the horizon or mid-distance), whether they turn like cards or their
leaves flicker - and two switches in the headset that split the candidates: Oblivion.ini [LOD]
bDisplayLODTrees=0 (the LOD trees gone) and OBVR's [Render] StableLeafBillboards=0 (the leaves'
basis back to the camera's).

**Tilt and pitch measured 2026-10-22, negative.** The hand script's head learned to roll (`head roll`,
the synthetic head's third angle). `tree-tilt.txt` (the lake, yaw 44 across the wooded hills, the head
level, rolled 20 each way, pitched up and down, then the trees toggled with TT and the distant land with
TLL) and `tree-tilt-90.txt` (rolled 90 and 60): at a quarter turn every tree in the picture, near and
on the distant hills, lies across it with the land - none stays upright with the head. A first reading
of the 20-degree pictures looked otherwise; it was the run's own fault (a command right after a mark is
already in the mark's picture, so the pictures showed the next pose - the scenarios now hold each pose
3 s past its mark). TLL takes the hills and the trees on them away, so those trees are the distant
land's LOD trees; TT left them. The distant LOD's vertex shaders (shaderpackage DISTLOD2002/2003.vso,
read with a CTAB and bytecode dump 2026-10-22) turn their instances by the horizontal part of EyeDir
(c14) only, with the vertical from the vertex - roll and pitch cannot tilt them either. So a turn
(tree-billboards.txt), a tilt and a nod of the head all leave the trees as they stand in one eye's
picture. Not yet looked at: the two eyes against each other, mid-distance SpeedTrees whose leaf setup
may take a path the leaf fix does not wrap (their cards would turn with a big yaw, which 4-degree steps
hide), and what Nadi sees - which trees and how they move.

**Cause found and fixed 2026-10-22.** Nadi in the headset: "die ganz weit entfernten bäume bewegen sich
mit dem headset. das war mal auch so mit laub nah. der fix war afaik das laub an den body zu koppeln".
The distant land's trees are drawn instanced by DISTLOD2003.vso, which stands each quad across the
horizontal part of EyeDir (c14) - the camera's VIEW direction: a turn of the head turns every distant
tree by the same angle. The pictures above could not show it: a quad kept square to the view looks the
same in one eye's picture wherever the head points (it is never foreshortened); what moves is its
orientation in the world, which the two eyes and the head's motion see. The "a turn ... leaves the
trees as they stand" above was a reading of the method's blind spot, not of the trees.
The fix is the near leaves' rule (render/LeafFacing.h faces by the place, not the view): the game's
shader is copied with a prologue that computes the instance's index and eye - tree (copies of its own
instructions for the fog), and its two EyeDir reads read the negated result instead
(render/DistantTreeFacing.h, pure, distant_tree_facing_test; the swap on SetVertexShader in
render/DistantTreeShader.cpp, each shader looked at once). `[Render] StableDistantTrees` (default 1)
switches it, hot reloaded. Checked on the game's own bytecode (shaderpackage002 and 019): patched, two
EyeDir reads, r5 free, 366 to 384 dwords; DISTLOD2002 (no EyeDir) and the shader model 1 copy are left
alone. Harness PASS headless: `distant-trees-on.txt` ("shader ... turns its trees by EyeDir - replaced",
"the place-faced shader set for the first time", the hills' trees drawn as before) and
`distant-trees-off.txt` (the control, nothing replaced). Not covered by the harness: the feel in the
headset - that the distant trees now stand still as the head turns.


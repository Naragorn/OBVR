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

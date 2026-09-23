# Issue #1: September 20 follow-up

The reporter has two failures. The save-load headset failure matches an existing
OBVR fix; the Steam startup crash has a symbolized fault location but no verified
root cause yet. Do not close both as fixed before the reporter retests.

## Evidence

- [Latest report](https://github.com/Naragorn/OBVR/issues/1#issuecomment-5737688654):
  VR starts through `obse_loader.exe`, but headset delivery stops after loading a
  save. Virtual Desktop, Steam Link and Meta Link all reproduce it.
- [Latest OBVR log](https://github.com/user-attachments/files/32406605/OBVR.log.txt)
  identifies **OBVR 0.1.1**, 2064x2208 eye textures and these raw frusta:
  left (-1.3764, 0.8391, -1.4281, 0.9657), right
  (-0.8391, 1.3764, -1.4281, 0.9657). Lines 471-479 show the first camera
  rebuild, a refused linear stretch, and `Submit returned 105 (left 0, right 105)`.
  The game continues logging afterward.
- [Startup report](https://github.com/Naragorn/OBVR/issues/1#issuecomment-5737497080):
  disabling the camera hook and rendering did not cure the Steam launch crash;
  launching through `obse_loader.exe` did. Logs from those disabled-hook runs have
  not been supplied, so their effective configuration is not independently verified.
- [Application event](https://github.com/user-attachments/files/32399717/Event.Viewer.-.Application.error.evtx.txt):
  access violation `c0000005`, `obse_1_2_416.dll` 0.22.13.0,
  timestamp `684b4fe5`, fault RVA `00086cc6`.
- [DXVK log](https://github.com/user-attachments/files/32399639/Oblivion_d3d9.log):
  identifies DXVK v3.1. The original issue's `10.0.17763.1` field should not be used
  as the DXVK release version.

Downloaded evidence and the local symbol lookup utility are in `artifacts/issue-1/`.

## Headset failure: existing fix applies to these inputs

With the gameplay frustum, the old cinema width is 2064 * 0.9 truncated to 1857,
and its 16:9 height is 1044. The view axes are x=1282 and x=781. Centering that
width produces horizontal rectangles [354, 2211] and [-147, 1710], both outside
the 2064-pixel texture. This is the same geometry defect addressed by
[2ad0101](https://github.com/Naragorn/OBVR/commit/2ad0101b23641e55a285fd1b15335ffe1dfc7f1f),
released in 0.1.2.

`FitFlatPicture` reduces the copy to 1562x878, which fits both eyes without
changing their relative scale. That commit also makes a refused flat copy fall
back to the mono game frame instead of the D3D11 test pattern. The original
pattern's right-eye 105 is not itself explained by this geometry fix.

The published [v0.2.1 source](https://github.com/Naragorn/OBVR/blob/v0.2.1/src/render/EyeMirror.cpp)
still calls `FitFlatPicture`. Recommend the complete current release and its
installation instructions, not a speculative new DLL patch. Confirm the next log
actually reports 0.2.1; an old DLL may remain installed if it still says 0.1.1.

Validation: added issue #1's exact dimensions to `tests/EyeGeometryTest.cpp`,
including the original overflow and both corrected rectangles. Built the test
directly with MSVC x86 and ran the complete eye-geometry executable: all checks
passed. The initial CMake/Ninja build did not progress after configuration and was
cancelled; this is not a completed CTest run. No game/headset reproduction was run.

## Startup crash: precise location, cause still open

The local xOBSE 22.13 DLL has the same timestamp as the event record. DbgHelp loaded
its matching PDB (`PdbUnmatched=0`) and resolved RVA 0x86cc6 to:

```
EventManager::HandleEventForCallingObject + 0x56
EventManager.cpp:1454
EventInfo* eventInfo = s_eventInfos[id];
```

The [22.13 source](https://github.com/llde/xOBSE/blob/22.13/obse/obse/EventManager.cpp#L1454)
indexes the event collection without a bounds check here. This identifies the
failing lookup, not the caller, event ID, or why the collection/index was invalid.
The reporter's DLL was not attached; timestamp/version agreement and matching
local symbols support the mapping, but a crash dump is needed to establish runtime
state. Do not turn an initialization-order hypothesis into a claimed diagnosis or
patch xOBSE from OBVR on that basis.

`obse_loader.exe` is a reporter-confirmed workaround for startup, not a root-cause
fix. If the Steam-only crash persists on the current OBVR release, obtain a crash
dump and `obse.log`/`obse_steam_loader.log` from that same launch. A comparison with
only OBVR.dll temporarily removed while DXVK remains installed would separate
plugin presence from the graphics-wrapper installation.

## Suggested reply (not posted)

Thanks, the new logs identify two separate problems. Your latest log still shows
OBVR 0.1.1. The headset stopping when you load a save matches the Quest cinema
rectangle bug fixed in 0.1.2: the menu copy extended outside the eye textures and
the fallback then stopped headset submission. Please install the complete 0.2.1
release following its current installation instructions, then launch through
obse_loader.exe as you did successfully. Please attach the new OBVR.log if loading
the same save still fails, and check that its first line says 0.2.1.

The Steam launch crash is separate. Your Windows event records an access violation
inside xOBSE 22.13; matching symbols place it in its event lookup. That is not enough
to identify the caller or cause yet. The loader gets around this startup problem
on your machine, but I am keeping that part open rather than calling it fixed.

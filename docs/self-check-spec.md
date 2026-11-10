# Install check: the doctor and the in-game self-check

Two layers, the same rules. Every requirement comes from the README.

1. **The doctor** (`OBVR-doctor.bat` → `OBVR-doctor.ps1` on Windows,
   `obvr-doctor.py` on Linux; shipped in `Data/OBSE/Plugins/`). The player runs it
   after installing, before the first start. It only reads files. One line per
   check, `OK` or `PROBLEM`, a `Fix:` line under each problem, and `INFO` for what
   it could not check. At the end: "All good - start the game!" or "N problems
   found". Exit code 0 / 1.
2. **The self-check** (`game/SelfCheck.cpp`, its rules in `core/SelfCheckLogic.h`).
   It runs at every start, as an xOBSE task on the game's loop. Each check writes
   `Self-check: <name> ok|PROBLEM|HINT|could not verify - <text>` to `OBVR.log`
   once it has an answer, and again only when the answer changes. When something
   is wrong, a window in the main menu (`OBVR_SelfCheck.xml`, OK = 9402) shows up
   to five lines, problems first. It shows once per start, only over the main menu,
   after every check except the controllers has its answer. `[SelfCheck]
   ShowPanel=0` keeps the window away; the log lines stay. The harness turns the
   window off for every run unless a scenario asks for it, the same way it does for
   the start guide.

The doctor exists for the case the self-check cannot cover: OBVR does not load at
all. It also prints the self-check's PROBLEM/HINT lines from the last `OBVR.log`.

## The checks and how each fact is read

Evidence is one of **measured** (read on the test installs on 2026-11-10),
**source** (read in the named code or docs) or **could not verify**.

| Check | Doctor reads | Self-check reads | Evidence |
|---|---|---|---|
| Oblivion 1.2.0.416 | `Oblivion.exe` FileVersion (VS_FIXEDFILEINFO) | OBSE's Query `oblivionVersion` = 0x010201A0 | measured: both installs' exe read 1.2.0.416. The SHA-256 differs between them because of the 4GB patch's LAA bit, so a hash is not used. |
| xOBSE ≥ 22.13 | `obse_1_2_416.dll` FileVersion 0.22.13.0 (minor, build); `obse_loader.exe` present | `obse.log`: `OBSE: initialize (version = 22.13 010201A0)` | source: xOBSE obse.cpp `_MESSAGE("OBSE: initialize (version = %d.%d %08X)")` (deepwiki llde/xOBSE). `OBSEInterface.obseVersion` carries only the major. measured: the line and the FileVersion 0.22.13.0. Only one version, 22.13, was read. |
| DXVK | Windows: `d3d9.dll` next to the exe holds `-gplasync` → OK. `DXVK` without it → PROBLEM. Neither → not DXVK. A 64-bit file → PROBLEM. Linux: OK (Proton's own) | the device answers `ID3D9VkInteropDevice` (render/GameDevice `IdentifyDevice`). Windows: the loaded d3d9.dll's file holds `-gplasync`, else a HINT. Proton: Proton's DXVK is OK | measured: Windows d3d9.dll has `v3.1.1-1-gplasync`; Proton's has `v3.1.1-97-…-pcos`. Wine detection is ntdll's `wine_get_version` export (measured in Proton Hotfix's ntdll.dll). A native device under Proton names `PROTON_USE_WINED3D` (source: deepwiki ValveSoftware/Proton). |
| dxvk.conf | the file next to the exe; both lines with their values | the file `DXVK_CONFIG_FILE` names, else `dxvk.conf` in the game folder | source: DXVK config.cpp reads `key = value`, `#` comments, true/false in any case, `DXVK_CONFIG_FILE` first, else the working directory (deepwiki doitsujin/dxvk). Limit: a working directory other than the game folder is not seen. Doctor extra: a `dxvk.conf.txt` is named (Windows hides `.txt`). On Linux another case of the name is named. |
| MenuQue v16b | `MenuQue.dll` and `MenuQue/Submodule.Game.dll`; the submodule's SHA-256 `b1f904…bc91` = v16b → OK, another hash → INFO | the loaded `Submodule.Game.dll`'s poll function byte for byte (`game/MenuQueSignature.h` `MenuQueModuleIsV16b`, the same test the native menus use) | measured: that hash on both installs, where OBVR.log says "MenuQue v16b poll verified". |
| Steam overlay off | the newest `userdata/<id>/config/localconfig.vdf`: `UserLocalConfigStore/system/EnableGameOverlay "0"` (off everywhere) or `UserLocalConfigStore/apps/22330/OverlayAppEnable "0"`. Anything else → on, Steam's default | `GameOverlayRenderer.dll` loaded → PROBLEM. Not loaded: Windows OK, Proton could not verify | source: Special K's `SKIF_Steam_isSteamOverlayEnabled` (github.com/SpecialKO/SKIF, src/stores/Steam/steam_library.cpp) reads it the same way. measured: the Windows install has `"22330" { … "OverlayAppEnable" "0" }` under `apps`; the Linux install has no such entry, so the overlay is on there. |
| OBVR's files | the 40 files of `kExpectedFiles` under Data (and in MO2's enabled mods), plus `OBVR.dll` | the same 40, by `BuildPluginPath` / `BuildGamePath("Data\\…")` | `tests/SelfCheckAssetsTest.py` keeps the C++ list, both doctors' lists and what the zip ships the same. |
| openvr_api.dll 32-bit | PE Machine 0x14C | the same, from the file's first 4 KB | the PE header layout. |
| Conflicts | `OBSE_EnhancedCamera.dll`, `OblivionReloaded.dll` in Data/OBSE/Plugins | the same names as loaded modules | the README names Enhanced Camera's file. `OblivionReloaded.dll` comes from "Oblivion Reloaded for Dummies" (nexusmods.com/oblivion/articles/44296). **Could not verify** the DLL names of ORC and E3, so neither is checked by name. |
| Trifle | `Trifle.ini` `EnableFirstPersonShadow=1` → PROBLEM | the same | the README. |
| SteamVR | `steamapps/common/SteamVR` in a Steam library | (it runs, or OBVR has no headset) | — |
| Controllers (Full VR) | — | SteamVR's `Prop_ControllerType_String` per hand (`knuckles` = Index) and the HMD's `Prop_ModelNumber_String` (1001) / `Prop_ManufacturerName_String` (1005). Index controllers on a headset whose model holds "Quest" or whose maker holds "Oculus"/"Meta" → HINT: turn off the Index controller emulation in Virtual Desktop or ALVR | source: the property IDs (deepwiki ValveSoftware/openvr). Virtual Desktop's "Emulate Index Controllers" (note.com/mint_milkshake, 2024-03-17) and ALVR's Index emulation (ALVR v20.11.0 release notes) exist. **Could not verify** which model/maker strings VD, ALVR, Link or Steam Link report, so this is a hint, not an error. Tester case: a Quest 3 that walked forward without end with the spell ring stuck open (2026-11-10, not confirmed as emulation). |
| OBVR.log | only INFO: missing = "not run yet", fine before the first start; with or without "OBVR ready"; the self-check's PROBLEM/HINT lines passed on | — | — |

## Finding the game and Mod Organizer 2 (doctor)

- **The game folder**, in this order:
  - the argument;
  - the doctor's own place: the zip puts it in `Data/OBSE/Plugins`, so up to four folders up;
  - the current folder;
  - every Steam library's `steamapps/common/Oblivion`. On Windows the libraries come from the registry's `HKCU\Software\Valve\Steam\SteamPath` and `Program Files`, then `steamapps/libraryfolders.vdf`. On Linux from `~/.local/share/Steam`, `~/.steam/steam` and the Flatpak path;
  - MO2's `gamePath`.
- **MO2**, in this order:
  - `--mo2` / `-Mo2`;
  - a `ModOrganizer.ini` in a folder above the doctor. That covers a portable instance and a global one, because with MO2 the doctor sits inside `<base>/mods/<mod>/OBSE/Plugins`;
  - on Windows, `%LOCALAPPDATA%\ModOrganizer\*\ModOrganizer.ini` with `gameName=Oblivion`.
- **What is read from MO2:**
  - `[General] selected_profile` and `gamePath` (`@ByteArray(...)`, doubled backslashes);
  - `[Settings] base_directory`, `mod_directory`, `profiles_directory` and `overwrite_directory`, with `%BASE_DIR%`. The defaults are `<ini folder>`, `/mods`, `/profiles` and `/overwrite`;
  - `profiles/<profile>/modlist.txt`, where `+Name` means enabled.
  - Data files are looked up in the real Data folder, then the overwrite folder, then the enabled mods. That shows whether a file is there, not which mod wins.
  - Source: deepwiki ModOrganizer2/modorganizer (the global/portable instance paths, `gamePath`, `selected_profile`, the `modlist.txt` prefixes, the mods default). The `profiles_directory` and `overwrite_directory` keys and their defaults **could not be verified** there, so an instance that moves them falls back to "could not read MO2's mod list", which the doctor says.
- **Linux:** paths are matched in any case, because Wine is case-insensitive. Under Wine, an MO2 `Z:\` path is the Linux root.

## Tests

- `tests/SelfCheckTest.cpp` (`self_check`): every check's flows, Windows against Proton, the readers (obse.log, dxvk.conf, Trifle.ini, PE header), the log lines and the panel's choice and lines.
- `tests/ObvrDoctorTest.py` (`obvr_doctor`): the doctor's readers and every check on fake installs. That includes Windows' and Linux' rules, Steam's overlay settings with two users, a second library, MO2 with mods, overwrite and a wrong profile, finding the game, and the whole run's output and exit code.
- `tests/SelfCheckAssetsTest.py` (`self_check_assets`): the four file lists alike, both release scripts pack the doctor, and the `.bat` keeps Bypass, `%~dp0`, `pause` and CRLF.
- `tests/HandScriptRunnerTest.py`: the harness turns the window off unless asked.
- Harness: `tools/hand-scripts/self-check.txt` (a world run: every check's line, no window) and `self-check-panel.txt` (main menu: a forced problem, `[Debug] ForceSelfCheckProblem=1`, opens the window with its lines).

## Open

- **Open bug - the PowerShell doctor has never run.** There is no PowerShell on the Linux test machine. Its logic mirrors the tested Python doctor, but it needs one run on Windows: double-click the `.bat`, both with a sound install and with a provoked problem.
- **Open bug - GOG and disc installs untested.** Without a Steam library the overlay and SteamVR checks say INFO. The game is found by the doctor's own place.
- **Open bug - MO2 untested on a real instance.** Only fake instances in the tests.
- **Open bug - `tools/package-release.ps1` packs no `Meshes/` folder** (the mirrored staggers), while `package-release.py` does. Found by these lists on 2026-11-10 and not fixed here: a zip built on Windows makes the doctor and the self-check report 4 missing files.
- **Open bug - the Linux test install lacks `OBVR_Controls.ini`.** The doctor measured it on 2026-11-10: `tools/build-linux.sh --assets` does not deploy it. The Windows install has it.

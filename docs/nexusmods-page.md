# Nexus Mods page for OBVR

Everything the upload form asks for, ready to paste. Field names follow Nexus Mods'
own guides (wiki.nexusmods.com "Creating a mod page", help.nexusmods.com "Best
Practices for Mod Authors", and the April 2026 upload walkthrough video). The
description is BBCode, which is what mod pages are stored as; the editor has a
BBCode view to paste it into. Keep this file in step with the README when the page
is updated.

## General

| Field | Value |
| --- | --- |
| Game | Oblivion (the 2006 game, not Remastered) |
| Name | `OBVR - Native VR` (Nexus advises against the game name and version in the title) |
| Category | Utilities (xOBSE and the Graphics Extender live there; "Visuals and Graphics" is the alternative) |
| Language | English |
| Version | 0.1.3 |
| Author | Naragorn |
| Classification | not adult |

**Short description** (shown on the mod card; two sentences at most):

> Native VR for the original Oblivion: real stereo, 6DoF head tracking, head-based aiming,
> HUD and menus in the headset, as an xOBSE plugin on SteamVR. Seated experience, early
> public test build, free and open source.

## Description (BBCode)

```
[size=5][b]OBVR - Oblivion (2006) in VR[/b][/size]

Native VR for the original [b]The Elder Scrolls IV: Oblivion[/b]. Not a cinema screen and not a depth trick: the world is drawn twice per frame, once per eye, at the headset's own resolution, with six-degrees-of-freedom head tracking. It runs as an xOBSE plugin on SteamVR through DXVK, and leaves the game exactly as it was on a machine without a headset.

[b]Not for the Remastered edition.[/b]

[b]Status: early public test build.[/b] One developer, one headset, one machine so far. The seated experience below works and is what this release is for; the standing experience with motion controllers is under construction and switched off. Please test, and please report what you see.

[size=4][b]What works[/b][/size]
Everything here has been confirmed in a headset.

[list]
[*][b]Dual-pass stereo.[/b] The world is rendered twice per game frame, once per eye, from cameras one interpupillary distance apart, into eye-sized targets at the headset's resolution. Alternate eye rendering is there as a fallback mode.
[*][b]6DoF head tracking.[/b] The head rotates and moves the camera; leaning works. The character never turns with the head. Locomotion stays with mouse, keyboard or gamepad.
[*][b]Head-based aiming.[/b] Bows, spells and melee go where the head looks, in first and third person, while the walking direction stays with the movement controls. In first person the weapon visibly points where the shot goes.
[*][b]One crosshair, at the depth the aim ray hits[/b], instead of a flat reticle that reads as two. Shown in third person as well, and only while it is of use.
[*][b]HUD, menus and dialogue in the headset.[/b] In-game menus keep the world behind them in stereo with Oblivion's own sepia pause look; the main menu, loading screens and videos take a cinema screen. The dialogue zoom is disabled.
[*][b]OBVR's own settings menu in the headset[/b] (Insert), and a hot-reloaded OBVR.ini for every setting.
[*][b]Recenter[/b] on a key (Del by default), [b]smooth turning[/b] as a comfort option, a [b]depth boost[/b] setting (EyeSeparationScale) for those who like more parallax.
[*]A first-start walkthrough in the headset.
[*]Works with and without the 4GB patch, and under Mod Organizer 2 without Root Builder.
[/list]

[size=4][b]Under construction, switched off[/b][/size]
The standing experience: motion controllers in both hands, swings that strike what they pass through, spells from the left hand, menus on the wrists. All of it is built, [b]none of it has been seen working in a headset, and it is not working as intended[/b]. It is off by default; [i][Hands] Enabled=1[/i] switches it on at your own risk.

Not built at all: snap turning, teleport, room-scale locomotion, physical interaction with objects by hand, Linux under Proton.

[size=4][b]Requirements[/b][/size]
[list]
[*]Oblivion [b]1.2.0.416[/b], 32-bit. Every edition with this executable version: the original release with the final patch and the Game of the Year edition alike. Steam (tested), GOG (untested), retail disc with the final patch (untested). OBVR checks the version and stays inactive on any other.
[*][url=https://www.nexusmods.com/oblivion/mods/37952]xOBSE[/url] 22.13 or newer.
[*][b]SteamVR[/b], and a headset it drives. OBVR talks OpenVR; there is no OpenXR path, because a 32-bit game needs a 32-bit runtime and SteamVR's OpenVR ships one.
[*][b]DXVK[/b] as the game's d3d9.dll. Not optional: it is what turns Oblivion's frame into a Vulkan image the SteamVR compositor accepts. Without it OBVR says so in the log and shows a test pattern.
[*]Windows 10 or 11.
[/list]
Tested on one headset (a Dream Air) with Valve Index controllers through SteamVR, an RTX 4090, Windows 11. Anything else is untested, which is exactly what reports are for.

[size=4][b]Installation[/b][/size]
[list=1]
[*]Install [url=https://www.nexusmods.com/oblivion/mods/37952]xOBSE[/url] as its page describes.
[*]Install [url=https://github.com/doitsujin/dxvk/releases/latest]DXVK[/url]: from the archive's [i]x32[/i] folder, put [i]d3d9.dll[/i] next to Oblivion.exe. Only that one file.
[*]Extract the OBVR archive into Oblivion's [i]Data[/i] folder. It contains OBSE/Plugins/OBVR.dll, OBSE/Plugins/OBVR.ini and the license text, nothing else. Under Mod Organizer 2 install it from the archive as an ordinary mod.
[*]Copy the [b]32-bit[/b] [i]openvr_api.dll[/i] from SteamVR, at [i]Steam\steamapps\common\SteamVR\bin\win32\openvr_api.dll[/i], into [i]Data\OBSE\Plugins\[/i] next to OBVR.dll. The 64-bit one from bin\win64 will not load. Under MO2 put it into the same folder of the OBVR mod.
[*]Start SteamVR, then start the game through the OBSE loader (Steam users: the Steam loader DLL does this for the normal Play button).
[/list]
The first start opens the walkthrough in the headset: choose the seated experience, set the comfort basics, done. [i]OBVR.log[/i] is written next to Oblivion.exe and opens with the OBVR, xOBSE and Oblivion versions found; further down it says whether DXVK answered ("DXVK") and whether SteamVR was reached ("OpenVR").

[b]Uninstalling:[/b] delete OBVR.dll, OBVR.ini, OBVR-crosshair.cache and openvr_api.dll from Data\OBSE\Plugins. OBVR writes nothing else and touches no save.

[size=4][b]Compatibility[/b][/size]
[list]
[*][b]Oblivion Reloaded, Oblivion Reloaded Combined, E3: incompatible.[/b] They take over the same engine functions OBVR needs (scene render, camera update, dialogue camera). OBVR refuses a site that is already patched and names the DLL that got there first in the log. There is no configuration that makes the two coexist.
[*][b]ENB[/b]: not tested. ENB replaces d3d9.dll, which is where DXVK has to sit.
[*][b]Enhanced Camera[/b]: not tested. It patches the first-person camera OBVR also reads.
[*]Other xOBSE plugins that do not touch the renderer or the player camera are expected to work; the usual engine fixes have been in the test load order throughout.
[/list]

[size=4][b]Known issues[/b][/size]
[list]
[*]The recenter key does nothing during the intro films; the picture rides the head until the main menu.
[*]Text entry (a character's name) still needs the keyboard.
[/list]

[size=4][b]Reporting a problem[/b][/size]
Attach [b]OBVR.log and OBVR.log.prev[/b] from the Oblivion directory. The log is the evidence; a report without it can rarely be acted on. It contains the install path and nothing else personal. Say what you did and what you saw (both eyes, one eye, the monitor, the log's last line), your headset, SteamVR version, GPU and driver, DXVK version, 4GB patch yes or no, MO2 yes or no, and the other xOBSE plugins loaded. The bug tracker here or the issue form on GitHub both work.

[size=4][b]Source, license, support[/b][/size]
OBVR is free and open source under the GPL-3.0: [url=https://github.com/Naragorn/OBVR]github.com/Naragorn/OBVR[/url]. Every build reaches that repository. HANDOFF.md there is the engineering record: every hook, every address, every dead end.

If it puts you in Cyrodiil and you want to buy a coffee for the evenings it took, there is a Patreon: [url=https://www.patreon.com/naragorn]patreon.com/naragorn[/url]. Patrons get new builds of this and future VR mods before they go public.

[size=4][b]Credits[/b][/size]
llde and the xOBSE team, for the script extender this runs in. The DXVK project, for the Direct3D 9 to Vulkan layer and its interop API that makes the eye textures reach SteamVR. Valve, for OpenVR.
```

## Documentation tab

- **ReadMe**: paste `README.md` as plain text, or the Requirements, Installing,
  Uninstalling and Reporting sections of it.
- **Changelog**: one entry per release. For 0.1.3:

  > 0.1.3: the game's antialiasing no longer stops the headset at the first world frame.
  >
  > 0.1.2: fixes the first outside report (Quest 3 through Air Link): the cinema picture
  > for menus and loading screens reached past the eye texture on lenses whose view axis
  > sits far off centre, the copy was refused, and the headset fell to SteamVR's loading
  > view for the session as soon as the world appeared. The picture now shrinks to fit
  > both eyes, and a failed copy shows the plain picture instead of stopping. INI note
  > under MenuScale.
  >
  > 0.1.1: defaults now match the tested settings. Crosshair on, shown in third person as
  > well, in both views only while it is of use; EyeSeparationScale 1.0. README describes
  > the depth boost and the alternate eye rendering mode.
  >
  > 0.1.0: first public test build.

## Files tab

| Field | Value |
| --- | --- |
| File name | `OBVR 0.1.3` |
| File version | 0.1.3 |
| Latest version | yes |
| Category | Main Files |
| Main Vortex file | yes |
| Description | Extracts into Oblivion's Data folder: OBSE/Plugins/OBVR.dll, OBVR.ini and the license. Needs xOBSE, DXVK and SteamVR's 32-bit openvr_api.dll next to the DLL, see the description. |
| Archive | `dist/OBVR-0.1.3.zip`, the same file as the GitHub release |

The zip is already laid out the way Nexus recommends: the game-relative folder
structure, no extra parent folder.

## Requirements tab

| Requirement | URL | Note |
| --- | --- | --- |
| Oblivion Script Extender (OBSE xOBSE) | https://www.nexusmods.com/oblivion/mods/37952 | 22.13 or newer; pick it from the Nexus list so it links |
| DXVK | https://github.com/doitsujin/dxvk/releases/latest | 32-bit d3d9.dll next to Oblivion.exe, from the x32 folder |
| SteamVR | https://store.steampowered.com/app/250820/SteamVR/ | and its 32-bit openvr_api.dll next to OBVR.dll |

Official DLC requirements: none.

**Mirrors:** GitHub Releases, https://github.com/Naragorn/OBVR/releases.

## Permissions tab

OBVR is GPL-3.0, which is broader than any of the preset options, so tick "specify my
own permissions" and use this text:

> OBVR is licensed under the GNU General Public License v3.0. You may use, modify,
> redistribute and build on it under the terms of that license: keep the license and
> the copyright notice, publish the source of any modified version you distribute
> under the same license, and say what you changed. No further permission is needed and
> none will be refused. Source: https://github.com/Naragorn/OBVR

- Third-party content: none. The archive contains only OBVR's own DLL, INI and the GPL
  text. `openvr_api.dll` is deliberately not included; the description tells users to
  take it from their SteamVR install.
- Credits field: the same names as the Credits section of the description.
- Donations: on, with the Patreon link. (Whether Nexus lets a mod page link Patreon as
  the donation target or only shows a generic switch is something to confirm in the
  form; the help articles read for this file only mention a "Donations" switch.)

## Media tab

Nexus recommends 1920 x 1080 screenshots, the first one becomes the thumbnail, the
first five are visible without scrolling, and a header of about 1300 x 372. Text
overlays are discouraged. The SteamVR mirror window and the game's monitor window both
give flat shots; the SteamVR mirror is the one that shows the headset's picture.

Suggested shots, in order:

1. The Imperial City or Weynon Priory from the headset's view, weapon drawn, crosshair
   at depth (thumbnail).
2. Third person with the borrowed crosshair.
3. An in-game menu with the stereo world behind it (inventory or map).
4. A dialogue, to show the face without the zoom.
5. The OBVR settings menu in the headset.
6. The first-start walkthrough.
7. A side-by-side of both eye textures (the SteamVR mirror in "both eyes" mode) as
   proof of real stereo.

Header: a wide crop of a landscape shot.

## Tags

Pick from the list the form offers; the ones that fit OBVR's content are the VR,
camera, user interface and utility kind. Do not tag generative AI: none was used for
the mod's assets or images. (The exact tag names on Oblivion Nexus were not checked
for this file.)

## Publishing checklist

- [ ] Draft created, game Oblivion, category Utilities.
- [ ] Short description and BBCode description pasted; preview checked, lists render.
- [ ] Files: OBVR-0.1.3.zip uploaded as Main File, version 0.1.3, main Vortex file.
- [ ] Requirements: xOBSE linked from the Nexus list, DXVK and SteamVR as external.
- [ ] Permissions: own text (GPL-3.0), credits, donations with Patreon.
- [ ] Documentation: readme and changelog.
- [ ] Media: at least five 16:9 shots and a header.
- [ ] Mirror: GitHub Releases.
- [ ] Preview, then publish. Then add the Nexus link to README.md under Installing.

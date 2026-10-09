# Nexus Mods page for OBVR

Everything the upload form asks for, ready to paste. Field names follow Nexus Mods'
own guides (wiki.nexusmods.com "Creating a mod page", help.nexusmods.com "Best
Practices for Mod Authors"). The description is BBCode, which is what mod pages are
stored as; the editor has a BBCode view to paste it into. Keep this file and
`docs/nexus-description.bbcode` in step with `README.md` when the page is updated
(2026-10-09: both rewritten from the README of 0.2.x; the earlier text here was the
0.1.3 page).

## General

| Field | Value |
| --- | --- |
| Game | Oblivion (the 2006 game, not Remastered) |
| Name | `OBVR - Oblivion in VR` (Nexus advises against the version in the title) |
| Category | Utilities (xOBSE and MenuQue live there; "Visuals and Graphics" is the alternative) |
| Language | English |
| Version | 0.2.1 (the latest GitHub release; 0.2.2 is the version in the tree, unreleased) |
| Author | Naragorn |
| Classification | not adult |

**Short description** (shown on the mod card; two sentences at most):

> Play the original Oblivion in real VR: real 3D, head tracking, aim with your head,
> HUD and menus in the headset, as an xOBSE plugin on SteamVR. Seated play works today,
> motion controllers are coming; early test build, free and open source.

## Description (BBCode)

The whole description is in **`docs/nexus-description.bbcode`**: the README turned into
Nexus BBCode, same words, same order, with the README's tables as lists (Nexus BBCode has
no tables), the Files-tab download next to the GitHub one, and the Bugs tab next to
Discord and GitHub issues. Paste the file's contents into the editor's BBCode view.

What the BBCode uses, all of it supported by Nexus: `[size=5]`/`[size=4]` with `[b]`
for the headings, `[list]`/`[list=1]` with `[*]`, `[url=...]`, `[i]`, `[code]` for the
dxvk.conf line.

## Documentation tab

- **ReadMe**: paste `README.md` as plain text, or its Requirements, Installing,
  Uninstalling and Found-a-bug sections.
- **Changelog**: one entry per release, from the GitHub releases:

  > 0.2.1: fixed the remaining sneak-eye fragment displayed in the Oblivion main menu.
  >
  > 0.2.0: rendering fixes and MenuQue settings. Fixed foliage orientation and
  > visibility issues when looking around. Improved water reflections so they remain
  > stable during headset movement while preserving animated waves. Moved OBVR's
  > in-game settings to MenuQue. Disabled Full VR selection and marked it "Under
  > construction". Fixed the sneak icon appearing in the main menu. Fixed main-menu
  > recentering not carrying over when loading a save. Fixed water reflections and
  > stereo player rendering after toggling reflections off and on in-game.
  >
  > 0.1.3: the game's antialiasing no longer stops the headset at the first world frame.
  >
  > 0.1.2: fixes the first outside report (Quest 3 through Air Link): the cinema picture
  > for menus and loading screens reached past the eye texture on lenses whose view axis
  > sits far off centre, the copy was refused, and the headset fell to SteamVR's loading
  > view for the session as soon as the world appeared. The picture now shrinks to fit
  > both eyes, and a failed copy shows the plain picture instead of stopping.
  >
  > 0.1.1: defaults now match the tested settings. Crosshair on, shown in third person as
  > well, in both views only while it is of use; EyeSeparationScale 1.0.
  >
  > 0.1.0: first public test build.

## Files tab

| Field | Value |
| --- | --- |
| File name | `OBVR 0.2.1` |
| File version | 0.2.1 |
| Latest version | yes |
| Category | Main Files |
| Main Vortex file | yes |
| Description | Extracts into Oblivion's Data folder: OBSE/Plugins/OBVR.dll, OBVR.ini, the GPL license, the SteamVR bindings (OBVR_Input), the 32-bit openvr_api.dll with its license, and the MenuQue menus (Menus/Generic/OBVR_*.xml, Menus/Prefabs/OBVR). Needs xOBSE, DXVK, MenuQue v16b and SteamVR - see the description. |
| Archive | the GitHub release zip of the same version (`tools/package-release.ps1` builds it into `dist/`) |

The zip is already laid out the way Nexus recommends: the game-relative folder
structure, no extra parent folder.

## Requirements tab

| Requirement | URL | Note |
| --- | --- | --- |
| Oblivion Script Extender (xOBSE) | https://www.nexusmods.com/oblivion/mods/37952 | 22.13 or newer; pick it from the Nexus list so it links |
| MenuQue | https://www.nexusmods.com/oblivion/mods/32200 | v16b, for the in-headset menus; without it the fallback menus |
| DXVK | https://gitlab.com/Ph42oN/dxvk-gplasync/-/releases | the gplasync build, newest entry, its zip; 32-bit d3d9.dll next to Oblivion.exe, from the x32 folder, plus the dxvk.conf line |
| SteamVR | https://store.steampowered.com/app/250820/SteamVR/ | installed and running; its client library ships with OBVR |

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

- Third-party content: Valve's `openvr_api.dll` (OpenVR, BSD-3-Clause), shipped with
  its license as `openvr_api-LICENSE.txt`; see `third_party/openvr/README.md`.
  Everything else is OBVR's own.
- Credits field: the names from the "Thanks to" section of the description.
- Donations: on, with the Patreon link.

## Media tab

Nexus recommends 1920 x 1080 screenshots, the first one becomes the thumbnail, the
first five are visible without scrolling, and a header of about 1300 x 372. Text
overlays are discouraged. The SteamVR mirror window shows the headset's picture.

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

Promotional image ready to upload from the repository: `obvr.png` (an Imperial Legion
guard wearing a Quest 3 headset). It is AI-generated and not part of the mod; disclose
or tag it according to Nexus's current media policy. The in-game 16:9 captures remain
the primary screenshots.

## Tags

Pick from the list the form offers; the ones that fit OBVR are the VR, camera, user
interface and utility kind. (The exact tag names on Oblivion Nexus were not checked for
this file.)

## Publishing checklist

- [ ] Draft created, game Oblivion, category Utilities.
- [ ] Short description and `docs/nexus-description.bbcode` pasted; preview checked,
      lists and the code line render.
- [ ] Files: the 0.2.1 release zip uploaded as Main File, version 0.2.1, main Vortex file.
- [ ] Requirements: xOBSE and MenuQue linked from the Nexus list, DXVK and SteamVR as
      external.
- [ ] Permissions: own text (GPL-3.0), credits, donations with Patreon.
- [ ] Documentation: readme and changelog.
- [ ] Media: at least five 16:9 shots and a header.
- [ ] Mirror: GitHub Releases.
- [ ] Preview, then publish. Then add the Nexus link to README.md under Installing.

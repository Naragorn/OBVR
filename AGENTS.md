# Repository instructions

## Quality and scope rules

- Prefer root-cause fixes over workarounds.
- Require hard, reproducible proof and state assumptions explicitly.
- Exercise every meaningful logic flow in tests, including relevant branch combinations, refusal paths, and fallback paths. Extract decision logic into testable units when needed.
- Preserve existing behavior unless a change is explicitly intended.
- Keep changes focused and minimal.
- Do not hide unexpected failures or claim coverage that was not run.
- Code, comments, documentation, and Git commit messages must be in English.
- No giant functions or files. A new function stays at or under 100 lines and a new file at or under 1000 lines; put new logic in its own small unit (a pure logic header plus a thin caller) instead of growing a big one. Functions and files already over those limits are recorded in `tests/function-sizes.txt` and may only shrink: `function_sizes_test` fails when one grows. When it does, split the code, never raise the table; lower the table when code shrinks (`function_sizes_test --print > tests/function-sizes.txt`).

## Words the player reads

- Everything a player reads in the game - the guide, the hints, the settings' names and help lines, the controls card, notices - is written for gamers, not for developers (Nadi, 2026-11-15: "absofort keine zu technischen erklärungen oder ähnliches. wir dealen mit gamern"). Short, plain, about what the player does and feels; no engine words (frames, pixels, units, INI keys, addresses, function names), no internals of how it works, no numbers a player cannot act on. One name means one thing: no two rows called the same.
- The technical side belongs in the code's comments, the specs and the log, never in the text a player sees.
- Line breaks that read well, in every menu and the guide (Nadi, 2026-11-15: "achte auch auf ansprechendere zeilenumbrüche"): break lines yourself at the end of a thought - a sentence, a clause, a list - instead of leaving it to the wrap; one idea a paragraph, a blank line between; no line ending on a lone word of the next thought, no single word alone on the last line. In MenuQue texts a line break is `\n` in the C++ string (NativeTextCommand writes xOBSE's `%r`); check the result in the layer's dump (`[Debug] LayerDump`), not by counting characters.

## Performance

- Build everything extremely performant, and make what already exists faster too - whatever can be optimized is optimized, new code and old (Nadi, 2026-11-13). The game runs in VR: a frame has about 11 ms for everything, and a single long frame is a stutter in the headset.
- Cost first: before writing a feature, decide what must run every frame, what once a second and what only on an event. Per frame only what has to be: no allocation, no file I/O, no console line, no lookup that can be kept, no loop over n x m.
- Measure, never guess: every optimization carries a number from before and one from after, taken under the same load; the mean and the worst frame both count. An instrument's own cost is measured too.
- Here: use the plugin's own performance instruments and the harness runs (see `docs/` and the hand-script scenarios); the two eye passes and everything run per eye count twice.
- Write each measured result where the project keeps its findings (the spec or the knowledge files), with the commit, so the next change can be compared against it.

## Running the harness on Linux

- Full detail: `docs/hand-script-harness.md`, "Without SteamVR" and "Several runs at once". The short of it, since 2026-11-17:
- The default for a scenario about the game itself (hands, menus, the laser, combat, movement) - no SteamVR, no deploy, beside any other run:
  `python3 tools/hand-script-run.py tools/hand-scripts/<scenario>.txt --headless --no-steamvr --parallel --plugin build-linux-clangcl/OBVR.dll`
- `--no-steamvr`: SteamVR does not have to run; OBVR loads the stand-in runtime `openvr_mock.dll` (built with `OBVR.dll`). Steam itself still has to run.
- `--parallel`: the run takes one of four slots under `~/.local/share/obvr-harness` - its own mirror of the game folder and its own Proton prefix. The player's game is not touched, so such a run needs no busy/free with other sessions. `--plugin FILE` (repeatable) puts a build under test into the mirror's plugin folder (`OBVR.dll`, `OBNG.dll`, an INI); files elsewhere in `Data` (menus, textures, an `.esp`) are the player's, linked, and still need a deploy.
- The stand-in's headset is the Pimax Dream Air by default (4036x3376 an eye, 72 Hz, IPD 63 mm); `--headset beyond` is the Bigscreen Beyond (2568x2568, 75 Hz) the older Linux scenarios were written with; `--hz`, `--eye WIDTHxHEIGHT`, `--ipd MM` over the headset's own.
- What such a run does not test: anything SteamVR does itself - compositing, overlays shown, the loading dome, frame statistics and reprojection, real controllers. Performance numbers of a run without SteamVR are not comparable with one with it. A scenario about those runs plain (no `--no-steamvr`), needs SteamVR running, and is announced with busy/free as before; so is every deploy (`tools/build-linux.sh`), which waits for every game to end, a slot's too.
- A scenario that writes a save for later runs (`make-harness-save*.txt`) runs without `--parallel`: a slot's saves stay in the slot.
- Start several runs at once from a bash script with `&` and `wait`; never wait with `pgrep -f` on the runner's name (it finds the waiting shell itself).
- On Windows: `tools/hand-script-run.ps1 -NoSteamVR` (with `-Headset`, `-Hz`, `-Eye`, `-Ipd`) is written and has not run yet; there are no slots there.

## Findings for OBNG

- OBNG (Oblivion Next Generation) is the planned community modernisation of Oblivion.exe 1.2.416: reverse engineering the engine and rebuilding it in modern form. Its knowledge lives outside this repo, in `../OBNG/knowledge/` (`/run/media/nadi/Games/Modding/OBNG/knowledge/`).
- Whenever work here turns up a finding that would matter to OBNG - how the engine does something, where it does it, what it costs, or how a modern engine would do it instead - write it there as you go, not only at the end.
- One file per topic, as `../OBNG/README.md` describes: what was read in the EXE (addresses), what was measured in the game, and what a modern engine does instead, each with its source. Add to an existing topic file before starting a new one, and mark what is read, measured, derived or unverified.

## Reading Oblivion.exe

- Every function of the EXE is already decompiled: `../OBNG/re/export/rea-all/functions/0x<address>.c` (`/run/media/nadi/Games/Modding/OBNG/re/export/rea-all/` on Linux, `D:\Modding\OBNG\re\export\rea-all\` on Windows; on the shared disk only, not in Git - `../OBNG/re/README.md` says how it was made). Grep it before starting a Ghidra or REA session, and start one only for what the files do not answer (assembly, cross-references, data).

## Keeping the OpenWiki current

- No scheduled OpenWiki workflow runs in this repository: the configured provider has no unattended login for CI, so the sentence about a scheduled workflow in the block below does not apply here. Refresh the wiki with `openwiki --update` or through the OpenWiki skill in Claude Code.

<!-- OPENWIKI:START -->

## OpenWiki

This repository has a generated `openwiki/` evidence index. It is optional just-in-time context, not required startup reading.

- Do not enumerate, preload, or search wikis at task start. Use retrieval when the user asks for it, when unfamiliar architecture or dependency behavior materially affects the task, or when source inspection leaves an important uncertainty. Stop once the question is grounded.
- When those conditions apply and OpenWiki retrieval tools are available, use `openwiki_search` for just-in-time context and `openwiki_read` for the relevant complete sections. If search returns `workspace_required`, ask which listed workspace to use and retry with its ID.
- Use `openwiki_list_workspaces` or `openwiki_list_wikis` when workspace membership itself needs to be discovered.
- If the retrieval tools are unavailable, read `openwiki/quickstart.md` and follow its links to the relevant pages.
- Treat source code and tests as authoritative. A brief's unknowns and review items are verification gaps, not automatic requirements.
- Prefer the narrowest quiet validation that proves the changed behavior. Preserve complete failure output.

Use the repository's documented maintenance process to refresh generated pages after source changes. Check its configuration and completed runs before relying on automatic updates. Do not hand-edit generated OpenWiki pages unless explicitly asked; prefer updating source code/docs and letting OpenWiki regenerate.

<!-- OPENWIKI:END -->

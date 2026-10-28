# Repository instructions

## Quality and scope rules

- Prefer root-cause fixes over workarounds.
- Require hard, reproducible proof and state assumptions explicitly.
- Exercise every meaningful logic flow in tests, including relevant branch combinations, refusal paths, and fallback paths. Extract decision logic into testable units when needed.
- Preserve existing behavior unless a change is explicitly intended.
- Keep changes focused and minimal.
- Do not hide unexpected failures or claim coverage that was not run.
- Code, comments, documentation, and Git commit messages must be in English.

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

The scheduled OpenWiki GitHub Actions workflow refreshes the repository wiki. Do not hand-edit generated OpenWiki pages unless explicitly asked; prefer updating source code/docs and letting OpenWiki regenerate.

<!-- OPENWIKI:END -->

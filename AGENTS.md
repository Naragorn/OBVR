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

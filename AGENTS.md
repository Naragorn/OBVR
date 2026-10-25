# Repository instructions

## Quality and scope rules

- Prefer root-cause fixes over workarounds.
- Require hard, reproducible proof and state assumptions explicitly.
- Exercise every meaningful logic flow in tests, including relevant branch combinations, refusal paths, and fallback paths. Extract decision logic into testable units when needed.
- Preserve existing behavior unless a change is explicitly intended.
- Keep changes focused and minimal.
- Do not hide unexpected failures or claim coverage that was not run.
- Code, comments, documentation, and Git commit messages must be in English.

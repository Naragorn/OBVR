# Rendering checkpoint — 2026-09-20

This is a work-in-progress checkpoint, not acceptance of all rendering fixes.

- User confirms character alignment immediately after loading is fixed.
- User reports live Water Reflections OFF -> ON still causes missing reflections and a displaced player body in the right eye. This remains open; the earlier successful capture was not sufficient acceptance evidence.
- Walking reflection flicker remains under investigation. Earlier disabled/enabled captures used different camera viewpoints and do not establish an identical root cause.
- Current automated suite: 59/59 CTest targets pass on 2026-09-20 outside sandbox restrictions. The sandbox run failed in Python temporary-directory access; it was rerun successfully without changing tests.
- No new in-game acceptance test was performed for this checkpoint.
- Source, tests, harness tools and the existing project image change are included. Large local captures, machine-specific build helpers and Python caches remain local.

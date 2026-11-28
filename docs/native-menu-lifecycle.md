# Native personal menu lifecycle

OBVR observes the Oblivion interface manager's real active stack. The
insertion entry at `0x0057D640` and removal entry at `0x0057CFE0` are guarded
with the exact 1.2.0.416 prologues before either is patched. Each wrapper calls
the original first, preserves its signed return value, and reads the ten
stack entries at `InterfaceManager + 0xE0` plus the root at `+0x68` only after
the engine mutation completes.

The personal Tab pages are represented by stack id `1`. The pointer at
`InterfaceManager + 0x68` is the shared UI root, not a personal-menu tile
identity; the bridge pairs that observed root with the personal stack anchor
and a generation. Keeping the anchor while pages or child modal entries
change preserves one session identity. An anchor disappearance creates a
close observation; a later appearance creates a new generation even when the
shared root pointer is reused. A root replacement while the anchor remains is
a replacement event and invalidates the previous identity. A foreign stack
after anchor removal is recorded as foreign, so no close request can target
it.

`RequestController` separates a queued command from an observed engine event.
Opening requires available, focused, loaded gameplay and an empty stack.
Closing requires the current owned root and generation and focus, but does not
require interaction, cursor geometry, or a visible child page. The
`RequestMailbox` is the only cross-thread seam: producers publish one fixed
request and the game thread consumes it before any native call. Lifecycle
state and engine calls remain game-thread owned.

The bridge does not claim that xOBSE `TapKey` can be revoked. Cancelling before
dispatch removes the mailbox request. Cancelling after dispatch reports that
the queued input cannot be revoked; a later personal insertion is ambiguous
unless an adapter supplies explicit synchronous engine causality. The bridge
therefore never closes a late or foreign observation as if it were owned.
There is no live Tab input adapter, laser/touch path, or rendering change in
this slice.

The source ABI excerpts and guards are in `src/game/GameAddresses.h` and
`src/game/VRMenuBridge.cpp`; pure flow coverage is in
`tests/VRMenuBridgeTest.cpp`. `InstallVRMenuBridge()` is not called from the
default plugin load path yet: the feature has no user setting or verified
native opener, so leaving the detour uninstalled preserves legacy/default-Off
runtime behavior until the next adapter decision.

## State-change audit (2026-11-28)

Checked against the rule set "State changes without glitches or side effects"
(`D:\Modding\claude-md-state-changes.md`; principles and labels as in
native-menus.md's audit section). Code read at origin/main 5214464 (the cited files are the same as at 88dae75
unless a line says otherwise); nothing built
or run.

Findings:

- **The explicit lifecycle machine is not on the live path (principle 3).** The bridge with generations and owned/foreign/replaced events is
  installed only with `[Debug] NativeMenuLifecycleProbe=1` (default 0):
  `src/Plugin.cpp:105-108`, `src/core/Config.h:124`,
  `src/game/VRMenuBridge.cpp:458-476`. OBVR's own windows and the game's menus
  are tracked elsewhere without it - by the generic root pointer
  (`src/game/NativeMenuPrototype.cpp:597`) and by reading the top menu each
  frame. Read. Fix: either wire the bridge's generation into the native windows'
  ownership or record here that it stays a diagnostic.
- **This document is stale (verification: what is claimed must match).** It says `InstallVRMenuBridge()` is not called
  from the load path and there is no adapter. Since then a probe path exists
  (`InstallNativeMenuLifecycleProbe`, the log names "diagnostic F9=open
  F10=close"). Read. Fix: one paragraph on the probe and its switch.

Complies (read, from this document and the code it names): each wrapper calls
the original first and reads the stack only after the engine's change; a
reused root after an anchor's disappearance is a new generation; a late or
foreign observation is never closed as owned; a queued TapKey is not claimed
revocable. Flows are unit-tested in `tests/VRMenuBridgeTest.cpp`;
`tests/NativeMenuLifecycleHarnessTest.ps1` exists.

Not verified: the probe's F9/F10 run in the game - no harness log of it was
read; whether any live menu path still needs the generation (a foreign generic
menu reusing OBVR's root address after an outside close) - a per-frame log of
root address and owner across an Escape-close and a mod's ShowGenericMenu
would show it.

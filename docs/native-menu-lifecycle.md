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

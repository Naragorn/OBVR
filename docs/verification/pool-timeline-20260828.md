# Pool timeline of a collapsing frame (2026-08-28)

One dual-pass frame captured from the live game while a body was collapsing
in the headset, standing next to an NPC inside the specular distance.
`pool-timeline-20260828.txt` is the raw dump; this is what it establishes.

## The two passes are identical

Split at the markers, pass one and pass two are event-for-event equal - a
plain `diff` of the two halves finds nothing. Same locks (offset, size,
DISCARD flag), same unlocks with the same 256-byte write fingerprints, same
draws from the same buffers in the same order. Whatever collapses one eye's
bodies is not visible as any difference in what the two passes ask of D3D9.

## The pool is small and hammered

Four dynamic buffers appear. One (`4FD14DD8`, ~20KB) takes 40 DISCARD locks
in a single pass - the software-skinning scratch buffer, refilled per body.
A second (`4FD8E4C8`, ~112KB) takes 14. A third is the between-pass HUD
buffer. A fourth is only ever drawn, never locked here - packed in an
earlier frame.

## The isolated traffic does not reproduce

`tools/DxvkRepro` replays this exact sequence twice per frame against a real
d3d9.dll, with deterministic vertex data, and compares the two passes pixel
for pixel. Against the system runtime and against the game's own DXVK 3.0.2
(confirmed loaded via its log), both passes render identically, at repeat
factors up to 32. So the discard traffic alone, in a plain fixed-function
context, is not the collapse. What the isolation strips away - the real
skinning vertex shader, or OBVR's own mid-frame interop - is where the
cause now has to be. That is the next iteration's target.

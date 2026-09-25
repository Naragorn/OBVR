#pragma once

#include "core/Types.h"

namespace obvr::game {

// Whether the first-person model - the hands and what they hold - is drawn
// against the world's depth or on top of everything.
//
// Vanilla draws the first-person node in its own RenderObject pass after the
// world (docs/vr-modding/engine-behavior.md) and clears the depth buffer just
// before it, so a weapon never sinks into a wall on a flat screen. In
// Oblivion.exe 1.2.0.416:
//
//   0040CDFE  38 88 0C 02 00 00   cmp  [eax+20Ch], cl
//   0040CE04  75 10               jne  0040CE16        ; skip the clear
//   0040CE06  8B C8 8B 11         mov  ecx, eax / mov edx, [ecx]
//   0040CE0A  8B 82 3C 01 00 00   mov  eax, [edx+13Ch] ; the renderer's clear
//   0040CE10  6A 04 6A 00 FF D0   push 4, push 0, call eax
//   0040CE16  ...                 the first-person RenderObject follows
//
// (the 4 is the depth-buffer flag of the Gamebryo clear, as far as this
// reading goes - the call's name is not documented anywhere OBVR has read).
// In VR that clear put the hands in front of everything, as if on the HUD:
// a hand reaching for an object never went behind or into it (2026-09-25).
// Keeping the depth turns the jne into a jmp, so the clear is skipped and
// the hands are hidden by what stands in front of them, as in the world.

inline constexpr UInt32 kFirstPersonDepthClearBranch = 0x0040CE04;
inline constexpr UInt8 kJumpIfNotEqualShort = 0x75;
inline constexpr UInt8 kJumpShort = 0xEB;

// The byte the branch should hold, or 0 when the byte found is neither form
// of this branch - another build or another patch, which is left alone.
inline UInt8 FirstPersonDepthBranchByte(UInt8 found, bool keepDepth) {
	if (found != kJumpIfNotEqualShort && found != kJumpShort) {
		return 0;
	}
	return keepDepth ? kJumpShort : kJumpIfNotEqualShort;
}

// Brings the branch to the wanted form. Safe to call every frame: the byte is
// only written when it has to change. Answers whether the wanted form is in
// place.
bool KeepFirstPersonDepth(bool keepDepth);

}  // namespace obvr::game

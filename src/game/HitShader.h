#pragma once

#include "core/Types.h"

namespace obvr::game {

// The hit blur switched off ([Look] NoHitBlur). When the player is hit, the
// game blurs the view and shakes the picture sideways - Oblivion's
// GethitShader ([GethitShader] fBlurAmmount, fHitTexOffset,
// fBlockedTexOffset in Oblivion.ini). NoPlayerStagger leaves it, and in a
// headset it is a distortion some want gone (2026-09-29).
//
// Read in Oblivion.exe 1.2.0.416:
//
// - Startup, 0x0040E8A2..0x0040E8ED, hands the three INI values to
//   0x007B4870, which keeps them as the hit offset 0xB46128, the blocked
//   offset 0xB4612C and the blur 0xB46130.
// - Two starts write the effect's running offset 0xB46124 and strength
//   0xB46120, both gated on the byte 0xB2D91C:
//   0x007EB010 (cdecl, one bool: blocked) - the hit or blocked offset and
//   strength 1.0 or 0.3; called at 0x00600574 for the player's blocked hit.
//   0x007EB080 (cdecl, one float: amount) - called at 0x005E71C4 for the
//   player (1.5), at 0x0065DD03 with a distance falloff, and by the console
//   command TriggerHitShader at 0x00506492.
// - Every frame, 0x007EB918..0x007EB9CE lets offset and strength decay and
//   turns them into the picture's displacement and blur.
//
// Both starts are detoured at their entries. With the setting on they return
// at once - the caller cleans the stack, so an early return is safe - and an
// effect already running decays on its own. With it off, the original runs.
inline constexpr UInt32 kHitShaderStartBlocked = 0x007EB010;
inline constexpr UInt32 kHitShaderStartAmount = 0x007EB080;

// Whether a start of the hit blur runs.
inline bool HitShaderRuns(bool noHitBlur) { return !noHitBlur; }

void InstallHitShader();
void SetNoHitBlur(bool enabled);

}  // namespace obvr::game

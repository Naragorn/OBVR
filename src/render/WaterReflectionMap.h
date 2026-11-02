#pragma once

// The size of the engine's water reflection map, and whether OBVR makes it
// bigger.
//
// WaterManager::RenderReflections (0x0049BEF0) creates the reflection map on
// its first run as a square rendered texture of 256 by 256 texels: the push
// of the immediate 0x100 at 0x0049BFAE (`68 00 01 00 00`, read in the EXE
// 2026-11-02; TES Reloaded writes the same site, Hooks.cpp "ReflectionMapSize
// // Constructor"), the argument to the renderer's create-by-size routine
// 0x007C2420 (width = height = size, then 0x007C1B50). The water pixel
// shader samples that map through the capture's projective matrix, so in the
// headset one texel of it spans about 1.1 x 100 degrees / 256 = 0.43 degrees
// - ten eye pixels at the eyes' 2568 across 105 degrees (derived from the
// headset's frustum, not measured). The stable capture renders the map
// afresh from the head's rotation every stereo pair (WaterReflectionHook.cpp),
// so every small turn lands the reflected shore on another texel: its edges
// jump by whole texels while the head moves and stand still when it stops -
// the reflections that "reset or come again" as the head turns (Nadi in the
// headset, 2026-10-22; a Quest 3 tester's "low-quality and jittery",
// 2026-10-21). A bigger map makes the jumps smaller in the same measure.
//
// [Render] WaterReflectionMapSize replaces the immediate before the map is
// made: 0 keeps the engine's 256, otherwise a power of two from 256 to 4096.
// The write happens once at the hook's install (the texture, once created,
// is not re-made), so the key is read at the start and not hot reloaded.

#include "core/Types.h"

namespace obvr::render {

inline constexpr UInt32 kEngineWaterReflectionMapSize = 256;
inline constexpr UInt32 kSmallestWaterReflectionMapSize = 256;
inline constexpr UInt32 kLargestWaterReflectionMapSize = 4096;

enum class WaterReflectionMapAction : UInt8 {
	Keep,     // the engine's own size stays (0, or 256 asked for)
	Patch,    // the immediate is replaced with `size`
	Refused,  // the wish is not a usable size, or the site is not the engine's
};

struct WaterReflectionMapDecision {
	WaterReflectionMapAction action = WaterReflectionMapAction::Keep;
	UInt32 size = kEngineWaterReflectionMapSize;
	const char* reason = "";
};

inline bool IsPowerOfTwo(UInt32 value) {
	return value != 0 && (value & (value - 1)) == 0;
}

// What to do with the immediate at the creation site: `wanted` is the INI
// key, `immediate` the 32-bit value found at the site before any write.
inline WaterReflectionMapDecision DecideWaterReflectionMapPatch(UInt32 wanted,
                                                                UInt32 immediate) {
	WaterReflectionMapDecision out;
	if (wanted == 0) {
		out.reason = "0: the engine's own size";
		return out;
	}
	if (!IsPowerOfTwo(wanted) || wanted < kSmallestWaterReflectionMapSize ||
	    wanted > kLargestWaterReflectionMapSize) {
		out.action = WaterReflectionMapAction::Refused;
		out.reason = "not a power of two between 256 and 4096";
		return out;
	}
	if (immediate != kEngineWaterReflectionMapSize) {
		// Another plugin (TES Reloaded's ReflectionMapSize) or another
		// executable: the site is not the engine's, so it is left alone.
		out.action = WaterReflectionMapAction::Refused;
		out.size = immediate;
		out.reason = "the site does not hold the engine's 256 - another plugin's";
		return out;
	}
	if (wanted == kEngineWaterReflectionMapSize) {
		out.reason = "256 is the engine's own size";
		return out;
	}
	out.action = WaterReflectionMapAction::Patch;
	out.size = wanted;
	out.reason = "bigger than the engine's 256";
	return out;
}

}  // namespace obvr::render

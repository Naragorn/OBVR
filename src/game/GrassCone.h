#pragma once

// The cone the engine builds its grass in - grass at the edge of the view
// that came and went as the head turned (avolpe, beta 6, 2026-10-24: "as i
// turn my head, the grass spawns and despawns", a video from the Quest).
//
// Read with REA/objdump, 2026-10-24: at the end of the camera update
// (FUN_0066B710) the engine takes the world camera's frustum's right edge
// (+0xF0), its angle atan(right) / LODAdjust (+0x120), and rebuilds the grass
// (0x004EBDA0, cdecl: the camera's position, its heading, and that angle less
// 0.25) when the camera has gone 64 units, or turned until the dot with the
// heading of the last build falls under 1.6 less the angle. The build
// (0x004EB3F0) keeps every patch within 512 units, and beyond that only the
// patches with a corner whose direction is within twice its argument of the
// heading (the CRT call 0x00986130 taken as acos of the dot); the others are
// removed. On a monitor at 75 degrees that is 46 degrees either side for a
// view of 37.5, rebuilt every 19 degrees of turn; with the harness's headset
// (98.1 across, the union frustum) 69 degrees for a view of 49, rebuilt every
// 42 - so before a rebuild the turning side lacks grass up to 21 degrees in
// from the edge of the view, twice the monitor's gap, and a head turns slowly
// and looks there. Measured with this detour (grass-turn-off.txt, 2026-10-24):
// the cone 0.606, a rebuild exactly at the 42-degree turn, the patches under
// the Grass node 40, 37, 38 as the head turned.
//
// With a headset OBVR hands the build a cone that passes every corner: the
// grass stands all around within its distance, a turn of the head finds it
// already there, and only walking builds new patches, as the engine's 64
// units already did. What lies behind the eyes is culled when drawn like
// anything else. [Render] GrassAllAround, default on.
//
// Pure apart from the install; covered by grass_cone_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kGrassBuild = 0x004EBDA0;
// push -1; push 0x009CACCB - the build's exception frame, nothing relative.
inline constexpr UInt8 kGrassBuildEntry[7] = {0x6A, 0xFF, 0x68, 0xCB, 0xAC, 0x9C, 0x00};
// The "Grass" node the build hangs the patches under (FUN_004EBDA0 makes it
// once and keeps it here).
inline constexpr UInt32 kGrassNode = 0x00B36094;

// Twice this is over pi, so acos of any dot passes.
inline constexpr float kGrassAllAroundCone = 1.6f;

inline float GrassConeFor(bool allAround, float engineCone) {
	if (!allAround || !(engineCone == engineCone)) {
		return engineCone;
	}
	return engineCone > kGrassAllAroundCone ? engineCone : kGrassAllAroundCone;
}

inline bool GrassConeCoversAll(float cone) {
	return cone + cone >= 3.14159265f;
}

// How far either side of the heading a patch's corner may lie, in degrees.
inline float GrassConeDegrees(float cone) {
	const float twice = cone + cone;
	if (!(twice > 0.0f)) {
		return 0.0f;
	}
	const float degrees = twice * 57.2957795f;
	return degrees < 180.0f ? degrees : 180.0f;
}

// The detour on the grass build. Logs each build for a while, then a line
// every hundred builds; refuses when the bytes are not as read.
void InstallGrassCone();

// Whether the next builds get the all-around cone: [Render] GrassAllAround
// with a headset. Set by the camera update, which calls the build itself.
void SetGrassAllAround(bool allAround);

// For the mark lines: the builds so far, their time, and the Grass node's
// children now.
struct GrassBuildStats {
	UInt32 builds = 0;
	double totalMs = 0.0;
	double maxMs = 0.0;
	float lastEngineCone = 0.0f;
	float lastUsedCone = 0.0f;
	UInt32 patches = 0;
};
GrassBuildStats ReadGrassBuildStats();

}  // namespace obvr::game

#pragma once

#include "core/Types.h"

namespace obvr::render {

// D3DPRESENT_INTERVAL_DEFAULT, _ONE and _IMMEDIATE, as d3d9caps.h defines
// them (Wine's include/d3d9caps.h, the same values as the Windows SDK's).
// Immediate is "always available for both windowed and full-screen swap
// chains" (Microsoft Learn, D3DPRESENT).
constexpr UInt32 kPresentIntervalDefault = 0x00000000u;
constexpr UInt32 kPresentIntervalOne = 0x00000001u;
constexpr UInt32 kPresentIntervalImmediate = 0x80000000u;

// The present interval the game's device is made with.
//
// While OBVR drives the headset the monitor window is only a mirror, and its
// vsync costs the headset its frames. DXVK presents from its submission
// thread while it holds the Vulkan queue's mutex, and with vsync that present
// waits for the monitor; OBVR takes the same mutex several times a frame
// (LockSubmissionQueue) to hand its pictures to SteamVR, and so waited with
// it - 2.3 to 2.5 ms a lock on average under Proton, 37 % of the frames
// reprojected; presented at once, 0.03 ms and 1.3 % (2026-10-14,
// docs/linux.md "The stutter"). So the window presents at once and SteamVR
// alone paces the frame, unless MonitorVsync asks to keep the game's own
// interval. Without a headset to drive, the game keeps its own either way:
// nothing else would pace it.
//
// Whether the window is to present at once: only while OBVR drives the
// headset, and MonitorVsync does not keep the game's own interval.
inline bool MonitorPresentsAtOnce(bool drivesHeadset, bool keepMonitorVsync) {
	return drivesHeadset && !keepMonitorVsync;
}

// DXVK takes any interval but immediate as vsync (default counts as one), so
// every other value is a wait for the monitor and is replaced.
inline UInt32 MonitorPresentInterval(UInt32 asked, bool drivesHeadset, bool keepMonitorVsync) {
	return MonitorPresentsAtOnce(drivesHeadset, keepMonitorVsync) ? kPresentIntervalImmediate : asked;
}

}  // namespace obvr::render

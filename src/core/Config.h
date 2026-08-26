#pragma once

#include "camera/LookControl.h"
#include "core/Types.h"
#include "vr/HeadTracker.h"

namespace obvr {

// Settings from OBVR.ini next to Oblivion.exe.
struct Config {
	bool cameraHookEnabled = true;

	vr::TrackerSettings tracker;

	// What happens to the look controls once a headset has taken over. Only
	// applied while one actually is; without a headset the game keeps its own
	// camera entirely.
	camera::LookSettings look;

	// Virtual-key code of the key that takes the current head pose as the new
	// zero. Default is VK_DELETE (0x2E), the Del key above the arrow block,
	// which vanilla Oblivion leaves unbound. 0 disables recentering.
	UInt32 recenterKey = 0x2E;

	// How often the camera state is logged, in frames. 0 turns the running
	// log off; state changes are still reported.
	UInt32 logEveryFrames = 0;

	// How often the INI is re-read while the game runs, in frames.
	// 0 turns it off.
	//
	// Without this, every change to the test angles costs a full restart plus
	// loading a save. With the hot reload the camera can be tuned while the
	// game is running.
	UInt32 reloadEveryFrames = 0;

	// Cuts the dual pass down to a stage, to find which stage loses the GPU.
	//
	// The first dual-pass run died with VK_ERROR_DEVICE_LOST on its first
	// world frames, and the trigger cannot be told from reasoning: the
	// candidates are the second engine render itself, the camera move between
	// the passes, and the mid-frame captures. This ladder separates them,
	// one run per rung:
	//
	//   0  the whole thing (the default)
	//   1  the world is rendered twice, but the camera never moves and
	//      nothing is captured - the headset shows mono
	//   2  rendered twice with the camera moved, still nothing captured
	//
	// In the [Debug] section, so the hot reload can change it while the game
	// runs. Values above 2 behave like 0.
	UInt32 dualPassProbe = 0;

	// true when the file was found and read.
	bool Load(const char* fileName);

	// Re-reads only the values that are safe to change while the game runs.
	// cameraHookEnabled stays out of it: by this point the hook has long been
	// installed.
	bool Reload(const char* fileName);
};

Config& GetConfig();

}  // namespace obvr

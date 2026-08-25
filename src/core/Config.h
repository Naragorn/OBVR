#pragma once

#include "core/Types.h"
#include "vr/HeadTracker.h"

namespace obvr {

// Settings from OBVR.ini next to Oblivion.exe.
struct Config {
	bool cameraHookEnabled = true;

	vr::TrackerSettings tracker;

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

	// true when the file was found and read.
	bool Load(const char* fileName);

	// Re-reads only the values that are safe to change while the game runs.
	// cameraHookEnabled stays out of it: by this point the hook has long been
	// installed.
	bool Reload(const char* fileName);
};

Config& GetConfig();

}  // namespace obvr

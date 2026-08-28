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

	// Whether the game's dialogue camera zoom is left alive. Off by default:
	// in a headset the zoom itself never shows - OBVR owns the camera - and
	// the transition it rides on showed up twice, first as a dead pause and
	// then as a grey flash at every dialogue's end. Off, SetDialogCamera is
	// patched to return immediately, so the transition never starts at all;
	// see DialogZoom.h for why the gentler fDlgFocus override lost this job.
	// In [Look], hot-reloadable both ways.
	bool dialogZoom = false;

	// Whether the shim keeps vanilla's habit of flipping a third-person
	// player into first person for a conversation and back after it. On by
	// default because that is the vanilla feel the zoom removal should not
	// have taken; off leaves the point of view entirely alone. Read live by
	// the shim, so the hot reload reaches it mid-session.
	bool dialogFirstPerson = true;

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

	// Renders the right eye first and the left eye second, swapping the whole
	// order of the dual pass.
	//
	// A diagnostic for a fault that sits in one eye. There are two ways for
	// that to be true and no way to tell them apart from inside one ordering:
	// either the first render goes wrong - because of what the frame leaves in
	// front of it, or because it is first - or the left eye goes wrong, which
	// would make it the camera position. Swap the order and the two separate: a
	// fault that follows the order belongs to the render, a fault that stays in
	// the left eye belongs to the eye.
	//
	// In [Debug] with the rest, so the hot reload can flip it mid-game.
	bool swapEyeOrder = false;

	// Paints an opaque red square into the middle of the HUD overlay texture
	// just before it is handed over - the instrument for a HUD that arrives
	// as nothing. Square visible: the overlay path works, the layer's alpha
	// is what is missing. Square absent: the display path itself is at
	// fault. Hot reloaded, like the rest of [Debug].
	bool hudProbe = false;

	// true when the file was found and read.
	bool Load(const char* fileName);

	// Re-reads only the values that are safe to change while the game runs.
	// cameraHookEnabled stays out of it: by this point the hook has long been
	// installed.
	bool Reload(const char* fileName);
};

Config& GetConfig();

}  // namespace obvr

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

	// Runs the menu-world probe: on a handful of menu frames per episode,
	// OBVR calls the engine's world render itself and logs how many draw
	// calls came out. The measurement behind keeping the world live in
	// 3D behind pause menus - Oblivion stops calling that render entirely
	// while one is up, so a live background means self-initiated renders,
	// and whether those draw at all is what this answers. Held and cinema
	// menu frames both qualify, because both sit on the same stopped render;
	// the cinema case is what a headless run produces, where a sleeping
	// headset never arms stereo. The probe's render lands in the back buffer,
	// which nobody reads on a held frame and which IS the picture on a cinema
	// one - so the monitor may show the world instead of the menu for those
	// frames, which is the probe being visible, not a fault. In [Debug],
	// hot-reloadable, off by default.
	bool menuWorldProbe = false;

	// Measures which rectangle of the frame the 2D actually lands in, every
	// couple of seconds while it is on: the bounding box of the non-black
	// pixels in the back buffer on a cinema frame (films, loading screens,
	// the main menu), and of the alpha-carrying pixels in the menu capture
	// texture on a held menu frame. The instrument for the eye-sized frame's
	// 2D symptoms - each measurement says whether that part of the interface
	// lays out against the size the game believes or the size the frame
	// really is. Costs a GPU stall per measurement, so it is a switch and
	// not a default. In [Debug], hot-reloadable, off by default.
	bool layoutProbe = false;

	// Logs the interface cursor's internals every couple of seconds while a
	// menu is up: the InterfaceManager's position triples, the cursor tile's
	// node translation (where the sprite is planted) and the active tile with
	// its translation (where the game holds the mouse). The instrument for
	// the raised copy's mouse offset - clicks landing above the visible
	// cursor. Read-only. In [Debug], hot-reloadable, off by default.
	bool cursorProbe = false;

	// Paints an opaque red square into the middle of the HUD overlay texture
	// just before it is handed over - the instrument for a HUD that arrives
	// as nothing. Square visible: the overlay path works, the layer's alpha
	// is what is missing. Square absent: the display path itself is at
	// fault. Hot reloaded, like the rest of [Debug].
	bool hudProbe = false;

	// Logs the player's own rotation beside the camera's and the head's, a few
	// times a second.
	//
	// The question it settles: an arrow leaves along the player's rotation,
	// which OBVR never touches, while the view is the camera's, which OBVR
	// replaces. So the mouse still aims, invisibly, and the arrow ignores where
	// the wearer is looking. Reading the three angles together says which way
	// rotX grows and in what units - and writing a pitch with the sign guessed
	// wrong aims at the floor when the wearer looks at the sky, which is worse
	// than not aiming at all.
	bool aimProbe = false;

	// Points the player where the head is looking, so an arrow leaves along
	// the gaze instead of along the mouse.
	//
	// The answer to what the probe above measured. In [Look] rather than in
	// [Debug] because it is a feature and not an instrument, and hot-reloaded
	// like the rest of that section - it can be switched off from inside the
	// headset if it ever aims somewhere wrong.
	//
	// First person only, and camera::AimPitchWanted says why: in third person
	// the tilt is read back out of the engine's camera and turned into camera
	// height, so writing it there could feed OBVR's own value into its own
	// camera a frame later.
	bool aimFollowsGaze = true;

	// true when the file was found and read.
	bool Load(const char* fileName);

	// Re-reads only the values that are safe to change while the game runs.
	// cameraHookEnabled stays out of it: by this point the hook has long been
	// installed.
	bool Reload(const char* fileName);
};

Config& GetConfig();

}  // namespace obvr

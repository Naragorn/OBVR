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

	// The control that means "I am aiming", as a Windows virtual-key code.
	//
	// While it is held, and only then, the character is turned to face where
	// the head is looking, so that a shot leaves along the gaze sideways as
	// well as vertically. Letting go leaves the character facing where the
	// shot went and stops following again - which is what keeps the body still
	// during ordinary looking around, as asked for.
	//
	// 1 is VK_LBUTTON, Oblivion's attack control at its default binding, and
	// on a bow that is precisely the drawing of the string. 0 switches the
	// sideways half off while leaving the vertical one alone.
	UInt32 aimAttackKey = 1;

	// How fast the body comes round to the gaze, as a share of the remaining
	// turn per second.
	//
	// Zero means all of it at once, and that is the default: the view does not
	// move while the body turns - the turn is taken back out of the camera at
	// the base rotation - so there is nothing for easing to smooth away, and a
	// body that is already round is a body that shoots where the crosshair is.
	// Easing it only puts a lag between aiming and hitting.
	//
	// A value above zero is there for the case the instant turn upsets an
	// animation or reads badly in third person, where the character is in
	// view; 8.0 matches the vertical look's own easing.
	float aimTurnSpeed = 0.0f;

	// Whether the turn handed to the body is given back when the attack control
	// is released.
	//
	// Without it the body keeps the heading it was aimed at, so the character
	// walks the way the shot went rather than the way the wearer is looking.
	// With it, the body comes back to where it started the moment the arrow
	// leaves.
	//
	// THIS IS THE CHANGE THAT CAUSED NAUSEA ONCE - commit 7e57e69, reverted as
	// a527686 - and it is deliberately not the same change. That one unwound
	// the body over about a quarter of a second, and the leading suspect was
	// the shape rather than the speed: the compensation reads the NEXT frame's
	// base rotation, so a multi-frame unwind can leave the view lagging the
	// body by one frame every frame, which is a small continuous drift and
	// exactly what makes people ill. This gives the whole turn back in ONE
	// frame, where a single frame of lag is not a motion at all.
	//
	// The view should not move while it happens. If it does, or if this brings
	// the sickness back, switch it off - the walking fault it fixes is
	// annoying, and being ill is not a trade worth making.
	bool aimReturnOnRelease = true;

	// WHEN the body is turned to the gaze: only for the shot itself (1), or for
	// as long as the attack control is held (0).
	//
	// Naragorn's suggestion, and the better shape: "wir entkoppeln die ziel kamera
	// mit der richtung vom char. dann kann ich weiter richtung vom gehen
	// bestimmen mit maus oder gamepad. und kann froehlich zielen in alle
	// richtungen und ohne reset wie jetzt."
	//
	// The aim and the walking cannot be separated in SPACE. That is measured
	// rather than assumed: the arrow leaves along rotZ, walking follows rotZ,
	// and turning it moves both - which is exactly what the sideways-walking
	// fault has been reporting all along.
	//
	// They separate in TIME. The arrow does not exist while the bow is drawn;
	// it is made when the shot is released. So the heading only has to be right
	// for the handful of frames between letting go and the arrow leaving, and
	// for everything else - the whole draw included - the body can be left
	// exactly where the mouse put it.
	//
	// What that gives: aim anywhere, for as long as you like, while still
	// steering where you walk. Nothing is left standing afterwards, so there is
	// nothing to reset and no offset to be sick over.
	//
	// The cost, said here rather than found later: during the draw the body is
	// not turned, so the bow points where the character faces rather than where
	// you are looking. In first person the hands are drawn against the camera
	// rather than the body, so this may not show at all - but it is not known
	// yet, and it is the thing to watch for.
	//
	// Needs aimReturnOnRelease as well. Without it the turn made for the shot
	// would stay, and the fault this avoids would be back a moment later.
	bool aimTurnOnShotOnly = true;

	// Logs one line a frame for forty frames from the moment the attack control
	// is released: what the engine says the actor is doing, and whether the body
	// is being held turned.
	//
	// For one measurement. The turn window is currently the whole attack, and
	// that is visibly too long - the character walks the aimed way for its
	// length and the bow swings across and springs back. It has to shrink to
	// the frame the arrow is actually made on, and which action change that is
	// has been read out of an enum once with nothing to check it against.
	// Guessing at it would be the thing the project's own notes forbid.
	bool aimShotTrace = false;

	// Whether the first person weapon follows the gaze while it is being aimed.
	//
	// The missing third piece. Turning the body aims the shot but takes the
	// walking with it, so the body is only turned for the shot itself - which
	// leaves the bow pointing forwards while somebody aims elsewhere:
	// "der bogen zeigt egal wo ich hinschaue nach vorne".
	//
	// The bow is only DRAWN. Its direction is a picture, not a fact about the
	// world: nothing is fired along it and nobody walks along it. So the first
	// person arm node is turned instead, which moves the weapon and nothing
	// else.
	//
	// The angle is the one already being computed for the aiming -
	// AimYawRemaining, how far the head is turned beyond what the body has
	// taken. Drawing: the body has taken nothing, so the arms turn the whole
	// way. Shooting: the body takes it and the remainder falls to zero as the
	// arms give it up. The sum stays constant, so the weapon does not jump when
	// one hands over to the other - which was the other half of the report,
	// "dann springt er nach dem schuss zurueck mittig".
	//
	// First person only. In third person the body is what is being looked at
	// and the arms belong to it.
	bool aimWeaponFollowsGaze = true;

	// The key that opens OBVR's own settings menu in the headset, as a Windows
	// virtual-key code. 0 disables it entirely.
	//
	// VK_INSERT (0x2D) by default, next to the recenter key on the same block
	// and unbound in vanilla Oblivion - which is the whole of the requirement,
	// since a key the game also uses would do both things at once.
	UInt32 settingsMenuKey = 0x2D;

	// Where that menu hangs, in metres ahead of the head, and how wide it is
	// there. Reading distance rather than the HUD's, because it is text at a
	// small size and the wearer is looking at it deliberately rather than
	// glancing.
	float settingsMenuDistanceMetres = 1.0f;
	float settingsMenuWidthMetres = 1.1f;

	// Whether that menu stands in the room or is carried on the head.
	//
	// The room by default, and the difference is not cosmetic: a panel fixed to
	// the head cannot be read, because every attempt to look at the row below
	// the middle one moves that row with it and the eyes have nothing that
	// holds still to converge on. Standing in the room it becomes an object -
	// lean in, turn away, look back and it is where it was left.
	//
	// Head is kept because it is what the intro films fall back to anyway,
	// there being no pose to anchor to before WaitGetPoses has run, and because
	// a menu that has to be walked back to is worse than one that follows.
	bool settingsMenuInWorld = true;

	// true when the file was found and read.
	bool Load(const char* fileName);

	// Re-reads only the values that are safe to change while the game runs.
	// cameraHookEnabled stays out of it: by this point the hook has long been
	// installed.
	bool Reload(const char* fileName);
};

Config& GetConfig();

// Writes one value back into OBVR.ini, into the same file Load and Reload read.
//
// This is what makes a change from the in-headset settings menu stick, and it
// is needed for a reason beyond surviving a quit: ReloadEveryFrames re-reads
// the file while the game runs, so a change held only in memory is overwritten
// by the file within a couple of seconds. The first run of that menu did
// exactly this, and it looked like every keypress was being ignored.
//
// The file is the one BuildPath finds - beside the plugin if it is there, in
// the game root otherwise - so the value is written where it will be read.
//
// False when the file could not be written: a read-only INI, or one under a
// mod manager's virtual file system that does not accept writes. The caller
// should say so once rather than every frame.
bool SaveSetting(const char* section, const char* key, const char* value);

}  // namespace obvr

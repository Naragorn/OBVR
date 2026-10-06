#pragma once

#include "core/Types.h"
#include "perf/ProfileLogic.h"

namespace obvr::render {

// Who is holding DXVK's queue. Written into the profiler's pass_index for the
// interop_flush, interop_lock and interop_held rows, so a capture can say
// which bracket stalled rather than only that one did.
//
// Why it exists: the first in-game capture (2026-10-06) showed two frames
// where one held bracket took 98 and 106 ms with every span inside it at its
// usual size - and four brackets a frame, all spelled the same, so the file
// could not say whose. The number is the bracket's owner; the name is for the
// person reading the report.
enum class BracketOwner : UInt32 {
	Unnamed = 0,
	Poses = 1,         // WaitGetPoses, held so the compositor's timestamp cannot collide
	GameFrame = 2,     // the eyes' submit
	EyeMirror = 3,     // the eye copies
	Hud = 4,           // the 2D layer's overlay
	Crosshair = 5,     // the crosshair quad
	HandHud = 6,       // the HUD pieces on the hands
	Vignette = 7,      // the snap-turn vignette
	Canvas = 8,        // a canvas overlay (quick menu, holster fit, stow spot, onboarding)
	SettingsMenu = 9,  // the in-headset settings menu
};

// The context the bracket's spans carry: the owner in pass_index, and no eye,
// because a bracket spans both.
inline perf::EventContext BracketContext(BracketOwner owner) {
	perf::EventContext context{};
	context.passIndex = static_cast<UInt32>(owner);
	context.eye = 2;
	return context;
}

// The owner's name, for a report; "?" for a number no owner has.
inline const char* BracketOwnerName(BracketOwner owner) {
	switch (owner) {
		case BracketOwner::Unnamed: return "unnamed";
		case BracketOwner::Poses: return "poses";
		case BracketOwner::GameFrame: return "game_frame";
		case BracketOwner::EyeMirror: return "eye_mirror";
		case BracketOwner::Hud: return "hud";
		case BracketOwner::Crosshair: return "crosshair";
		case BracketOwner::HandHud: return "hand_hud";
		case BracketOwner::Vignette: return "vignette";
		case BracketOwner::Canvas: return "canvas";
		case BracketOwner::SettingsMenu: return "settings_menu";
	}
	return "?";
}

}  // namespace obvr::render

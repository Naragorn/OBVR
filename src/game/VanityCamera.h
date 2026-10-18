#pragma once

// The engine's auto vanity camera, held off in the headset: after
// fVanityModeAutoDelay seconds without a key or the mouse the engine
// switches to third person and orbits the camera round the player. The
// headset's own movement is no input to it, so a player standing and
// looking round got the orbit (the harness, 2026-10-18, while measuring
// people in view; a Full VR player at a vendor's counter would get it too).
// OBVR resets the engine's idle clock every frame instead of the input the
// engine never sees - the clock, a float global, not the setting, which the
// engine persists to the user's Oblivion.ini (game/IniSettings.h's lesson).
// [Look] AutoVanityCamera=1 leaves the engine its orbit.
//
// The pure half here; the addresses in GameAddresses.h.

#include "core/Types.h"

namespace obvr::game {

// Whether the clock is reset this frame: the orbit is not asked for and the
// headset is there - on a monitor the engine keeps its own behaviour.
inline bool AutoVanityHeldOff(bool orbitAllowed, bool headsetConnected) {
	return !orbitAllowed && headsetConnected;
}

// Zeroes the engine's idle clock, after checking it reads as a clock
// (finite, not negative, under a day); false, nothing written, otherwise.
bool ResetAutoVanityClock();

// The engine's vanity flag (what Enhanced Camera reads as isVanityMode).
bool EngineVanityModeOn();

}  // namespace obvr::game

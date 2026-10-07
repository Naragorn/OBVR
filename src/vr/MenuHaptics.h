#pragma once

// A tick in the controller at every button (the tester, 2026-10-07: "ein
// leichtes klicken der vibrationsmotoren der controller an jedem button
// ... wenn man über dem hovert oder klickt, gleich wie beim steamvr
// menü"): a light pulse as the laser comes onto a new thing it can press,
// a firmer one as it presses. For the game's menus the thing is the
// engine's own tile under the cursor (InterfaceManager's activeTile, which
// the engine sets only for tiles that take the cursor); for OBVR's panels
// and the quick menu's ring it is the row or the slot. [Hands] MenuHaptics.
//
// Pure; menu_haptics_test covers it.

#include "core/Types.h"

namespace obvr::vr {

enum class MenuPulse : UInt8 { None, Hover, Click };

// The pulses, as OpenVRBackend::Pulse takes them: seconds, hertz, amplitude.
// Tiny on purpose (the tester, 2026-10-07: "die vibrationen müssen weniger
// stark sein ... soll nur ein tick sein wie in steamvr" - the first build's
// 0.25 and 0.6 were too much).
inline constexpr float kHoverPulseSeconds = 0.005f;
inline constexpr float kHoverPulseHertz = 180.0f;
inline constexpr float kHoverPulseAmplitude = 0.084f;  // "5 % stärker" (2026-10-07)
inline constexpr float kClickPulseSeconds = 0.012f;
inline constexpr float kClickPulseHertz = 200.0f;
inline constexpr float kClickPulseAmplitude = 0.21f;

// What the laser is on, remembered so a pulse comes once per thing.
struct MenuHapticState {
	UInt32 tile = 0;  // the engine's tile under the cursor last frame
	int row = -1;     // the panel's row or the ring's slot last frame
};

// The game's menus: `pointing` is the laser on the menu's picture (or the
// finger on it), `tile` the engine's tile under the cursor (0 for none),
// `clickEdge` the click going down this frame. A click pulses even on a
// tile already hovered; coming onto a tile pulses once; leaving onto
// nothing, or the laser leaving the picture, pulses not at all.
inline MenuPulse StepTileHaptic(MenuHapticState& s, bool pointing, UInt32 tile, bool clickEdge) {
	if (!pointing) {
		s.tile = 0;
		return MenuPulse::None;
	}
	if (clickEdge) {
		s.tile = tile;
		return MenuPulse::Click;
	}
	if (tile != s.tile) {
		s.tile = tile;
		return tile != 0 ? MenuPulse::Hover : MenuPulse::None;
	}
	return MenuPulse::None;
}

// OBVR's own rows and slots: `row` -1 for none.
inline MenuPulse StepRowHaptic(MenuHapticState& s, bool pointing, int row, bool clickEdge) {
	if (!pointing) {
		s.row = -1;
		return MenuPulse::None;
	}
	if (clickEdge && row >= 0) {
		s.row = row;
		return MenuPulse::Click;
	}
	if (row != s.row) {
		s.row = row;
		return row >= 0 ? MenuPulse::Hover : MenuPulse::None;
	}
	return MenuPulse::None;
}

}  // namespace obvr::vr

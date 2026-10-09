#pragma once

#include "core/Types.h"
#include "game/KeyScanCodes.h"

namespace obvr::game {

// Where the controllers' keys and mouse go, frame by frame - the pure half,
// kept apart so every flow can be tested without a game.
//
// The hand controls press the game's own keys. Through Windows
// (keybd_event, mouse_event) they reach DirectInput, which is where the game
// reads them - but Windows hands its input to the window in front, and the
// game's DirectInput devices are DISCL_FOREGROUND (0x00403E33): with another
// window in front the game gets nothing, and the keys land in that window
// instead - "w" typed into the chat while the stick walks, the trigger
// clicking whatever is under the desktop cursor. With the game kept running
// in the background (OblivionAlwaysActive) the player then stood still until
// the game was brought to the front again.
//
// So the route depends on the window in front:
// - the game in front: through Windows, exactly as before;
// - the game behind another window: straight into the game's input state,
//   at its poll (game/EngineInput.h) - and nothing through Windows at all;
// - behind, with [Hands] BackgroundInput=0 (or the poll not reachable):
//   nowhere - the controllers do nothing while the game is behind.
enum class InputRoute : UInt8 { Windows, Engine, Off };

inline InputRoute WantedInputRoute(bool gameInFront, bool engineRouteAllowed) {
	if (gameInFront) {
		return InputRoute::Windows;
	}
	return engineRouteAllowed ? InputRoute::Engine : InputRoute::Off;
}

struct InputRouteState {
	InputRoute route = InputRoute::Windows;
};

struct InputRouteStep {
	InputRoute route = InputRoute::Windows;
	// The route changed this frame (for the log).
	bool changed = false;
	// Let go of every key and button held through Windows, now: the game is
	// leaving the front, and a Shift or a mouse button left down would be
	// held in whatever window takes it.
	bool releaseWindowsKeys = false;
	// The engine overlay goes into the game's next poll(s). False clears it.
	bool engineInject = false;
	// The frame the game comes back to the front: the overlay stays as it
	// was for one more poll, while the same keys go down through Windows -
	// those arrive at the next device read, so the game sees no frame with
	// them up (a drawn bow would loose on one).
	bool engineKeepsLast = false;
};

inline InputRouteStep StepInputRoute(InputRouteState& state, bool gameInFront,
                                     bool engineRouteAllowed) {
	InputRouteStep step;
	const InputRoute from = state.route;
	const InputRoute to = WantedInputRoute(gameInFront, engineRouteAllowed);
	step.route = to;
	step.changed = from != to;
	step.releaseWindowsKeys = from == InputRoute::Windows && to != InputRoute::Windows;
	step.engineKeepsLast = from == InputRoute::Engine && to == InputRoute::Windows;
	step.engineInject = to == InputRoute::Engine || step.engineKeepsLast;
	state.route = to;
	return step;
}

inline const char* InputRouteName(InputRoute route) {
	switch (route) {
	case InputRoute::Windows: return "through Windows";
	case InputRoute::Engine: return "straight into the game's input";
	case InputRoute::Off: return "nowhere";
	}
	return "?";
}

// What a virtual key becomes in the game's input state: a DirectInput key
// code (DIK_, the set 1 scan code with 0x80 for the extended keys, the
// index into OSInputGlobals' CurrentKeyState) or a mouse button (the index
// into DIMOUSESTATE2's rgbButtons).
struct EngineInputTarget {
	enum class Kind : UInt8 { None, Key, Button };
	Kind kind = Kind::None;
	UInt8 index = 0;
};

// The same physical key the Windows route sends for it (HandControls.cpp,
// SendKey): the left and right mouse buttons as buttons, any other key by
// its US scan code, or - for a key the table does not know - the layout's,
// which the caller reads with MapVirtualKeyA and passes in.
inline EngineInputTarget EngineTargetFor(UInt32 virtualKey, UInt32 layoutScan) {
	EngineInputTarget target;
	if (virtualKey == 0x01 || virtualKey == 0x02) {  // VK_LBUTTON, VK_RBUTTON
		target.kind = EngineInputTarget::Kind::Button;
		target.index = static_cast<UInt8>(virtualKey - 1);
		return target;
	}
	UInt32 scan = UsScanCode(virtualKey);
	if (scan == 0) {
		scan = layoutScan;
	}
	if (scan == 0 || scan > 0x7F) {
		return target;
	}
	target.kind = EngineInputTarget::Kind::Key;
	target.index = static_cast<UInt8>(scan | (IsExtendedKey(virtualKey) ? 0x80u : 0u));
	return target;
}

// What the controllers hold in the game's input, and the movement still to
// be delivered.
struct EngineInputOverlay {
	UInt8 keys[256] = {};   // non-zero: the key with that DIK code is held
	UInt8 buttons[8] = {};  // non-zero: that mouse button is held
	SInt32 dx = 0;          // mouse movement, in DirectInput's units (mickeys)
	SInt32 dy = 0;
	SInt32 wheel = 0;       // the wheel, WHEEL_DELTA (120) a notch, up positive
};

inline void HoldEngineTarget(EngineInputOverlay& overlay, const EngineInputTarget& target, bool down) {
	const UInt8 value = down ? 1 : 0;
	if (target.kind == EngineInputTarget::Kind::Key) {
		overlay.keys[target.index] = value;
	} else if (target.kind == EngineInputTarget::Kind::Button && target.index < 8) {
		overlay.buttons[target.index] = value;
	}
}

inline bool EngineTargetHeld(const EngineInputOverlay& overlay, const EngineInputTarget& target) {
	if (target.kind == EngineInputTarget::Kind::Key) {
		return overlay.keys[target.index] != 0;
	}
	if (target.kind == EngineInputTarget::Kind::Button && target.index < 8) {
		return overlay.buttons[target.index] != 0;
	}
	return false;
}

inline void ClearEngineOverlay(EngineInputOverlay& overlay) { overlay = EngineInputOverlay{}; }

// How much of the overlay is held, for the log.
inline UInt32 EngineKeysHeld(const EngineInputOverlay& overlay) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < 256; ++i) {
		n += overlay.keys[i] != 0 ? 1u : 0u;
	}
	return n;
}

inline UInt32 EngineButtonsHeld(const EngineInputOverlay& overlay) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < 8; ++i) {
		n += overlay.buttons[i] != 0 ? 1u : 0u;
	}
	return n;
}

// Writes the overlay into the state a device read just produced, the way
// a real key down reads there: 0x80 in CurrentKeyState (OSInputGlobals
// +0x18F4) and in the mouse buttons, the movement added to the axes
// (DIMOUSESTATE2 at +0x1B20: lX, lY, lZ, rgbButtons[8]). Held keys stay in
// the overlay for every poll until let go; the movement is delivered once.
inline void ApplyEngineOverlay(EngineInputOverlay& overlay, UInt8* currentKeys, SInt32* mouseAxes,
                               UInt8* mouseButtons) {
	for (UInt32 i = 0; i < 256; ++i) {
		if (overlay.keys[i] != 0) {
			currentKeys[i] = 0x80;
		}
	}
	for (UInt32 i = 0; i < 8; ++i) {
		if (overlay.buttons[i] != 0) {
			mouseButtons[i] = 0x80;
		}
	}
	mouseAxes[0] += overlay.dx;
	mouseAxes[1] += overlay.dy;
	mouseAxes[2] += overlay.wheel;
	overlay.dx = 0;
	overlay.dy = 0;
	overlay.wheel = 0;
}

}  // namespace obvr::game

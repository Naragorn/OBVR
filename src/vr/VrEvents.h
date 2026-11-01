#pragma once

// OpenVR's events, taken every frame.
//
// Until 2026-11-01 OBVR polled IVRSystem::PollNextEvent only while its
// SteamVR keyboard was open (vr/VrKeyboard.h), so SteamVR's VREvent_Quit
// went unseen: closing SteamVR, or quitting the game from SteamVR's
// dashboard, ended with vrserver killing Oblivion.exe once its timeout ran
// out - unsaved progress gone, no normal exit. The header says so for
// AcknowledgeQuit_Exiting (openvr.h, IVRSystem): "Call this to acknowledge
// to the system that VREvent_Quit has been received and that the process is
// exiting. This extends the timeout until the process is killed."
//
// Now every frame drains the queue. The keyboard's events count only while
// OBVR's keyboard is open - as before, when nothing polled otherwise - and
// a quit is answered whatever is open:
// - AcknowledgeQuit_Exiting, once;
// - the engine's own quit flag raised, OSGlobals->quitGame (0x00B33398,
//   byte +0x00; xOBSE obse/GameOSDepend.h "UInt8 quitGame; // 00"). Read in
//   Oblivion.exe 1.2.416: each of the game's three ways out writes exactly
//   that byte - the console's qqq at 0x005077EA, the main menu's Exit at
//   0x005B5A0D, the pause menu's Quit at 0x005BDE60 (each "mov reg,
//   [0x00B33398]; mov byte [reg], 1", xOBSE's quit hooks sit there) - and
//   the main loop leaves on it.
// A quit the engine cannot take yet (its globals unreadable) stays pending
// and is tried again the next frame; a quit while the engine already quits
// is only acknowledged.
//
// Pure; vr_events_test covers it.

#include "core/Types.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// What one frame's events said.
struct FrameEvents {
	char chars[64] = {};
	UInt32 charEvents = 0;
	bool done = false;
	bool closed = false;
	bool quit = false;
};

// One event into the frame's record. `capacity` is how much of `chars` may
// be used, the terminator included.
inline void TakeEvent(FrameEvents& events, const openvr::VREvent& event, bool keyboardOpen, UInt32 capacity) {
	if (capacity > sizeof(events.chars)) {
		capacity = sizeof(events.chars);
	}
	if (event.eventType == openvr::kEventQuit) {
		events.quit = true;
		return;
	}
	if (!keyboardOpen) {
		return;
	}
	if (event.eventType == openvr::kEventKeyboardCharInput) {
		++events.charEvents;
		UInt32 written = 0;
		while (written < sizeof(events.chars) && events.chars[written] != '\0') {
			++written;
		}
		for (UInt32 i = 0; i < 8 && event.data.keyboard.newInput[i] != '\0'; ++i) {
			if (written + 1 < capacity) {
				events.chars[written++] = event.data.keyboard.newInput[i];
				events.chars[written] = '\0';
			}
		}
	} else if (event.eventType == openvr::kEventKeyboardDone) {
		events.done = true;
		events.closed = true;
	} else if (event.eventType == openvr::kEventKeyboardClosed) {
		events.closed = true;
	}
}

struct QuitState {
	bool pending = false;       // seen, not yet answered
	bool acknowledged = false;  // answered; nothing more to do
};

enum class QuitAction {
	None,
	NotYet,              // the engine's globals unreadable: kept pending
	AcknowledgeAndQuit,  // AcknowledgeQuit_Exiting, then quitGame = 1
	AcknowledgeOnly,     // the engine already quits
};

// `engineReady`: OSGlobals is readable. `engineQuitting`: its quitGame
// byte is already set.
inline QuitAction StepQuit(QuitState& state, bool quitEvent, bool engineReady, bool engineQuitting) {
	if (state.acknowledged) {
		return QuitAction::None;
	}
	if (quitEvent) {
		state.pending = true;
	}
	if (!state.pending) {
		return QuitAction::None;
	}
	if (!engineReady) {
		return QuitAction::NotYet;
	}
	state.pending = false;
	state.acknowledged = true;
	return engineQuitting ? QuitAction::AcknowledgeOnly : QuitAction::AcknowledgeAndQuit;
}

}  // namespace obvr::vr

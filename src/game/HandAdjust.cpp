#include "game/HandAdjust.h"

#include <cstdio>

#include "core/AtomicFlag.h"
#include "core/Config.h"
#include "core/Log.h"
#include "ui/NativeHandAdjust.h"

namespace obvr::game {
namespace {

// A placing window's handshake: asked for, its command for the frame, on.
struct PlaceWindow {
	AtomicFlag requested;
	AtomicFlag active;
	volatile long command = 0;  // a vr::StowPlaceCommand or vr::HolsterPlaceCommand, 0 for none
};

AtomicFlag g_guideRequested;
AtomicFlag g_finishRequested;
AtomicFlag g_active;
AtomicFlag g_controlsCardRequested;
PlaceWindow g_stowWindow;     // the stow spot (vr/StowPlace.h)
PlaceWindow g_holsterWindow;  // the weapon places' circles (vr/HolsterPlace.h)
ui::HandAdjustSession g_session;  // stepped only in the frame
// The T-pose measurement's handshake (SetBodyMeasureWanted, NoteBodyMeasured).
bool g_bodyMeasureWanted = false;
AtomicFlag g_bodyMeasured;
char g_bodyMeasuredWords[320] = "";
bool g_bodyMeasuredOk = false;
bool g_indexControllers = false;

}  // namespace

void RequestHandAdjustGuide() { g_guideRequested.Set(true); }
bool TakeHandAdjustGuideRequest() { return g_guideRequested.Take(); }

void SetBodyMeasureWanted(bool wanted) { g_bodyMeasureWanted = wanted; }
void NoteIndexControllers(bool index) { g_indexControllers = index; }
bool IndexControllers() { return g_indexControllers; }
bool BodyMeasureWanted() { return g_bodyMeasureWanted; }
void NoteBodyMeasured(const char* words, bool measured) {
	std::snprintf(g_bodyMeasuredWords, sizeof(g_bodyMeasuredWords), "%s", words != nullptr ? words : "");
	g_bodyMeasuredOk = measured;
	g_bodyMeasured.Set(true);
}
bool TakeBodyMeasured(char* out, unsigned capacity, bool& measured) {
	if (!g_bodyMeasured.Take()) {
		return false;
	}
	measured = g_bodyMeasuredOk;
	if (out != nullptr && capacity > 0) {
		std::snprintf(out, capacity, "%s", g_bodyMeasuredWords);
	}
	return true;
}
namespace {

void SendCommand(volatile long& slot, int command) {
#if defined(_MSC_VER)
	_InterlockedExchange(&slot, command);
#else
	__atomic_store_n(&slot, command, __ATOMIC_SEQ_CST);
#endif
}

int TakeCommand(volatile long& slot) {
#if defined(_MSC_VER)
	return static_cast<int>(_InterlockedExchange(&slot, 0));
#else
	return static_cast<int>(__atomic_exchange_n(&slot, 0, __ATOMIC_SEQ_CST));
#endif
}

}  // namespace

void RequestStowPlaceWindow() { g_stowWindow.requested.Set(true); }
bool TakeStowPlaceWindowRequest() { return g_stowWindow.requested.Take(); }
void RequestControlsCard() { g_controlsCardRequested.Set(true); }
bool TakeControlsCardRequest() { return g_controlsCardRequested.Take(); }
void SendStowPlaceCommand(int command) { SendCommand(g_stowWindow.command, command); }
int TakeStowPlaceCommand() { return TakeCommand(g_stowWindow.command); }
void NoteStowPlaceActive(bool active) { g_stowWindow.active.Set(active); }
bool StowPlaceActive() { return g_stowWindow.active.Get(); }
void RequestHolsterPlaceWindow() { g_holsterWindow.requested.Set(true); }
bool TakeHolsterPlaceWindowRequest() { return g_holsterWindow.requested.Take(); }
void SendHolsterPlaceCommand(int command) { SendCommand(g_holsterWindow.command, command); }
int TakeHolsterPlaceCommand() { return TakeCommand(g_holsterWindow.command); }
void NoteHolsterPlaceActive(bool active) { g_holsterWindow.active.Set(active); }
bool HolsterPlaceActive() { return g_holsterWindow.active.Get(); }

void StartHandAdjust() {
	ui::StartHandAdjustSession(g_session);
	g_finishRequested.Set(false);
	g_active.Set(true);
	OBVR_LOG("Hands: adjusting started - a closed grip holds its hand, opening it fits");
}

void StopHandAdjust() {
	g_session = ui::HandAdjustSession{};
	g_finishRequested.Set(false);
	g_active.Set(false);
	OBVR_LOG("Hands: adjusting ended");
}

bool HandAdjustActive() { return g_active.Get(); }

void NoteHandAdjustFrame(bool rightCommitted, bool leftCommitted, bool anyGripDown,
                         float dtSeconds) {
	if (!g_active.Get()) {
		return;
	}
	if (!g_session.active) {
		ui::StartHandAdjustSession(g_session);
	}
	if (ui::StepHandAdjustSession(g_session, rightCommitted, leftCommitted, anyGripDown,
	                              dtSeconds)) {
		g_finishRequested.Set(true);
		OBVR_LOG("Hands: both hands fitted - the finish window is next");
	}
}

bool TakeHandAdjustFinishRequest() { return g_finishRequested.Take(); }

bool ResetHandsToDefaults() {
	const vr::HandSettings defaults;
	vr::HandSettings& live = GetConfig().hands;
	live.rightHandRoll = defaults.rightHandRoll;
	live.rightHandPitch = defaults.rightHandPitch;
	live.rightHandYaw = defaults.rightHandYaw;
	live.leftHandRoll = defaults.leftHandRoll;
	live.leftHandPitch = defaults.leftHandPitch;
	live.leftHandYaw = defaults.leftHandYaw;
	live.rightHandGripX = defaults.rightHandGripX;
	live.rightHandGripY = defaults.rightHandGripY;
	live.rightHandGripZ = defaults.rightHandGripZ;
	live.leftHandGripX = defaults.leftHandGripX;
	live.leftHandGripY = defaults.leftHandGripY;
	live.leftHandGripZ = defaults.leftHandGripZ;
	struct Entry {
		const char* key;
		float value;
	};
	const Entry entries[] = {
		{"RightHandRoll", defaults.rightHandRoll},   {"RightHandPitch", defaults.rightHandPitch},
		{"RightHandYaw", defaults.rightHandYaw},     {"LeftHandRoll", defaults.leftHandRoll},
		{"LeftHandPitch", defaults.leftHandPitch},   {"LeftHandYaw", defaults.leftHandYaw},
		{"RightHandGripX", defaults.rightHandGripX}, {"RightHandGripY", defaults.rightHandGripY},
		{"RightHandGripZ", defaults.rightHandGripZ}, {"LeftHandGripX", defaults.leftHandGripX},
		{"LeftHandGripY", defaults.leftHandGripY},   {"LeftHandGripZ", defaults.leftHandGripZ},
	};
	bool saved = true;
	for (const Entry& entry : entries) {
		char value[32];
		std::snprintf(value, sizeof(value), "%g", static_cast<double>(entry.value));
		saved = SaveSetting("Hands", entry.key, value) && saved;
	}
	OBVR_LOG("Hands: both hands reset to their defaults%s", saved ? "" : " - COULD NOT SAVE the INI");
	return saved;
}

}  // namespace obvr::game

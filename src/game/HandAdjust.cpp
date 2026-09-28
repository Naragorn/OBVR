#include "game/HandAdjust.h"

#include <cstdio>

#include "core/AtomicFlag.h"
#include "core/Config.h"
#include "core/Log.h"
#include "ui/NativeHandAdjust.h"

namespace obvr::game {
namespace {

AtomicFlag g_guideRequested;
AtomicFlag g_holsterFitRequested;
AtomicFlag g_finishRequested;
AtomicFlag g_active;
AtomicFlag g_stowWindowRequested;
AtomicFlag g_stowPlaceActive;
volatile long g_stowCommand = 0;  // a vr::StowPlaceCommand, 0 for none
ui::HandAdjustSession g_session;  // stepped only in the frame

}  // namespace

void RequestHandAdjustGuide() { g_guideRequested.Set(true); }
bool TakeHandAdjustGuideRequest() { return g_guideRequested.Take(); }
void RequestHolsterFit() { g_holsterFitRequested.Set(true); }
bool TakeHolsterFitRequest() { return g_holsterFitRequested.Take(); }
void RequestStowPlaceWindow() { g_stowWindowRequested.Set(true); }
bool TakeStowPlaceWindowRequest() { return g_stowWindowRequested.Take(); }
void SendStowPlaceCommand(int command) {
#if defined(_MSC_VER)
	_InterlockedExchange(&g_stowCommand, command);
#else
	__atomic_store_n(&g_stowCommand, command, __ATOMIC_SEQ_CST);
#endif
}
int TakeStowPlaceCommand() {
#if defined(_MSC_VER)
	return static_cast<int>(_InterlockedExchange(&g_stowCommand, 0));
#else
	return static_cast<int>(__atomic_exchange_n(&g_stowCommand, 0, __ATOMIC_SEQ_CST));
#endif
}
void NoteStowPlaceActive(bool active) { g_stowPlaceActive.Set(active); }
bool StowPlaceActive() { return g_stowPlaceActive.Get(); }

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

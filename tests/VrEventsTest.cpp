// Checks how OpenVR's events are taken each frame (vr/VrEvents.h): the
// keyboard's characters only while OBVR's keyboard is open, SteamVR's quit
// whatever is open, and the quit's steps - acknowledged once, the engine's
// own quit flag raised once, a quit the engine cannot take yet tried again.

#include <cstdio>
#include <cstring>

#include "vr/VrEvents.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

openvr::VREvent EventOf(UInt32 type, const char* input = "") {
	openvr::VREvent event{};
	event.eventType = type;
	for (UInt32 i = 0; i < 8 && input[i] != '\0'; ++i) {
		event.data.keyboard.newInput[i] = input[i];
	}
	return event;
}

void TestNumbers() {
	std::printf("The event numbers (openvr_capi.h, EVREventType)\n");
	Check(openvr::kEventQuit == 700, "VREvent_Quit is 700");
	Check(openvr::kEventKeyboardClosed == 1200 && openvr::kEventKeyboardCharInput == 1201 &&
	          openvr::kEventKeyboardDone == 1202,
	      "the keyboard's three stay 1200..1202");
	Check(offsetof(openvr::IVRSystemFnTable, AcknowledgeQuit_Exiting) == 47 * sizeof(void*),
	      "AcknowledgeQuit_Exiting is entry 47 of VR_IVRSystem_FnTable");
	Check(offsetof(openvr::IVRSystemFnTable, PollNextEvent) == 30 * sizeof(void*) &&
	          offsetof(openvr::IVRSystemFnTable, GetControllerState) == 37 * sizeof(void*),
	      "the entries before it keep their places");
}

void TestKeyboardEvents() {
	std::printf("The keyboard's events\n");
	FrameEvents events{};
	TakeEvent(events, EventOf(openvr::kEventKeyboardCharInput, "ab"), true, sizeof(events.chars));
	TakeEvent(events, EventOf(openvr::kEventKeyboardCharInput, "c"), true, sizeof(events.chars));
	Check(std::strcmp(events.chars, "abc") == 0 && events.charEvents == 2 && !events.done && !events.closed,
	      "characters while the keyboard is open: gathered in order, each event counted");
	TakeEvent(events, EventOf(openvr::kEventKeyboardDone), true, sizeof(events.chars));
	Check(events.done && events.closed, "Done: done and closed");

	FrameEvents closedOnly{};
	TakeEvent(closedOnly, EventOf(openvr::kEventKeyboardClosed), true, sizeof(closedOnly.chars));
	Check(closedOnly.closed && !closedOnly.done, "Closed: closed, not done");

	FrameEvents notOpen{};
	TakeEvent(notOpen, EventOf(openvr::kEventKeyboardCharInput, "x"), false, sizeof(notOpen.chars));
	TakeEvent(notOpen, EventOf(openvr::kEventKeyboardDone), false, sizeof(notOpen.chars));
	Check(notOpen.chars[0] == '\0' && notOpen.charEvents == 0 && !notOpen.done && !notOpen.closed,
	      "keyboard events while OBVR's keyboard is not open: left alone (as before, when nothing polled)");

	FrameEvents full{};
	TakeEvent(full, EventOf(openvr::kEventKeyboardCharInput, "abcdefgh"), true, 4);
	Check(std::strcmp(full.chars, "abc") == 0 && full.charEvents == 1,
	      "a buffer of four: three characters and the terminator, the rest left out");

	FrameEvents other{};
	TakeEvent(other, EventOf(100), true, sizeof(other.chars));
	Check(!other.quit && other.chars[0] == '\0' && !other.closed, "another event: nothing taken");
}

void TestQuitEvent() {
	std::printf("SteamVR's quit\n");
	FrameEvents events{};
	TakeEvent(events, EventOf(openvr::kEventQuit), false, sizeof(events.chars));
	Check(events.quit, "VREvent_Quit with the keyboard closed: seen");
	FrameEvents typing{};
	TakeEvent(typing, EventOf(openvr::kEventQuit), true, sizeof(typing.chars));
	Check(typing.quit, "and with it open");
	FrameEvents near{};
	TakeEvent(near, EventOf(701), false, sizeof(near.chars));
	TakeEvent(near, EventOf(703), false, sizeof(near.chars));
	TakeEvent(near, EventOf(704), false, sizeof(near.chars));
	Check(!near.quit, "ProcessQuit (another process), QuitAcknowledged and DriverRequestedQuit: not ours");
}

void TestQuitSteps() {
	std::printf("The quit's steps\n");
	QuitState state{};
	Check(StepQuit(state, false, true, false) == QuitAction::None && !state.acknowledged, "no quit: nothing");

	QuitState once{};
	Check(StepQuit(once, true, true, false) == QuitAction::AcknowledgeAndQuit && once.acknowledged,
	      "the quit, the engine ready: acknowledged and the engine's quit flag raised");
	Check(StepQuit(once, true, true, true) == QuitAction::None, "the quit again: nothing more");
	Check(StepQuit(once, false, true, true) == QuitAction::None, "and the frames after: nothing");

	QuitState notReady{};
	Check(StepQuit(notReady, true, false, false) == QuitAction::NotYet && !notReady.acknowledged,
	      "the quit with the engine's globals unreadable: not yet, nothing acknowledged");
	Check(notReady.pending, "the quit kept pending");
	Check(StepQuit(notReady, false, true, false) == QuitAction::AcknowledgeAndQuit && notReady.acknowledged,
	      "the next frame with the engine ready: acknowledged and quit, without a second event");

	QuitState engineQuitting{};
	Check(StepQuit(engineQuitting, true, true, true) == QuitAction::AcknowledgeOnly && engineQuitting.acknowledged,
	      "the quit while the engine already quits (its own menu): acknowledged only");
}

}  // namespace

int main() {
	TestNumbers();
	TestKeyboardEvents();
	TestQuitEvent();
	TestQuitSteps();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

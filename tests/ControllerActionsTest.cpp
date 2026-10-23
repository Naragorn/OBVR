#include <cmath>
#include <cstdio>
#include <cstring>

#include "vr/ActionsWatch.h"
#include "vr/ControllerActions.h"

namespace {

using obvr::vr::HandPose;
using obvr::vr::input::Action;
using obvr::vr::input::ActionSet;
using obvr::vr::input::AnalogData;
using obvr::vr::input::DigitalData;
using obvr::vr::input::Table;

int g_failures = 0;
bool g_digitalActive[5]{};
bool g_digitalState[5]{};
int g_digitalError[5]{};
// The analog actions: the trigger, the stick, the grip's pull and the grip's
// force (vr/ControllerActions.h, from Action::Trigger on).
bool g_analogActive[4]{};
float g_analogX[4]{};
float g_analogY[4]{};
int g_analogError[4]{};
int g_manifestError = 0;
int g_setError = 0;
UInt64 g_setHandle = 77;
int g_actionErrorAt = -1;
int g_actionError = 0;
int g_actionZeroAt = -1;
int g_actionCalls = 0;
char g_manifestPath[128]{};
char g_actionPaths[18][128]{};
UInt32 g_updateSize = 0;
UInt32 g_updateCount = 0;
UInt64 g_updatedSet = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float actual, float expected) { return std::fabs(actual - expected) < 0.0001f; }

void ResetFake() {
	for (unsigned i = 0; i < 5; ++i) {
		g_digitalActive[i] = true;
		g_digitalState[i] = false;
		g_digitalError[i] = 0;
	}
	for (unsigned i = 0; i < 4; ++i) {
		g_analogActive[i] = true;
		g_analogX[i] = 0.0f;
		g_analogY[i] = 0.0f;
		g_analogError[i] = 0;
	}
}

void ResetSetupFake() {
	g_manifestError = 0;
	g_setError = 0;
	g_setHandle = 77;
	g_actionErrorAt = -1;
	g_actionError = 0;
	g_actionZeroAt = -1;
	g_actionCalls = 0;
	g_manifestPath[0] = '\0';
	for (auto& path : g_actionPaths) {
		path[0] = '\0';
	}
	g_updateSize = 0;
	g_updateCount = 0;
	g_updatedSet = 0;
}

int __stdcall FakeDigital(UInt64 handle, DigitalData* data, UInt32, UInt64) {
	const unsigned action = static_cast<unsigned>(handle - 100);
	if (action >= 5 || g_digitalError[action] != 0) {
		return action < 5 ? g_digitalError[action] : 99;
	}
	*data = DigitalData{};
	data->active = g_digitalActive[action];
	data->state = g_digitalState[action];
	return 0;
}

int __stdcall FakeAnalog(UInt64 handle, AnalogData* data, UInt32, UInt64) {
	const unsigned action = static_cast<unsigned>(handle - 105);
	if (action >= 4 || g_analogError[action] != 0) {
		return action < 4 ? g_analogError[action] : 99;
	}
	*data = AnalogData{};
	data->active = g_analogActive[action];
	data->x = g_analogX[action];
	data->y = g_analogY[action];
	return 0;
}

int __stdcall FakeSetManifest(const char* path) {
	std::strncpy(g_manifestPath, path != nullptr ? path : "", sizeof(g_manifestPath) - 1);
	return g_manifestError;
}

int __stdcall FakeGetSet(const char* path, UInt64* handle) {
	Check(path != nullptr && std::strcmp(path, "/actions/obvr") == 0,
	      "setup resolves the normalized action set path");
	if (g_setError == 0 && handle != nullptr) {
		*handle = g_setHandle;
	}
	return g_setError;
}

int __stdcall FakeGetAction(const char* path, UInt64* handle) {
	const int call = g_actionCalls++;
	if (call < 18 && path != nullptr) {
		std::strncpy(g_actionPaths[call], path, sizeof(g_actionPaths[call]) - 1);
	}
	if (call == g_actionErrorAt) {
		return g_actionError;
	}
	if (handle != nullptr) {
		*handle = call == g_actionZeroAt ? 0 : 200 + static_cast<UInt64>(call);
	}
	return 0;
}

int __stdcall FakeUpdate(ActionSet* sets, UInt32 size, UInt32 count) {
	g_updateSize = size;
	g_updateCount = count;
	g_updatedSet = sets != nullptr ? sets->set : 0;
	return 0;
}

Table FakeTable() {
	Table table{};
	table.Digital = &FakeDigital;
	table.Analog = &FakeAnalog;
	return table;
}

Table FullFakeTable() {
	Table table = FakeTable();
	table.SetManifest = &FakeSetManifest;
	table.GetSet = &FakeGetSet;
	table.GetAction = &FakeGetAction;
	table.Update = &FakeUpdate;
	return table;
}

void TestUnavailableIsNeutral() {
	std::printf("Unavailable action tables\n");
	HandPose out{};
	out.buttonsPressed = ~0ull;
	out.trigger = 1.0f;
	out.thumbX = 1.0f;
	out.thumbY = -1.0f;
	out.gripPull = 1.0f;
	out.gripForce = 1.0f;
	UInt32 activeMask = 0xFFFFFFFFu;
	Check(obvr::vr::input::ReadControls(nullptr, nullptr, false, out, &activeMask) == -1,
		  "missing table refuses the read");
	Check(out.buttonsPressed == 0 && Near(out.trigger, 0.0f) && Near(out.thumbX, 0.0f) &&
	          Near(out.thumbY, 0.0f) && Near(out.gripPull, 0.0f) && Near(out.gripForce, 0.0f) &&
	          activeMask == 0,
	      "missing table leaves every control neutral, the grip's pull and force too");

	Table empty{};
	Check(obvr::vr::input::ReadControls(&empty, nullptr, true, out, &activeMask) == -1,
	      "missing callbacks refuse the read");
	Table available = FakeTable();
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106, 107, 108};
	Check(obvr::vr::input::ReadControls(&available, handles, false, out, &activeMask) == -1,
	      "an action frame that was not updated refuses the read");
}

void TestNormalizedControls() {
	std::printf("Normalized controller actions\n");
	ResetFake();
	for (unsigned i = 0; i < 5; ++i) {
		g_digitalState[i] = true;
	}
	g_analogX[static_cast<unsigned>(Action::Trigger) - 5] = 0.75f;
	g_analogX[static_cast<unsigned>(Action::Stick) - 5] = 0.5f;
	g_analogY[static_cast<unsigned>(Action::Stick) - 5] = -0.25f;
	g_analogX[static_cast<unsigned>(Action::GripPull) - 5] = 0.4f;
	g_analogX[static_cast<unsigned>(Action::GripForce) - 5] = 0.15f;
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106, 107, 108};
	HandPose out{};
	UInt32 activeMask = 0;
	Table table = FakeTable();
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == 0,
	      "all active actions read successfully");
	Check(Near(out.gripPull, 0.4f) && Near(out.gripForce, 0.15f),
	      "the grip's pull and force are read as they are");
	Check((out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexJoystick)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonA)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexB)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexGrip)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexTrackpad)) != 0,
	      "digital actions use stable normalized button roles");
	Check(activeMask == 0x1FFu, "active mask reports every action independently, nine of them");
	Check(Near(out.trigger, 0.75f) && Near(out.thumbX, 0.5f) && Near(out.thumbY, -0.25f) &&
	          out.thumbFromJoystickAxis,
	      "analog trigger and stick values are preserved");
	HandPose posed{};
	posed.valid = true;
	posed.position = {1.0f, 2.0f, 3.0f};
	obvr::vr::input::ApplyActionControls(posed, out, activeMask, 0);
	Check(posed.valid && Near(posed.position.x, 1.0f) && Near(posed.position.y, 2.0f) &&
	          Near(posed.position.z, 3.0f) && posed.actionInput &&
	          posed.actionActiveMask == activeMask && posed.actionError == 0,
	      "applying controls preserves a valid real pose and records provenance");
	Check(Near(posed.gripPull, 0.4f) && Near(posed.gripForce, 0.15f),
	      "applying controls carries the grip's pull and force over");
}

void TestFailuresStayNeutral() {
	std::printf("Failed and inactive actions\n");
	ResetFake();
	g_digitalState[0] = true;
	g_digitalState[1] = true;
	g_digitalError[1] = 7;
	g_digitalActive[2] = false;
	g_digitalState[3] = true;
	g_digitalError[3] = 11;
	g_analogX[0] = 1.5f;
	g_analogX[1] = 2.0f;
	g_analogY[1] = -2.0f;
	g_analogError[1] = 9;
	// The grip's pull out of range; the force inactive, as on a Touch, which
	// has no force sensor and leaves the action unbound.
	g_analogX[2] = 1.5f;
	g_analogActive[3] = false;
	g_analogX[3] = 0.9f;
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106, 107, 108};
	HandPose out{};
	UInt32 activeMask = 0;
	Table table = FakeTable();
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == 7,
	      "first action error is returned");
	Check(Near(out.gripPull, 0.0f) && Near(out.gripForce, 0.0f) && (activeMask & (1u << 7)) == 0 &&
	          (activeMask & (1u << 8)) == 0,
	      "an out-of-range pull and an inactive force stay neutral and unflagged");
	Check((out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexJoystick)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonA)) == 0 &&
          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexB)) == 0 &&
          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexGrip)) == 0,
	      "failed, inactive, and failed-grip actions do not manufacture presses");
	Check((activeMask & (1u << 0)) != 0 && (activeMask & (1u << 1)) == 0 &&
	          (activeMask & (1u << 2)) == 0 && (activeMask & (1u << 3)) == 0 &&
	          (activeMask & (1u << 4)) != 0 && (activeMask & (1u << 5)) == 0 &&
	          (activeMask & (1u << 6)) == 0,
	      "active mask distinguishes failed, inactive, malformed, and active controls");
	Check(Near(out.trigger, 0.0f) && Near(out.thumbX, 0.0f) && Near(out.thumbY, 0.0f),
	      "out-of-range and failed analog actions stay neutral");
}

void TestAnalogValidation() {
	std::printf("Analog validation\n");
	ResetFake();
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106, 107, 108};
	Table table = FakeTable();
	HandPose out{};
	UInt32 activeMask = 0;
	g_analogX[0] = 0.0f;
	g_analogX[1] = -1.0f;
	g_analogY[1] = 1.0f;
	g_analogX[2] = 1.0f;
	g_analogX[3] = 0.0f;
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == 0 &&
	          (activeMask & (1u << 5)) != 0 && (activeMask & (1u << 6)) != 0 &&
	          Near(out.trigger, 0.0f) && Near(out.thumbX, -1.0f) && Near(out.thumbY, 1.0f),
	      "inclusive analog bounds remain active");
	Check((activeMask & (1u << 7)) != 0 && (activeMask & (1u << 8)) != 0 && Near(out.gripPull, 1.0f) &&
	          Near(out.gripForce, 0.0f),
	      "a full pull and a force of nothing are in bounds");

	ResetFake();
	g_analogX[0] = NAN;
	g_analogX[1] = INFINITY;
	g_analogY[1] = 0.25f;
	g_analogX[2] = NAN;
	g_analogX[3] = -0.5f;
	activeMask = 0;
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == -2 &&
	          (activeMask & (1u << 5)) == 0 && (activeMask & (1u << 6)) == 0 &&
	          Near(out.trigger, 0.0f) && Near(out.thumbX, 0.0f) && Near(out.thumbY, 0.0f),
	      "NaN and infinity make their analog actions unavailable");
	Check((activeMask & (1u << 7)) == 0 && (activeMask & (1u << 8)) == 0 && Near(out.gripPull, 0.0f) &&
	          Near(out.gripForce, 0.0f),
	      "a pull that is not a number and a negative force are unavailable too");

	ResetFake();
	g_analogX[1] = 0.5f;
	g_analogY[1] = -INFINITY;
	activeMask = 0;
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == -2 &&
	          (activeMask & (1u << 6)) == 0 && Near(out.thumbX, 0.0f) &&
	          Near(out.thumbY, 0.0f),
	      "one malformed stick component rejects the whole stick action");
}

void TestSetupAndUpdateBoundaries() {
	std::printf("Action setup and frame update boundaries\n");
	ResetSetupFake();
	Table table = FullFakeTable();
	UInt64 actionSet = 0;
	UInt64 handles[2][obvr::vr::input::Count]{};
	Check(obvr::vr::input::ConfigureActions(&table, "actions.json", actionSet, handles) == 0 &&
	          std::strcmp(g_manifestPath, "actions.json") == 0 && actionSet == 77 &&
	          handles[0][0] == 200 && handles[1][obvr::vr::input::Count - 1] == 217 &&
	          std::strcmp(g_actionPaths[0], "/actions/obvr/in/right_stick_click") == 0 &&
	          std::strcmp(g_actionPaths[9], "/actions/obvr/in/left_stick_click") == 0,
	      "complete setup resolves manifest, set, and both hands in order");
	Check(std::strcmp(g_actionPaths[7], "/actions/obvr/in/right_grip_pull") == 0 &&
	          std::strcmp(g_actionPaths[8], "/actions/obvr/in/right_grip_force") == 0 &&
	          std::strcmp(g_actionPaths[16], "/actions/obvr/in/left_grip_pull") == 0 &&
	          std::strcmp(g_actionPaths[17], "/actions/obvr/in/left_grip_force") == 0,
	      "the grip's pull and force are resolved after the stick, on both hands");

	ActionSet active{};
	Check(obvr::vr::input::UpdateActionState(&table, actionSet, active) == 0 &&
	          g_updatedSet == 77 && g_updateSize == sizeof(ActionSet) && g_updateCount == 1,
	      "frame update passes byte size then one-entry count");
	Table noUpdate = table;
	noUpdate.Update = nullptr;
	Check(obvr::vr::input::UpdateActionState(&noUpdate, actionSet, active) == -1,
	      "missing frame update refuses the action frame");

	ResetSetupFake();
	UInt64 staleSet = 88;
	UInt64 staleHandles[2][obvr::vr::input::Count]{};
	for (auto& hand : staleHandles) {
		for (auto& handle : hand) {
			handle = 99;
		}
	}
	Table missing = table;
	missing.Analog = nullptr;
	Check(obvr::vr::input::ConfigureActions(&missing, "actions.json", staleSet, staleHandles) == -1 &&
	          staleSet == 0 && staleHandles[0][0] == 0,
	      "missing required table methods refuse before manifest setup");
	missing = table;
	missing.SetManifest = nullptr;
	Check(obvr::vr::input::ConfigureActions(&missing, "actions.json", staleSet, staleHandles) == -1,
	      "missing manifest method refuses setup");
	missing = table;
	missing.GetSet = nullptr;
	Check(obvr::vr::input::ConfigureActions(&missing, "actions.json", staleSet, staleHandles) == -1,
	      "missing action set method refuses setup");
	missing = table;
	missing.GetAction = nullptr;
	Check(obvr::vr::input::ConfigureActions(&missing, "actions.json", staleSet, staleHandles) == -1,
	      "missing action handle method refuses setup");
	missing = table;
	missing.Update = nullptr;
	Check(obvr::vr::input::ConfigureActions(&missing, "actions.json", staleSet, staleHandles) == -1,
	      "missing update method refuses setup");
	missing = table;
	missing.Digital = nullptr;
	Check(obvr::vr::input::ConfigureActions(&missing, "actions.json", staleSet, staleHandles) == -1,
	      "missing digital read method refuses setup");

	ResetSetupFake();
	Table manifestFailure = table;
	g_manifestError = 21;
	Check(obvr::vr::input::ConfigureActions(&manifestFailure, "actions.json", staleSet,
	                                        staleHandles) == 21 &&
	          staleSet == 0 && staleHandles[1][8] == 0,
	      "manifest failure clears all partial outputs");

	ResetSetupFake();
	g_setError = 22;
	Check(obvr::vr::input::ConfigureActions(&table, "actions.json", staleSet, staleHandles) == 22 &&
	          staleSet == 0 && staleHandles[0][0] == 0,
	      "action set failure clears all partial outputs");

	ResetSetupFake();
	g_actionErrorAt = 3;
	g_actionError = 23;
	Check(obvr::vr::input::ConfigureActions(&table, "actions.json", staleSet, staleHandles) == 23 &&
	          staleSet == 0 && staleHandles[0][0] == 0 && staleHandles[1][8] == 0,
	      "partial action handle failure clears the complete setup");

	ResetSetupFake();
	g_setHandle = 0;
	Check(obvr::vr::input::ConfigureActions(&table, "actions.json", staleSet, staleHandles) == -1 &&
	          staleSet == 0,
	      "a missing action set handle is rejected");

	ResetSetupFake();
	g_actionZeroAt = 0;
	Check(obvr::vr::input::ConfigureActions(&table, "actions.json", staleSet, staleHandles) == -1 &&
	          staleSet == 0,
	      "a missing action handle is rejected");
}

}  // namespace

void TestActionsInactiveWatch() {
	std::printf("The actions going dead on a hand (vr/ActionsWatch.h)\n");
	using obvr::vr::ActionsInactiveVerdict;
	using obvr::vr::ActionsInactiveWatch;
	using obvr::vr::StepActionsInactive;
	const float frame = 1.0f / 90.0f;
	ActionsInactiveWatch w;
	// Alive: nothing, however long.
	bool quiet = true;
	for (int i = 0; i < 2000; ++i) {
		const ActionsInactiveVerdict v = StepActionsInactive(w, true, true, 0x6F, frame);
		quiet = quiet && !v.reportInactive && !v.reportBack;
	}
	Check(quiet && !w.inactive, "actions alive for 22 s: nothing said");
	// A blip of two frames: nothing said either way.
	ActionsInactiveVerdict v = StepActionsInactive(w, true, true, 0, frame);
	Check(!v.reportInactive && w.inactive, "dead for a frame: counted, not yet said");
	v = StepActionsInactive(w, true, true, 0, frame);
	v = StepActionsInactive(w, true, true, 0x6F, frame);
	Check(!v.reportInactive && !v.reportBack && !w.inactive, "back after two frames: a blip, nothing said");
	// Dead for a second: said once at the second, not every frame after.
	int said = 0;
	float saidAt = 0.0f;
	for (int i = 0; i < 180; ++i) {
		v = StepActionsInactive(w, true, true, 0, frame);
		if (v.reportInactive) {
			++said;
			saidAt = v.seconds;
		}
	}
	Check(said == 1 && saidAt >= 1.0f && saidAt < 1.05f, "dead for two seconds: said once, at the first second");
	// Then every ten seconds: at 11 and 21 over the next twenty.
	said = 0;
	for (int i = 0; i < 1800; ++i) {
		v = StepActionsInactive(w, true, true, 0, frame);
		if (v.reportInactive) {
			++said;
			saidAt = v.seconds;
		}
	}
	Check(said == 2 && saidAt >= 21.0f && saidAt < 21.05f, "dead for twenty seconds more: said at 11 and 21");
	// Back: said once with the time it took, and the count starts over.
	v = StepActionsInactive(w, true, true, 0x6F, frame);
	Check(v.reportBack && v.seconds > 21.9f && v.seconds < 22.1f && !w.inactive && w.seconds == 0.0f,
	      "back after 22 s: said once with the time, the watch cleared");
	v = StepActionsInactive(w, true, true, 0x6F, frame);
	Check(!v.reportBack, "...and not again");
	// A controller that drops out of tracking while dead: nothing to say,
	// the mask means nothing untracked.
	for (int i = 0; i < 100; ++i) {
		v = StepActionsInactive(w, true, true, 0, frame);
	}
	v = StepActionsInactive(w, false, true, 0, frame);
	Check(!v.reportBack && !v.reportInactive && !w.inactive, "untracked while dead: cleared without a word");
	// Legacy input carries no mask: never dead.
	for (int i = 0; i < 200; ++i) {
		v = StepActionsInactive(w, true, false, 0, frame);
	}
	Check(!v.reportInactive && !w.inactive, "legacy input with an empty mask: not dead");
	// A frame of no time does not count up, and one bit alive is alive.
	v = StepActionsInactive(w, true, true, 0, 0.0f);
	Check(w.inactive && w.seconds == 0.0f, "a frame of no time: dead, no time counted");
	v = StepActionsInactive(w, true, true, 0x08, frame);
	Check(!w.inactive, "one action alive: not dead");
}

int main() {
	TestActionsInactiveWatch();
	TestUnavailableIsNeutral();
	TestNormalizedControls();
	TestFailuresStayNeutral();
	TestAnalogValidation();
	TestSetupAndUpdateBoundaries();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

#include <cmath>
#include <cstdio>
#include <cstring>

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
bool g_analogActive[2]{};
float g_analogX[2]{};
float g_analogY[2]{};
int g_analogError[2]{};
int g_manifestError = 0;
int g_setError = 0;
UInt64 g_setHandle = 77;
int g_actionErrorAt = -1;
int g_actionError = 0;
int g_actionZeroAt = -1;
int g_actionCalls = 0;
char g_manifestPath[128]{};
char g_actionPaths[14][128]{};
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
	for (unsigned i = 0; i < 2; ++i) {
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
	if (action >= 2 || g_analogError[action] != 0) {
		return action < 2 ? g_analogError[action] : 99;
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
	if (call < 14 && path != nullptr) {
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
	UInt32 activeMask = 0xFFFFFFFFu;
	Check(obvr::vr::input::ReadControls(nullptr, nullptr, false, out, &activeMask) == -1,
		  "missing table refuses the read");
	Check(out.buttonsPressed == 0 && Near(out.trigger, 0.0f) && Near(out.thumbX, 0.0f) &&
	          Near(out.thumbY, 0.0f) && activeMask == 0,
	      "missing table leaves every control neutral");

	Table empty{};
	Check(obvr::vr::input::ReadControls(&empty, nullptr, true, out, &activeMask) == -1,
	      "missing callbacks refuse the read");
	Table available = FakeTable();
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106};
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
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106};
	HandPose out{};
	UInt32 activeMask = 0;
	Table table = FakeTable();
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == 0,
	      "all active actions read successfully");
	Check((out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexJoystick)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonA)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexB)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexGrip)) != 0 &&
	          (out.buttonsPressed & (1ull << obvr::vr::openvr::kButtonIndexTrackpad)) != 0,
	      "digital actions use stable normalized button roles");
	Check(activeMask == 0x7Fu, "active mask reports every action independently");
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
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106};
	HandPose out{};
	UInt32 activeMask = 0;
	Table table = FakeTable();
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == 7,
	      "first action error is returned");
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
	const UInt64 handles[] = {100, 101, 102, 103, 104, 105, 106};
	Table table = FakeTable();
	HandPose out{};
	UInt32 activeMask = 0;
	g_analogX[0] = 0.0f;
	g_analogX[1] = -1.0f;
	g_analogY[1] = 1.0f;
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == 0 &&
	          (activeMask & (1u << 5)) != 0 && (activeMask & (1u << 6)) != 0 &&
	          Near(out.trigger, 0.0f) && Near(out.thumbX, -1.0f) && Near(out.thumbY, 1.0f),
	      "inclusive analog bounds remain active");

	ResetFake();
	g_analogX[0] = NAN;
	g_analogX[1] = INFINITY;
	g_analogY[1] = 0.25f;
	activeMask = 0;
	Check(obvr::vr::input::ReadControls(&table, handles, true, out, &activeMask) == -2 &&
	          (activeMask & (1u << 5)) == 0 && (activeMask & (1u << 6)) == 0 &&
	          Near(out.trigger, 0.0f) && Near(out.thumbX, 0.0f) && Near(out.thumbY, 0.0f),
	      "NaN and infinity make their analog actions unavailable");

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
	          handles[0][0] == 200 && handles[1][obvr::vr::input::Count - 1] == 213 &&
	          std::strcmp(g_actionPaths[0], "/actions/obvr/in/right_stick_click") == 0 &&
	          std::strcmp(g_actionPaths[7], "/actions/obvr/in/left_stick_click") == 0,
	      "complete setup resolves manifest, set, and both hands in order");

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
	          staleSet == 0 && staleHandles[1][6] == 0,
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
	          staleSet == 0 && staleHandles[0][0] == 0 && staleHandles[1][6] == 0,
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

int main() {
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

// Checks the pure half of the controllers' input routes (game/InputRoute.h):
// which route a frame takes, what every change of route does, which key a
// virtual key becomes in the game's input, and how the overlay is written
// into the state a device read left.

#include <cstdio>
#include <cstring>

#include "game/InputRoute.h"

namespace {

using obvr::game::ApplyEngineOverlay;
using obvr::game::ClearEngineOverlay;
using obvr::game::EngineButtonsHeld;
using obvr::game::EngineInputOverlay;
using obvr::game::EngineInputTarget;
using obvr::game::EngineKeysHeld;
using obvr::game::EngineTargetFor;
using obvr::game::EngineTargetHeld;
using obvr::game::HoldEngineTarget;
using obvr::game::InputRoute;
using obvr::game::InputRouteName;
using obvr::game::InputRouteState;
using obvr::game::InputRouteStep;
using obvr::game::StepInputRoute;
using obvr::game::WantedInputRoute;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestWantedRoute() {
	std::printf("Wanted route\n");
	Check(WantedInputRoute(true, true) == InputRoute::Windows, "in front: through Windows");
	Check(WantedInputRoute(true, false) == InputRoute::Windows,
	      "in front: through Windows even with the engine route not allowed");
	Check(WantedInputRoute(false, true) == InputRoute::Engine, "behind, allowed: into the game's input");
	Check(WantedInputRoute(false, false) == InputRoute::Off, "behind, not allowed: nowhere");
}

// One step from a given route to the route a front/allowed pair wants.
InputRouteStep From(InputRoute from, bool inFront, bool allowed, InputRoute* after = nullptr) {
	InputRouteState state;
	state.route = from;
	const InputRouteStep step = StepInputRoute(state, inFront, allowed);
	if (after != nullptr) {
		*after = state.route;
	}
	return step;
}

void TestEveryChange() {
	std::printf("Every change of route\n");
	InputRoute after = InputRoute::Windows;

	// Windows -> Windows: nothing happens.
	InputRouteStep s = From(InputRoute::Windows, true, true, &after);
	Check(s.route == InputRoute::Windows && !s.changed && !s.releaseWindowsKeys && !s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Windows,
	      "Windows to Windows: no change, nothing released, nothing injected");

	// Windows -> Engine: Windows lets go, the overlay goes in.
	s = From(InputRoute::Windows, false, true, &after);
	Check(s.route == InputRoute::Engine && s.changed && s.releaseWindowsKeys && s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Engine,
	      "Windows to Engine: Windows keys released, the overlay injected");

	// Windows -> Off: Windows lets go, nothing goes in.
	s = From(InputRoute::Windows, false, false, &after);
	Check(s.route == InputRoute::Off && s.changed && s.releaseWindowsKeys && !s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Off,
	      "Windows to Off: Windows keys released, nothing injected");

	// Engine -> Engine: the overlay keeps going in.
	s = From(InputRoute::Engine, false, true, &after);
	Check(s.route == InputRoute::Engine && !s.changed && !s.releaseWindowsKeys && s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Engine,
	      "Engine to Engine: injected, nothing released");

	// Engine -> Windows: one frame of overlap.
	s = From(InputRoute::Engine, true, true, &after);
	Check(s.route == InputRoute::Windows && s.changed && !s.releaseWindowsKeys && s.engineInject &&
	          s.engineKeepsLast && after == InputRoute::Windows,
	      "Engine to Windows: the overlay kept for one more poll, nothing to release in Windows");

	// Engine -> Off: the overlay is dropped.
	s = From(InputRoute::Engine, false, false, &after);
	Check(s.route == InputRoute::Off && s.changed && !s.releaseWindowsKeys && !s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Off,
	      "Engine to Off: the overlay dropped");

	// Off -> Windows: Windows takes over from nothing.
	s = From(InputRoute::Off, true, true, &after);
	Check(s.route == InputRoute::Windows && s.changed && !s.releaseWindowsKeys && !s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Windows,
	      "Off to Windows: nothing kept, nothing released");

	// Off -> Engine: the overlay starts.
	s = From(InputRoute::Off, false, true, &after);
	Check(s.route == InputRoute::Engine && s.changed && !s.releaseWindowsKeys && s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Engine,
	      "Off to Engine: injected, nothing released");

	// Off -> Off: still nothing.
	s = From(InputRoute::Off, false, false, &after);
	Check(s.route == InputRoute::Off && !s.changed && !s.releaseWindowsKeys && !s.engineInject &&
	          !s.engineKeepsLast && after == InputRoute::Off,
	      "Off to Off: nothing");
}

void TestSequence() {
	std::printf("A switch away and back\n");
	InputRouteState state;  // starts on Windows, the game in front
	const InputRouteStep a = StepInputRoute(state, true, true);
	const InputRouteStep b = StepInputRoute(state, false, true);
	const InputRouteStep c = StepInputRoute(state, false, true);
	const InputRouteStep d = StepInputRoute(state, true, true);
	const InputRouteStep e = StepInputRoute(state, true, true);
	Check(!a.changed && !a.engineInject, "first frame in front: through Windows, unchanged");
	Check(b.changed && b.releaseWindowsKeys && b.engineInject, "leaving the front: released, injected");
	Check(!c.changed && !c.releaseWindowsKeys && c.engineInject, "behind: injected, released only once");
	Check(d.changed && d.engineKeepsLast && d.engineInject && !d.releaseWindowsKeys,
	      "back in front: one poll of overlap");
	Check(!e.changed && !e.engineInject && !e.engineKeepsLast, "the frame after: the overlay is gone");
}

void TestNames() {
	std::printf("Names\n");
	Check(std::strcmp(InputRouteName(InputRoute::Windows), "through Windows") == 0, "Windows named");
	Check(std::strcmp(InputRouteName(InputRoute::Engine), "straight into the game's input") == 0,
	      "Engine named");
	Check(std::strcmp(InputRouteName(InputRoute::Off), "nowhere") == 0, "Off named");
	Check(std::strcmp(InputRouteName(static_cast<InputRoute>(7)), "?") == 0, "out of range named ?");
}

void TestTargets() {
	std::printf("Virtual keys in the game's input\n");
	EngineInputTarget t = EngineTargetFor(0x01, 0);
	Check(t.kind == EngineInputTarget::Kind::Button && t.index == 0, "left mouse button: button 0");
	t = EngineTargetFor(0x02, 0);
	Check(t.kind == EngineInputTarget::Kind::Button && t.index == 1, "right mouse button: button 1");
	t = EngineTargetFor('W', 0);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x11, "W: DIK_W 0x11");
	t = EngineTargetFor('W', 0x2C);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x11,
	      "W: the US scan code wins over the layout's");
	t = EngineTargetFor(0x10, 0);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x2A, "Shift: left shift 0x2A (Run)");
	t = EngineTargetFor(0x11, 0);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x1D, "Ctrl: left ctrl 0x1D (sneak)");
	t = EngineTargetFor(0x20, 0);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x39, "Space: 0x39 (activate)");
	t = EngineTargetFor('1', 0);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x02, "1: 0x02 (hotkey 1)");
	t = EngineTargetFor(0x26, 0x48);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0xC8,
	      "up arrow by the layout's 0x48, extended: DIK_UP 0xC8");
	t = EngineTargetFor(0x60, 0x52);
	Check(t.kind == EngineInputTarget::Kind::Key && t.index == 0x52, "numpad 0 by the layout's 0x52, not extended");
	t = EngineTargetFor(0x04, 0);
	Check(t.kind == EngineInputTarget::Kind::None, "middle button: nothing (Windows cannot press it either)");
	t = EngineTargetFor(0xE8, 0);
	Check(t.kind == EngineInputTarget::Kind::None, "a key without any scan code: nothing");
	t = EngineTargetFor(0xE8, 0xE0);
	Check(t.kind == EngineInputTarget::Kind::None, "a scan code above 0x7F: nothing");
}

void TestHolds() {
	std::printf("Holding and letting go\n");
	EngineInputOverlay o;
	const EngineInputTarget w = EngineTargetFor('W', 0);
	const EngineInputTarget left = EngineTargetFor(0x01, 0);
	const EngineInputTarget none = EngineTargetFor(0xE8, 0);
	Check(!EngineTargetHeld(o, w) && !EngineTargetHeld(o, left), "nothing held at first");
	HoldEngineTarget(o, w, true);
	HoldEngineTarget(o, left, true);
	Check(EngineTargetHeld(o, w) && EngineTargetHeld(o, left), "W and the left button held");
	Check(EngineKeysHeld(o) == 1 && EngineButtonsHeld(o) == 1, "counted: one key, one button");
	HoldEngineTarget(o, none, true);
	Check(EngineKeysHeld(o) == 1 && EngineButtonsHeld(o) == 1 && !EngineTargetHeld(o, none),
	      "a key without a target changes nothing");
	EngineInputTarget wild;
	wild.kind = EngineInputTarget::Kind::Button;
	wild.index = 9;
	HoldEngineTarget(o, wild, true);
	Check(EngineButtonsHeld(o) == 1 && !EngineTargetHeld(o, wild), "a button past the eighth is ignored");
	HoldEngineTarget(o, w, false);
	Check(!EngineTargetHeld(o, w) && EngineTargetHeld(o, left), "W let go, the button still held");
	o.dx = 5;
	ClearEngineOverlay(o);
	Check(EngineKeysHeld(o) == 0 && EngineButtonsHeld(o) == 0 && o.dx == 0, "cleared: everything let go");
}

void TestApply() {
	std::printf("Writing into the poll's state\n");
	EngineInputOverlay o;
	HoldEngineTarget(o, EngineTargetFor('W', 0), true);
	HoldEngineTarget(o, EngineTargetFor(0x02, 0), true);
	o.dx = 12;
	o.dy = -3;
	o.wheel = 240;

	UInt8 keys[256];
	std::memset(keys, 0, sizeof(keys));
	keys[0x1E] = 0x80;  // A held for real
	SInt32 axes[3] = {4, 0, 0};
	UInt8 buttons[8] = {0x80, 0, 0, 0, 0, 0, 0, 0};  // left held for real

	ApplyEngineOverlay(o, keys, axes, buttons);
	Check(keys[0x11] == 0x80, "W reads down, as DirectInput writes it");
	Check(keys[0x1E] == 0x80, "a real key down stays down");
	Check(keys[0x1F] == 0, "a key nobody holds stays up");
	Check(buttons[1] == 0x80 && buttons[0] == 0x80 && buttons[2] == 0,
	      "the right button down, the real left kept, the rest up");
	Check(axes[0] == 16 && axes[1] == -3 && axes[2] == 240, "the movement added to the real movement");
	Check(o.dx == 0 && o.dy == 0 && o.wheel == 0, "the movement is delivered once");
	Check(EngineKeysHeld(o) == 1 && EngineButtonsHeld(o) == 1, "the held keys stay held");

	std::memset(keys, 0, sizeof(keys));
	axes[0] = axes[1] = axes[2] = 0;
	std::memset(buttons, 0, sizeof(buttons));
	ApplyEngineOverlay(o, keys, axes, buttons);
	Check(keys[0x11] == 0x80 && buttons[1] == 0x80, "the next poll: still held");
	Check(axes[0] == 0 && axes[1] == 0 && axes[2] == 0, "the next poll: no movement twice");
}

}  // namespace

int main() {
	TestWantedRoute();
	TestEveryChange();
	TestSequence();
	TestNames();
	TestTargets();
	TestHolds();
	TestApply();
	std::printf(g_failures == 0 ? "All input route checks passed\n" : "%d input route check(s) FAILED\n",
	            g_failures);
	return g_failures == 0 ? 0 : 1;
}

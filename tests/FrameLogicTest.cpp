// Checks the per-frame bookkeeping of the camera hook.
//
// The callback these belong to cannot be tested - it needs a live CameraNode
// and reads the player through a hard-coded address - so the decisions it makes
// are kept in FrameLogic.h where they can be. All three are small enough to
// look obviously correct, which is exactly why they are worth pinning down: the
// recenter edge and the point-of-view transition were each written once, read
// once, and would break silently. A recenter that fires every frame while the
// key is held would re-zero the view continuously, and the only symptom would
// be a camera that refuses to move.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cstdio>

#include "camera/FrameLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestKeyEdge() {
	std::printf("Recenter key edge\n");

	obvr::camera::KeyEdge edge;

	Check(!edge.IsDown(), "a fresh edge starts up");
	Check(!edge.Update(false), "a key that stays up is no edge");
	Check(!edge.Update(false), "and still is not on the next frame");

	Check(edge.Update(true), "pressing the key is an edge");
	Check(edge.IsDown(), "and the key is remembered as down");

	// The one that matters. At 60 frames per second a held key would otherwise
	// recenter 60 times a second, taking the current pose as the new zero every
	// time - the camera would appear frozen for as long as the key is held.
	Check(!edge.Update(true), "holding it is not an edge again");
	Check(!edge.Update(true), "however long it is held");

	Check(!edge.Update(false), "releasing it is not an edge");
	Check(!edge.IsDown(), "and the key is remembered as up");

	Check(edge.Update(true), "pressing it again is an edge");
}

void TestKeyEdgeReset() {
	std::printf("Recenter switched off while the key is held\n");

	// RecenterKey=0 in the INI takes this path, and the INI can be reloaded
	// while the game runs. Without the reset, a key held across the change
	// would count as still down and the next poll would see no edge - the first
	// press after switching the feature back on would be swallowed.
	obvr::camera::KeyEdge edge;

	Check(edge.Update(true), "the key goes down");
	edge.Reset();
	Check(!edge.IsDown(), "reset forgets that it was down");
	Check(edge.Update(true), "so the next press counts as an edge again");
}

void TestPointOfView() {
	std::printf("Point of view\n");

	using obvr::camera::PovEvent;
	obvr::camera::State state;

	Check(!state.sawCameraNode, "nothing has been seen yet");

	Check(state.ObservePointOfView(false) == PovEvent::FirstPass,
	      "the first pass reports itself as the first pass");
	Check(state.sawCameraNode, "and is recorded");
	Check(!state.isThirdPerson, "with the point of view it saw");

	Check(state.ObservePointOfView(false) == PovEvent::Unchanged,
	      "the same point of view again is no event");

	Check(state.ObservePointOfView(true) == PovEvent::Switched,
	      "a change is reported");
	Check(state.isThirdPerson, "and the new point of view is recorded");

	Check(state.ObservePointOfView(true) == PovEvent::Unchanged,
	      "staying in third person is no event");
	Check(state.ObservePointOfView(false) == PovEvent::Switched,
	      "switching back is reported as well");
	Check(!state.isThirdPerson, "and recorded");
}

void TestFirstPassInThirdPerson() {
	std::printf("First pass in third person\n");

	// The default of the field is first person, so a game loaded in third
	// person has to be reported as a first pass and not as a switch - there
	// was nothing to switch from.
	using obvr::camera::PovEvent;
	obvr::camera::State state;

	Check(state.ObservePointOfView(true) == PovEvent::FirstPass,
	      "starting in third person is a first pass, not a switch");
	Check(state.isThirdPerson, "and the point of view is taken over");
	Check(state.ObservePointOfView(true) == PovEvent::Unchanged,
	      "the frame after it is quiet");
}

void TestIsDue() {
	std::printf("Periodic actions\n");

	using obvr::camera::IsDue;

	// 0 means off. Reaching the modulo with it would divide by zero, which is
	// the whole reason this is a function rather than an expression.
	Check(!IsDue(0, 0), "an interval of 0 is never due");
	Check(!IsDue(1, 0), "not on any frame");
	Check(!IsDue(120, 0), "however far the game has run");

	Check(IsDue(1, 1), "an interval of 1 is due every frame");
	Check(IsDue(2, 1), "really every frame");

	Check(!IsDue(119, 120), "an interval of 120 is not due at 119");
	Check(IsDue(120, 120), "is due at 120");
	Check(!IsDue(121, 120), "is not due at 121");
	Check(IsDue(240, 120), "and is due again at 240");

	// The callback counts from 1, so this case never occurs there. It is
	// checked anyway because the function is written to be usable on its own,
	// and a caller counting from 0 would get an action on its very first frame.
	Check(IsDue(0, 120), "frame 0 counts as due, which is why counting starts at 1");
}

}  // namespace

int main() {
	std::printf("OBVR frame logic test\n\n");

	TestKeyEdge();
	std::printf("\n");
	TestKeyEdgeReset();
	std::printf("\n");
	TestPointOfView();
	std::printf("\n");
	TestFirstPassInThirdPerson();
	std::printf("\n");
	TestIsDue();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}

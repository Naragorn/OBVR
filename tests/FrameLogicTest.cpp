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

void TestEyeAlternation() {
	std::printf("Which eye a frame belongs to\n");

	// Two places have to agree: the camera hook moves the camera to an eye,
	// and the renderer gives the finished frame to that eye. If they ever
	// disagreed the wearer would see each eye showing the other's viewpoint,
	// which is worse than no depth at all - so both ask this one function
	// rather than each deciding what "every other frame" means.
	Check(obvr::camera::IsLeftEyeFrame(0), "frame 0 is the left eye");
	Check(!obvr::camera::IsLeftEyeFrame(1), "frame 1 is the right");
	Check(obvr::camera::IsLeftEyeFrame(2), "and it alternates");

	// It has to keep alternating, including where a counter would wrap. A
	// frame count is 32 bits and a long session reaches large numbers.
	bool alternates = true;
	for (UInt32 frame = 0; frame < 1000; ++frame) {
		if (obvr::camera::IsLeftEyeFrame(frame) == obvr::camera::IsLeftEyeFrame(frame + 1)) {
			alternates = false;
			break;
		}
	}
	Check(alternates, "no two consecutive frames go to the same eye");

	// Across the wrap, where an implementation using division or a signed
	// counter would stumble.
	Check(obvr::camera::IsLeftEyeFrame(0xFFFFFFFEu) !=
	          obvr::camera::IsLeftEyeFrame(0xFFFFFFFFu),
	      "it still alternates at the top of the counter");
	Check(obvr::camera::IsLeftEyeFrame(0xFFFFFFFFu) != obvr::camera::IsLeftEyeFrame(0u),
	      "and across the wrap to zero");
}


void TestBackBufferEye() {
	std::printf("Which eye the back buffer's picture belongs to\n");

	using obvr::camera::BackBufferEyeIsLeft;

	// From the camera hook, which runs before the frame is drawn. The back
	// buffer holds the previous frame, drawn from the previous camera
	// position, which under alternate eyes is the other eye.
	Check(!BackBufferEyeIsLeft(true, false),
	      "before drawing, a left-eye frame finds the right eye's picture waiting");
	Check(BackBufferEyeIsLeft(false, false), "and a right-eye frame finds the left eye's");

	// From a hook at the end of the frame, where the picture is the one just
	// drawn.
	Check(BackBufferEyeIsLeft(true, true), "after drawing, a left-eye frame holds the left");
	Check(!BackBufferEyeIsLeft(false, true), "and a right-eye frame holds the right");

	// The whole point of the function, said as one property: moving the
	// submit from one end of the frame to the other inverts the answer. That
	// is why this is a named function rather than an expression at the call
	// site - the expression was written once, and when the submit moved it
	// would have kept quietly meaning the opposite.
	//
	// The failure is not a lost depth cue but an inverted one: each eye shown
	// the other eye's viewpoint, which the eyes cannot fuse. It was reported
	// from a headset as a picture that would not hold still.
	for (int eye = 0; eye < 2; ++eye) {
		const bool isLeft = eye == 0;
		Check(BackBufferEyeIsLeft(isLeft, true) != BackBufferEyeIsLeft(isLeft, false),
		      "moving the submit across the frame inverts which eye the picture is for");
	}
}

void TestFrameClock() {
	std::printf("Frame clock\n");

	// A counter that ticks a million times a second, so one tick is one
	// microsecond and every number below reads directly as a duration.
	constexpr long long kPerSecond = 1000000;
	obvr::camera::FrameClock clock;

	// Nothing to measure against on the first call. Returning some invented
	// frame time would be worse than saying so: the caller treats 0 as "no
	// timing" and skips smoothing for that one frame.
	Check(clock.Tick(5000000, kPerSecond) == 0.0f, "the first reading reports no frame time");

	const float sixtyFps = clock.Tick(5000000 + 16667, kPerSecond);
	Check(sixtyFps > 0.0166f && sixtyFps < 0.0167f, "16667 microseconds is about a 60 fps frame");

	const float thirtyFps = clock.Tick(5000000 + 16667 + 33333, kPerSecond);
	Check(thirtyFps > 0.0333f && thirtyFps < 0.0334f, "and 33333 is about a 30 fps one");

	// A frequency of 0 would divide by zero. It should not happen, but the
	// value comes from an API call rather than from OBVR.
	Check(clock.Tick(6000000, 0) == 0.0f, "a frequency of 0 reports no frame time");
}

void TestFrameClockGaps() {
	std::printf("Frame clock, pauses and oddities\n");

	constexpr long long kPerSecond = 1000000;
	obvr::camera::FrameClock clock;
	clock.Tick(0, kPerSecond);

	// A loading screen, an alt-tab or a breakpoint. Fed unclamped into an
	// exponential approach, a gap of seconds covers the whole remaining
	// distance in one step - so the smoothing would vanish precisely on the
	// first frame back, which is the one frame where a jump is most visible.
	const float afterPause = clock.Tick(30 * kPerSecond, kPerSecond);
	Check(afterPause == obvr::camera::FrameClock::kMaxDeltaSeconds,
	      "a pause of 30 seconds is reported as the maximum, not as 30 seconds");

	// Recovery has to be immediate: the frame after the pause is measured
	// against the pause, not against the frame before it.
	const float next = clock.Tick(30 * kPerSecond + 16667, kPerSecond);
	Check(next > 0.0166f && next < 0.0167f, "the frame after a pause is measured normally");

	// The counter is monotonic, so this means a stale reading rather than
	// time running backwards. A negative frame time would run the smoothing
	// the wrong way.
	Check(clock.Tick(0, kPerSecond) == 0.0f, "a reading that went backwards reports nothing");

	// Two frames so close together that the counter has not moved.
	clock.Tick(40 * kPerSecond, kPerSecond);
	Check(clock.Tick(40 * kPerSecond, kPerSecond) == 0.0f,
	      "two readings at the same tick report nothing");

	// After a reset the next call is a first call again.
	obvr::camera::FrameClock reset;
	reset.Tick(1000, kPerSecond);
	reset.Reset();
	Check(reset.Tick(2000, kPerSecond) == 0.0f, "after a reset the next reading is a first one");
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
	TestEyeAlternation();
	std::printf("\n");
	TestBackBufferEye();
	std::printf("\n");
	TestFrameClock();
	std::printf("\n");
	TestFrameClockGaps();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}

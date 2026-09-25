// Checks snap turning: one step per push of the right stick, written as a turn
// of the player's heading in Oblivion's convention (positive clockwise, to the
// right), instant or eased, and nothing at all while it is switched off.

#include <cstdio>

#include "camera/SnapTurn.h"

using obvr::camera::LookSettings;
using obvr::camera::SnapTurnState;
using obvr::camera::SnapTurnStep;
using obvr::camera::StepSnapTurn;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.0001f) { return a - b < eps && b - a < eps; }

constexpr float kFrame = 1.0f / 90.0f;
constexpr float kDegree = 0.01745329252f;

LookSettings Instant() {
	LookSettings s;
	s.snapTurning = true;
	s.snapTurnInstant = true;
	s.snapTurnAngle = 45.0f;
	s.snapTurnDeadZone = 0.3f;
	return s;
}

void TestOff() {
	std::printf("Switched off\n");
	LookSettings s = Instant();
	s.snapTurning = false;
	SnapTurnState state;
	state.armed = false;
	state.remaining = 1.0f;
	const SnapTurnStep step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(step.yawRadians == 0.0f && !step.fired, "no turn");
	Check(!step.ownsStick, "the stick keeps its continuous turn");
	Check(state.armed && state.remaining == 0.0f, "and nothing is left over for later");
}

void TestInstant() {
	std::printf("Instant\n");
	const LookSettings s = Instant();
	SnapTurnState state;

	SnapTurnStep step = StepSnapTurn(state, 0.0f, s, kFrame, true);
	Check(step.ownsStick && step.yawRadians == 0.0f && !step.fired,
	      "a centred stick: no turn, but the stick is the snap's");

	step = StepSnapTurn(state, 0.29f, s, kFrame, true);
	Check(step.yawRadians == 0.0f, "short of the dead zone: nothing");

	step = StepSnapTurn(state, 0.8f, s, kFrame, true);
	Check(step.fired && Near(step.yawRadians, 45.0f * kDegree), "pushed right: 45 degrees clockwise");

	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(!step.fired && step.yawRadians == 0.0f, "held: no second snap");

	step = StepSnapTurn(state, 0.2f, s, kFrame, true);
	step = StepSnapTurn(state, 0.8f, s, kFrame, true);
	Check(!step.fired, "back only to 0.2, past half the dead zone: still not re-armed");

	step = StepSnapTurn(state, 0.1f, s, kFrame, true);
	step = StepSnapTurn(state, -0.8f, s, kFrame, true);
	Check(step.fired && Near(step.yawRadians, -45.0f * kDegree),
	      "back to the centre, then left: 45 degrees anticlockwise");

	step = StepSnapTurn(state, 0.0f, s, kFrame, true);
	step = StepSnapTurn(state, 0.3f, s, kFrame, true);
	Check(step.fired, "exactly at the dead zone counts");
}

void TestEased() {
	std::printf("Eased\n");
	LookSettings s = Instant();
	s.snapTurnInstant = false;
	s.snapTurnSpeed = 6.0f;
	SnapTurnState state;

	SnapTurnStep step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(step.fired && Near(step.yawRadians, 6.0f * kFrame), "the first frame turns speed * dt");
	float total = step.yawRadians;
	int frames = 1;
	int refired = 0;
	while (state.remaining != 0.0f && frames < 1000) {
		step = StepSnapTurn(state, 1.0f, s, kFrame, true);
		refired += step.fired ? 1 : 0;
		total += step.yawRadians;
		++frames;
	}
	Check(refired == 0, "the held stick does not start another while it eases");
	Check(Near(total, 45.0f * kDegree), "all of it arrives, and no more");
	Check(frames == 12, "in twelve frames at 90 Hz (0.785 rad at 6 rad/s is 11.8 frames)");

	// A long frame is a hitch, not time to ease through.
	state = SnapTurnState{};
	StepSnapTurn(state, 0.0f, s, kFrame, true);
	step = StepSnapTurn(state, 1.0f, s, 0.5f, true);
	Check(step.fired && step.yawRadians == 0.0f && Near(state.remaining, 45.0f * kDegree),
	      "a half-second frame fires but turns nothing yet");
	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(Near(step.yawRadians, 6.0f * kFrame), "and resumes on the next ordinary frame");

	// A second push while one is still easing adds to it.
	state = SnapTurnState{};
	StepSnapTurn(state, 1.0f, s, kFrame, true);
	StepSnapTurn(state, 0.0f, s, kFrame, true);
	const float before = state.remaining;
	StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(state.remaining > before + 40.0f * kDegree, "a second push adds its whole angle");

	// And the other way cancels it out.
	state = SnapTurnState{};
	StepSnapTurn(state, 1.0f, s, 0.0f, true);
	StepSnapTurn(state, 0.0f, s, 0.0f, true);
	StepSnapTurn(state, -1.0f, s, 0.0f, true);
	Check(Near(state.remaining, 0.0f), "right then left before any time passed: nothing left");
}

void TestNotAllowed() {
	std::printf("Not allowed (a menu, no player)\n");
	LookSettings s = Instant();
	s.snapTurnInstant = false;
	SnapTurnState state;
	StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(state.remaining > 0.0f, "an eased snap under way");

	SnapTurnStep step = StepSnapTurn(state, 1.0f, s, kFrame, false);
	Check(step.yawRadians == 0.0f && !step.fired && state.remaining == 0.0f,
	      "a menu opens: the rest is dropped, nothing turns");
	Check(step.ownsStick, "and the stick still does not turn continuously");

	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(!step.fired, "the menu closes with the stick still pushed: no snap");
	step = StepSnapTurn(state, 0.0f, s, kFrame, true);
	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(step.fired, "only after it came back");

	state = SnapTurnState{};
	step = StepSnapTurn(state, 0.0f, s, kFrame, false);
	Check(state.armed, "centred while not allowed: armed");
	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(step.fired, "so the first push afterwards snaps");
}

void TestBadValues() {
	std::printf("Values out of range\n");
	LookSettings s = Instant();
	SnapTurnState state;

	s.snapTurnAngle = 500.0f;
	SnapTurnStep step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(Near(step.yawRadians, 180.0f * kDegree), "an angle past half a turn is held at 180");

	state = SnapTurnState{};
	volatile float zero = 0.0f;
	const float nan = zero / zero;
	s.snapTurnAngle = nan;
	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(Near(step.yawRadians, 45.0f * kDegree), "a NaN angle falls back to 45");

	state = SnapTurnState{};
	s = Instant();
	s.snapTurnDeadZone = 0.0f;
	step = StepSnapTurn(state, 0.04f, s, kFrame, true);
	Check(!step.fired, "a zero dead zone is held at 0.05, so rest noise cannot snap");

	state = SnapTurnState{};
	s = Instant();
	step = StepSnapTurn(state, nan, s, kFrame, true);
	Check(!step.fired && step.yawRadians == 0.0f, "a NaN stick is a centred stick");

	state = SnapTurnState{};
	s = Instant();
	s.snapTurnInstant = false;
	s.snapTurnSpeed = -3.0f;
	step = StepSnapTurn(state, 1.0f, s, kFrame, true);
	Check(step.yawRadians > 0.0f, "a negative speed is held at the floor and still turns forward");
}

}  // namespace

int main() {
	TestOff();
	TestInstant();
	TestEased();
	TestNotAllowed();
	TestBadValues();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

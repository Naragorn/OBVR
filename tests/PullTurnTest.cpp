// Checks the turn of a weapon pulled to the hand (game/PullTurnLogic.h):
// from as it lay towards as the hand holds it, by the float's progress,
// there a little before the catch, never faster than the cap.

#include <cstdio>

#include "core/Rotation.h"
#include "game/PullTurnLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kDegree = 0.017453292f;

bool NearAngle(float radians, float degrees, float toleranceDegrees = 0.5f) {
	const float d = radians / kDegree - degrees;
	return d < toleranceDegrees && -d < toleranceDegrees;
}

bool Same(const NiMatrix33& a, const NiMatrix33& b, float toleranceDegrees = 0.5f) {
	return AngleBetweenRotations(a, b) < toleranceDegrees * kDegree;
}

NiMatrix33 AboutZ(float degrees) { return AxisAngleRotation(0.0f, 0.0f, 1.0f, degrees * kDegree); }
NiMatrix33 AboutX(float degrees) { return AxisAngleRotation(1.0f, 0.0f, 0.0f, degrees * kDegree); }

void TestShare() {
	std::printf("The share of the turn by the float's progress\n");
	Check(PullTurnShare(0.0f) == 0.0f, "at the start: none");
	Check(PullTurnShare(-1.0f) == 0.0f, "before the start: none");
	volatile float zero = 0.0f;
	Check(PullTurnShare(zero / zero) == 0.0f, "a progress that is not a number: none");
	Check(PullTurnShare(kPullTurnDoneAt * 0.5f) > 0.49f && PullTurnShare(kPullTurnDoneAt * 0.5f) < 0.51f,
	      "half way to the done mark: half");
	Check(PullTurnShare(kPullTurnDoneAt) == 1.0f, "at the done mark (before the catch): all");
	Check(PullTurnShare(1.0f) == 1.0f, "at the catch: all");
}

void TestAngles() {
	std::printf("Angles and the turn between two rotations\n");
	Check(NearAngle(RotationAngleOf(NiMatrix33::Identity()), 0.0f), "no turn: 0");
	Check(NearAngle(RotationAngleOf(AboutZ(90.0f)), 90.0f), "a quarter turn: 90");
	Check(NearAngle(RotationAngleOf(AboutX(180.0f)), 180.0f), "a half turn: 180");
	Check(NearAngle(AngleBetweenRotations(AboutZ(30.0f), AboutZ(100.0f)), 70.0f), "between 30 and 100: 70");
	Check(Same(TurnPart(AboutZ(0.0f), AboutZ(90.0f), 0.5f), AboutZ(45.0f)), "half of a quarter turn: 45");
	Check(Same(TurnPart(AboutZ(10.0f), AboutZ(90.0f), 0.0f), AboutZ(10.0f)), "no share: where it was");
	Check(Same(TurnPart(AboutZ(10.0f), AboutZ(90.0f), 1.0f), AboutZ(90.0f)), "all of it: the target");
	Check(Same(TurnPart(AboutZ(40.0f), AboutZ(40.0f), 0.5f), AboutZ(40.0f)), "already there: stays");
	const NiMatrix33 half = TurnPart(NiMatrix33::Identity(), AboutX(180.0f), 0.5f);
	Check(NearAngle(RotationAngleOf(half), 90.0f) && (Same(half, AboutX(90.0f)) || Same(half, AboutX(-90.0f))),
	      "half of a half turn: a quarter, about the same axis");
	const NiMatrix33 halfY = TurnPart(NiMatrix33::Identity(), AxisAngleRotation(0.0f, 1.0f, 0.0f, 3.14159265f), 0.5f);
	Check(NearAngle(RotationAngleOf(halfY), 90.0f), "a half turn about y: half of it a quarter");
	const NiMatrix33 halfZ = TurnPart(NiMatrix33::Identity(), AboutZ(180.0f), 0.5f);
	Check(NearAngle(RotationAngleOf(halfZ), 90.0f), "a half turn about z: half of it a quarter");
}

// A float of `seconds` at 90 Hz towards a grip turned `gripDegrees` about z
// against the hand, the hand turning `handTurnPerSecond` more meanwhile; the
// angle to the grip's turn at each frame.
struct Flight {
	float firstAngle = 0.0f;
	float atCatch = 0.0f;
	bool shrinking = true;
	float largestStep = 0.0f;
};

Flight Fly(float seconds, float gripDegrees, float handTurnPerSecond, const NiMatrix33& lay) {
	PullTurn t;
	BeginPullTurn(t, lay);
	Flight f;
	const float dt = 1.0f / 90.0f;
	float last = -1.0f;
	NiMatrix33 before = t.relative;
	for (float elapsed = 0.0f; elapsed <= seconds + 1e-4f; elapsed += dt) {
		const float floated = seconds > 0.0f ? elapsed / seconds : 1.0f;
		const NiMatrix33 grip = AboutZ(gripDegrees + handTurnPerSecond * elapsed);
		const NiMatrix33 now = StepPullTurn(t, grip, floated > 1.0f ? 1.0f : floated, dt);
		const float angle = AngleBetweenRotations(now, grip);
		const float step = AngleBetweenRotations(before, now);
		f.largestStep = step > f.largestStep ? step : f.largestStep;
		before = now;
		if (last < 0.0f) {
			f.firstAngle = angle;
		} else if (angle > last + 0.5f * kDegree) {
			f.shrinking = false;
		}
		last = angle;
	}
	f.atCatch = last;
	return f;
}

void TestFlight() {
	std::printf("A weapon floating in\n");
	const Flight calm = Fly(0.4f, 120.0f, 0.0f, NiMatrix33::Identity());
	Check(calm.firstAngle > 110.0f * kDegree, "it leaves as it lay: about 120 degrees from the grip at first");
	Check(calm.shrinking, "the angle to the grip only shrinks on the way");
	Check(calm.atCatch < 1.0f * kDegree, "at the catch it is held as the hand holds it (under a degree)");
	Check(calm.largestStep <= kPullTurnMostRadiansPerSecond / 90.0f + 1e-4f, "never faster than the cap");
	const Flight turning = Fly(0.4f, 60.0f, 90.0f, AboutX(30.0f));
	Check(turning.atCatch < 2.0f * kDegree, "the hand turning on the way: still about there at the catch");
	const Flight shortPull = Fly(0.02f, 170.0f, 0.0f, NiMatrix33::Identity());
	Check(shortPull.largestStep <= kPullTurnMostRadiansPerSecond / 90.0f + 1e-4f,
	      "a pull of two frames: no jump round, the cap holds");
	Check(shortPull.atCatch > 90.0f * kDegree, "a pull of two frames: the turn is not done at the catch yet");
	// The rest is done in the hand, while the engine lets go (up to a second).
	PullTurn t;
	BeginPullTurn(t, NiMatrix33::Identity());
	for (int i = 0; i < 30; ++i) {
		StepPullTurn(t, AboutZ(170.0f), 1.0f, 1.0f / 90.0f);
	}
	Check(Same(t.relative, AboutZ(170.0f)), "held on afterwards: it gets there within a third of a second");
	const Flight harness = Fly(0.1f, 103.0f, 0.0f, NiMatrix33::Identity());
	Check(harness.atCatch < 15.0f * kDegree, "the harness's pull (9 frames, 103 degrees off): under 15 degrees at the catch");
}

void TestEdges() {
	std::printf("No turn, no time, a cancelled pull\n");
	PullTurn idle;
	Check(Same(StepPullTurn(idle, AboutZ(90.0f), 0.5f, 0.01f), NiMatrix33::Identity()),
	      "not begun (not a weapon): the grip's turn is not used");
	PullTurn t;
	BeginPullTurn(t, AboutZ(10.0f));
	Check(Same(StepPullTurn(t, AboutZ(90.0f), 0.5f, 0.0f), AboutZ(10.0f)), "no time passed: it stays");
	volatile float zero = 0.0f;
	Check(Same(StepPullTurn(t, AboutZ(90.0f), 0.5f, zero / zero), AboutZ(10.0f)), "a time that is not a number: stays");
	Check(Same(StepPullTurn(t, AboutZ(90.0f), 0.0f, 0.01f), AboutZ(10.0f)), "at the start of the float: as it lay");
	t = PullTurn{};
	Check(!t.active, "a cancelled pull (the hold let go): the turn is forgotten");
	BeginPullTurn(t, AboutZ(50.0f));
	Check(t.active && Same(t.relative, AboutZ(50.0f)), "the next pull begins from its own lay");
}

void TestWatch() {
	std::printf("The watch on the way in\n");
	PullTurnWatch w;
	WatchPullTurn(w, 1.0f);
	WatchPullTurn(w, 0.8f);
	WatchPullTurn(w, 0.81f);
	WatchPullTurn(w, 0.2f);
	Check(PullTurnNeverTurnedAway(w), "shrinking, a hair back within the tolerance: never turned away");
	WatchPullTurn(w, 0.6f);
	Check(!PullTurnNeverTurnedAway(w), "grown by more than the tolerance: turned away");
	Check(PullTurnCaughtClose(10.0f * kDegree), "10 degrees at the catch: close");
	Check(!PullTurnCaughtClose(20.0f * kDegree), "20 degrees at the catch: not close");
	PullTurnWatch fresh;
	WatchPullTurn(fresh, 2.0f);
	Check(PullTurnNeverTurnedAway(fresh), "one frame alone: nothing to compare");
}

}  // namespace

int main() {
	TestWatch();
	TestShare();
	TestAngles();
	TestFlight();
	TestEdges();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

// Checks the analog walk of the left stick (vr/AnalogWalkLogic.h): the push
// read past the dead zone, the octant's keys, the gait by the push with its
// band, the pace wanted and the step turned and scaled.

#include <cstdio>
#include <limits>

#include "vr/AnalogWalkLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.01f) { return a - b < tolerance && b - a < tolerance; }

using namespace obvr;
using namespace obvr::vr;

void TestRead() {
	std::printf("The stick past its dead zone\n");
	AnalogStick s = ReadAnalogStick(0.0f, 0.1f, 0.2f);
	Check(Near(s.amount, 0.0f), "inside the dead zone: no push");
	s = ReadAnalogStick(0.0f, 1.0f, 0.2f);
	Check(Near(s.amount, 1.0f) && Near(s.x, 0.0f) && Near(s.y, 1.0f), "at the rim forward: the whole push, forward");
	s = ReadAnalogStick(0.0f, 0.6f, 0.2f);
	Check(Near(s.amount, 0.5f), "half way past the dead zone: half the push");
	s = ReadAnalogStick(0.2f, 0.2f, 0.25f);
	Check(s.amount > 0.0f && Near(s.x, 0.7071f) && Near(s.y, 0.7071f),
	      "a diagonal past the round dead zone though each axis is under it: a push, the diagonal");
	s = ReadAnalogStick(0.9f, 0.9f, 0.2f);
	Check(Near(s.amount, 1.0f), "past the rim (a square gate): kept at 1");
	s = ReadAnalogStick(0.0f, 0.5f, 0.0f);
	Check(Near(s.amount, 0.5f), "no dead zone: the length itself");
	s = ReadAnalogStick(0.0f, 0.5f, 2.0f);
	Check(Near(s.amount, 0.5f), "a dead zone out of range: none");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	s = ReadAnalogStick(nan, 0.5f, 0.2f);
	Check(Near(s.amount, 0.0f), "not a number: no push");
}

void TestKeys() {
	std::printf("The octant's keys\n");
	StickDirections d = AnalogStickKeys(AnalogStick{});
	Check(!d.forward && !d.back && !d.left && !d.right, "no push: no key");
	d = AnalogStickKeys(ReadAnalogStick(0.0f, 0.5f, 0.2f));
	Check(d.forward && !d.back && !d.left && !d.right, "forward: W alone");
	d = AnalogStickKeys(ReadAnalogStick(0.2f, 0.9f, 0.2f));
	Check(d.forward && !d.right, "forward a little right (12 degrees): W alone");
	d = AnalogStickKeys(ReadAnalogStick(0.5f, 0.5f, 0.2f));
	Check(d.forward && d.right && !d.back && !d.left, "the diagonal: W and D");
	d = AnalogStickKeys(ReadAnalogStick(-0.6f, -0.1f, 0.2f));
	Check(d.left && !d.forward && !d.back && !d.right, "left: A alone");
	d = AnalogStickKeys(ReadAnalogStick(0.0f, -0.7f, 0.2f));
	Check(d.back && !d.forward, "back: S");
}

void TestGait() {
	std::printf("The gait by the push\n");
	bool running = false;
	Check(!StepAnalogGait(running, 0.3f, 0.65f), "a light push walks");
	Check(!StepAnalogGait(running, 0.68f, 0.65f), "just past runFrom, inside the band: still walks");
	Check(StepAnalogGait(running, 0.71f, 0.65f), "past the band: runs");
	Check(StepAnalogGait(running, 0.62f, 0.65f), "back just under runFrom, inside the band: still runs");
	Check(!StepAnalogGait(running, 0.59f, 0.65f), "under the band: walks again");
	running = false;
	Check(!StepAnalogGait(running, 1.0f, 1.0f), "runFrom 1: the push never runs");
	running = false;
	Check(StepAnalogGait(running, 0.8f, 0.0f), "runFrom out of range: the default 0.65");
}

void TestSpeed() {
	std::printf("The pace wanted (walk 100, run 300, runFrom 0.65)\n");
	Check(Near(AnalogTargetSpeed(0.0f, false, false, 0.65f, 100.0f, 300.0f), 0.0f), "no push: standing");
	Check(Near(AnalogTargetSpeed(0.325f, false, false, 0.65f, 100.0f, 300.0f), 50.0f),
	      "half the walk's band: half the walk");
	Check(Near(AnalogTargetSpeed(0.65f, false, false, 0.65f, 100.0f, 300.0f), 100.0f), "at runFrom walking: the walk");
	Check(Near(AnalogTargetSpeed(0.65f, true, false, 0.65f, 100.0f, 300.0f), 100.0f),
	      "at runFrom running: the walk too - no jump when the legs switch");
	Check(Near(AnalogTargetSpeed(0.825f, true, false, 0.65f, 100.0f, 300.0f), 200.0f), "half the run's band: between");
	Check(Near(AnalogTargetSpeed(1.0f, true, false, 0.65f, 100.0f, 300.0f), 300.0f), "the rim: the run");
	Check(Near(AnalogTargetSpeed(0.8f, false, false, 0.65f, 100.0f, 300.0f), 100.0f),
	      "walking past runFrom (inside the band): kept at the walk");
	Check(Near(AnalogTargetSpeed(0.6f, true, false, 0.65f, 100.0f, 300.0f), 100.0f),
	      "running under runFrom (inside the band): kept at the walk");
	Check(Near(AnalogTargetSpeed(0.2f, true, true, 0.65f, 100.0f, 300.0f), 300.0f),
	      "a forced run (stick in, toggle, arms): the full run, as before");
	Check(Near(AnalogTargetSpeed(0.5f, false, false, 0.65f, 0.0f, 300.0f), 0.0f), "the walk's speed unread: nothing");
	Check(Near(AnalogTargetSpeed(1.0f, true, false, 0.65f, 100.0f, 50.0f), 100.0f),
	      "a run slower than the walk read: the walk");
}

void TestPace() {
	std::printf("The engine's pace factor (walk 100, run 300, runFrom 0.65)\n");
	Check(Near(AnalogPaceFactor(0.325f, false, false, 0.65f, 100.0f, 300.0f), 0.5f), "half the walk's band: the walk at half");
	Check(Near(AnalogPaceFactor(0.65f, false, false, 0.65f, 100.0f, 300.0f), 1.0f), "at runFrom walking: the whole walk");
	Check(Near(AnalogPaceFactor(0.65f, true, false, 0.65f, 100.0f, 300.0f), 0.3333f),
	      "at runFrom running: the run's legs at a third - the walk's pace, no jump");
	Check(Near(AnalogPaceFactor(1.0f, true, false, 0.65f, 100.0f, 300.0f), 1.0f), "the rim: the whole run");
	Check(Near(AnalogPaceFactor(0.2f, false, true, 0.65f, 100.0f, 300.0f), 1.0f), "a forced run: the whole run");
	Check(Near(AnalogPaceFactor(0.0f, false, false, 0.65f, 100.0f, 300.0f), 0.0f), "no push: 0");
	Check(Near(AnalogPaceFactor(0.5f, false, false, 0.65f, 0.0f, 300.0f), 1.0f), "the speeds unread: 1, vanilla's");
	Check(Near(AnalogPaceFactor(1.0f, true, false, 0.65f, 100.0f, 50.0f), 1.0f), "a run read slower than the walk: kept at 1");
}

void TestStep() {
	std::printf("The step turned to the stick\n");
	const AnalogStick right30 = ReadAnalogStick(0.5f, 0.866f, 0.0f);
	float step[3] = {0.0f, 10.0f, 1.0f};  // W: forward 10
	AnalogTurnStep(step, right30);
	Check(Near(step[0], 5.0f) && Near(step[1], 8.66f) && Near(step[2], 1.0f),
	      "forward 10, the stick 30 degrees right: 10 long, 30 degrees right, z kept");
	float diag[3] = {7.0f, 7.0f, 0.0f};
	AnalogTurnStep(diag, ReadAnalogStick(0.5f, 0.5f, 0.0f));
	Check(Near(diag[0], 7.0f) && Near(diag[1], 7.0f), "the octant's own direction: the engine's step");
	float none[3] = {0.0f, 0.0f, 0.0f};
	AnalogTurnStep(none, right30);
	Check(Near(none[0], 0.0f) && Near(none[1], 0.0f), "no engine step: nothing made up");
	float still[3] = {0.0f, 10.0f, 0.0f};
	AnalogTurnStep(still, AnalogStick{});
	Check(Near(still[0], 0.0f) && Near(still[1], 10.0f), "no push: the engine's step");
}

void TestRunKey() {
	std::printf("The run key and Always Run\n");
	Check(AnalogRunKey(true, false), "a run, Always Run off: the key down");
	Check(!AnalogRunKey(false, false), "a walk, Always Run off: the key up");
	Check(!AnalogRunKey(true, true), "a run, Always Run on: the key up");
	Check(AnalogRunKey(false, true), "a walk, Always Run on: the key down");
}

void TestGate() {
	std::printf("When it walks analog\n");
	AnalogWalkSettings s;
	AnalogWalkGate g;
	g.fullVr = true;
	g.inWorld = true;
	Check(AnalogWalkAllowed(s, g), "Full VR in the world: yes");
	AnalogWalkGate r = g;
	r.riding = true;
	Check(!AnalogWalkAllowed(s, r), "on a horse: no");
	AnalogWalkGate f = g;
	f.flyCam = true;
	Check(!AnalogWalkAllowed(s, f), "the fly camera: no");
	AnalogWalkGate w = g;
	w.inWorld = false;
	Check(!AnalogWalkAllowed(s, w), "not in the world: no");
	AnalogWalkGate v = g;
	v.fullVr = false;
	Check(!AnalogWalkAllowed(s, v), "not Full VR: no");
	AnalogWalkSettings off;
	off.enabled = false;
	Check(!AnalogWalkAllowed(off, g), "switched off: no");
}

}  // namespace

int main() {
	TestRead();
	TestKeys();
	TestGait();
	TestSpeed();
	TestPace();
	TestStep();
	TestRunKey();
	TestGate();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

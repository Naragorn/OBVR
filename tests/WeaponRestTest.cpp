// Checks the drawn weapon kept at its rest in the hand through an action and
// the animation's way back after it (vr/WeaponRestLogic.h, camera/hands/
// TwoHands.cpp HoldWeaponAtRest).

#include <cstdio>
#include <limits>

#include "vr/WeaponRestLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::vr;

const float kDt = 1.0f / 72.0f;

// The staff's rest in the hand and its block pose (2026-11-24, the Weapon
// node's local place): the block holds it 5.7 units further along.
WeaponPose At(float z) {
	WeaponPose p;
	p.pos = NiPoint3{13.9f, 3.1f, z};
	return p;
}
const WeaponPose kRest = At(36.7f);
const WeaponPose kBlock = At(42.4f);

// A rotation turned `degrees` about z.
NiMatrix33 TurnedZ(float degrees) {
	const float r = degrees * 3.14159265f / 180.0f;
	const float c = math::Cos(r);
	const float s = math::Sin(r);
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = c;
	m.data[0][1] = -s;
	m.data[1][0] = s;
	m.data[1][1] = c;
	return m;
}

void TestPlain() {
	std::printf("No action\n");
	RestHoldState st;
	Check(StepRestHold(st, false, true, kRest, kRest, kDt) == RestStep::Take, "nothing plays: the engine's pose is the rest");
	Check(StepRestHold(st, false, false, kRest, kRest, kDt) == RestStep::Take, "nothing kept yet: taken");
	std::printf("An action plays\n");
	Check(StepRestHold(st, true, true, kBlock, kRest, kDt) == RestStep::Hold, "the block: the rest written over it");
	RestHoldState fresh;
	Check(StepRestHold(fresh, true, false, kBlock, kRest, kDt) == RestStep::Leave,
	      "no rest known yet: the engine's pose stays, nothing taken");
	Check(StepRestHold(fresh, false, false, kRest, kRest, kDt) == RestStep::Take, "and after it: taken as before");
}

void TestWayBack() {
	std::printf("The way back after the action\n");
	RestHoldState st;
	StepRestHold(st, true, true, kBlock, kRest, kDt);
	// The staff's six frames after the block let go, then on to its idle.
	const float zs[] = {42.4f, 42.4f, 41.5f, 40.3f, 39.3f, 38.2f, 37.3f, 36.75f};
	bool held = true;
	for (int i = 0; i < 7; ++i) {
		held = held && StepRestHold(st, false, true, At(zs[i]), kRest, kDt) == RestStep::Hold;
	}
	Check(held, "the measured way back from the block pose: held at the rest, frame by frame");
	Check(StepRestHold(st, false, true, At(zs[7]), kRest, kDt) == RestStep::Take, "back within a tenth of a unit: taken");
	Check(StepRestHold(st, false, true, kRest, kRest, kDt) == RestStep::Take, "and from then on taken");
}

void TestStill() {
	std::printf("Standing still elsewhere\n");
	RestHoldState st;
	StepRestHold(st, true, true, kBlock, kRest, kDt);
	int frames = 0;
	while (StepRestHold(st, false, true, kBlock, kRest, kDt) == RestStep::Hold && frames < 200) {
		++frames;
	}
	Check(frames >= 13 && frames <= 15, "the engine's pose still away from the rest: held a fifth of a second, then taken");
	RestHoldState two;
	StepRestHold(two, true, true, kBlock, kRest, kDt);
	StepRestHold(two, false, true, kBlock, kRest, kDt);
	Check(StepRestHold(two, false, true, kBlock, kRest, kDt) == RestStep::Hold,
	      "the two still frames before the blend starts: held");
	RestHoldState during;
	for (int i = 0; i < 100; ++i) {
		StepRestHold(during, true, true, kBlock, kRest, kDt);
	}
	Check(StepRestHold(during, false, true, kBlock, kRest, kDt) == RestStep::Hold,
	      "standing still through the action does not count after it");
}

void TestTimeout() {
	std::printf("Never back\n");
	RestHoldState st;
	StepRestHold(st, true, true, kBlock, kRest, kDt);
	int frames = 0;
	float z = 50.0f;
	while (StepRestHold(st, false, true, At(z), kRest, kDt) == RestStep::Hold && frames < 200) {
		z += frames % 2 == 0 ? 0.5f : -0.5f;  // moving on, never still
		++frames;
	}
	Check(frames >= 53 && frames <= 55, "moving on away from the rest: held three quarters of a second, then taken");
	RestHoldState lost;
	StepRestHold(lost, true, true, kBlock, kRest, kDt);
	Check(StepRestHold(lost, false, false, kBlock, kRest, kDt) == RestStep::Take, "the rest gone: taken");
}

void TestTurn() {
	std::printf("Turned\n");
	WeaponPose turned = kRest;
	turned.rot = TurnedZ(10.0f);
	WeaponPose nearly = kRest;
	nearly.rot = TurnedZ(0.3f);
	RestHoldState st;
	StepRestHold(st, true, true, turned, kRest, kDt);
	Check(StepRestHold(st, false, true, turned, kRest, kDt) == RestStep::Hold, "in place but turned 10 degrees: held");
	Check(StepRestHold(st, false, true, nearly, kRest, kDt) == RestStep::Take, "turned 0.3 degrees: back, taken");
	Check(TurnDegrees(NiMatrix33::Identity(), TurnedZ(30.0f)) > 29.9f &&
	          TurnDegrees(NiMatrix33::Identity(), TurnedZ(30.0f)) < 30.1f,
	      "the turn between two rotations: 30 degrees");
}

void TestNoTime() {
	std::printf("Frames without time\n");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	RestHoldState st;
	StepRestHold(st, true, true, kBlock, kRest, kDt);
	bool held = true;
	for (int i = 0; i < 100; ++i) {
		held = held && StepRestHold(st, false, true, kBlock, kRest, i % 2 == 0 ? 0.0f : nan) == RestStep::Hold;
	}
	Check(held, "a frame time of 0 or none: neither stillness nor the way back counted");
}

}  // namespace

int main() {
	TestPlain();
	TestWayBack();
	TestStill();
	TestTimeout();
	TestTurn();
	TestNoTime();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

// Checks the feet planted while the player stands and stepped after the body
// (game/BodyStepLogic.h): put down on the first frame, held in the world as
// the body moves under the trigger, a step past it (the further foot first,
// one foot at a time, the other one next), lifted at the middle and landing
// where the body has gone; a step for a turn alone; a jump taken at once;
// walking, kicking, an unread foot or a place of no number the animation's;
// the frame's time of no number or too long.

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/BodyStepLogic.h"

namespace {

using obvr::NiPoint3;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 0.01f) { return a == a && a >= b - within && a <= b + within; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float within = 0.01f) {
	return Near(a.x, b.x, within) && Near(a.y, b.y, within) && Near(a.z, b.z, within);
}

constexpr float kDt = 1.0f / 90.0f;
constexpr float kDeg = 0.01745329252f;

// Both feet wanted 8 units either side of `x`, ahead by `y`.
FeetPlantInput Standing(float x, float y, float yaw = 0.0f) {
	FeetPlantInput in;
	in.standing = true;
	in.valid[0] = in.valid[1] = true;
	in.wanted[0] = NiPoint3{x + 8.0f, y, 0.0f};
	in.wanted[1] = NiPoint3{x - 8.0f, y, 0.0f};
	in.bodyYaw = yaw;
	in.dt = kDt;
	return in;
}

void TestPlanted() {
	std::printf("planted\n");
	FeetPlantState s;
	FeetPlantVerdict v = StepFeetPlant(s, Standing(0, 0));
	Check(NearPoint(v.target[0], NiPoint3{8, 0, 0}) && NearPoint(v.target[1], NiPoint3{-8, 0, 0}) && !v.began[0] &&
	          !v.began[1],
	      "the first frame: both put down where wanted, no step");
	v = StepFeetPlant(s, Standing(5, 0));
	Check(NearPoint(v.target[0], NiPoint3{8, 0, 0}) && NearPoint(v.target[1], NiPoint3{-8, 0, 0}) && !v.stepping[0] &&
	          !v.stepping[1],
	      "the body 5 units on, under the trigger: the feet stay in the world");
	v = StepFeetPlant(s, Standing(0, 0, 20.0f * kDeg));
	Check(!v.began[0] && !v.began[1], "turned 20 degrees: under the turn's trigger, no step");
}

void TestStep() {
	std::printf("a step after the body\n");
	FeetPlantState s;
	StepFeetPlant(s, Standing(0, 0));
	// The body 9 units forward, the right foot a little further (its place 10 off).
	FeetPlantInput in = Standing(0, 9);
	in.wanted[0].y = 10.0f;
	FeetPlantVerdict v = StepFeetPlant(s, in);
	Check(v.began[0] && !v.began[1] && v.stepping[0] && !v.stepping[1], "past the trigger: the further foot steps");
	Check(NearPoint(v.target[1], NiPoint3{-8, 0, 0}), "the other one stays put while it steps");
	// Halfway: lifted, on its way.
	float most = 0.0f;
	for (int i = 0; i < 12; ++i) {
		v = StepFeetPlant(s, in);
		most = v.target[0].z > most ? v.target[0].z : most;
	}
	Check(!v.stepping[1], "the left foot waits while the right one is in the air");
	Check(most > 2.5f && most <= kFootStepLiftUnits + 1e-4f, "lifted at the middle of the step, no higher than the lift");
	Check(v.target[0].y > 0.0f && v.target[0].y < 10.0f, "on its way to the new place");
	for (int i = 0; i < 30 && s.foot[0].stepping; ++i) {
		v = StepFeetPlant(s, in);
	}
	Check(NearPoint(v.target[0], in.wanted[0]) && !v.stepping[0], "landed where wanted, on the ground");
	Check(s.lastStepped == 0, "the step noted as the right foot's");
	// The left foot next.
	bool leftBegan = false;
	for (int i = 0; i < 3 && !leftBegan; ++i) {
		v = StepFeetPlant(s, in);
		leftBegan = v.began[1];
	}
	Check(leftBegan, "then the other foot steps after it");
	for (int i = 0; i < 40; ++i) {
		v = StepFeetPlant(s, in);
	}
	Check(NearPoint(v.target[1], in.wanted[1]) && !v.stepping[0] && !v.stepping[1], "both caught up: standing again");
	// The place moving on while a foot steps: it lands where the body has gone.
	FeetPlantState m;
	StepFeetPlant(m, Standing(0, 0));
	for (int i = 0; i < 60; ++i) {
		v = StepFeetPlant(m, Standing(0, 8.0f + static_cast<float>(i) * 0.2f));
	}
	Check(v.target[0].y > 15.0f || v.target[1].y > 15.0f, "walking on through the room: the feet keep stepping after it");
}

void TestTurn() {
	std::printf("a step for a turn\n");
	FeetPlantState s;
	StepFeetPlant(s, Standing(0, 0, 0.0f));
	const FeetPlantVerdict v = StepFeetPlant(s, Standing(0, 0, 35.0f * kDeg));
	Check(v.began[0] != v.began[1], "turned 35 degrees in place: one foot steps");
	FeetPlantState t;
	StepFeetPlant(t, Standing(0, 0, 0.0f));
	t.lastStepped = 0;
	Check(StepFeetPlant(t, Standing(0, 0, -35.0f * kDeg)).began[1], "even: the foot that did not step last");
}

void TestSnap() {
	std::printf("a jump\n");
	FeetPlantState s;
	StepFeetPlant(s, Standing(0, 0));
	const FeetPlantVerdict v = StepFeetPlant(s, Standing(100, 0));
	Check(NearPoint(v.target[0], NiPoint3{108, 0, 0}) && NearPoint(v.target[1], NiPoint3{92, 0, 0}) && !v.began[0] &&
	          !v.began[1],
	      "100 units at once (a load, a teleport): taken there, no step");
}

void TestLiftOff() {
	std::printf("off the ground\n");
	FeetPlantState s;
	StepFeetPlant(s, Standing(0, 0));
	FeetPlantInput jump = Standing(3, 0);
	jump.wanted[0].z = jump.wanted[1].z = 15.0f;
	FeetPlantVerdict v = StepFeetPlant(s, jump);
	Check(NearPoint(v.target[0], jump.wanted[0]) && !v.began[0] && !v.began[1],
	      "the body 15 units up (a jump): the animation's feet, no step");
	FeetPlantInput fall = Standing(3, 0);
	fall.wanted[0].z = fall.wanted[1].z = -12.0f;
	StepFeetPlant(s, Standing(0, 0));
	v = StepFeetPlant(s, fall);
	Check(NearPoint(v.target[1], fall.wanted[1]), "12 units down (a fall): the same");
	FeetPlantInput stair = Standing(0, 0);
	stair.wanted[0].z = stair.wanted[1].z = 6.0f;
	StepFeetPlant(s, Standing(0, 0));
	v = StepFeetPlant(s, stair);
	Check(Near(v.target[0].z, 0.0f) && Near(v.target[1].z, 0.0f), "6 units up (a stair): still planted");
}

void TestNotStanding() {
	std::printf("the animation's feet\n");
	FeetPlantState s;
	StepFeetPlant(s, Standing(0, 0));
	FeetPlantInput walk = Standing(5, 0);
	walk.standing = false;
	FeetPlantVerdict v = StepFeetPlant(s, walk);
	Check(NearPoint(v.target[0], walk.wanted[0]) && !s.foot[0].valid && !s.foot[1].valid,
	      "walking: the animation's feet, the plant forgotten");
	StepFeetPlant(s, Standing(0, 0));
	FeetPlantInput kick = Standing(5, 0);
	kick.kicking[0] = true;
	v = StepFeetPlant(s, kick);
	Check(NearPoint(v.target[0], kick.wanted[0]) && !s.foot[0].valid && NearPoint(v.target[1], NiPoint3{-8, 0, 0}),
	      "a kicking leg its own; the other foot stays planted");
	FeetPlantInput unread = Standing(0, 0);
	unread.valid[1] = false;
	v = StepFeetPlant(s, unread);
	Check(!s.foot[1].valid, "an unread foot: forgotten");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	FeetPlantInput none = Standing(0, 0);
	none.wanted[0].x = nan;
	v = StepFeetPlant(s, none);
	Check(!s.foot[0].valid, "a place of no number: forgotten");
}

void TestTime() {
	std::printf("the frame's time\n");
	FeetPlantState s;
	StepFeetPlant(s, Standing(0, 0));
	FeetPlantInput in = Standing(0, 9);
	in.dt = std::numeric_limits<float>::quiet_NaN();
	FeetPlantVerdict v = StepFeetPlant(s, in);
	Check(v.began[0] || v.began[1], "no number for the time: a step still begins");
	const NiPoint3 before = v.target[0];
	v = StepFeetPlant(s, in);
	Check(NearPoint(v.target[0], before), "but does not move on without time");
	in.dt = 2.0f;
	v = StepFeetPlant(s, in);
	Check(NearPoint(v.target[0], before), "a frame of seconds: no move either");
}

// The weapon drawn with the body still: the stance 5 units further out,
// eased by the stance's speed - the feet glide out in place, no step.
void TestStanceGlide() {
	std::printf("a change of stance\n");
	FeetPlantState s;
	FeetPlantInput in = Standing(0, 0);
	in.stance[0] = NiPoint3{8, 0, 0};
	in.stance[1] = NiPoint3{-8, 0, 0};
	StepFeetPlant(s, in);
	FeetPlantVerdict v;
	bool anyStep = false;
	for (int i = 1; i <= 20; ++i) {
		const float out = 8.0f + 5.0f * static_cast<float>(i) / 20.0f;
		in.stance[0].x = out;
		in.stance[1].x = -out;
		in.wanted[0].x = out;
		in.wanted[1].x = -out;
		v = StepFeetPlant(s, in);
		anyStep = anyStep || v.began[0] || v.began[1];
	}
	Check(!anyStep, "the stance 5 units further out: no step");
	Check(NearPoint(v.target[0], NiPoint3{13, 0, 0}) && NearPoint(v.target[1], NiPoint3{-13, 0, 0}),
	      "the feet glided out with the stance");
	// The stance's change of height is the animation's lift, not carried: the plant keeps its height.
	in.stance[0].z = 2.0f;
	v = StepFeetPlant(s, in);
	Check(Near(v.target[0].z, 0.0f), "a stance higher at the foot: the plant stays on the ground");
	// Turned a quarter: the body's frame turned with it.
	FeetPlantState t;
	FeetPlantInput q = Standing(0, 0, 1.5707963f);
	StepFeetPlant(t, q);
	q.stance[0] = NiPoint3{1, 0, 0};
	v = StepFeetPlant(t, q);
	const NiPoint3 moved = v.target[0] - NiPoint3{8, 0, 0};
	const NiPoint3 expect = FromBodyFrame(NiPoint3{1, 0, 0}, 1.5707963f);
	Check(NearPoint(moved, NiPoint3{expect.x, expect.y, 0.0f}), "a quarter turned: the glide in the body's frame");
	// A shift of no number: the plant where it was.
	FootPlant f;
	f.valid = true;
	f.at = NiPoint3{1, 1, 0};
	GlideWithStance(f, NiPoint3{std::numeric_limits<float>::quiet_NaN(), 0, 0}, 0.0f);
	Check(NearPoint(f.at, NiPoint3{1, 1, 0}), "a stance of no number: no glide");
}

void TestFrom() {
	std::printf("the frame's input\n");
	const bool valid[2] = {true, false};
	const NiPoint3 animated[2] = {NiPoint3{1, 2, 3}, NiPoint3{-1, 2, 3}};
	FeetPlantInput in = FeetPlantFrom(true, 0.5f, kDt, valid, animated, NiPoint3{0, 2, 0}, NiPoint3{10, 0, 0}, true, false);
	Check(in.standing && Near(in.bodyYaw, 0.5f) && Near(in.dt, kDt), "standing, the yaw and the time as given");
	Check(NearPoint(in.wanted[0], NiPoint3{11, 2, 3}) && NearPoint(in.wanted[1], NiPoint3{9, 2, 3}),
	      "each foot carried with the body");
	Check(in.valid[0] && !in.valid[1], "the feet read as given");
	const NiPoint3 back = FromBodyFrame(in.stance[0], 0.5f);
	Check(NearPoint(back, NiPoint3{1, 0, 3}), "each foot's stance from the root, in the body's frame");
	Check(!in.kicking[0] && in.kicking[1], "a kick of the left leg: the left foot left to it");
	in = FeetPlantFrom(false, 0.0f, kDt, valid, animated, NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}, true, true);
	Check(!in.standing && in.kicking[0] && !in.kicking[1], "walking; the right leg's kick");
	in = FeetPlantFrom(true, 0.0f, kDt, valid, animated, NiPoint3{0, 0, 0}, NiPoint3{0, 0, 0}, false, true);
	Check(!in.kicking[0] && !in.kicking[1], "no kick: neither leg");
}

}  // namespace

int main() {
	TestFrom();
	TestStanceGlide();
	TestPlanted();
	TestStep();
	TestTurn();
	TestSnap();
	TestLiftOff();
	TestNotStanding();
	TestTime();
	if (g_failures == 0) {
		std::printf("all checks passed\n");
		return 0;
	}
	std::printf("%d check(s) failed\n", g_failures);
	return 1;
}

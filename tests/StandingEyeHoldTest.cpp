// Checks the view held still against the first-person animation while the
// player stands (camera/StandingEyeHold.h): the first frame the engine's,
// a frame's step of the draw spread over a second, a step there and back all
// but gone, the change from sheathed to drawn arriving as a glide, walking
// followed fast, the player carried and turned with the offset, a jump taken
// at once, not live the engine's own and forgotten, numbers of no number.

#include <cmath>
#include <cstdio>
#include <limits>

#include "camera/StandingEyeHold.h"

namespace {

using obvr::NiPoint3;
using namespace obvr::camera;

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

StandingEyeHoldInput Standing(const NiPoint3& eye, const NiPoint3& feet = NiPoint3{0, 0, 0}, float yaw = 0.0f) {
	StandingEyeHoldInput in;
	in.live = true;
	in.standing = true;
	in.eye = eye;
	in.feet = feet;
	in.yaw = yaw;
	in.dt = kDt;
	return in;
}

void TestStill() {
	std::printf("standing\n");
	StandingEyeHoldState s;
	const NiPoint3 eye{0.0f, 5.0f, 118.0f};
	Check(NearPoint(StepStandingEyeHold(s, Standing(eye)), eye) && s.valid, "the first frame: the engine's eye");
	// The draw's step: half a unit up for one frame, then back.
	NiPoint3 out = StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.5f}));
	Check(out.z - 118.0f < 0.01f && out.z > 118.0f, "a frame's step of half a unit: under a hundredth of it shown");
	out = StepStandingEyeHold(s, Standing(eye));
	Check(Near(out.z, 118.0f, 0.01f), "there and back: all but gone");
	// Drawn: the eye 0.9 units lower for good - a glide over about a second.
	float most = 0.0f;
	float last = 118.0f;
	for (int i = 0; i < 90; ++i) {
		out = StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 117.1f}));
		most = last - out.z > most ? last - out.z : most;
		last = out.z;
	}
	Check(most < 0.02f, "sheathed to drawn, 0.9 units: no frame moves the view 0.02 units");
	Check(out.z < 117.7f && out.z > 117.1f, "a second later: most of the way there");
	for (int i = 0; i < 450; ++i) {
		out = StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 117.1f}));
	}
	Check(Near(out.z, 117.1f, 0.01f), "five seconds later: where the engine has it");
}

void TestWalk() {
	std::printf("walking\n");
	StandingEyeHoldState s;
	StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	StandingEyeHoldInput walk = Standing(NiPoint3{0.0f, 5.0f, 119.0f});
	walk.standing = false;
	NiPoint3 out{0, 0, 0};
	for (int i = 0; i < 27; ++i) {
		out = StepStandingEyeHold(s, walk);
	}
	Check(out.z > 118.9f, "walking: followed within a third of a second");
}

void TestCarried() {
	std::printf("the player carried and turned\n");
	StandingEyeHoldState s;
	StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	// The player moved 10 units, the eye with it: the offset is the same.
	NiPoint3 out = StepStandingEyeHold(s, Standing(NiPoint3{10.0f, 5.0f, 118.0f}, NiPoint3{10.0f, 0.0f, 0.0f}));
	Check(NearPoint(out, NiPoint3{10.0f, 5.0f, 118.0f}), "the feet moved, the eye with them: no lag");
	// Turned a quarter: the eye 5 units ahead the new way, at once.
	const float quarter = 1.5707963f;
	const NiPoint3 ahead = EyeFromPlayerFrame(NiPoint3{0.0f, 5.0f, 118.0f}, quarter);
	out = StepStandingEyeHold(s, Standing(NiPoint3{10.0f, 0.0f, 0.0f} + ahead, NiPoint3{10.0f, 0.0f, 0.0f}, quarter));
	Check(NearPoint(out, NiPoint3{10.0f, 0.0f, 0.0f} + ahead), "turned a quarter: the offset turned with the player");
	const NiPoint3 back = EyeToPlayerFrame(ahead, quarter);
	Check(NearPoint(back, NiPoint3{0.0f, 5.0f, 118.0f}), "to the player's frame and back: the same offset");
}

void TestSnap() {
	std::printf("a jump\n");
	StandingEyeHoldState s;
	StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	const NiPoint3 out = StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 90.0f}));
	Check(NearPoint(out, NiPoint3{0.0f, 5.0f, 90.0f}) && Near(s.held.z, 90.0f), "28 units at once: taken at once");
}

void TestNotLive() {
	std::printf("not live\n");
	StandingEyeHoldState s;
	StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	StandingEyeHoldInput off = Standing(NiPoint3{0.0f, 5.0f, 117.0f});
	off.live = false;
	Check(NearPoint(StepStandingEyeHold(s, off), off.eye) && !s.valid, "not live: the engine's eye, the hold forgotten");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	StandingEyeHoldInput none = Standing(NiPoint3{nan, 5.0f, 117.0f});
	StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	StepStandingEyeHold(s, none);
	Check(!s.valid, "an eye of no number: forgotten");
	StandingEyeHoldInput noYaw = Standing(NiPoint3{0.0f, 5.0f, 117.0f});
	noYaw.yaw = nan;
	StepStandingEyeHold(s, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	Check(NearPoint(StepStandingEyeHold(s, noYaw), noYaw.eye) && !s.valid, "a yaw of no number: the engine's eye");
	StandingEyeHoldState t;
	StepStandingEyeHold(t, Standing(NiPoint3{0.0f, 5.0f, 118.0f}));
	StandingEyeHoldInput noTime = Standing(NiPoint3{0.0f, 5.0f, 117.0f});
	noTime.dt = nan;
	Check(Near(StepStandingEyeHold(t, noTime).z, 118.0f), "no time: the held eye, nothing eased");
	noTime.dt = 3.0f;
	Check(Near(StepStandingEyeHold(t, noTime).z, 118.0f), "a frame of seconds: nothing eased either");
}

// Sneaking: held across the ground, the height the engine's at once.
void TestSneak() {
	std::printf("sneaking\n");
	StandingEyeHoldState s;
	StandingEyeHoldInput in = Standing(NiPoint3{0.0f, 5.0f, 118.0f});
	in.heightFree = true;
	StepStandingEyeHold(s, in);
	in.eye = NiPoint3{0.8f, 4.2f, 95.0f};  // the sneak's drop and a frame of the equip's sideways move
	const NiPoint3 out = StepStandingEyeHold(s, in);
	Check(Near(out.z, 95.0f), "the height: the engine's at once (the sneak's drop is not eased)");
	Check(out.x < 0.02f && out.y > 4.98f, "across the ground: held, under a hundredth of the move shown");
	in.heightFree = false;
	const NiPoint3 up = StepStandingEyeHold(s, in);
	Check(Near(up.z, 95.0f, 0.05f), "the sneak over: the height held from where it was, no jump");
}

}  // namespace

int main() {
	TestSneak();
	TestStill();
	TestWalk();
	TestCarried();
	TestSnap();
	TestNotLive();
	if (g_failures == 0) {
		std::printf("all checks passed\n");
		return 0;
	}
	std::printf("%d check(s) failed\n", g_failures);
	return 1;
}

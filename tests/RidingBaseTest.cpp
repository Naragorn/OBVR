// Checks the base of the eyes on a horse (camera/RidingBase.h): the offset in
// the horse's frame and back, the first frame taking the camera as it is, the
// settle, the share of the bob at 0 and 1, a horse that turns, a change of
// horse, a bad frame time, and the reset; the eyes stood upright over the
// saddle on every incline.

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "camera/RidingBase.h"

using namespace obvr;
using namespace obvr::camera;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
bool Near(const NiPoint3& a, const NiPoint3& b, float eps = 1e-3f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

constexpr float kHalfPi = 1.57079632679f;

void TestFrame() {
	std::printf("The offset in the horse's frame\n");
	float f = 0.0f, r = 0.0f, u = 0.0f;
	// Heading 0: forward is +y, right is +x.
	RidingOffsetInHorseFrame(NiPoint3{3.0f, 10.0f, 175.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f, f, r, u);
	Check(Near(f, 10.0f) && Near(r, 3.0f) && Near(u, 175.0f), "heading 0: forward +y, right +x, up +z");
	// Heading a quarter turn: forward is +x (sin 90 = 1), right is -y.
	RidingOffsetInHorseFrame(NiPoint3{10.0f, -3.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, kHalfPi, f, r, u);
	Check(Near(f, 10.0f) && Near(r, 3.0f) && Near(u, 0.0f), "heading 90 degrees: forward +x, right -y");
	// Back again, at a horse's place, at any heading.
	const NiPoint3 at{-54563.4f, 89147.9f, 13445.1f};
	const NiPoint3 camera{-54559.5f, 89149.4f, 13620.9f};
	RidingOffsetInHorseFrame(camera, at, 5.7807f, f, r, u);
	Check(Near(RidingOffsetToWorld(at, 5.7807f, f, r, u), camera, 1e-2f), "to the frame and back gives the camera");
}

// A rotation about the body's own x: nose-up by `radians` - forward (0,1,0)
// lifted to (0, cos, sin), up (0,0,1) leaned to (0, -sin, cos).
NiMatrix33 PitchedBy(float radians) {
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[1][1] = std::cos(radians);
	m.data[2][1] = std::sin(radians);
	m.data[1][2] = -std::sin(radians);
	m.data[2][2] = std::cos(radians);
	return m;
}

float Apart(const NiPoint3& a, const NiPoint3& b) {
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// The frame is the horse node's whole rotation, not its heading alone: on a
// slope the engine pitches the horse's scene node with the ground
// (horse-ride-slope.txt, 2026-10-23: up to 13.6 degrees nose-up on the
// priory's hill, the reference's rotX and rotY 0, the Bip01 bone level), and
// a yaw-only frame reads a saddle 170 units up as a camera moved 170 sin 13.6
// = 40 units back, which the settle lagged for seconds - the eyes ahead of
// the saddle (a Quest 3 tester, beta 6: "the camera gets 'thrown' out of
// the saddle").
void TestTilt() {
	std::printf("The frame with the horse's tilt\n");
	const NiPoint3 at{-54563.4f, 89147.9f, 13445.1f};
	const NiPoint3 camera{-54559.5f, 89149.4f, 13620.9f};
	float f = 0.0f, r = 0.0f, u = 0.0f;
	RidingOffsetInHorseFrame(camera, at, 5.7807f, f, r, u);
	float mf = 0.0f, mr = 0.0f, mu = 0.0f;
	RidingOffsetInFrame(camera, at, RidingFrameFromHeading(5.7807f), mf, mr, mu);
	Check(Near(f, mf, 1e-2f) && Near(r, mr, 1e-2f) && Near(u, mu, 1e-2f),
	      "a heading's frame as a matrix: the same offset as the heading's arithmetic");
	Check(Near(RidingOffsetToWorld(at, RidingFrameFromHeading(5.7807f), mf, mr, mu), camera, 1e-2f),
	      "and back to the camera");

	// Pitched 12 degrees nose-up at heading 0, the saddle 3 back and 170 up
	// on the body: the node's frame reads the body's own numbers, the
	// heading's frame 35 units further back and 4 lower.
	const float twelve = 12.0f * 3.14159265f / 180.0f;
	const NiMatrix33 tilted = RidingFrameFromHeading(0.0f) * PitchedBy(twelve);
	const NiPoint3 saddle = RidingOffsetToWorld(NiPoint3{0.0f, 0.0f, 0.0f}, tilted, -3.0f, 0.0f, 170.0f);
	Check(Near(saddle, NiPoint3{0.0f, -3.0f * std::cos(twelve) - 170.0f * std::sin(twelve),
	                            170.0f * std::cos(twelve) - 3.0f * std::sin(twelve)}, 1e-2f),
	      "the saddle on a pitched body, in the world");
	RidingOffsetInFrame(saddle, NiPoint3{0.0f, 0.0f, 0.0f}, tilted, mf, mr, mu);
	Check(Near(mf, -3.0f, 1e-2f) && Near(mr, 0.0f, 1e-2f) && Near(mu, 170.0f, 1e-2f),
	      "in the node's frame the saddle is where it is on the body");
	RidingOffsetInHorseFrame(saddle, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f, f, r, u);
	Check(Near(f, -38.28f, 0.05f) && Near(u, 165.66f, 0.05f), "in the heading's frame alone it reads 35 back and 4 down");

	// The climb: the body pitches from level to 13 degrees over 60 frames
	// while the camera sits in the saddle and the horse moves on. In the
	// node's frame the base stays in the saddle to the unit; in the heading's
	// frame alone the settle lags it 20 units out of the saddle - the bug.
	RidingBaseState node;
	RidingBaseState yawOnly;
	float worstNode = 0.0f;
	float worstYaw = 0.0f;
	for (int i = 0; i <= 60; ++i) {
		const float p = 13.0f * 3.14159265f / 180.0f * static_cast<float>(i) / 60.0f;
		const NiMatrix33 frame = RidingFrameFromHeading(0.3f) * PitchedBy(p);
		const NiPoint3 horseAt{100.0f + 5.0f * static_cast<float>(i), 200.0f, 50.0f + static_cast<float>(i)};
		const NiPoint3 cam = RidingOffsetToWorld(horseAt, frame, -3.0f, 0.0f, 170.0f);
		const NiPoint3 byNode = StepRidingBase(node, 7u, cam, horseAt, frame, 1.0f / 75.0f, 0.3f);
		const NiPoint3 byYaw = StepRidingBase(yawOnly, 7u, cam, horseAt, 0.3f, 1.0f / 75.0f, 0.3f);
		worstNode = std::fmax(worstNode, Apart(byNode, cam));
		worstYaw = std::fmax(worstYaw, Apart(byYaw, cam));
	}
	Check(worstNode < 0.05f, "pitched 13 degrees over 60 frames: the base stays in the saddle in the node's frame");
	Check(worstYaw > 15.0f, "in the heading's frame alone the same climb lags the base over 15 units out of it");
	Check(Near(node.forward, -3.0f, 0.05f) && Near(node.up, 170.0f, 0.05f), "the settled saddle unchanged by the tilt");

	// The stride's bob is still settled out in the node's frame: the camera
	// bobbing 4 units up and down over a second on a level body, the base
	// moves a fraction of it.
	RidingBaseState bob;
	const NiMatrix33 level = RidingFrameFromHeading(1.0f);
	float lowest = 1e9f;
	float highest = -1e9f;
	for (int i = 0; i < 300; ++i) {
		const float phase = 6.2831853f * static_cast<float>(i) / 75.0f;
		const NiPoint3 horseAt{0.0f, 0.0f, 0.0f};
		const NiPoint3 cam = RidingOffsetToWorld(horseAt, level, -3.0f, 0.0f, 170.0f + 2.0f * std::sin(phase));
		const NiPoint3 b = StepRidingBase(bob, 7u, cam, horseAt, level, 1.0f / 75.0f, 0.0f);
		if (i >= 150) {
			lowest = std::fmin(lowest, b.z);
			highest = std::fmax(highest, b.z);
		}
	}
	Check(highest - lowest < 0.5f, "a 4-unit stride bob on a level body: under half a unit of it left at bob 0");
}

void TestFirstFrame() {
	std::printf("The first frame on a horse\n");
	RidingBaseState state;
	const NiPoint3 at{100.0f, 200.0f, 50.0f};
	const NiPoint3 camera{103.0f, 198.0f, 225.0f};
	const NiPoint3 base = StepRidingBase(state, 0x4101A318u, camera, at, 0.5f, 1.0f / 75.0f, 0.3f);
	Check(Near(base, camera), "the engine's camera as it is: no jump at the mount");
	Check(state.valid && state.horse == 0x4101A318u, "the state settled on this horse");
	float f = 0.0f, r = 0.0f, u = 0.0f;
	RidingOffsetInHorseFrame(camera, at, 0.5f, f, r, u);
	Check(Near(state.forward, f) && Near(state.right, r) && Near(state.up, u), "the offset taken whole");
}

void TestSettleAndShare() {
	std::printf("The settle and the share\n");
	const NiPoint3 at{0.0f, 0.0f, 0.0f};
	const float dt = 1.0f / 75.0f;
	// Settled at 175 up; the camera bobs 4 units higher.
	RidingBaseState state;
	StepRidingBase(state, 1u, NiPoint3{0.0f, 0.0f, 175.0f}, at, 0.0f, dt, 0.0f);
	NiPoint3 base = StepRidingBase(state, 1u, NiPoint3{0.0f, 0.0f, 179.0f}, at, 0.0f, dt, 0.0f);
	const float expectedUp = 175.0f + 4.0f * (dt / kRidingSettleSeconds);
	Check(Near(state.up, expectedUp), "the offset follows the camera by dt over the settle time");
	Check(Near(base.z, expectedUp), "share 0: the base is the settled offset alone");
	// Share 1: the engine's camera untouched, whatever the state.
	base = StepRidingBase(state, 1u, NiPoint3{0.0f, 0.0f, 179.0f}, at, 0.0f, dt, 1.0f);
	Check(Near(base.z, 179.0f), "share 1: the engine's camera as it is");
	// Share a third: a third of the way from the settled offset to the camera.
	RidingBaseState third;
	StepRidingBase(third, 1u, NiPoint3{0.0f, 0.0f, 175.0f}, at, 0.0f, dt, 0.3f);
	base = StepRidingBase(third, 1u, NiPoint3{0.0f, 0.0f, 179.0f}, at, 0.0f, dt, 0.3f);
	Check(Near(base.z, third.up + (179.0f - third.up) * 0.3f), "share 0.3: the settled offset plus 0.3 of the bob");
	// Shares outside 0..1 are clamped.
	base = StepRidingBase(third, 1u, NiPoint3{0.0f, 0.0f, 179.0f}, at, 0.0f, dt, 7.0f);
	Check(Near(base.z, 179.0f), "a share over 1 counts as 1");
	base = StepRidingBase(third, 1u, NiPoint3{0.0f, 0.0f, 179.0f}, at, 0.0f, dt, -2.0f);
	Check(Near(base.z, third.up), "a share under 0 counts as 0");
	// A long settle: after many frames the offset is the camera's.
	RidingBaseState settled;
	StepRidingBase(settled, 1u, NiPoint3{0.0f, 0.0f, 175.0f}, at, 0.0f, dt, 0.0f);
	for (int i = 0; i < 75 * 20; ++i) {
		StepRidingBase(settled, 1u, NiPoint3{0.0f, 0.0f, 179.0f}, at, 0.0f, dt, 0.0f);
	}
	Check(Near(settled.up, 179.0f, 0.01f), "twenty seconds later the offset has settled on the camera's");
	// A one-second cycle is mostly gone from the base at share 0.
	RidingBaseState cycle;
	StepRidingBase(cycle, 1u, NiPoint3{0.0f, 0.0f, 175.0f}, at, 0.0f, dt, 0.0f);
	float lowest = 1e9f, highest = -1e9f;
	for (int i = 0; i < 75 * 10; ++i) {
		const float z = 175.0f + 2.0f * std::sin(static_cast<float>(i) * dt * 2.0f * 3.14159265f);
		const NiPoint3 b = StepRidingBase(cycle, 1u, NiPoint3{0.0f, 0.0f, z}, at, 0.0f, dt, 0.0f);
		if (i >= 75 * 5) {
			lowest = b.z < lowest ? b.z : lowest;
			highest = b.z > highest ? b.z : highest;
		}
	}
	Check(highest - lowest < 0.4f, "a bob of 4 units at 1 Hz leaves under 0.4 in the base at share 0");
}

void TestTurnAndChange() {
	std::printf("A horse that turns, another horse, a bad frame time, the reset\n");
	const float dt = 1.0f / 75.0f;
	RidingBaseState state;
	// Settled 10 ahead of the horse at heading 0 (+y).
	StepRidingBase(state, 1u, NiPoint3{0.0f, 10.0f, 175.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f, dt, 0.0f);
	// The horse turns a quarter right: the settled offset turns with it, 10
	// ahead is now +x, whatever the camera does this frame.
	NiPoint3 base =
		StepRidingBase(state, 1u, NiPoint3{10.0f, 0.0f, 175.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, kHalfPi, dt, 0.0f);
	Check(Near(base, NiPoint3{10.0f, 0.0f, 175.0f}, 1e-2f), "the settled offset turns with the horse's heading");
	// The horse moves: the base moves with its place.
	base = StepRidingBase(state, 1u, NiPoint3{510.0f, 300.0f, 175.0f}, NiPoint3{500.0f, 300.0f, 0.0f}, kHalfPi, dt,
	                      0.0f);
	Check(Near(base, NiPoint3{510.0f, 300.0f, 175.0f}, 1e-2f), "the base rides with the horse's place");
	// Another horse: the offset starts anew from its camera.
	base = StepRidingBase(state, 2u, NiPoint3{0.0f, -7.0f, 160.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f, dt, 0.0f);
	Check(Near(base, NiPoint3{0.0f, -7.0f, 160.0f}) && state.horse == 2u && Near(state.up, 160.0f),
	      "another horse: its camera taken whole");
	// A bad frame time: a whole step to the camera.
	base = StepRidingBase(state, 2u, NiPoint3{0.0f, -7.0f, 170.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f);
	Check(Near(state.up, 170.0f) && Near(base.z, 170.0f), "dt 0: the offset jumps to the camera's");
	base = StepRidingBase(state, 2u, NiPoint3{0.0f, -7.0f, 180.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.0f, 5.0f, 0.0f);
	Check(Near(state.up, 180.0f), "dt 5 s: the same");
	ResetRidingBase(state);
	Check(!state.valid && state.horse == 0u, "the reset forgets the horse");
}

void TestSnap() {
	std::printf("A camera that moved rather than bobbed\n");
	const float dt = 1.0f / 75.0f;
	const NiPoint3 at{0.0f, 0.0f, 0.0f};
	RidingBaseState state;
	// The mount: the engine's camera at 57.9 over the horse's place on the
	// switch to first person, 175 a frame later.
	StepRidingBase(state, 1u, NiPoint3{0.0f, -3.6f, 57.9f}, at, 0.0f, dt, 0.3f);
	NiPoint3 base = StepRidingBase(state, 1u, NiPoint3{0.0f, -3.6f, 175.0f}, at, 0.0f, dt, 0.3f);
	Check(Near(base.z, 175.0f) && Near(state.up, 175.0f), "117 units up in a frame: the offset jumps, the base is the camera");
	// The gallop's bob stays under the snap: 11 units up is followed slowly.
	base = StepRidingBase(state, 1u, NiPoint3{0.0f, -3.6f, 186.0f}, at, 0.0f, dt, 0.0f);
	Check(Near(state.up, 175.0f + 11.0f * (dt / kRidingSettleSeconds)) && base.z < 176.0f,
	      "11 units up: a bob, followed by the settle alone");
	// A sideways re-placement snaps too.
	base = StepRidingBase(state, 1u, NiPoint3{40.0f, -3.6f, 175.0f}, at, 0.0f, dt, 0.0f);
	Check(Near(state.right, 40.0f) && Near(base.x, 40.0f), "40 units aside in a frame: the offset jumps");
	// Just under the limit: not a snap.
	base = StepRidingBase(state, 1u, NiPoint3{40.0f, 25.0f, 175.0f}, at, 0.0f, dt, 0.0f);
	Check(Near(state.forward, -3.6f + (25.0f + 3.6f) * (dt / kRidingSettleSeconds)), "29 units ahead: followed, not jumped");
}

// ---------------------------------------------------------------- Upright
//
// The eyes stood upright over the saddle (a Quest 3 tester, beta 8,
// 2026-11-06: "the camera shifts sideways during ascents or descents (as the
// horse's body tilts)"; measured 2026-11-08: the eyes rode the node's pitch
// about its origin on the ground, 59 units back of the horse's place at 18
// degrees). The saddle rides the tilted body; the rider above it stays
// upright, as a rider does.

NiMatrix33 RolledBy(float radians) {
	// About the body's own y (its forward): right (1,0,0) to (cos, 0, -sin),
	// up (0,0,1) to (sin, 0, cos) - a roll to the right lowers the right side.
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = std::cos(radians);
	m.data[2][0] = -std::sin(radians);
	m.data[0][2] = std::sin(radians);
	m.data[2][2] = std::cos(radians);
	return m;
}

float Radians(float degrees) { return degrees * 3.14159265f / 180.0f; }

bool NearMatrix(const NiMatrix33& a, const NiMatrix33& b, float eps = 1e-4f) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.data[r][c], b.data[r][c], eps)) {
				return false;
			}
		}
	}
	return true;
}

bool Finite(const NiPoint3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

// The offset `d` (world) in the heading's frame: right, forward, up.
NiPoint3 InHeading(const NiPoint3& d, float heading) {
	float f = 0.0f, r = 0.0f, u = 0.0f;
	RidingOffsetInHorseFrame(d, NiPoint3{0.0f, 0.0f, 0.0f}, heading, f, r, u);
	return NiPoint3{r, f, u};
}

void TestUprightFrame() {
	std::printf("The upright frame of a tilted body\n");
	NiMatrix33 up{};
	Check(UprightFrameOf(RidingFrameFromHeading(0.3f), up) && NearMatrix(up, RidingFrameFromHeading(0.3f)),
	      "level: the frame itself");
	const float pitches[] = {-40.0f, -30.0f, -18.0f, -8.0f, 8.0f, 18.0f, 30.0f, 40.0f};
	bool allPitches = true;
	for (float p : pitches) {
		allPitches = allPitches && UprightFrameOf(RidingFrameFromHeading(5.78f) * PitchedBy(Radians(p)), up) &&
		             NearMatrix(up, RidingFrameFromHeading(5.78f));
	}
	Check(allPitches, "nose up and down 8 to 40 degrees: the heading's frame");
	bool allRolls = true;
	for (float r : {-30.0f, -15.0f, 15.0f, 30.0f}) {
		allRolls = allRolls && UprightFrameOf(RidingFrameFromHeading(2.0f) * RolledBy(Radians(r)), up) &&
		           NearMatrix(up, RidingFrameFromHeading(2.0f));
	}
	Check(allRolls, "rolled left and right 15 and 30 degrees: the heading's frame");
	Check(UprightFrameOf(RidingFrameFromHeading(1.1f) * PitchedBy(Radians(25.0f)) * RolledBy(Radians(-20.0f)), up) &&
	          NearMatrix(up, RidingFrameFromHeading(1.1f)),
	      "pitched and rolled together: the heading's frame");
	// The nose straight up: no heading in the forward, the right gives it.
	Check(UprightFrameOf(RidingFrameFromHeading(0.7f) * PitchedBy(Radians(90.0f)), up) &&
	          NearMatrix(up, RidingFrameFromHeading(0.7f)),
	      "the nose straight up: the heading from the right");
	// A scale in the node (a reference's scale): the upright frame keeps it.
	NiMatrix33 scaled = RidingFrameFromHeading(0.4f) * PitchedBy(Radians(12.0f));
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			scaled.data[r][c] *= 1.2f;
		}
	}
	NiMatrix33 expected = RidingFrameFromHeading(0.4f);
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			expected.data[r][c] *= 1.2f;
		}
	}
	Check(UprightFrameOf(scaled, up) && NearMatrix(up, expected), "a node scaled 1.2: the upright frame scaled 1.2");
	NiMatrix33 zero{};
	Check(!UprightFrameOf(zero, up), "a collapsed node: no upright frame");
	NiMatrix33 bad = RidingFrameFromHeading(0.0f);
	bad.data[0][1] = std::nanf("");
	Check(!UprightFrameOf(bad, up), "a node with a NaN: no upright frame");
}

void TestUprightOverSaddle() {
	std::printf("The eyes upright over the saddle\n");
	const NiPoint3 saddle{0.0f, -3.0f, 110.0f};  // right, forward, up on the body
	const NiPoint3 eyes{2.0f, -5.0f, 175.0f};
	const NiPoint3 aboveSaddle = eyes - saddle;
	bool everyIncline = true;
	bool saddleOnBody = true;
	for (float heading : {0.3f, 5.78f}) {
		for (float p : {-40.0f, -30.0f, -18.0f, -8.0f, 0.0f, 8.0f, 18.0f, 30.0f, 40.0f}) {
			for (float r : {-30.0f, -15.0f, 0.0f, 15.0f, 30.0f}) {
				const NiMatrix33 frame = RidingFrameFromHeading(heading) * PitchedBy(Radians(p)) * RolledBy(Radians(r));
				NiMatrix33 upright{};
				UprightFrameOf(frame, upright);
				const NiPoint3 world = UprightOverSaddle(frame, upright, saddle, eyes, 1.0f);
				const NiPoint3 saddleWorld = frame * saddle;
				saddleOnBody = saddleOnBody && Near(saddleWorld, frame * saddle);
				everyIncline = everyIncline && Near(InHeading(world - saddleWorld, heading), aboveSaddle, 1e-2f);
			}
		}
	}
	Check(everyIncline,
	      "up, down, rolled, both together, 8 to 40 degrees: the eyes stand over the saddle as on the level");
	Check(saddleOnBody, "the saddle itself rides the tilted body");
	// Share 0: the whole node's frame, as before; on 18 degrees the eyes then
	// stand 65 sin 18 = 20 units back of over the saddle - the measured ride.
	const NiMatrix33 climb = RidingFrameFromHeading(0.0f) * PitchedBy(Radians(18.0f));
	NiMatrix33 upright{};
	UprightFrameOf(climb, upright);
	Check(Near(UprightOverSaddle(climb, upright, saddle, eyes, 0.0f), climb * eyes, 1e-3f),
	      "share 0: the eyes turned with the whole body, as before");
	const NiPoint3 old = InHeading(climb * eyes - climb * saddle, 0.0f);
	Check(old.y < aboveSaddle.y - 15.0f, "and that leaves them over 15 units back of over the saddle at 18 degrees");
	const NiPoint3 half = UprightOverSaddle(climb, upright, saddle, eyes, 0.5f);
	Check(Near(half, (climb * eyes + UprightOverSaddle(climb, upright, saddle, eyes, 1.0f)) * 0.5f, 1e-3f),
	      "share 0.5: half way between");
	Check(Near(UprightOverSaddle(RidingFrameFromHeading(0.9f), RidingFrameFromHeading(0.9f), saddle, eyes, 1.0f),
	           RidingFrameFromHeading(0.9f) * eyes, 1e-3f),
	      "level: the same as the body's frame");
}

// The rider's eyes and the saddle as the engine has them on the body, the
// body tilting: the camera and the saddle in the node's frame hold.
struct Ride {
	NiPoint3 horseAt;
	NiMatrix33 frame;
	NiPoint3 camera;
	NiPoint3 saddle;  // in the node's frame
};

Ride RideAt(int i, float pitchDegrees, float rollDegrees, float heading) {
	Ride r{};
	r.frame = RidingFrameFromHeading(heading) * PitchedBy(Radians(pitchDegrees)) * RolledBy(Radians(rollDegrees));
	r.horseAt = NiPoint3{100.0f + 5.0f * static_cast<float>(i), 200.0f, 50.0f + static_cast<float>(i)};
	r.saddle = NiPoint3{0.0f, -3.0f, 110.0f};
	r.camera = RidingOffsetToWorld(r.horseAt, r.frame, -5.0f, 2.0f, 175.0f);
	return r;
}

float OffUpright(const NiPoint3& base, const Ride& r, float heading) {
	const NiPoint3 above = InHeading(base - (r.horseAt + r.frame * r.saddle), heading);
	return std::sqrt((above.x - 2.0f) * (above.x - 2.0f) + (above.y + 2.0f) * (above.y + 2.0f));
}

void TestStepUpright() {
	std::printf("The base upright over the saddle, frame by frame\n");
	const float dt = 1.0f / 75.0f;
	// Level: with the saddle the same as without.
	{
		RidingBaseState with;
		RidingBaseState without;
		float worst = 0.0f;
		for (int i = 0; i < 150; ++i) {
			const Ride r = RideAt(i, 0.0f, 0.0f, 0.3f);
			const NiPoint3 a = StepRidingBase(with, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
			const NiPoint3 b = StepRidingBase(without, 7u, r.camera, r.horseAt, r.frame, dt, 0.3f);
			worst = std::fmax(worst, Apart(a, b));
		}
		Check(worst < 1e-2f, "level: the saddle changes nothing");
	}
	// Mounted on an 18-degree slope: the first frame the camera, then the
	// eyes come upright over half a second without a jump.
	{
		RidingBaseState s;
		const Ride r0 = RideAt(0, 18.0f, 0.0f, 0.3f);
		const NiPoint3 first = StepRidingBase(s, 7u, r0.camera, r0.horseAt, r0.frame, &r0.saddle, dt, 0.3f);
		Check(Near(first, r0.camera), "the mount on a slope: the first frame is the engine's camera");
		Check(Near(s.upright, 0.0f), "and the upright share starts at 0");
		NiPoint3 last = first;
		float worstStep = 0.0f;
		for (int i = 1; i <= 75; ++i) {
			const Ride r = RideAt(0, 18.0f, 0.0f, 0.3f);
			const NiPoint3 b = StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
			worstStep = std::fmax(worstStep, Apart(b, last));
			last = b;
		}
		Check(worstStep < 1.0f, "coming upright on 18 degrees: under a unit a frame");
		Check(Near(s.upright, 1.0f) && OffUpright(last, r0, 0.3f) < 0.05f, "a second later the eyes stand over the saddle");
	}
	// Every incline, steep ones too, ridden into from the level: the eyes stay
	// over the saddle within the settle's lag of the camera's bob (none here).
	{
		struct Incline {
			float pitch;
			float roll;
			const char* what;
		};
		const Incline inclines[] = {
			{8.0f, 0.0f, "up 8"},     {18.0f, 0.0f, "up 18"},   {30.0f, 0.0f, "up 30"},   {40.0f, 0.0f, "up 40"},
			{-8.0f, 0.0f, "down 8"},  {-18.0f, 0.0f, "down 18"}, {-30.0f, 0.0f, "down 30"}, {-40.0f, 0.0f, "down 40"},
			{0.0f, 15.0f, "roll right 15"}, {0.0f, 30.0f, "roll right 30"}, {0.0f, -15.0f, "roll left 15"},
			{0.0f, -30.0f, "roll left 30"}, {20.0f, 20.0f, "up and right 20"}, {-25.0f, -15.0f, "down and left"},
		};
		bool all = true;
		for (const Incline& in : inclines) {
			RidingBaseState s;
			float worst = 0.0f;
			for (int i = 0; i < 300; ++i) {
				// Level for a second, then over two seconds into the incline, then held.
				const float t = i < 75 ? 0.0f : std::fmin(1.0f, static_cast<float>(i - 75) / 150.0f);
				const Ride r = RideAt(i, in.pitch * t, in.roll * t, 5.78f);
				const NiPoint3 b = StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
				if (i >= 40) {
					worst = std::fmax(worst, OffUpright(b, r, 5.78f));
				}
			}
			if (!(worst < 0.05f)) {
				std::printf("    %s: %.3f units off upright\n", in.what, static_cast<double>(worst));
				all = false;
			}
		}
		Check(all, "up and down 8 to 40 degrees, rolled 15 and 30 each way, combined: over the saddle throughout");
	}
	// The saddle lost (its bone not found): back to the body's frame, smoothly.
	{
		RidingBaseState s;
		NiPoint3 last{};
		for (int i = 0; i < 150; ++i) {
			const Ride r = RideAt(0, 18.0f, 0.0f, 0.3f);
			last = StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.0f);
		}
		float worstStep = 0.0f;
		NiPoint3 b = last;
		for (int i = 0; i < 75; ++i) {
			const Ride r = RideAt(0, 18.0f, 0.0f, 0.3f);
			b = StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, nullptr, dt, 0.0f);
			worstStep = std::fmax(worstStep, Apart(b, last));
			last = b;
		}
		const Ride r = RideAt(0, 18.0f, 0.0f, 0.3f);
		Check(worstStep < 1.0f, "the saddle lost: under a unit a frame back");
		Check(Near(s.upright, 0.0f) && Apart(b, r.camera) < 0.05f, "half a second later: the body's frame, as before");
		const NiPoint3 nan{std::nanf(""), 0.0f, 110.0f};
		b = StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &nan, dt, 0.0f);
		Check(Finite(b) && !s.saddleValid, "a saddle with a NaN counts as lost, the base stays finite");
	}
	// A collapsed node: no upright frame, the body's frame as before.
	{
		RidingBaseState s;
		const Ride r = RideAt(0, 0.0f, 0.0f, 0.3f);
		StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
		NiMatrix33 zero{};
		const NiPoint3 b = StepRidingBase(s, 7u, r.camera, r.horseAt, zero, &r.saddle, dt, 0.3f);
		Check(Finite(b), "a collapsed node: the base stays finite");
	}
	// The saddle's own settle: a bob followed slowly, a jump taken whole.
	{
		RidingBaseState s;
		Ride r = RideAt(0, 0.0f, 0.0f, 0.0f);
		StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.0f);
		Check(s.saddleValid && Near(s.saddle, r.saddle), "the first saddle taken whole");
		r.saddle.z += 4.0f;
		StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.0f);
		Check(Near(s.saddle.z, 110.0f + 4.0f * (dt / kRidingSettleSeconds)), "a 4-unit bob of the saddle: followed by the settle");
		r.saddle.y += 40.0f;
		StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.0f);
		Check(Near(s.saddle, r.saddle), "40 units in a frame: the saddle taken whole");
	}
	// Another horse and the reset: the upright share starts anew.
	{
		RidingBaseState s;
		for (int i = 0; i < 100; ++i) {
			const Ride r = RideAt(0, 10.0f, 0.0f, 0.3f);
			StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
		}
		const Ride r = RideAt(0, 10.0f, 0.0f, 0.3f);
		const NiPoint3 b = StepRidingBase(s, 8u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
		Check(Near(b, r.camera) && Near(s.upright, 0.0f), "another horse: its camera as it is, the share at 0");
		ResetRidingBase(s);
		Check(!s.valid && !s.saddleValid && Near(s.upright, 0.0f), "the dismount's reset forgets saddle and share");
		// A bad frame time: the share a whole step.
		StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, dt, 0.3f);
		StepRidingBase(s, 7u, r.camera, r.horseAt, r.frame, &r.saddle, 0.0f, 0.3f);
		Check(Near(s.upright, 1.0f), "dt 0: the share a whole step");
	}
}

}  // namespace

int main() {
	TestFrame();
	TestTilt();
	TestFirstFrame();
	TestSettleAndShare();
	TestTurnAndChange();
	TestSnap();
	TestUprightFrame();
	TestUprightOverSaddle();
	TestStepUpright();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

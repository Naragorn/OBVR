// Checks the base of the eyes on a horse (camera/RidingBase.h): the offset in
// the horse's frame and back, the first frame taking the camera as it is, the
// settle, the share of the bob at 0 and 1, a horse that turns, a change of
// horse, a bad frame time, and the reset.

#include <cmath>
#include <cstdio>

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

}  // namespace

int main() {
	TestFrame();
	TestFirstFrame();
	TestSettleAndShare();
	TestTurnAndChange();
	TestSnap();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

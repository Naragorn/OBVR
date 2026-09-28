// Checks the weapon's and the hands' push (game/WorldPush.h): which rays a
// frame traces, where a hit lies on the pusher, how fast the pusher is there,
// and what a body it meets is given.

#include <cmath>
#include <cstdio>

#include "game/WorldPush.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
bool NearP(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

void TestRays() {
	std::printf("The rays\n");
	const PushSegment still{NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 60.0f, 0.0f}};
	PushRay rays[kPushRays];
	UInt32 n = PushRaysFor(still, still, rays);
	Check(n == 1 && NearP(rays[0].from, still.a) && NearP(rays[0].to, still.b) && rays[0].s0 == 0.0f &&
	          rays[0].s1 == 1.0f,
	      "held still: only along the pusher, grip to tip");

	const PushSegment moved{NiPoint3{10.0f, 0.0f, 0.0f}, NiPoint3{10.0f, 60.0f, 0.0f}};
	n = PushRaysFor(still, moved, rays);
	Check(n == 4, "moved: along it, and the sweeps of its tip, middle and grip");
	Check(NearP(rays[1].from, still.b) && NearP(rays[1].to, moved.b) && rays[1].s0 == 1.0f &&
	          rays[1].s1 == 1.0f,
	      "the tip's sweep, from where it was to where it is");
	Check(NearP(rays[2].from, NiPoint3{0.0f, 30.0f, 0.0f}) && rays[2].s0 == 0.5f, "the middle's");
	Check(NearP(rays[3].to, moved.a) && rays[3].s0 == 0.0f, "the grip's");

	// Turned about its grip: the grip did not move, so it sweeps nothing.
	const PushSegment turned{NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{60.0f, 0.0f, 0.0f}};
	n = PushRaysFor(still, turned, rays);
	Check(n == 3 && rays[2].s0 == 0.5f, "turned about the grip: the grip's sweep left out");
	const PushSegment crept{NiPoint3{0.2f, 0.0f, 0.0f}, NiPoint3{0.2f, 60.0f, 0.0f}};
	Check(PushRaysFor(still, crept, rays) == 1, "a creep under half a unit sweeps nothing");
}

void TestPoints() {
	std::printf("Where on the pusher, how fast\n");
	PushRay along{NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 60.0f, 0.0f}, 0.0f, 1.0f};
	Check(Near(PushPointOnRay(along, 0.25f), 0.25f), "along the pusher: the hit's fraction is the point");
	PushRay sweep{NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{10.0f, 0.0f, 0.0f}, 1.0f, 1.0f};
	Check(Near(PushPointOnRay(sweep, 0.7f), 1.0f), "a sweep: always the point that swept");

	const PushSegment last{NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 60.0f, 0.0f}};
	const PushSegment now{NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{6.0f, 60.0f, 0.0f}};
	const NiPoint3 tip = PusherVelocityAt(last, now, 1.0f, 0.01f);
	const NiPoint3 mid = PusherVelocityAt(last, now, 0.5f, 0.01f);
	const NiPoint3 grip = PusherVelocityAt(last, now, 0.0f, 0.01f);
	Check(Near(tip.x, 600.0f) && Near(mid.x, 300.0f) && Near(grip.x, 0.0f),
	      "a swing about the grip: the tip fastest, the grip still");
	Check(PusherVelocityAt(last, now, 1.0f, 0.0f).LengthSquared() == 0.0f, "no frame time: no speed");
}

void TestPush() {
	std::printf("What the body is given\n");
	NiPoint3 out;
	Check(PushedVelocity(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, out) &&
	          NearP(out, NiPoint3{20.0f, 0.0f, 0.0f}),
	      "a resting body takes the pusher's speed");
	Check(!PushedVelocity(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{2.0f, 0.0f, 0.0f}, out),
	      "a pusher slower than 0.3 m/s leaves it alone");
	Check(!PushedVelocity(NiPoint3{30.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, out),
	      "a body already faster that way keeps its speed");
	Check(PushedVelocity(NiPoint3{0.0f, 0.0f, -9.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, out) &&
	          NearP(out, NiPoint3{20.0f, 0.0f, -9.0f}),
	      "what it has across the push (falling) it keeps");
	Check(PushedVelocity(NiPoint3{-10.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, out) &&
	          NearP(out, NiPoint3{20.0f, 0.0f, 0.0f}),
	      "one coming the other way is turned round");
	Check(PushedVelocity(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 500.0f, 0.0f}, out) &&
	          Near(out.y, kPushMaxHavokPerSecond),
	      "never more than 15 m/s");
	const float nan = std::nan("");
	Check(!PushedVelocity(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{nan, 0.0f, 0.0f}, out),
	      "a pusher speed that is not a number pushes nothing");
}

void TestShapes() {
	std::printf("The blade and the hand\n");
	Check(Near(BladeLengthFromBound(40.0f), 76.0f), "the blade about the bound's diameter");
	Check(BladeLengthFromBound(0.0f) == kPushFallbackBladeUnits &&
	          BladeLengthFromBound(500.0f) == kPushFallbackBladeUnits &&
	          BladeLengthFromBound(std::nan("")) == kPushFallbackBladeUnits,
	      "no bound, or one that cannot be a weapon: a longsword's length");
	const NiPoint3 grip{100.0f, 0.0f, 50.0f};
	const NiPoint3 forward{0.0f, 1.0f, 0.0f};
	const PushSegment blade = BladeSegment(grip, forward, 70.0f);
	Check(NearP(blade.a, grip) && NearP(blade.b, NiPoint3{100.0f, 70.0f, 50.0f}),
	      "the blade: from the grip along the hand's forward");
	const PushSegment hand = HandSegment(grip, forward);
	Check(NearP(hand.a, NiPoint3{100.0f, -kPushHandBackUnits, 50.0f}) &&
	          NearP(hand.b, NiPoint3{100.0f, kPushHandAheadUnits, 50.0f}),
	      "the hand: wrist behind the grip, fingers ahead");
}

}  // namespace

int main() {
	TestRays();
	TestPoints();
	TestPush();
	TestShapes();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

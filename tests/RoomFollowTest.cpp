// Checks the body following the head through the room (vr/RoomFollowLogic.h).

#include <cstdio>
#include <limits>

#include "vr/RoomFollowLogic.h"

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

RoomFollowGate Free() {
	RoomFollowGate g;
	g.fullVr = true;
	g.headset = true;
	g.firstPerson = true;
	g.inWorld = true;
	return g;
}

void TestGate() {
	std::printf("the gate: Full VR, a tracked head, first person, on foot in the world\n");
	Check(RoomFollowAllowed(RoomFollowSettings{}, Free()), "on by default");
	RoomFollowSettings s;
	s.enabled = true;
	Check(RoomFollowAllowed(s, Free()), "all there: follows");
	RoomFollowSettings off;
	off.enabled = false;
	Check(!RoomFollowAllowed(off, Free()), "[Hands] RoomScaleBody=0: never");
	RoomFollowGate g = Free();
	g.fullVr = false;
	Check(!RoomFollowAllowed(s, g), "seated (not Full VR): the head moves only the camera, as before");
	g = Free();
	g.headset = false;
	Check(!RoomFollowAllowed(s, g), "no tracked head");
	g = Free();
	g.firstPerson = false;
	Check(!RoomFollowAllowed(s, g), "third person");
	g = Free();
	g.inWorld = false;
	Check(!RoomFollowAllowed(s, g), "not in the world");
	g = Free();
	g.menuUp = true;
	Check(!RoomFollowAllowed(s, g), "a menu up (a conversation too)");
	g = Free();
	g.riding = true;
	Check(!RoomFollowAllowed(s, g), "on a horse");
	g = Free();
	g.flyCam = true;
	Check(!RoomFollowAllowed(s, g), "the fly camera on");
	g = Free();
	g.knockedDown = true;
	Check(!RoomFollowAllowed(s, g), "knocked down");
}

void TestWanted() {
	std::printf("the step wanted: the head's horizontal lead over the followed, capped\n");
	const RoomFollowSettings s;
	RoomFollowState st;
	NiPoint3 w = RoomStepWanted(NiPoint3{5.0f, 3.0f, 9.0f}, NiPoint3{1.0f, 1.0f, 0.0f}, s, st);
	Check(Near(w.x, 4.0f) && Near(w.y, 2.0f) && w.z == 0.0f, "the difference, the height left out");
	w = RoomStepWanted(NiPoint3{2.0f, 2.0f, 0.0f}, NiPoint3{2.0f, 2.0f, 0.0f}, s, st);
	Check(w.x == 0.0f && w.y == 0.0f, "followed all of it: no step");
	w = RoomStepWanted(NiPoint3{0.03f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, s, st);
	Check(w.x == 0.0f && w.y == 0.0f, "a rounding's remainder (0.03 units): no step");
	w = RoomStepWanted(NiPoint3{100.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, s, st);
	Check(Near(w.x, s.maxStepUnits) && w.y == 0.0f, "a jump of 100 units: one frame takes only maxStepUnits");
	RoomFollowSettings free = s;
	free.maxStepUnits = 0.0f;
	w = RoomStepWanted(NiPoint3{100.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, free, st);
	Check(Near(w.x, 100.0f), "maxStepUnits 0: no cap");
}

// The start of a walk (RoomFollowState, [Hands] RoomScaleStart): a lead
// under the start zone is a sway and asks nothing; past it the body follows
// every frame until it has caught up (under kRoomStepLeastUnits), then a
// new sway asks nothing again. Without the state, or with the zone 0, every
// lead over a twentieth of a unit steps - which walked the body in bursts
// of 0.05 units every four to five frames under a turning head and let the
// engine re-ground the player at each (the water's "reset", 2026-11-02).
void TestStart() {
	std::printf("the walk's start: a sway stays a camera offset, a walk follows every frame\n");
	RoomFollowSettings s;
	s.startUnits = 2.0f;
	RoomFollowState state;
	NiPoint3 w = RoomStepWanted(NiPoint3{1.5f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, s, state);
	Check(w.x == 0.0f && w.y == 0.0f && !state.following, "1.5 units of lead under the start of 2: a sway, no step");
	w = RoomStepWanted(NiPoint3{2.5f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, s, state);
	Check(Near(w.x, 2.5f) && state.following, "2.5 units: the walk starts with the whole lead");
	w = RoomStepWanted(NiPoint3{2.5f, 0.0f, 0.0f}, NiPoint3{2.0f, 0.0f, 0.0f}, s, state);
	Check(Near(w.x, 0.5f) && state.following, "following: 0.5 left is still walked, under the start zone");
	w = RoomStepWanted(NiPoint3{2.5f, 0.0f, 0.0f}, NiPoint3{2.48f, 0.0f, 0.0f}, s, state);
	Check(w.x == 0.0f && !state.following, "caught up (0.02 left): the walk ends");
	w = RoomStepWanted(NiPoint3{3.5f, 0.0f, 0.0f}, NiPoint3{2.48f, 0.0f, 0.0f}, s, state);
	Check(w.x == 0.0f && !state.following, "a new sway of 1.02 units: nothing, the offset stays the camera's");
	w = RoomStepWanted(NiPoint3{100.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, s, state);
	Check(Near(w.x, s.maxStepUnits) && state.following, "a jump past the start is capped as before");
	RoomFollowSettings zero = s;
	zero.startUnits = 0.0f;
	RoomFollowState fresh;
	w = RoomStepWanted(NiPoint3{0.06f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, zero, fresh);
	Check(Near(w.x, 0.06f) && fresh.following, "a start zone of 0: every lead over a twentieth steps, as before");
	w = RoomStepWanted(NiPoint3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, s, state);
	Check(w.x == 0.0f && !state.following, "a lead that is not a number: nothing, and the walk ends");
}

void TestToActor() {
	std::printf("a world step in the actor's frame\n");
	NiPoint3 l = WorldStepToActor(0.0f, 10.0f, 0.0f);
	Check(Near(l.x, 0.0f) && Near(l.y, 10.0f), "heading 0 (north), a step north: forward");
	l = WorldStepToActor(10.0f, 0.0f, 0.0f);
	Check(Near(l.x, 10.0f) && Near(l.y, 0.0f), "heading 0, a step east: to the right");
	l = WorldStepToActor(10.0f, 0.0f, 1.5707963f);
	Check(Near(l.x, 0.0f) && Near(l.y, 10.0f), "heading east (90), a step east: forward");
	l = WorldStepToActor(0.0f, 10.0f, 1.5707963f);
	Check(Near(l.x, -10.0f) && Near(l.y, 0.0f), "heading east, a step north: to the left");
}

void TestShare() {
	std::printf("the share of the step achieved\n");
	Check(Near(RoomStepShare(4.0f, 0.0f, 4.0f, 0.0f), 1.0f), "all of it moved: 1");
	Check(Near(RoomStepShare(0.0f, 0.0f, 4.0f, 0.0f), 0.0f), "a wall: 0");
	Check(Near(RoomStepShare(2.0f, 0.0f, 4.0f, 0.0f), 0.5f), "half: 0.5");
	Check(Near(RoomStepShare(2.0f, 3.0f, 4.0f, 0.0f), 0.5f), "slid along a wall: only the part along the step");
	Check(Near(RoomStepShare(-2.0f, 0.0f, 4.0f, 0.0f), 0.0f), "pushed back: 0, never negative");
	Check(Near(RoomStepShare(9.0f, 0.0f, 4.0f, 0.0f), 1.0f), "more than asked (a slope's slide): 1 at most");
	Check(RoomStepShare(5.0f, 5.0f, 0.0f, 0.0f) == 0.0f, "nothing asked: 0");
}

}  // namespace

int main() {
	TestGate();
	TestWanted();
	TestStart();
	TestToActor();
	TestShare();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

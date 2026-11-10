// Checks the body following the head through the room (vr/RoomFollowLogic.h).

#include <cstdio>
#include <limits>

#include "vr/Quaternion.h"
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

void TestAchieved() {
	std::printf("what the room's step achieved, in the world\n");
	NiPoint3 a = RoomStepAchieved(4.0f, 0.0f, 4.0f, 0.0f, 4.0f, 0.0f, false);
	Check(Near(a.x, 4.0f) && Near(a.y, 0.0f), "a free floor: all of it");
	a = RoomStepAchieved(0.0f, 0.0f, 4.0f, 0.0f, 4.0f, 0.0f, false);
	Check(Near(a.x, 0.0f) && Near(a.y, 0.0f), "a wall straight ahead: nothing");
	a = RoomStepAchieved(3.0f, 0.0f, 3.0f, 20.0f, 3.0f, 20.0f, false);
	Check(Near(a.x, 3.0f) && Near(a.y, 0.0f), "a wall ahead, the step slanted: the slide along the wall counts whole");
	a = RoomStepAchieved(-1.0f, 0.5f, 4.0f, 0.0f, 4.0f, 0.0f, false);
	Check(Near(a.x, -1.0f) && Near(a.y, 0.5f), "pushed back: the push counts, the feet are where they are");
	a = RoomStepAchieved(30.0f, 40.0f, 3.0f, 4.0f, 3.0f, 4.0f, false);
	Check(Near(a.x, 3.0f) && Near(a.y, 4.0f), "more than asked: at most the step's length");
	a = RoomStepAchieved(5.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, false);
	Check(Near(a.x, 0.0f) && Near(a.y, 0.0f), "no room step asked: nothing of it");
	a = RoomStepAchieved(std::numeric_limits<float>::quiet_NaN(), 0.0f, 4.0f, 0.0f, 4.0f, 0.0f, false);
	Check(Near(a.x, 0.0f) && Near(a.y, 0.0f), "a NaN move: nothing");
	a = RoomStepAchieved(10.0f, 0.0f, 12.0f, 0.0f, 2.0f, 0.0f, true);
	Check(Near(a.x, 2.0f * 10.0f / 12.0f) && Near(a.y, 0.0f), "walking as well: the room's part by the share");
	a = RoomStepAchieved(3.0f, 0.0f, 3.0f, 20.0f, 0.0f, 10.0f, true);
	Check(Near(a.x, 0.0f) && Near(a.y, 9.0f / 409.0f * 10.0f, 0.02f),
	      "walking into a wall: only the share along the step, the walk's slide is the walk's");
}

// A wall at y = 0 the feet stand against (y < 0): a step is swept to it and
// slides along it, as the character controller does.
NiPoint3 WallMove(const NiPoint3& feet, float stepX, float stepY) {
	NiPoint3 to{feet.x + stepX, feet.y + stepY, 0.0f};
	if (to.y > -0.01f) {
		to.y = feet.y > -0.01f ? feet.y : -0.01f;
	}
	return NiPoint3{to.x - feet.x, to.y - feet.y, 0.0f};
}

void TestPressedAgainstAWall() {
	std::printf("the head pressed into a wall, moved sideways\n");
	RoomFollowSettings s;
	RoomFollowState state;
	NiPoint3 feet{0.0f, -0.01f, 0.0f};
	NiPoint3 followed{0.0f, 0.0f, 0.0f};
	NiPoint3 head{0.0f, 30.0f, 0.0f};  // 30 units into the wall
	auto run = [&](int frames) {
		for (int i = 0; i < frames; ++i) {
			const NiPoint3 want = RoomStepWanted(head, followed, s, state);
			if (want.x == 0.0f && want.y == 0.0f) {
				continue;
			}
			const NiPoint3 moved = WallMove(feet, want.x, want.y);
			feet = NiPoint3{feet.x + moved.x, feet.y + moved.y, 0.0f};
			const NiPoint3 got = RoomStepAchieved(moved.x, moved.y, want.x, want.y, want.x, want.y, false);
			followed = NiPoint3{followed.x + got.x, followed.y + got.y, 0.0f};
		}
	};
	run(90);
	Check(Near(feet.x, 0.0f, 0.05f), "pressed in: the feet stay put");
	const float eyesBefore = feet.x + (head.x - followed.x);
	head.x = 7.0f;  // the eyes 10 cm to the right (a turn of the head)
	run(90);
	Check(Near(feet.x, 7.0f, 0.1f), "the eyes 7 units aside: the feet 7 units along the wall, no more");
	Check(Near(feet.x + (head.x - followed.x), eyesBefore + 7.0f, 0.1f), "the eyes where the head is");
	const float feetThen = feet.x;
	run(180);
	Check(Near(feet.x, feetThen, 0.05f), "two seconds on: no slide");
	head.x = 0.0f;
	run(90);
	Check(Near(feet.x, 0.0f, 0.1f), "back to the middle: the feet back where they were");
}

// A head that only turns about its neck moves the eyes; the neck stays.
// The rotations are built the way HeadTracker builds its own: an OpenXR
// orientation through FromOpenXR and ToMatrix.
void TestNeckPivot() {
	std::printf("the neck's pivot\n");
	const float half = 0.70710678f;
	// OpenXR: about +Y by +90 degrees turns -Z (forward) to -X: a look left.
	const NiMatrix33 left = ToMatrix(FromOpenXR(Quaternion{0.0f, half, 0.0f, half}));
	const NiPoint3 forward = left * NiPoint3{0.0f, 1.0f, 0.0f};
	Check(Near(forward.x, -1.0f) && Near(forward.y, 0.0f) && Near(forward.z, 0.0f),
	      "the matrix as built: a look left turns forward to the left (-x)");
	const NiPoint3 eye{0.0f, kNeckToEyeForwardMetres * 70.0f, kNeckToEyeUpMetres * 70.0f};
	// The eyes where a real neck carries them: the turned neck-to-eye vector less the one at the recenter.
	const NiPoint3 eyesMoved = left * eye - eye;
	Check(eyesMoved.x < -5.0f, "turned 90 degrees, the eyes moved more than 5 units to the left");
	NiPoint3 pivot = NeckPivotOffset(eyesMoved, left, eye);
	Check(Near(pivot.x, 0.0f) && Near(pivot.y, 0.0f) && Near(pivot.z, 0.0f), "a look left: the neck has not moved");
	// OpenXR about +X by -45 degrees: a look down.
	const float s45 = 0.38268343f;
	const float c45 = 0.92387953f;
	const NiMatrix33 down = ToMatrix(FromOpenXR(Quaternion{-s45, 0.0f, 0.0f, c45}));
	const NiPoint3 ahead = down * NiPoint3{0.0f, 1.0f, 0.0f};
	Check(ahead.z < -0.5f, "the matrix as built: a look down turns forward downward");
	pivot = NeckPivotOffset(down * eye - eye, down, eye);
	Check(Near(pivot.x, 0.0f) && Near(pivot.y, 0.0f) && Near(pivot.z, 0.0f), "a nod: the neck has not moved");
	// A step to the right with the head turned left: the neck moved by the step alone.
	pivot = NeckPivotOffset(eyesMoved + NiPoint3{10.0f, 0.0f, 0.0f}, left, eye);
	Check(Near(pivot.x, 10.0f) && Near(pivot.y, 0.0f), "a step with the head turned: the step alone is walked");
	// Facing as at the recenter: the offset itself.
	pivot = NeckPivotOffset(NiPoint3{3.0f, 4.0f, 0.0f}, NiMatrix33::Identity(), eye);
	Check(Near(pivot.x, 3.0f) && Near(pivot.y, 4.0f), "no turn: the head's offset as it is");
	// A rotation of no number: the offset itself.
	NiMatrix33 bad = NiMatrix33::Identity();
	bad.data[0][1] = std::numeric_limits<float>::quiet_NaN();
	pivot = NeckPivotOffset(NiPoint3{3.0f, 4.0f, 0.0f}, bad, eye);
	Check(Near(pivot.x, 3.0f) && Near(pivot.y, 4.0f), "a rotation of no number: the head's offset");
	// The room follow on the pivot: a look left starts no walk.
	RoomFollowSettings set;
	RoomFollowState state;
	const NiPoint3 wanted = RoomStepWanted(NeckPivotOffset(eyesMoved, left, eye), NiPoint3{0, 0, 0}, set, state);
	Check(wanted.x == 0.0f && wanted.y == 0.0f && !state.following, "a look left: no step through the room");
	RoomFollowState raw;
	const NiPoint3 before = RoomStepWanted(eyesMoved, NiPoint3{0, 0, 0}, set, raw);
	Check(before.x < 0.0f && raw.following, "the eyes themselves (before 2026-11-10): the look walked the body");
}

}  // namespace

int main() {
	TestNeckPivot();
	TestGate();
	TestWanted();
	TestStart();
	TestToActor();
	TestShare();
	TestAchieved();
	TestPressedAgainstAWall();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

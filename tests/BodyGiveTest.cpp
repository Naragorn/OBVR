// Checks the living giving way to a hand or the blade (game/BodyGiveLogic.h):
// the parts built from the bones, a pusher's touch on a part, the turn a
// press asks of a joint, the kick of a strike, a joint's spring - following a
// press, coming back with its wobble, its caps, its rest - and the rotation
// written to the bone.

#include <cstdio>
#include <limits>

#include "game/BodyGiveLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

// MSVC refuses a constant 0/0 (C2124).
const float kNan = std::numeric_limits<float>::quiet_NaN();

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

const NiPoint3 kZero{0.0f, 0.0f, 0.0f};

// Someone standing at the origin: the neck at 100 units, the head bone 5
// above it, the arms hanging straight down 12 units either side (the upper
// arm 22 units, the forearm 15).
GiveBones Standing() {
	GiveBones b;
	b.head = true;
	b.neck = NiPoint3{0.0f, 0.0f, 100.0f};
	b.headBone = NiPoint3{0.0f, 0.0f, 105.0f};
	for (UInt32 arm = 0; arm < 2; ++arm) {
		const float x = arm == 0 ? -12.0f : 12.0f;
		b.arm[arm] = true;
		b.shoulder[arm] = NiPoint3{x, 0.0f, 98.0f};
		b.elbow[arm] = NiPoint3{x, 0.0f, 76.0f};
		b.wrist[arm] = NiPoint3{x, 0.0f, 61.0f};
	}
	return b;
}

// A hand as a ball of 2.5 units at `at`, moving at `velocity`.
GivePusher Ball(const NiPoint3& at, const NiPoint3& velocity = kZero) {
	GivePusher p;
	p.valid = true;
	p.a = at;
	p.b = at;
	p.radius = 2.5f;
	p.velA = velocity;
	p.velB = velocity;
	return p;
}

void TestParts() {
	std::printf("The parts from the bones\n");
	GivePart parts[kGivePartsMax];
	GiveBones b = Standing();
	Check(GiveParts(b, parts) == 5, "a head and two arms: five parts");
	Check(parts[0].joint == kGiveNeck && NearPoint(parts[0].pivot, b.neck), "the head turns about the neck");
	Check(NearPoint(parts[0].a, b.headBone) && NearPoint(parts[0].b, NiPoint3{0.0f, 0.0f, 114.0f}) &&
	          Near(parts[0].radius, 7.0f),
	      "the head reaches 9 units past its bone, away from the neck, 7 round");
	Check(!parts[0].hasUpper, "the head has no joint above");
	Check(parts[1].joint == kGiveLeftUpper && NearPoint(parts[1].pivot, b.shoulder[0]) &&
	          NearPoint(parts[1].b, b.elbow[0]) && Near(parts[1].radius, 4.5f) && !parts[1].hasUpper,
	      "the left upper arm from the shoulder to the elbow, about the shoulder");
	Check(parts[2].joint == kGiveLeftFore && NearPoint(parts[2].pivot, b.elbow[0]) &&
	          NearPoint(parts[2].b, NiPoint3{-12.0f, 0.0f, 56.0f}) && Near(parts[2].radius, 3.5f),
	      "the left forearm from the elbow to 5 units past the wrist, about the elbow");
	Check(parts[2].hasUpper && parts[2].upperJoint == kGiveLeftUpper && NearPoint(parts[2].upperPivot, b.shoulder[0]),
	      "the forearm names the shoulder above it");
	Check(parts[3].joint == kGiveRightUpper && parts[4].joint == kGiveRightFore &&
	          parts[4].upperJoint == kGiveRightUpper,
	      "the right arm the same");
	NiPoint3 toNeck;
	ik::Unit(b.neck - b.shoulder[0], toNeck);
	Check(NearPoint(parts[1].inward, toNeck) && NearPoint(parts[2].inward, toNeck) && parts[1].inward.x > 0.9f &&
	          parts[3].inward.x < -0.9f && NearPoint(parts[0].inward, kZero),
	      "each arm knows the way to the neck; the head has no such way");
	b.head = false;
	GiveParts(b, parts);
	Check(NearPoint(parts[0].inward, NiPoint3{1.0f, 0.0f, 0.0f}) && NearPoint(parts[2].inward, NiPoint3{-1.0f, 0.0f, 0.0f}),
	      "without a neck the way in is to the other shoulder");
	b.arm[1] = false;
	GiveParts(b, parts);
	Check(NearPoint(parts[0].inward, kZero), "with neither, an arm has no way in");
	b = Standing();

	b.arm[0] = false;
	Check(GiveParts(b, parts) == 3 && parts[1].joint == kGiveRightUpper, "an arm not read is left out");
	b = Standing();
	b.head = false;
	Check(GiveParts(b, parts) == 4 && parts[0].joint == kGiveLeftUpper, "a head not read is left out");
	b = GiveBones{};
	Check(GiveParts(b, parts) == 0, "nothing read: no parts");

	b = Standing();
	b.scale = 2.0f;
	GiveParts(b, parts);
	Check(Near(parts[0].radius, 14.0f) && NearPoint(parts[0].b, NiPoint3{0.0f, 0.0f, 123.0f}) &&
	          Near(parts[1].radius, 9.0f),
	      "someone twice the size: the flesh twice as thick");
	b.scale = 0.0f;
	GiveParts(b, parts);
	Check(Near(parts[0].radius, 7.0f), "a scale of nothing counts as 1");
	b.scale = 50.0f;
	GiveParts(b, parts);
	Check(Near(parts[0].radius, 7.0f), "a scale past 20 counts as 1");
	b = Standing();
	b.headBone = b.neck;
	GiveParts(b, parts);
	Check(NearPoint(parts[0].b, NiPoint3{0.0f, 0.0f, 109.0f}), "a head bone on the neck: the head goes straight up");
}

void TestHeadJoint() {
	std::printf("The bone the head turns about\n");
	Check(HeadTurnsAtParent(true, 1), "a parent carrying only the head: the head turns there");
	Check(!HeadTurnsAtParent(true, 3), "a parent carrying the clavicles too: the head turns at its own bone");
	Check(!HeadTurnsAtParent(false, 1), "a parent that is no bone: at its own bone");
	Check(!HeadTurnsAtParent(true, 0), "a parent whose children do not read: at its own bone");
}

void TestTouch() {
	std::printf("A pusher's touch on a part\n");
	GivePart parts[kGivePartsMax];
	GiveParts(Standing(), parts);
	const GivePart& head = parts[0];

	Check(!TouchOf(Ball(NiPoint3{10.0f, 0.0f, 110.0f}), head).hit, "a hand half a unit off the head: no touch");
	GiveTouch t = TouchOf(Ball(NiPoint3{8.0f, 0.0f, 110.0f}, NiPoint3{-70.0f, 0.0f, 0.0f}), head);
	Check(t.hit && Near(t.depth, 1.5f), "a hand 1.5 units in the head's side");
	Check(NearPoint(t.at, NiPoint3{0.0f, 0.0f, 110.0f}), "the touch is taken on the head's axis");
	Check(NearPoint(t.into, NiPoint3{-1.0f, 0.0f, 0.0f}), "the head is pushed away from the hand");
	Check(NearPoint(t.velocity, NiPoint3{-70.0f, 0.0f, 0.0f}), "with the hand's speed");

	GivePusher invalid = Ball(NiPoint3{8.0f, 0.0f, 110.0f});
	invalid.valid = false;
	Check(!TouchOf(invalid, head).hit, "a pusher that is not there touches nothing");

	// A blade across the head's side, its ends at different speeds.
	GivePusher blade;
	blade.valid = true;
	blade.a = NiPoint3{8.0f, -10.0f, 110.0f};
	blade.b = NiPoint3{8.0f, 10.0f, 110.0f};
	blade.radius = 2.5f;
	blade.velA = NiPoint3{-20.0f, 0.0f, 0.0f};
	blade.velB = NiPoint3{-60.0f, 0.0f, 0.0f};
	t = TouchOf(blade, head);
	Check(t.hit && Near(t.depth, 1.5f) && NearPoint(t.velocity, NiPoint3{-40.0f, 0.0f, 0.0f}),
	      "a blade touching with its middle: the speed there is between its ends'");
	blade.a = NiPoint3{8.0f, 0.0f, 110.0f};
	blade.b = NiPoint3{28.0f, 0.0f, 110.0f};
	t = TouchOf(blade, head);
	Check(t.hit && NearPoint(t.velocity, blade.velA), "a blade touching with its near end: that end's speed");

	// On the head's axis itself there is no way out but the pusher's own.
	t = TouchOf(Ball(NiPoint3{0.0f, 0.0f, 110.0f}, NiPoint3{0.0f, -50.0f, 0.0f}), head);
	Check(t.hit && NearPoint(t.into, NiPoint3{0.0f, -1.0f, 0.0f}) && Near(t.depth, 9.5f),
	      "a hand on the head's axis pushes the way it moves");
	Check(!TouchOf(Ball(NiPoint3{0.0f, 0.0f, 110.0f}), head).hit, "a still hand on the axis pushes nowhere");
}

void TestTurnFor() {
	std::printf("The turn a press asks for\n");
	const NiPoint3 lever{0.0f, 0.0f, 10.0f};
	const NiPoint3 turn = TurnFor(lever, NiPoint3{-2.0f, 0.0f, 0.0f});
	Check(NearPoint(turn, NiPoint3{0.0f, -0.2f, 0.0f}), "2 units across a lever of 10: a fifth of a radian");
	Check(NearPoint(ik::Cross(turn, lever), NiPoint3{-2.0f, 0.0f, 0.0f}), "that turn moves the point by the press");
	Check(NearPoint(TurnFor(lever, NiPoint3{0.0f, 0.0f, 3.0f}), kZero), "a press along the lever turns nothing");
	Check(NearPoint(TurnFor(lever, NiPoint3{-2.0f, 0.0f, 5.0f}), NiPoint3{0.0f, -0.2f, 0.0f}),
	      "only the press across the lever counts");
	Check(NearPoint(TurnFor(NiPoint3{0.0f, 0.0f, 2.0f}, NiPoint3{-2.0f, 0.0f, 0.0f}), NiPoint3{0.0f, -0.25f, 0.0f}),
	      "a touch 2 units from the joint turns it as one 4 units off would");
	Check(NearPoint(TurnFor(kZero, NiPoint3{-2.0f, 0.0f, 0.0f}), kZero), "a press on the joint itself turns nothing");
}

void TestDrives() {
	std::printf("What a frame asks of the joints\n");
	const GiveBones b = Standing();
	const GiveState fresh;
	GiveDrive d[kGiveJointCount];
	GivePusher pushers[kGivePushers];

	GatherDrives(b, pushers, kGivePushers, fresh, d);
	bool none = true;
	for (UInt32 j = 0; j < kGiveJointCount; ++j) {
		none = none && d[j].touchedBy == 0 && NearPoint(d[j].target, kZero) && NearPoint(d[j].kick, kZero);
	}
	Check(none, "no pusher: nothing asked of any joint");

	pushers[1] = Ball(NiPoint3{8.0f, 0.0f, 110.0f});
	GatherDrives(b, pushers, kGivePushers, fresh, d);
	Check(d[kGiveNeck].touchedBy == 2u, "the other hand in the head: the neck knows which pusher");
	Check(NearPoint(d[kGiveNeck].target, NiPoint3{0.0f, -0.15f, 0.0f}),
	      "the neck is asked to tip the head 1.5 units out of the hand");
	Check(NearPoint(d[kGiveNeck].kick, kZero), "a still hand kicks nothing");
	Check(d[kGiveLeftUpper].touchedBy == 0 && d[kGiveRightFore].touchedBy == 0, "the arms are not asked");

	pushers[1] = Ball(NiPoint3{8.0f, 0.0f, 110.0f}, NiPoint3{-70.0f, 0.0f, 0.0f});
	GatherDrives(b, pushers, kGivePushers, fresh, d);
	Check(NearPoint(d[kGiveNeck].kick, NiPoint3{0.0f, -4.2f, 0.0f}),
	      "a hand arriving at 1 m/s kicks the neck with 0.6 of its speed over the lever");
	GiveState touched;
	touched.joint[kGiveNeck].touchedBy = 2u;
	GatherDrives(b, pushers, kGivePushers, touched, d);
	Check(NearPoint(d[kGiveNeck].kick, kZero) && NearPoint(d[kGiveNeck].target, NiPoint3{0.0f, -0.15f, 0.0f}),
	      "a hand that lay in the head the frame before presses and does not kick again");
	touched.joint[kGiveNeck].touchedBy = 1u;
	GatherDrives(b, pushers, kGivePushers, touched, d);
	Check(NearPoint(d[kGiveNeck].kick, NiPoint3{0.0f, -4.2f, 0.0f}), "another pusher's touch the frame before does not count");
	pushers[1] = Ball(NiPoint3{8.0f, 0.0f, 110.0f}, NiPoint3{70.0f, 0.0f, 0.0f});
	GatherDrives(b, pushers, kGivePushers, fresh, d);
	Check(NearPoint(d[kGiveNeck].kick, kZero) && d[kGiveNeck].touchedBy == 2u, "a hand moving away kicks nothing");

	pushers[0] = Ball(NiPoint3{-8.0f, 0.0f, 110.0f});
	pushers[1] = Ball(NiPoint3{8.0f, 0.0f, 110.0f});
	GatherDrives(b, pushers, kGivePushers, fresh, d);
	Check(d[kGiveNeck].touchedBy == 3u && NearPoint(d[kGiveNeck].target, kZero),
	      "a hand on either side of the head: both known, their presses cancel");
	pushers[0] = GivePusher{};

	// An arm pushed towards the body gives a quarter of it.
	const NiPoint3 in{1.0f, 0.0f, 0.0f};
	Check(NearPoint(PushWay(in, in), NiPoint3{0.25f, 0.0f, 0.0f}), "a push straight at the body: a quarter of it taken");
	Check(NearPoint(PushWay(NiPoint3{-1.0f, 0.0f, 0.0f}, in), NiPoint3{-1.0f, 0.0f, 0.0f}),
	      "a push away from the body: taken whole");
	Check(NearPoint(PushWay(NiPoint3{0.0f, -1.0f, 0.0f}, in), NiPoint3{0.0f, -1.0f, 0.0f}),
	      "a push across: taken whole");
	Check(NearPoint(PushWay(NiPoint3{0.6f, -0.8f, 0.0f}, in), NiPoint3{0.15f, -0.8f, 0.0f}),
	      "a slanted push: only its part towards the body is cut");
	Check(NearPoint(PushWay(in, kZero), in), "no way in known: taken whole");
	GiveBones noNeck = Standing();
	noNeck.head = false;  // the way in is then straight along x
	pushers[1] = Ball(NiPoint3{-18.0f, 0.0f, 87.0f}, NiPoint3{50.0f, 0.0f, 0.0f});
	GatherDrives(noNeck, pushers, kGivePushers, fresh, d);
	Check(d[kGiveLeftUpper].touchedBy == 2u &&
	          NearPoint(d[kGiveLeftUpper].target, NiPoint3{0.0f, -0.25f * 1.0f * 11.0f / 121.0f, 0.0f}),
	      "a hand 1 unit in the upper arm from outside: the shoulder asked for a quarter of the turn");
	Check(NearPoint(d[kGiveLeftUpper].kick, NiPoint3{0.0f, -0.25f * 50.0f * 0.6f * 11.0f / 121.0f, 0.0f}),
	      "and kicked with a quarter of the strike");

	// The upper arm, from the front: pushed back about the shoulder alone.
	pushers[1] = Ball(NiPoint3{-12.0f, 6.0f, 87.0f});
	GatherDrives(b, pushers, kGivePushers, fresh, d);
	Check(d[kGiveLeftUpper].touchedBy == 2u && NearPoint(d[kGiveLeftUpper].target, NiPoint3{-1.0f / 11.0f, 0.0f, 0.0f}),
	      "a hand 1 unit in the upper arm, 11 below the shoulder: the shoulder turns it back");
	Check(d[kGiveLeftFore].touchedBy == 0 && d[kGiveNeck].touchedBy == 0 && d[kGiveRightUpper].touchedBy == 0,
	      "no other joint is asked");

	// The forearm: the elbow takes 0.65 of it, the shoulder the rest.
	pushers[1] = Ball(NiPoint3{-12.0f, 5.0f, 66.0f}, NiPoint3{0.0f, -100.0f, 0.0f});
	GatherDrives(b, pushers, kGivePushers, fresh, d);
	Check(d[kGiveLeftFore].touchedBy == 2u && NearPoint(d[kGiveLeftFore].target, NiPoint3{-0.065f, 0.0f, 0.0f}),
	      "a hand 1 unit in the forearm, 10 below the elbow: the elbow takes 0.65 of the press");
	Check(d[kGiveLeftUpper].touchedBy == 2u &&
	          NearPoint(d[kGiveLeftUpper].target, NiPoint3{-0.35f * 32.0f / 1024.0f, 0.0f, 0.0f}),
	      "the shoulder, 32 above the touch, takes the other 0.35");
	Check(NearPoint(d[kGiveLeftFore].kick, NiPoint3{-100.0f * 0.6f * 0.65f / 10.0f, 0.0f, 0.0f}) &&
	          NearPoint(d[kGiveLeftUpper].kick, NiPoint3{-100.0f * 0.6f * 0.35f * 32.0f / 1024.0f, 0.0f, 0.0f}),
	      "and the strike's kick is shared the same way");
	Check(d[kGiveRightFore].touchedBy == 0 && d[kGiveRightUpper].touchedBy == 0, "the other arm is not asked");
}

void TestJoint() {
	std::printf("A joint's spring\n");
	const float dt = 1.0f / 90.0f;
	GiveDrive rest;
	GiveJointState s;
	Check(!StepJoint(s, rest, kGiveNeckMost, dt), "at rest and untouched: at rest");

	// Following a press.
	GiveDrive press;
	press.touchedBy = 1u;
	press.target = NiPoint3{0.0f, -0.15f, 0.0f};
	s = GiveJointState{};
	bool active = true;
	float furthest = 0.0f;
	float after15 = 0.0f;
	for (int i = 0; i < 90; ++i) {
		active = StepJoint(s, press, kGiveNeckMost, dt) && active;
		furthest = s.turn.y < furthest ? s.turn.y : furthest;
		if (i == 14) {
			after15 = s.turn.y;
		}
	}
	Check(active, "a pressed joint is not at rest");
	Check(after15 < -0.12f, "a press is followed most of the way within a sixth of a second");
	Check(Near(s.turn.y, -0.15f, 0.002f) && Near(s.turn.x, 0.0f) && Near(s.turn.z, 0.0f),
	      "and held where the press asks");
	Check(furthest > -0.15f * 1.03f, "without swinging past it");
	Check(s.touchedBy == 1u, "the joint remembers who lay in it");

	// Let go: back through the middle once, then at rest.
	bool passedMiddle = false;
	float beyond = 0.0f;
	int restAt = -1;
	for (int i = 0; i < 360; ++i) {
		const bool moving = StepJoint(s, rest, kGiveNeckMost, dt);
		passedMiddle = passedMiddle || s.turn.y > 0.0f;
		beyond = s.turn.y > beyond ? s.turn.y : beyond;
		if (!moving && restAt < 0) {
			restAt = i;
		}
	}
	Check(passedMiddle && beyond > 0.01f && beyond < 0.075f, "let go, it swings past the middle by less than half");
	Check(restAt > 0 && restAt < 270, "and is at rest within three seconds");
	Check(s.turn.x == 0.0f && s.turn.y == 0.0f && s.turn.z == 0.0f && s.speed.y == 0.0f && s.touchedBy == 0,
	      "at rest its turn and speed are exactly nothing");

	// The caps.
	GiveDrive hard;
	hard.touchedBy = 1u;
	hard.target = NiPoint3{0.0f, -5.0f, 0.0f};
	s = GiveJointState{};
	for (int i = 0; i < 90; ++i) {
		StepJoint(s, hard, kGiveNeckMost, dt);
	}
	Check(Near(s.turn.y, -kGiveNeckMost, 0.002f), "a press past the most turns the joint to its most");
	GiveDrive strike;
	strike.touchedBy = 1u;
	strike.kick = NiPoint3{0.0f, -100.0f, 0.0f};
	s = GiveJointState{};
	StepJoint(s, strike, kGiveNeckMost, dt);
	Check(ik::Length(s.speed) <= kGiveMostSpeed + 0.001f && s.speed.y < -10.0f, "a kick is capped at the most speed");
	s = GiveJointState{};
	s.turn = NiPoint3{0.0f, -kGiveNeckMost, 0.0f};
	s.speed = NiPoint3{3.0f, -8.0f, 0.0f};
	StepJoint(s, rest, kGiveNeckMost, dt);
	Check(Near(ik::Length(s.turn), kGiveNeckMost, 0.001f), "a joint going past its most is held there");
	Check(ik::Dot(s.speed, s.turn) <= 0.001f, "and its speed further out is gone");
	Check(s.speed.x > 1.0f, "its speed along the cap is kept");

	// Swung past the press by a strike: the spring back, until the press
	// catches it.
	GiveDrive light;
	light.touchedBy = 1u;
	light.target = NiPoint3{0.0f, -0.1f, 0.0f};
	s = GiveJointState{};
	s.turn = NiPoint3{0.0f, -0.4f, 0.0f};
	StepJoint(s, light, kGiveNeckMost, 0.01f);
	Check(Near(s.speed.y, 0.4f * kGiveReturnRate * kGiveReturnRate * 0.01f, 0.01f),
	      "a joint swung past its press comes back on the spring");
	for (int i = 0; i < 200; ++i) {
		StepJoint(s, light, kGiveNeckMost, dt);
	}
	Check(Near(s.turn.y, -0.1f, 0.003f) && Near(s.speed.y, 0.0f, 0.02f), "and is caught at the press, not past the middle");
	// Short of the press: eased up to it, the spring off.
	s = GiveJointState{};
	s.turn = NiPoint3{0.0f, -0.05f, 0.0f};
	StepJoint(s, light, kGiveNeckMost, 0.01f);
	Check(Near(s.turn.y, -0.05f - 0.05f * (1.0f - math::Exp(-0.35f)), 0.0005f) && Near(s.speed.y, 0.0f),
	      "a joint short of its press closes a share of the rest, with no speed of its own");
	// A kick that came with the press is not braked while the joint is short.
	GiveDrive both = light;
	both.kick = NiPoint3{0.0f, -6.0f, 0.0f};
	s = GiveJointState{};
	StepJoint(s, both, kGiveNeckMost, 0.01f);
	Check(Near(s.speed.y, -6.0f), "a strike's kick is kept whole while the joint is short of the press");
	// A speed back towards the middle is taken out while short of the press.
	s = GiveJointState{};
	s.turn = NiPoint3{0.0f, -0.05f, 0.0f};
	s.speed = NiPoint3{2.0f, 3.0f, 0.0f};
	StepJoint(s, light, kGiveNeckMost, 0.01f);
	Check(Near(s.speed.y, 0.0f) && Near(s.speed.x, 2.0f), "short of the press, speed back from it is gone, speed across it kept");
	// Two presses that cancel ask for nothing: the spring stays on.
	GiveDrive cancelled;
	cancelled.touchedBy = 3u;
	s = GiveJointState{};
	s.turn = NiPoint3{0.0f, -0.2f, 0.0f};
	StepJoint(s, cancelled, kGiveNeckMost, 0.01f);
	Check(s.speed.y > 0.2f, "touched but asked for nothing, the joint comes back on the spring");

	// The frame's length.
	GiveJointState longFrame;
	GiveJointState capped;
	StepJoint(longFrame, press, kGiveNeckMost, 1.0f);
	StepJoint(capped, press, kGiveNeckMost, kGiveMostStepSeconds);
	Check(NearPoint(longFrame.turn, capped.turn) && NearPoint(longFrame.speed, capped.speed),
	      "a frame of a second is stepped as a thirtieth");
	s = GiveJointState{};
	s.turn = NiPoint3{0.0f, -0.2f, 0.0f};
	Check(StepJoint(s, rest, kGiveNeckMost, 0.0f) && Near(s.turn.y, -0.2f), "a frame of no length moves nothing");
	Check(StepJoint(s, rest, kGiveNeckMost, kNan) && Near(s.turn.y, -0.2f), "nor does one that is not a number");
	Check(StepJoint(s, rest, kGiveNeckMost, -0.01f) && Near(s.turn.y, -0.2f), "nor a negative one");
	s = GiveJointState{};
	Check(!StepJoint(s, rest, kGiveNeckMost, 0.0f), "a joint at rest stays at rest over a frame of no length");
	Check(StepJoint(s, press, kGiveNeckMost, 0.0f) && s.touchedBy == 1u,
	      "a touched one is not at rest, and the touch is remembered");
	s = GiveJointState{};
	s.speed = NiPoint3{1.0f, 0.0f, 0.0f};
	Check(StepJoint(s, rest, kGiveNeckMost, 0.0f), "nor is one still turning");

	Check(Near(GiveMostTurn(kGiveNeck), kGiveNeckMost) && Near(GiveMostTurn(kGiveLeftUpper), kGiveUpperMost) &&
	          Near(GiveMostTurn(kGiveRightUpper), kGiveUpperMost) && Near(GiveMostTurn(kGiveLeftFore), kGiveForeMost) &&
	          Near(GiveMostTurn(kGiveRightFore), kGiveForeMost),
	      "each joint has its own most");
	Check(NearPoint(GiveCapped(NiPoint3{3.0f, 4.0f, 0.0f}, 2.5f), NiPoint3{1.5f, 2.0f, 0.0f}) &&
	          NearPoint(GiveCapped(NiPoint3{0.3f, 0.4f, 0.0f}, 2.5f), NiPoint3{0.3f, 0.4f, 0.0f}),
	      "a vector is cut to its cap, a shorter one kept");
}

void TestSomeone() {
	std::printf("Someone, frame by frame\n");
	const GiveBones b = Standing();
	const float dt = 1.0f / 90.0f;
	GiveState s;
	GivePusher pushers[kGivePushers];
	Check(!StepGive(s, b, pushers, kGivePushers, dt), "nobody touches them: at rest");

	// A hand strikes the head's side at 4 m/s and stays in it.
	pushers[kGivePusherOffHand] = Ball(NiPoint3{8.0f, 0.0f, 110.0f}, NiPoint3{-280.0f, 0.0f, 0.0f});
	Check(StepGive(s, b, pushers, kGivePushers, dt), "a strike on the head: not at rest");
	const float kicked = ik::Length(s.joint[kGiveNeck].speed);
	Check(kicked > 8.0f && kicked <= kGiveMostSpeed + 0.001f, "the neck is kicked");
	Check(s.joint[kGiveNeck].turn.y < 0.0f, "the head tips away from the hand");
	bool othersStill = true;
	for (UInt32 j = kGiveLeftUpper; j < kGiveJointCount; ++j) {
		othersStill = othersStill && s.joint[j].turn.LengthSquared() == 0.0f && s.joint[j].speed.LengthSquared() == 0.0f;
	}
	Check(othersStill, "the arms stay as they are");
	// A slower strike: the kick is given once.
	GiveState slow;
	pushers[kGivePusherOffHand] = Ball(NiPoint3{8.0f, 0.0f, 110.0f}, NiPoint3{-50.0f, 0.0f, 0.0f});
	StepGive(slow, b, pushers, kGivePushers, dt);
	const float once = ik::Length(slow.joint[kGiveNeck].speed);
	StepGive(slow, b, pushers, kGivePushers, dt);
	Check(Near(once, 3.0f, 0.01f) && ik::Length(slow.joint[kGiveNeck].speed) <= once + 0.001f,
	      "the same hand the frame after does not kick again");

	// The hand taken away: back to rest.
	pushers[kGivePusherOffHand] = GivePusher{};
	bool active = true;
	for (int i = 0; i < 360 && active; ++i) {
		active = StepGive(s, b, pushers, kGivePushers, dt);
	}
	Check(!active && s.joint[kGiveNeck].turn.LengthSquared() == 0.0f, "the hand gone, the head comes back to rest");

	Check(PusherNear(pushers, kGivePushers, b.neck, 30.0f) == false, "no pusher: nobody near");
	pushers[kGivePusherBlade] = Ball(NiPoint3{40.0f, 0.0f, 100.0f});
	Check(!PusherNear(pushers, kGivePushers, b.neck, 30.0f), "a blade 40 units off is not within 30 and its 2.5");
	Check(PusherNear(pushers, kGivePushers, b.neck, 38.0f), "but within 38 and its 2.5");
}

void TestRotation() {
	std::printf("The rotation written to the bone\n");
	const NiMatrix33 none = TurnMatrix(kZero);
	Check(Near(none.data[0][0], 1.0f) && Near(none.data[1][1], 1.0f) && Near(none.data[2][2], 1.0f) &&
	          Near(none.data[0][1], 0.0f),
	      "no turn is no rotation");
	const NiMatrix33 nan = TurnMatrix(NiPoint3{kNan, 0.0f, 0.0f});
	Check(Near(nan.data[0][0], 1.0f) && Near(nan.data[1][1], 1.0f), "nor is one that is not a number");
	const NiMatrix33 quarter = TurnMatrix(NiPoint3{0.0f, 0.0f, 0.5f * math::kPi});
	Check(NearPoint(quarter * NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}),
	      "a quarter turn about z carries x to y");

	const NiMatrix33 parent = ik::TurnAbout(NiPoint3{0.0f, 0.0f, 1.0f}, 0.3f);
	const NiMatrix33 bone = ik::TurnAbout(NiPoint3{1.0f, 0.0f, 0.0f}, 0.5f);
	const NiPoint3 turn{0.0f, -0.2f, 0.1f};
	const NiMatrix33 local = TurnedLocalRot(parent, bone, turn);
	const NiMatrix33 world = parent * local;
	const NiMatrix33 wanted = TurnMatrix(turn) * bone;
	bool same = true;
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			same = same && Near(world.data[r][c], wanted.data[r][c]);
		}
	}
	Check(same, "under its parent the local rotation gives the bone's world rotation, turned");
	const NiMatrix33 unturned = parent * TurnedLocalRot(parent, bone, kZero);
	same = true;
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			same = same && Near(unturned.data[r][c], bone.data[r][c]);
		}
	}
	Check(same, "no turn gives the bone's own rotation back");
}

void TestPusher() {
	std::printf("A pusher from frame to frame\n");
	GivePusherMemory m;
	const NiPoint3 a{1.0f, 2.0f, 3.0f};
	const NiPoint3 b{1.0f, 12.0f, 3.0f};
	GivePusher p = PusherFrom(m, false, a, b, 2.5f, 0.01f);
	Check(!p.valid && !m.had, "one not there is not valid and leaves no memory");
	p = PusherFrom(m, true, a, b, 2.5f, 0.01f);
	Check(p.valid && NearPoint(p.a, a) && NearPoint(p.b, b) && Near(p.radius, 2.5f), "one there is taken as it is");
	Check(NearPoint(p.velA, kZero) && NearPoint(p.velB, kZero) && m.had, "its first frame has no speed");
	p = PusherFrom(m, true, a + NiPoint3{1.0f, 0.0f, 0.0f}, b + NiPoint3{0.0f, 0.0f, 2.0f}, 2.5f, 0.01f);
	Check(NearPoint(p.velA, NiPoint3{100.0f, 0.0f, 0.0f}) && NearPoint(p.velB, NiPoint3{0.0f, 0.0f, 200.0f}),
	      "the frame after, each end has its own speed");
	p = PusherFrom(m, true, a + NiPoint3{150.0f, 0.0f, 0.0f}, b + NiPoint3{150.0f, 0.0f, 0.0f}, 2.5f, 0.01f);
	Check(p.valid && NearPoint(p.velA, kZero) && NearPoint(p.velB, kZero), "a jump of 150 units is no speed");
	p = PusherFrom(m, true, a + NiPoint3{151.0f, 0.0f, 0.0f}, b + NiPoint3{151.0f, 0.0f, 0.0f}, 2.5f, 0.0f);
	Check(p.valid && NearPoint(p.velA, kZero), "nor is a frame of no length");
	p = PusherFrom(m, true, a + NiPoint3{152.0f, 0.0f, 0.0f}, b + NiPoint3{152.0f, 0.0f, 0.0f}, 2.5f, kNan);
	Check(p.valid && NearPoint(p.velA, kZero), "nor one that is not a number");
	p = PusherFrom(m, true, NiPoint3{kNan, 0.0f, 0.0f}, b, 2.5f, 0.01f);
	Check(!p.valid && !m.had, "a place that is not a number is not valid and forgets the last");
	p = PusherFrom(m, true, a, b, 0.0f, 0.01f);
	Check(!p.valid && !m.had, "nor one without a radius");
	PusherFrom(m, true, a, b, 2.5f, 0.01f);
	PusherFrom(m, false, a, b, 2.5f, 0.01f);
	p = PusherFrom(m, true, a + NiPoint3{1.0f, 0.0f, 0.0f}, b, 2.5f, 0.01f);
	Check(NearPoint(p.velA, kZero), "one that was gone a frame starts without speed");
}

void TestPushersNow() {
	std::printf("Which pushers there are\n");
	GivePushersNow n = PushersNow(true, true, true, true, false, false);
	Check(n.hand[0] && n.hand[1] && !n.blade, "both hands tracked and free: two hands, no blade");
	n = PushersNow(false, false, true, true, true, true);
	Check(!n.hand[0] && !n.hand[1] && !n.blade, "nothing tracked: no pusher");
	n = PushersNow(true, true, false, false, false, false);
	Check(!n.hand[0] && !n.hand[1] && !n.blade, "both hands busy and nothing drawn: no pusher");
	n = PushersNow(true, true, false, true, true, true);
	Check(!n.hand[0] && n.hand[1] && n.blade, "a sword drawn: the blade and the other hand");
	n = PushersNow(true, false, false, true, true, true);
	Check(!n.hand[1] && n.blade, "the other hand not tracked: the blade alone");
	n = PushersNow(false, true, false, true, true, true);
	Check(n.hand[1] && !n.blade, "the weapon hand not tracked: no blade");
	n = PushersNow(true, true, false, true, false, true);
	Check(!n.blade, "the weapon hand busy with something that is no swung weapon: no blade");
	n = PushersNow(true, true, false, true, true, false);
	Check(!n.blade, "a sword whose span was not read yet: no blade");
	n = PushersNow(true, true, true, true, true, true);
	Check(n.hand[0] && !n.blade, "the weapon hand meeting the world as a hand: the hand, not the blade");
}

}  // namespace

int main() {
	TestParts();
	TestHeadJoint();
	TestTouch();
	TestTurnFor();
	TestDrives();
	TestJoint();
	TestSomeone();
	TestRotation();
	TestPusher();
	TestPushersNow();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

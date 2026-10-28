// Checks the spell from the casting hand (vr/SpellAim.h, game/SpellReachLogic.h):
// the hand raised - ahead and high enough, too low, too close, behind, a view
// straight up or down, numbers that are not numbers; the aim along a hand's
// ray - ahead, turned either way, up and down, a ray straight up; the step -
// switched off, not Full VR, third person, a menu, the hand lost, no target
// spell, raised without a cast, a cast without the hand raised, a touch or
// self spell cast; and the walk of a spell's effects for one at range Target
// - none, the first, a later one, an unreadable effect, a list that loops.

#include <cstdio>
#include <limits>

#include "game/SpellReachLogic.h"
#include "vr/SpellAim.h"

namespace {

using namespace obvr::vr;
using obvr::NiPoint3;
using ::UInt32;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();
const NiPoint3 kAhead{0.0f, 1.0f, 0.0f};

void TestRaised() {
	std::printf("The casting hand raised\n");
	const float below = 0.45f;
	const float ahead = 0.15f;
	Check(CastingHandRaised(NiPoint3{0.1f, 0.4f, -0.25f}, kAhead, below, ahead),
	      "held out ahead at chest height: raised");
	Check(!CastingHandRaised(NiPoint3{0.2f, 0.1f, -0.65f}, kAhead, below, ahead),
	      "hanging at the side: not raised");
	Check(!CastingHandRaised(NiPoint3{0.0f, 0.4f, -0.60f}, kAhead, below, ahead),
	      "ahead but low at the belly: not raised");
	Check(!CastingHandRaised(NiPoint3{0.0f, 0.10f, -0.10f}, kAhead, below, ahead),
	      "high but at the face: not far enough ahead");
	Check(!CastingHandRaised(NiPoint3{0.0f, -0.4f, -0.20f}, kAhead, below, ahead),
	      "behind the head: not raised");
	Check(CastingHandRaised(NiPoint3{0.0f, 0.4f, -0.45f}, kAhead, below, ahead),
	      "exactly at the height's limit: raised");
	Check(CastingHandRaised(NiPoint3{0.0f, 0.4f, 0.30f}, kAhead, below, ahead),
	      "held over the head and ahead: raised");
	// The view is levelled: looking down at the hand does not take "ahead" away.
	Check(CastingHandRaised(NiPoint3{0.0f, 0.4f, -0.25f}, NiPoint3{0.0f, 0.5f, -0.866f}, below, ahead),
	      "looking down at it: ahead along the levelled view, raised");
	Check(CastingHandRaised(NiPoint3{0.4f, 0.0f, -0.25f}, NiPoint3{1.0f, 0.0f, 0.0f}, below, ahead),
	      "the view turned right, the hand out to the right: raised");
	Check(!CastingHandRaised(NiPoint3{0.0f, 0.4f, -0.25f}, NiPoint3{0.0f, 0.0f, 1.0f}, below, ahead),
	      "a view straight up has no level ahead: not raised");
	Check(!CastingHandRaised(NiPoint3{kNaN, 0.4f, -0.25f}, kAhead, below, ahead), "a hand that is not a number");
	Check(!CastingHandRaised(NiPoint3{0.0f, 0.4f, -0.25f}, NiPoint3{kNaN, 1.0f, 0.0f}, below, ahead),
	      "a view that is not a number");
	Check(!CastingHandRaised(NiPoint3{0.0f, 0.4f, -0.25f}, kAhead, kNaN, ahead), "a height limit that is not a number");
}

void TestAim() {
	std::printf("The aim along the hand's ray\n");
	float yaw = 9.0f;
	float sinPitch = 9.0f;
	Check(AimAlong(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, yaw, sinPitch) && Near(yaw, 0.0f) &&
	          Near(sinPitch, 0.0f),
	      "straight ahead: no turn, level");
	Check(AimAlong(NiPoint3{1.0f, 1.0f, 0.0f}, NiPoint3{1.0f, 1.0f, 0.0f}, yaw, sinPitch) &&
	          Near(yaw, -0.7854f),
	      "to the right: a turn of -45 degrees, as the grab's reach turns (atan2(-x, y))");
	Check(AimAlong(NiPoint3{-1.0f, 1.0f, 0.0f}, NiPoint3{-1.0f, 1.0f, 0.0f}, yaw, sinPitch) &&
	          Near(yaw, 0.7854f),
	      "to the left: +45 degrees");
	Check(AimAlong(NiPoint3{0.0f, 1.0f, 1.0f}, NiPoint3{0.0f, 1.0f, 1.0f}, yaw, sinPitch) &&
	          Near(sinPitch, 0.7071f),
	      "up 45 degrees: the sine of the world's pitch, positive up");
	Check(AimAlong(NiPoint3{0.0f, 2.0f, 0.0f}, NiPoint3{0.0f, 3.0f, -3.0f}, yaw, sinPitch) &&
	          Near(sinPitch, -0.7071f),
	      "the pitch from the world's ray, not the head's (a head looking down)");
	Check(!AimAlong(NiPoint3{0.0f, 0.0f, 1.0f}, NiPoint3{0.0f, 0.0f, 1.0f}, yaw, sinPitch),
	      "straight up along the head: no heading, refused");
	Check(!AimAlong(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, yaw, sinPitch),
	      "a world ray of no length: refused");
	Check(!AimAlong(NiPoint3{kNaN, 1.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, yaw, sinPitch), "not a number: refused");
}

SpellAimInput Ready() {
	SpellAimInput in;
	in.fullVr = true;
	in.firstPerson = true;
	in.menuUp = false;
	in.castingHandValid = true;
	in.reach = SpellReach::Target;
	in.castActive = false;
	in.raised = true;
	return in;
}

void TestStep() {
	std::printf("The step\n");
	SpellAimSettings on;
	on.enabled = true;

	SpellAimVerdict v = StepSpellAim(Ready(), on);
	Check(v.crosshair && !v.aim && !v.origin, "a target spell, the hand raised, no cast yet: the crosshair only");

	SpellAimInput casting = Ready();
	casting.castActive = true;
	v = StepSpellAim(casting, on);
	Check(v.crosshair && v.aim && v.origin, "and casting: the crosshair, the aim and the origin at the hand");

	casting.raised = false;
	v = StepSpellAim(casting, on);
	Check(v.crosshair && v.aim && v.origin, "a cast with the hand not raised still aims along it and shows where");

	SpellAimInput lowered = Ready();
	lowered.raised = false;
	v = StepSpellAim(lowered, on);
	Check(!v.crosshair && !v.aim && !v.origin, "the hand down, no cast: nothing");

	SpellAimInput touch = Ready();
	touch.reach = SpellReach::Touch;
	touch.castActive = true;
	v = StepSpellAim(touch, on);
	Check(!v.crosshair && v.aim && v.touchReach && v.origin,
	      "a touch spell cast (Nadi: \"das ziel an der hand ... direkt nach der hand\"): no crosshair, aimed at "
	      "what is just past the hand, from the hand");
	touch.raised = false;
	v = StepSpellAim(touch, on);
	Check(!v.crosshair && v.aim && v.touchReach, "the hand not raised: the same");
	touch.castActive = false;
	touch.raised = true;
	v = StepSpellAim(touch, on);
	Check(!v.crosshair && !v.aim && !v.origin, "a touch spell ready, no cast: nothing, not even raised");
	touch.castActive = true;
	touch.reach = SpellReach::Self;
	v = StepSpellAim(touch, on);
	Check(!v.crosshair && !v.aim && !v.touchReach && v.origin, "a self spell: the gaze's aim, from the hand");
	touch.reach = SpellReach::Unknown;
	v = StepSpellAim(touch, on);
	Check(!v.crosshair && !v.aim && v.origin, "an unreadable spell the same");
	v = StepSpellAim(casting, on);
	Check(!v.touchReach, "a target spell aims along the laser, not at the reach");

	SpellAimSettings off;
	off.enabled = false;
	v = StepSpellAim(casting, off);
	Check(!v.crosshair && !v.aim && !v.origin, "switched off: nothing");

	SpellAimInput flat = casting;
	flat.fullVr = false;
	v = StepSpellAim(flat, on);
	Check(!v.crosshair && !v.aim && !v.origin, "not Full VR: nothing");

	SpellAimInput third = casting;
	third.firstPerson = false;
	v = StepSpellAim(third, on);
	Check(!v.crosshair && !v.aim && !v.origin, "third person: nothing");

	SpellAimInput menu = casting;
	menu.menuUp = true;
	v = StepSpellAim(menu, on);
	Check(!v.crosshair && !v.aim && !v.origin, "a menu up: nothing");

	SpellAimInput lost = casting;
	lost.castingHandValid = false;
	v = StepSpellAim(lost, on);
	Check(!v.crosshair && !v.aim && !v.origin, "the casting hand not tracked: nothing");
}

void TestDepth() {
	std::printf("The crosshair's depth along an aim ray\n");
	// The ray from 0.3 m ahead of the hand, 60 m long.
	Check(Near(AimRayDepthMetres(true, true, 0.25f, 0.3f, 60.0f, 3.0f, 0.5f), 15.3f),
	      "a hit a quarter along: its distance from the ray's start plus the start");
	Check(Near(AimRayDepthMetres(true, false, 1.0f, 0.3f, 60.0f, 3.0f, 0.5f), 3.0f), "nothing hit: the fallback");
	Check(Near(AimRayDepthMetres(false, true, 0.25f, 0.3f, 60.0f, 3.0f, 0.5f), 3.0f),
	      "the world not asked: the fallback");
	Check(Near(AimRayDepthMetres(true, true, 0.0f, 0.1f, 60.0f, 3.0f, 0.5f), 0.5f),
	      "a wall at the hand: no nearer than the least depth");
	Check(Near(AimRayDepthMetres(true, true, kNaN, 0.3f, 60.0f, 3.0f, 0.5f), 3.0f), "a fraction not a number");
	Check(Near(AimRayDepthMetres(true, true, 1.5f, 0.3f, 60.0f, 3.0f, 0.5f), 3.0f), "a fraction past the end");
	Check(Near(AimRayDepthMetres(true, true, -0.1f, 0.3f, 60.0f, 3.0f, 0.5f), 3.0f), "a fraction before the start");
}

void TestTouchPoint() {
	std::printf("Where a touch spell is aimed\n");
	const NiPoint3 p = TouchReachPoint(NiPoint3{10.0f, 20.0f, 30.0f}, NiPoint3{0.0f, 2.0f, 0.0f}, 14.0f);
	Check(Near(p.x, 10.0f) && Near(p.y, 34.0f) && Near(p.z, 30.0f),
	      "the palm plus the reach along the hand's ray, the ray's length taken out");
	const NiPoint3 q = TouchReachPoint(NiPoint3{1.0f, 2.0f, 3.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 14.0f);
	Check(Near(q.x, 1.0f) && Near(q.y, 2.0f) && Near(q.z, 3.0f), "a ray of no length: the palm itself");
}

// A small memory of words for the walk, by address.
struct FakeMemory {
	UInt32 address[32];
	UInt32 value[32];
	int count = 0;
	void Set(UInt32 a, UInt32 v) {
		address[count] = a;
		value[count] = v;
		++count;
	}
	UInt32 operator()(UInt32 a) const {
		for (int i = 0; i < count; ++i) {
			if (address[i] == a) {
				return value[i];
			}
		}
		return 0;
	}
};

bool Plausible(UInt32 a) { return a >= 0x1000; }

void TestReach() {
	std::printf("The spell's reach from its effects\n");
	using obvr::game::SpellReachOf;
	const UInt32 item = 0x2000;
	// The list's head is embedded at item+0x0C: its effect at +0x10 (item+0x0C+4)
	// and its next node at +0x14; a further node's effect at its +0 and next at
	// its +4 (the engine's walk at 0x0069A060: data at node+4, next-4).
	FakeMemory none;
	Check(SpellReachOf(item, none, Plausible, 8) == SpellReach::Unknown, "no effect at all: unknown");

	FakeMemory self;
	self.Set(item + 0x10, 0x3000);
	self.Set(0x3000 + 0x10, 0);
	Check(SpellReachOf(item, self, Plausible, 8) == SpellReach::Self, "one effect on self: self");

	FakeMemory target;
	target.Set(item + 0x10, 0x3000);
	target.Set(0x3000 + 0x10, 2);
	Check(SpellReachOf(item, target, Plausible, 8) == SpellReach::Target, "one effect at a target: target");

	FakeMemory later;
	later.Set(item + 0x10, 0x3000);
	later.Set(0x3000 + 0x10, 1);
	later.Set(item + 0x14, 0x4000);
	later.Set(0x4000, 0x5000);
	later.Set(0x5000 + 0x10, 2);
	Check(SpellReachOf(item, later, Plausible, 8) == SpellReach::Target,
	      "touch first, a target effect second: target (the widest)");

	FakeMemory touchOnly;
	touchOnly.Set(item + 0x10, 0x3000);
	touchOnly.Set(0x3000 + 0x10, 1);
	touchOnly.Set(item + 0x14, 0x4000);
	touchOnly.Set(0x4000, 0x5000);
	touchOnly.Set(0x5000 + 0x10, 0);
	Check(SpellReachOf(item, touchOnly, Plausible, 8) == SpellReach::Touch, "touch and self: touch");

	FakeMemory odd;
	odd.Set(item + 0x10, 0x3000);
	odd.Set(0x3000 + 0x10, 7);
	Check(SpellReachOf(item, odd, Plausible, 8) == SpellReach::Unknown, "a range that is no range: unknown");

	FakeMemory badEffect;
	badEffect.Set(item + 0x10, 0x10);
	Check(SpellReachOf(item, badEffect, Plausible, 8) == SpellReach::Unknown, "an effect pointer that is no object");

	FakeMemory loop;
	loop.Set(item + 0x10, 0x3000);
	loop.Set(0x3000 + 0x10, 1);
	loop.Set(item + 0x14, 0x4000);
	loop.Set(0x4000, 0x3000);
	loop.Set(0x4004, 0x4000);  // the node points at itself
	Check(SpellReachOf(item, loop, Plausible, 8) == SpellReach::Touch, "a list that loops ends after the limit");

	Check(SpellReachOf(0, target, Plausible, 8) == SpellReach::Unknown, "no item: unknown");
}

}  // namespace

int main() {
	TestRaised();
	TestAim();
	TestStep();
	TestDepth();
	TestTouchPoint();
	TestReach();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}

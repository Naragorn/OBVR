// Checks the off hand in a fight (game/GrappleLogic.h): its strike with a
// weapon drawn, where its grip takes hold, which holds stop a blow, how a
// hold is let go, thrown, broken or lost, and the drag - every flow of
// StepGrapple. And the body parts the capsules carry (BladeContactLogic.h).

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/GrappleLogic.h"

namespace {

using namespace obvr::game;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 1e-3f) { return std::fabs(a - b) <= within; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

ShoveHand Hand(float towards, bool valid = true, bool grip = false, bool open = false) {
	ShoveHand h;
	h.valid = valid;
	h.gripHeld = grip;
	h.open = open;
	h.towardsSpeed = towards;
	return h;
}

void TestStrike() {
	std::printf("The off hand's strike\n");
	const GrappleSettings g;
	const ShoveSettings s;  // 1.3 and 3.2 m/s
	Check(OffHandStrikeFor(g, s, true, true, Hand(2.0f)) == ShoveKind::Light, "a fist at 2 m/s, a sword drawn: light");
	Check(OffHandStrikeFor(g, s, true, true, Hand(2.0f, true, false, true)) == ShoveKind::Light,
	      "an open hand the same: light");
	Check(OffHandStrikeFor(g, s, true, true, Hand(3.5f)) == ShoveKind::Hard, "at 3.5 m/s: knocked down");
	Check(OffHandStrikeFor(g, s, true, true, Hand(1.3f)) == ShoveKind::Light, "exactly ShoveSpeed: light");
	Check(OffHandStrikeFor(g, s, true, true, Hand(1.0f)) == ShoveKind::None, "too slow: nothing");
	Check(OffHandStrikeFor(g, s, true, true, Hand(kNaN)) == ShoveKind::None, "a speed that is not a number: nothing");
	Check(OffHandStrikeFor(g, s, false, true, Hand(3.5f)) == ShoveKind::None,
	      "no blade or blunt weapon drawn: the hands' own (shove, fists)");
	Check(OffHandStrikeFor(g, s, true, false, Hand(3.5f)) == ShoveKind::None, "the off hand on a two-hander: nothing");
	Check(OffHandStrikeFor(g, s, true, true, Hand(3.5f, false)) == ShoveKind::None, "the hand not tracked: nothing");
	// A fist squeezes the grip (the Index): the grip held is no refusal of
	// itself (the tester, 2026-10-14: "linke faust trifft nicht") - only a
	// hand that holds someone, or whose grip is closing this frame (a grab,
	// not a punch).
	Check(OffHandStrikeFor(g, s, true, true, Hand(3.5f, true, true)) == ShoveKind::Hard,
	      "the grip held on nobody - a fist: the punch, hard");
	Check(OffHandStrikeFor(g, s, true, true, Hand(2.0f, true, true), true) == ShoveKind::None,
	      "the hand holds someone: nothing");
	Check(OffHandStrikeFor(g, s, true, true, Hand(2.0f, true, true), false, true) == ShoveKind::None,
	      "the grip closing this frame - a grab: nothing");
	GrappleSettings off;
	off.strikes = false;
	Check(OffHandStrikeFor(off, s, true, true, Hand(3.5f)) == ShoveKind::None, "OffHandStrikes=0: nothing");
}

BladeBodies TwoPeople() {
	BladeBodies b;
	// One standing at x 100: a torso from 40 to 100 up, radius 10; a head ball
	// over it; a right arm off to the side.
	b.Add(NiPoint3{100.0f, 0.0f, 40.0f}, NiPoint3{100.0f, 0.0f, 100.0f}, 10.0f, 0xA, BodyPart::Torso);
	b.Add(NiPoint3{100.0f, 0.0f, 110.0f}, NiPoint3{100.0f, 0.0f, 119.0f}, 7.0f, 0xA, BodyPart::Head);
	b.Add(NiPoint3{100.0f, -20.0f, 95.0f}, NiPoint3{100.0f, -20.0f, 70.0f}, 4.5f, 0xA, BodyPart::RightArm);
	// Another at x 300, read without bones.
	b.Add(NiPoint3{300.0f, 0.0f, 10.0f}, NiPoint3{300.0f, 0.0f, 110.0f}, 20.0f, 0xB);
	return b;
}

void TestTake() {
	std::printf("Where the grip takes hold\n");
	const BladeBodies b = TwoPeople();
	GrappleTake t = GrappleTakeAt(b, NiPoint3{88.0f, 0.0f, 70.0f}, kGrappleReachUnits);
	Check(t.found && t.actor == 0xA && t.part == BodyPart::Torso && Near(t.gap, 2.0f), "2 units off the chest: the torso");
	t = GrappleTakeAt(b, NiPoint3{100.0f, 0.0f, 70.0f}, kGrappleReachUnits);
	Check(t.found && t.part == BodyPart::Torso && t.gap < 0.0f, "inside the chest: the torso, a negative gap");
	t = GrappleTakeAt(b, NiPoint3{100.0f, 0.0f, 128.0f}, kGrappleReachUnits);
	Check(t.found && t.part == BodyPart::Head, "over the head ball: the head");
	t = GrappleTakeAt(b, NiPoint3{100.0f, -24.0f, 80.0f}, kGrappleReachUnits);
	Check(t.found && t.part == BodyPart::RightArm, "at the arm, nearer it than the chest: the right arm");
	t = GrappleTakeAt(b, NiPoint3{82.0f, 0.0f, 70.0f}, kGrappleReachUnits);
	Check(!t.found, "8 units off the chest: out of reach, not taken");
	t = GrappleTakeAt(b, NiPoint3{84.0f, 0.0f, 70.0f}, kGrappleReachUnits);
	Check(t.found && Near(t.gap, 6.0f), "exactly 6 units off: taken");
	t = GrappleTakeAt(b, NiPoint3{277.0f, 0.0f, 60.0f}, kGrappleReachUnits);
	Check(t.found && t.actor == 0xB && t.part == BodyPart::Column, "one read without bones: their column");
	t = GrappleTakeAt(b, NiPoint3{200.0f, 0.0f, 60.0f}, kGrappleReachUnits);
	Check(!t.found, "between them, near neither: nothing");
	t = GrappleTakeAt(BladeBodies{}, NiPoint3{0.0f, 0.0f, 0.0f}, kGrappleReachUnits);
	Check(!t.found, "nobody near: nothing");
	t = GrappleTakeAt(b, NiPoint3{kNaN, 0.0f, 0.0f}, kGrappleReachUnits);
	Check(!t.found, "a hand that is not a number: nothing");
}

void TestParts() {
	std::printf("Which holds stop a blow\n");
	Check(GrappleStopsBlows(BodyPart::Head), "the head");
	Check(GrappleStopsBlows(BodyPart::Neck), "the neck");
	Check(GrappleStopsBlows(BodyPart::RightArm), "the weapon arm");
	Check(!GrappleStopsBlows(BodyPart::LeftArm), "not the shield arm");
	Check(!GrappleStopsBlows(BodyPart::Torso), "not the torso");
	Check(!GrappleStopsBlows(BodyPart::LeftLeg) && !GrappleStopsBlows(BodyPart::RightLeg), "not the legs");
	Check(!GrappleStopsBlows(BodyPart::Column), "not a body read without bones");

	std::printf("The parts the capsules carry\n");
	NiPoint3 bones[kBladeBoneCount];
	bool have[kBladeBoneCount];
	for (UInt32 i = 0; i < kBladeBoneCount; ++i) {
		bones[i] = NiPoint3{0.0f, 0.0f, static_cast<float>(i) * 5.0f};
		have[i] = true;
	}
	BladeBodies out;
	const UInt32 added = BodyCapsulesFromBones(bones, have, 1.0f, 0xC, out);
	UInt32 heads = 0, necks = 0, torsos = 0, lefts = 0, rights = 0, legs = 0, columns = 0;
	for (UInt32 i = 0; i < out.count; ++i) {
		switch (out.cap[i].part) {
		case BodyPart::Head: ++heads; break;
		case BodyPart::Neck: ++necks; break;
		case BodyPart::Torso: ++torsos; break;
		case BodyPart::LeftArm: ++lefts; break;
		case BodyPart::RightArm: ++rights; break;
		case BodyPart::LeftLeg:
		case BodyPart::RightLeg: ++legs; break;
		default: ++columns; break;
		}
	}
	Check(added == 13 && heads == 1 && necks == 1 && torsos == 3 && lefts == 2 && rights == 2 && legs == 4 &&
	          columns == 0,
	      "a whole skeleton: a head, a neck, three torso pieces, two per arm, four leg pieces");
	BladeBodies col;
	BodyColumnFromBound(NiPoint3{0.0f, 0.0f, 60.0f}, 60.0f, 0xD, col);
	Check(col.count == 1 && col.cap[0].part == BodyPart::Column, "a bound's column: the column");
	Check(BodyPartName(BodyPart::RightArm)[0] == 'r' && BodyPartName(BodyPart::Column)[0] == 'b', "the names");
}

void TestRelease() {
	std::printf("How a grip lets go\n");
	const ShoveSettings s;
	Check(GrappleReleaseFor(s, 0.5f) == GrappleEnd::LetGo, "a slow hand: let go");
	Check(GrappleReleaseFor(s, 1.3f) == GrappleEnd::Thrown, "at ShoveSpeed: thrown, staggered");
	Check(GrappleReleaseFor(s, 3.2f) == GrappleEnd::ThrownDown, "at ShoveHardSpeed: thrown down");
	Check(GrappleReleaseFor(s, kNaN) == GrappleEnd::LetGo, "a speed that is not a number: let go");
	Check(Near(GrappleHoldSeconds(4.0f), 4.0f) && Near(GrappleHoldSeconds(0.5f), 0.5f) &&
	          Near(GrappleHoldSeconds(30.0f), 30.0f),
	      "the hold time: as set within 0.5 to 30 s");
	Check(Near(GrappleHoldSeconds(0.1f), 4.0f) && Near(GrappleHoldSeconds(100.0f), 4.0f) &&
	          Near(GrappleHoldSeconds(kNaN), 4.0f),
	      "out of reason: the default 4 s");
	const NiPoint3 from = GrappleThrowFrom(NiPoint3{100.0f, 50.0f, 60.0f}, NiPoint3{1.0f, 0.0f, 0.0f});
	Check(Near(from.x, 70.0f) && Near(from.y, 50.0f) && Near(from.z, 60.0f),
	      "a throw along +x pushes from 30 units behind them");
	const NiPoint3 still = GrappleThrowFrom(NiPoint3{100.0f, 50.0f, 60.0f}, NiPoint3{0.0f, 0.0f, 0.0f});
	Check(Near(still.x, 100.0f) && Near(still.y, 50.0f), "no motion: their centre");
	Check(GrappleEndName(GrappleEnd::ThrownDown)[0] == 't' && GrappleEndName(GrappleEnd::None)[0] == 'h', "the names");
}

GrappleInput Ready(bool grip) {
	GrappleInput in;
	in.allowed = true;
	in.handValid = true;
	in.grip = grip;
	in.hand = NiPoint3{60.0f, 0.0f, 100.0f};
	in.dtSeconds = 0.02f;
	return in;
}

GrappleInput Taking() {
	GrappleInput in = Ready(true);
	in.take.found = true;
	in.take.actor = 0xA;
	in.take.part = BodyPart::Head;
	in.takeFights = true;
	in.takeAt = NiPoint3{100.0f, 0.0f, 0.0f};
	return in;
}

GrappleInput Holding(const NiPoint3& hand, const NiPoint3& heldAt) {
	GrappleInput in = Ready(true);
	in.hand = hand;
	in.heldReadable = true;
	in.heldAt = heldAt;
	return in;
}

void TestTakeFlows() {
	std::printf("Taking hold\n");
	const GrappleSettings g;
	const ShoveSettings s;
	GrappleState st;
	GrappleVerdict v = StepGrapple(st, g, s, Ready(false));
	Check(!v.started && v.actor == 0 && st.actor == 0, "no grip: nothing");

	st = GrappleState{};
	st.gripWas = true;
	st.closedFor = 0.5f;
	v = StepGrapple(st, g, s, Taking());
	Check(!v.started && st.actor == 0, "a grip closed half a second before the hand got there: nothing");
	st = GrappleState{};
	st.gripWas = true;
	st.closedFor = 0.2f;
	v = StepGrapple(st, g, s, Taking());
	Check(v.started && st.actor == 0xA, "closed a fifth of a second before (a lunge squeezes first): held");
	st = GrappleState{};
	st.gripWas = true;
	st.closedFor = 0.2f;
	GrappleInput later = Taking();
	later.dtSeconds = 0.15f;
	v = StepGrapple(st, g, s, later);
	Check(!v.started && Near(st.closedFor, 0.35f), "the grace runs out as the frames pass");
	st = GrappleState{};
	st.gripWas = true;
	st.closedFor = 2.0f;
	GrappleInput opened = Taking();
	opened.grip = false;
	v = StepGrapple(st, g, s, opened);
	Check(!v.started && st.closedFor == 0.0f, "the grip opened: the count starts over");

	st = GrappleState{};
	GrappleInput in = Taking();
	in.allowed = false;
	v = StepGrapple(st, g, s, in);
	Check(!v.started && !v.refused && st.actor == 0, "not allowed (a shield, a handle, a menu): nothing");

	st = GrappleState{};
	in = Taking();
	in.handValid = false;
	v = StepGrapple(st, g, s, in);
	Check(!v.started && st.actor == 0, "the hand not tracked: nothing");

	st = GrappleState{};
	in = Taking();
	in.take.found = false;
	v = StepGrapple(st, g, s, in);
	Check(!v.started && !v.refused && st.actor == 0, "nobody at the hand: nothing");

	st = GrappleState{};
	GrappleSettings off;
	off.grabs = false;
	v = StepGrapple(st, off, s, Taking());
	Check(!v.started && st.actor == 0, "OffHandGrabs=0: nothing");

	st = GrappleState{};
	in = Taking();
	in.takeFights = false;
	v = StepGrapple(st, g, s, in);
	Check(v.refused && !v.started && v.actor == 0xA && st.actor == 0, "someone not fighting: refused, not held");

	st = GrappleState{};
	v = StepGrapple(st, g, s, Taking());
	Check(v.started && v.actor == 0xA && v.part == BodyPart::Head && st.actor == 0xA, "a fighter by the head: held");
	Check(Near(st.offset.x, 40.0f) && Near(st.offset.y, 0.0f) && Near(st.offset.z, 0.0f),
	      "where they stood from the hand, across the ground, kept");
}

void TestHoldFlows() {
	std::printf("Holding\n");
	const GrappleSettings g;
	const ShoveSettings s;
	GrappleState st;
	StepGrapple(st, g, s, Taking());

	GrappleVerdict v = StepGrapple(st, g, s, Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f}));
	Check(v.end == GrappleEnd::None && Near(v.gap, 0.0f) && Near(v.drag.x, 0.0f) && Near(v.drag.y, 0.0f),
	      "the hand still: no drag");
	Check(Near(v.seconds, 0.02f), "the hold's time counted");

	GrappleInput in = Holding(NiPoint3{50.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.dtSeconds = 0.1f;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::None && Near(v.drag.x, -10.0f) && Near(v.drag.y, 0.0f) && Near(v.drag.z, 0.0f),
	      "the hand 10 units back: placed 10 units back (under the 14 a tenth of a second allows)");

	in = Holding(NiPoint3{30.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.dtSeconds = 0.02f;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::None && Near(v.drag.x, -2.8f) && Near(v.gap, 30.0f),
	      "30 units back in one 50 Hz frame: at most 2 m/s, 2.8 units of the way");

	in = Holding(NiPoint3{60.0f, 0.0f, 400.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::None && Near(v.gap, 0.0f), "the hand lifted: across the ground only, no drag");

	in = Holding(NiPoint3{10.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Lost && v.actor == 0xA && st.actor == 0, "the hand 50 units from where it wants them: lost");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in = Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.dtSeconds = 3.9f;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::None, "3.9 s: still held");
	in.dtSeconds = 0.2f;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::BrokeFree && Near(v.seconds, 4.1f) && st.actor == 0, "past 4 s: broke free");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in = Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.heldReadable = false;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Gone && st.actor == 0, "dead, down or out of reach: gone");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in = Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.allowed = false;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Gone, "a menu, a shield, the mode off: gone");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in = Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.handValid = false;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Gone, "the hand lost by the tracking: gone");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	GrappleSettings off;
	off.grabs = false;
	v = StepGrapple(st, off, s, Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f}));
	Check(v.end == GrappleEnd::Gone, "the setting switched off while held: gone");
}

void TestLetGoFlows() {
	std::printf("Letting go\n");
	const GrappleSettings g;
	const ShoveSettings s;
	GrappleState st;
	StepGrapple(st, g, s, Taking());
	GrappleInput in = Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.grip = false;
	in.handVelocity = NiPoint3{0.3f, 0.0f, 0.5f};
	GrappleVerdict v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::LetGo && Near(v.speed, 0.3f) && st.actor == 0,
	      "opened slowly: let go (the hand's lift not counted)");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in.handVelocity = NiPoint3{0.0f, 2.0f, 0.0f};
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Thrown && Near(v.direction.y, 1.0f) && Near(v.direction.x, 0.0f),
	      "opened at 2 m/s: thrown along the hand");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in.handVelocity = NiPoint3{-3.0f, -3.0f, 0.0f};
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::ThrownDown && Near(v.speed, 4.243f) && Near(v.direction.x, -0.7071f) &&
	          Near(v.direction.y, -0.7071f),
	      "opened at 4.2 m/s: thrown down along it");

	std::printf("Again after an end\n");
	in = Ready(false);
	v = StepGrapple(st, g, s, in);
	Check(v.actor == 0 && !v.started, "the grip open, nobody held: nothing");
	v = StepGrapple(st, g, s, Taking());
	Check(v.started && st.actor == 0xA, "the grip closing again: held again");

	st = GrappleState{};
	StepGrapple(st, g, s, Taking());
	in = Holding(NiPoint3{60.0f, 0.0f, 100.0f}, NiPoint3{100.0f, 0.0f, 0.0f});
	in.heldReadable = false;
	StepGrapple(st, g, s, in);
	GrappleInput still = Taking();  // the grip still down after a loss
	v = StepGrapple(st, g, s, still);
	Check(!v.started && st.actor == 0, "the grip kept closed after a loss: not taken again until it closes anew");
}

// ------------------------------------------------------------ the ragdoll

GrappleInput TakingRagdoll(BodyPart part = BodyPart::Torso, bool standing = true, bool down = false,
                           bool body = true) {
	GrappleInput in = Taking();
	in.take.part = part;
	in.takeStanding = standing;
	in.takeDown = down;
	in.takeBody = body;
	return in;
}

GrappleInput HoldingRagdoll(const NiPoint3& hand, const NiPoint3& bodyAt, SInt32 knock = 1) {
	GrappleInput in = Ready(true);
	in.hand = hand;
	in.heldReadable = true;
	in.heldAt = NiPoint3{100.0f, 0.0f, 0.0f};
	in.heldKnockState = knock;
	in.heldBodyValid = true;
	in.heldBodyAt = bodyAt;
	return in;
}

void TestRagdollRules() {
	std::printf("The ragdoll hold's rules\n");
	const GrappleSettings g;
	Check(g.ragdoll && Near(g.throwStrength, 1.5f), "the defaults: on, the throw 1.5 times the hand");
	Check(GrappleHoldFor(g, BodyPart::Torso, true) == GrappleHold::Ragdoll, "the torso: the ragdoll hold");
	Check(GrappleHoldFor(g, BodyPart::LeftArm, true) == GrappleHold::Ragdoll &&
	          GrappleHoldFor(g, BodyPart::RightArm, true) == GrappleHold::Ragdoll,
	      "the arms: the ragdoll hold");
	Check(GrappleHoldFor(g, BodyPart::LeftLeg, true) == GrappleHold::Ragdoll &&
	          GrappleHoldFor(g, BodyPart::RightLeg, true) == GrappleHold::Ragdoll,
	      "the legs: the ragdoll hold");
	Check(GrappleHoldFor(g, BodyPart::Head, true) == GrappleHold::Standing, "the head: standing (the choke)");
	Check(GrappleHoldFor(g, BodyPart::Neck, true) == GrappleHold::Standing, "the neck: standing");
	Check(GrappleHoldFor(g, BodyPart::Column, true) == GrappleHold::Standing, "the column (a creature): standing");
	Check(GrappleHoldFor(g, BodyPart::Torso, false) == GrappleHold::Standing, "no Havok body found: standing");
	GrappleSettings off;
	off.ragdoll = false;
	Check(GrappleHoldFor(off, BodyPart::Torso, true) == GrappleHold::Standing, "GrabRagdoll=0: standing");
	Check(!GrapplePartRagdolls(BodyPart::Head) && !GrapplePartRagdolls(BodyPart::Neck) &&
	          !GrapplePartRagdolls(BodyPart::Column),
	      "the parts the ragdoll never takes");
	Check(GrapplePartRagdolls(BodyPart::Torso) && GrapplePartRagdolls(BodyPart::LeftArm) &&
	          GrapplePartRagdolls(BodyPart::RightLeg),
	      "the parts it takes");
	Check(KnockStateDown(1) && KnockStateDown(2) && KnockStateDown(3) && KnockStateDown(4), "down: 1 to 4");
	Check(!KnockStateDown(0) && !KnockStateDown(5) && !KnockStateDown(6) && !KnockStateDown(-1),
	      "not down: standing, the get-up, unreadable");
	Check(KnockStateGettingUp(5) && KnockStateGettingUp(6), "getting up: 5 and 6");
	Check(!KnockStateGettingUp(0) && !KnockStateGettingUp(1) && !KnockStateGettingUp(2) && !KnockStateGettingUp(-1),
	      "not getting up otherwise");
	Check(KnockRestampDue(1, 0.5f), "lying 0.5 s since the stamp: stamped anew");
	Check(!KnockRestampDue(1, 0.49f), "lying, too soon: not yet");
	Check(!KnockRestampDue(2, 1.0f), "the lead-in: the engine stamps itself");
	Check(!KnockRestampDue(5, 1.0f) && !KnockRestampDue(0, 1.0f) && !KnockRestampDue(-1, 1.0f),
	      "getting up, standing, unreadable: never");
	Check(Near(GrappleThrowStrength(2.0f), 2.0f) && Near(GrappleThrowStrength(0.0f), 0.0f) &&
	          Near(GrappleThrowStrength(5.0f), 5.0f),
	      "the throw strength as set, 0 to 5");
	Check(Near(GrappleThrowStrength(-1.0f), 1.5f) && Near(GrappleThrowStrength(6.0f), 1.5f) &&
	          Near(GrappleThrowStrength(kNaN), 1.5f),
	      "out of range or not a number: the default");
	Check(GrappleHoldName(GrappleHold::Ragdoll)[0] == 'r' && GrappleHoldName(GrappleHold::Standing)[0] == 's',
	      "the hold names");
	Check(GrappleEndName(GrappleEnd::StoodUp)[0] == 's', "the stood-up end's name");
	GrappleState st;
	st.hold = GrappleHold::Ragdoll;
	GrappleHoldDowngrade(st);
	Check(st.hold == GrappleHold::Standing, "the downgrade: the standing hold");
}

void TestRagdollTakeFlows() {
	std::printf("Taking hold as a ragdoll\n");
	const GrappleSettings g;
	const ShoveSettings s;
	GrappleState st;
	GrappleVerdict v = StepGrapple(st, g, s, TakingRagdoll());
	Check(v.started && v.hold == GrappleHold::Ragdoll && v.knock, "a standing fighter by the torso: taken, knocked down");
	Check(st.actor == 0xA && st.hold == GrappleHold::Ragdoll && st.part == BodyPart::Torso, "the state: the ragdoll hold");

	st = GrappleState{};
	v = StepGrapple(st, g, s, TakingRagdoll(BodyPart::LeftLeg, false, true));
	Check(v.started && v.hold == GrappleHold::Ragdoll && !v.knock, "one lying knocked down, by a leg: taken as they lie, no knockdown");

	st = GrappleState{};
	v = StepGrapple(st, g, s, TakingRagdoll(BodyPart::Torso, false, false));
	Check(!v.started && v.refused && v.hold == GrappleHold::Ragdoll && st.actor == 0, "one getting up: refused");

	st = GrappleState{};
	GrappleInput in = TakingRagdoll();
	in.takeFights = false;
	v = StepGrapple(st, g, s, in);
	Check(!v.started && v.refused && st.actor == 0, "not fighting: refused, ragdoll or not");

	st = GrappleState{};
	v = StepGrapple(st, g, s, TakingRagdoll(BodyPart::Head));
	Check(v.started && v.hold == GrappleHold::Standing && !v.knock && st.hold == GrappleHold::Standing,
	      "by the head: the standing hold, no knockdown");

	st = GrappleState{};
	v = StepGrapple(st, g, s, TakingRagdoll(BodyPart::Torso, true, false, false));
	Check(v.started && v.hold == GrappleHold::Standing && !v.knock, "by the torso with no body found: standing");

	st = GrappleState{};
	GrappleSettings off;
	off.ragdoll = false;
	v = StepGrapple(st, off, s, TakingRagdoll());
	Check(v.started && v.hold == GrappleHold::Standing && !v.knock, "GrabRagdoll=0: the standing hold");

	st = GrappleState{};
	v = StepGrapple(st, g, s, TakingRagdoll(BodyPart::Head, false, true));
	Check(!v.started && v.refused && v.hold == GrappleHold::Standing, "the standing hold at one lying down: refused");

	st = GrappleState{};
	v = StepGrapple(st, off, s, TakingRagdoll(BodyPart::Torso, false, true));
	Check(!v.started && v.refused, "GrabRagdoll=0 at one lying down: refused (as before)");
}

void TestRagdollHoldFlows() {
	std::printf("Holding a ragdoll\n");
	const GrappleSettings g;
	const ShoveSettings s;
	GrappleState st;
	StepGrapple(st, g, s, TakingRagdoll());
	const NiPoint3 hand{60.0f, 0.0f, 100.0f};
	GrappleVerdict v = StepGrapple(st, g, s, HoldingRagdoll(hand, NiPoint3{63.0f, 4.0f, 100.0f}));
	Check(v.actor == 0xA && v.end == GrappleEnd::None && v.hold == GrappleHold::Ragdoll, "held");
	Check(Near(v.driveTo.x, 60.0f) && Near(v.driveTo.y, 0.0f) && Near(v.driveTo.z, 100.0f),
	      "the body is driven to the hand");
	Check(Near(v.gap, 5.0f), "the gap: the body from the hand, in the air");
	Check(Near(v.drag.x, 0.0f) && Near(v.drag.y, 0.0f), "no placing of the actor");
	Check(Near(v.seconds, 0.02f), "the time counted");

	v = StepGrapple(st, g, s, HoldingRagdoll(hand, NiPoint3{60.0f, 0.0f, 100.0f}, 2));
	Check(v.end == GrappleEnd::None, "the lead-in (state 2) is down: held");
	v = StepGrapple(st, g, s, HoldingRagdoll(hand, NiPoint3{60.0f, 0.0f, 100.0f}, 3));
	Check(v.end == GrappleEnd::None, "state 3 is down: held");

	v = StepGrapple(st, g, s, HoldingRagdoll(hand, NiPoint3{60.0f, 0.0f, 141.0f}));
	Check(v.end == GrappleEnd::Lost && st.actor == 0 && st.hold == GrappleHold::Standing,
	      "the body 41 units from the hand (up): lost, the state cleared");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	v = StepGrapple(st, g, s, HoldingRagdoll(hand, hand, 5));
	Check(v.end == GrappleEnd::StoodUp && st.actor == 0, "the get-up (state 5): stood up");
	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	v = StepGrapple(st, g, s, HoldingRagdoll(hand, hand, 0));
	Check(v.end == GrappleEnd::StoodUp, "standing again (state 0): stood up");
	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	v = StepGrapple(st, g, s, HoldingRagdoll(hand, hand, 6));
	Check(v.end == GrappleEnd::StoodUp, "the get-up's end (state 6): stood up");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	GrappleInput in = HoldingRagdoll(hand, hand);
	in.dtSeconds = 4.0f;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::BrokeFree && Near(v.seconds, 4.0f), "GrabHoldSeconds over: broke free");
	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in = HoldingRagdoll(hand, hand, 5);
	in.dtSeconds = 4.0f;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::StoodUp, "the get-up and the time over at once: stood up");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in = HoldingRagdoll(hand, hand);
	in.heldBodyValid = false;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Gone, "the body no longer reads: gone");
	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in = HoldingRagdoll(hand, hand);
	in.heldReadable = false;
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Gone, "the actor dead or far: gone");
	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	GrappleSettings off;
	off.grabs = false;
	v = StepGrapple(st, off, s, HoldingRagdoll(hand, hand));
	Check(v.end == GrappleEnd::Gone, "OffHandGrabs off mid-hold: gone");
}

void TestRagdollLetGoFlows() {
	std::printf("Letting go of a ragdoll\n");
	const GrappleSettings g;
	const ShoveSettings s;  // ShoveSpeed 1.3
	const NiPoint3 hand{60.0f, 0.0f, 100.0f};
	GrappleState st;
	StepGrapple(st, g, s, TakingRagdoll());
	GrappleInput in = HoldingRagdoll(hand, hand);
	in.grip = false;
	in.handVelocity = NiPoint3{0.0f, 0.0f, 2.0f};
	GrappleVerdict v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Thrown && Near(v.speed, 2.0f), "opened at 2 m/s straight up: thrown (the speed in the air counts)");
	Check(Near(v.throwVelocity.x, 0.0f) && Near(v.throwVelocity.y, 0.0f) && Near(v.throwVelocity.z, 3.0f),
	      "the held part at the hand's speed times 1.5");
	Check(Near(v.direction.z, 1.0f), "the direction: up");
	Check(st.actor == 0 && st.hold == GrappleHold::Standing, "the state cleared");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in.handVelocity = NiPoint3{1.0f, 0.0f, 0.0f};
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::LetGo && Near(v.throwVelocity.LengthSquared(), 0.0f), "opened at 1 m/s: let go, they drop");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in.handVelocity = NiPoint3{0.0f, 0.0f, 0.0f};
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::LetGo && Near(v.direction.LengthSquared(), 0.0f), "the hand still: let go, no direction");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in.handVelocity = NiPoint3{3.0f, 0.0f, 0.0f};
	GrappleSettings noThrow;
	noThrow.throwStrength = 0.0f;
	v = StepGrapple(st, noThrow, s, in);
	Check(v.end == GrappleEnd::LetGo, "GrabThrowStrength=0 at 3 m/s: let go, not thrown (the body's own motion left alone)");

	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll());
	in.handVelocity = NiPoint3{4.0f, 0.0f, 0.0f};
	v = StepGrapple(st, g, s, in);
	Check(v.end == GrappleEnd::Thrown && Near(v.throwVelocity.x, 6.0f), "a hard throw along the ground: thrown, never 'thrown down' (they are down)");

	// The standing hold's release is unchanged: across the ground.
	st = GrappleState{};
	StepGrapple(st, g, s, TakingRagdoll(BodyPart::Head));
	GrappleInput standing = Holding(hand, NiPoint3{100.0f, 0.0f, 0.0f});
	standing.grip = false;
	standing.handVelocity = NiPoint3{0.0f, 0.0f, 5.0f};
	v = StepGrapple(st, g, s, standing);
	Check(v.end == GrappleEnd::LetGo && Near(v.speed, 0.0f), "the standing hold opened with the hand going up: let go (across the ground counts)");
}

// The weight: GrabWeight as used, the pull's budget from the Strength, the
// weight and the gravity, what it lifts, and the velocity the drive sets
// under it - every flow of PullLimitedVelocity and the step test.
void TestPull() {
	std::printf("The weight\n");
	Check(Near(GrappleWeight(1.0f), 1.0f) && Near(GrappleWeight(0.0f), 0.0f) && Near(GrappleWeight(5.0f), 5.0f) &&
	          Near(GrappleWeight(2.5f), 2.5f),
	      "GrabWeight 0 to 5 as set");
	Check(Near(GrappleWeight(-1.0f), 1.0f) && Near(GrappleWeight(6.0f), 1.0f) && Near(GrappleWeight(kNaN), 1.0f),
	      "out of range or not a number: 1");
	Check(Near(GrapplePullBudget(100.0f, 1.0f, 9.8f), 2940.0f, 0.1f), "Strength 100, weight 1, gravity 9.8: 300 lifted");
	Check(Near(GrapplePullBudget(50.0f, 1.0f, 9.8f), 1470.0f, 0.1f), "Strength 50: half");
	Check(Near(GrapplePullBudget(5.0f, 1.0f, 9.8f), 294.0f, 0.1f) && Near(GrapplePullBudget(-1.0f, 1.0f, 9.8f), 294.0f, 0.1f) &&
	          Near(GrapplePullBudget(kNaN, 1.0f, 9.8f), 294.0f, 0.1f),
	      "Strength under 10, or unread: as 10");
	Check(Near(GrapplePullBudget(300.0f, 1.0f, 9.8f), 5880.0f, 0.1f), "Strength over 200: as 200");
	Check(Near(GrapplePullBudget(100.0f, 2.0f, 9.8f), 1470.0f, 0.1f), "weight 2: half the budget (twice as heavy)");
	Check(Near(GrapplePullBudget(100.0f, 0.0f, 9.8f), 0.0f), "weight 0: no budget (no limit)");
	Check(Near(GrapplePullBudget(100.0f, kNaN, 9.8f), 2940.0f, 0.1f), "weight not a number: as 1");
	Check(Near(GrapplePullBudget(100.0f, 1.0f, 0.0f), 2940.0f, 0.1f) && Near(GrapplePullBudget(100.0f, 1.0f, kNaN), 2940.0f, 0.1f) &&
	          Near(GrapplePullBudget(100.0f, 1.0f, 5000.0f), 2940.0f, 0.1f),
	      "gravity unread, not a number or absurd: Havok's 9.8");
	Check(Near(GrapplePullBudget(100.0f, 1.0f, 98.0f), 29400.0f, 1.0f), "gravity 98: ten times the budget, the same lift");
	Check(Near(GrappleLiftMass(2940.0f, 9.8f), 300.0f, 0.1f), "the lift: the budget over the gravity");
	Check(Near(GrappleLiftMass(0.0f, 9.8f), 0.0f) && Near(GrappleLiftMass(2940.0f, 0.0f), 0.0f),
	      "no budget or no gravity: no lift to name");

	const NiPoint3 still{0.0f, 0.0f, 0.0f};
	const NiPoint3 last{3.0f, 0.0f, 0.0f};
	const float massInv = 1.0f / 30.0f;  // a forearm
	const float budget = 2940.0f;
	const float step = 1.0f / 60.0f;
	PullVerdict v = PullLimitedVelocity(still, NiPoint3{1.0f, 0.0f, 0.0f}, massInv, budget, step, false, last);
	Check(Near(v.velocity.x, 3.0f) && !v.injected && !v.capped, "Havok has not stepped: the velocity set last again, nothing given");
	v = PullLimitedVelocity(still, NiPoint3{1.0f, 0.0f, 0.0f}, massInv, budget, step, true, last);
	Check(Near(v.velocity.x, 1.0f) && v.injected && !v.capped && Near(v.accel, 60.0f),
	      "a change within the budget (1 of 1.63 Havok units a second): the wanted velocity, its change named");
	v = PullLimitedVelocity(still, NiPoint3{10.0f, 0.0f, 0.0f}, massInv, budget, step, true, last);
	Check(Near(v.velocity.x, 1.633f, 1e-3f) && v.capped && Near(v.accel, 98.0f, 0.1f),
	      "a change over the budget: cut to it along the wanted change, the acceleration the budget's");
	v = PullLimitedVelocity(NiPoint3{0.0f, 0.0f, -2.0f}, NiPoint3{0.0f, 0.0f, 2.0f}, massInv, budget, step, true, last);
	Check(Near(v.velocity.z, -2.0f + 1.633f, 1e-3f) && v.capped, "falling, wanted up: the cut change from the velocity it has");
	v = PullLimitedVelocity(still, NiPoint3{10.0f, 0.0f, 0.0f}, massInv, 0.0f, step, true, last);
	Check(Near(v.velocity.x, 10.0f) && !v.capped && v.injected, "no budget: the wanted velocity whole");
	v = PullLimitedVelocity(still, NiPoint3{10.0f, 0.0f, 0.0f}, 0.0f, budget, step, true, last);
	Check(Near(v.velocity.x, 10.0f) && !v.capped, "the mass unread: the wanted velocity whole");
	v = PullLimitedVelocity(still, NiPoint3{10.0f, 0.0f, 0.0f}, massInv, budget, 0.0f, true, last);
	Check(Near(v.velocity.x, 10.0f) && !v.capped, "no time since the last change: the wanted velocity whole");
	v = PullLimitedVelocity(still, NiPoint3{10.0f, 0.0f, 0.0f}, massInv, kNaN, step, true, last);
	Check(Near(v.velocity.x, 10.0f) && !v.capped, "a budget not a number: no cap");
	v = PullLimitedVelocity(NiPoint3{10.0f, 0.0f, 0.0f}, NiPoint3{10.0f, 0.0f, 0.0f}, massInv, budget, step, true, last);
	Check(Near(v.velocity.x, 10.0f) && !v.capped && Near(v.accel, 0.0f), "already at the wanted velocity: no change");
	v = PullLimitedVelocity(still, NiPoint3{10.0f, 0.0f, 0.0f}, massInv, budget, 2.0f * step, true, last);
	Check(Near(v.velocity.x, 3.267f, 1e-3f) && v.capped, "two steps' worth of time: twice the change");

	Check(!VelocityStillTheSet(false, last, last), "nothing set yet: Havok counts as stepped");
	Check(VelocityStillTheSet(true, last, last), "the velocity read back exactly the one set: not stepped");
	Check(!VelocityStillTheSet(true, NiPoint3{3.0f, 0.0f, -0.1f}, last), "changed by a step: stepped");
	Check(!VelocityStillTheSet(true, still, still), "nothing set and nothing read back: stepped (a body at rest)");
}

}  // namespace

int main() {
	TestPull();
	TestStrike();
	TestTake();
	TestParts();
	TestRelease();
	TestTakeFlows();
	TestHoldFlows();
	TestLetGoFlows();
	TestRagdollRules();
	TestRagdollTakeFlows();
	TestRagdollHoldFlows();
	TestRagdollLetGoFlows();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

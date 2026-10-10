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
	Check(OffHandStrikeFor(g, s, true, true, Hand(3.5f, true, true)) == ShoveKind::None,
	      "the grip held (it holds someone): nothing");
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

}  // namespace

int main() {
	TestStrike();
	TestTake();
	TestParts();
	TestRelease();
	TestTakeFlows();
	TestHoldFlows();
	TestLetGoFlows();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

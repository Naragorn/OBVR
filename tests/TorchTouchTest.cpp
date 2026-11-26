// Checks the torch's blow by touch (game/TorchTouchLogic.h): the body the
// torch's shaft meets this frame - newly, now or between the frame before's
// pose and this one's - whether the torch went into them there (its own speed
// there, the hand's and its turn carried out, a quarter or more into their
// surface) and the blow by the hand's speed. The tester's rat on 2026-11-26
// was met by a chop at 4.8 m/s whose part across the ground towards the rat's
// middle was 2.25 m/s, under the stagger's 3, so the old rule (the hand's
// speed towards the bound's centre across the ground) gave nothing.

#include <cstdio>

#include "game/TorchTouchLogic.h"

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

bool Near(float a, float b, float tolerance) { return a - b <= tolerance && b - a <= tolerance; }

constexpr UInt32 kRat = 0x640B0E70;
constexpr UInt32 kGoblin = 0x2B001A50;

// A rat lying along y on the floor: its body a capsule 12 units round, its
// axis 10 units over the floor (z 0), from y 0 to y 30.
BladeBodies RatBodies() {
	BladeBodies b;
	b.Add(NiPoint3{0.0f, 0.0f, 10.0f}, NiPoint3{0.0f, 30.0f, 10.0f}, 12.0f, kRat, BodyPart::Torso);
	b.Add(NiPoint3{0.0f, 30.0f, 12.0f}, NiPoint3{0.0f, 38.0f, 12.0f}, 6.0f, kRat, BodyPart::Head);
	return b;
}

// A torch pointing straight down, its head `low` units over the floor at
// x, y; 48 units long, held 20 units from its top.
TorchPose Down(float x, float y, float low) {
	TorchPose p;
	p.valid = true;
	p.a = NiPoint3{x, y, low + 48.0f};
	p.b = NiPoint3{x, y, low};
	p.grip = 20.0f / 48.0f;
	return p;
}

// The hand moving at `v` m/s, not turning.
TorchHand Moving(const NiPoint3& v) {
	TorchHand h;
	h.velocity = v;
	return h;
}

void TestNothing() {
	std::printf("Nothing met\n");
	BladeBodies none;
	const TorchTouch t = TorchTouchOf(none, Down(0.0f, 15.0f, 60.0f), Down(0.0f, 15.0f, 20.0f),
	                                  Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(t.actor == 0, "no bodies collected: nobody met");
	const BladeBodies rat = RatBodies();
	const TorchTouch far = TorchTouchOf(rat, Down(80.0f, 15.0f, 60.0f), Down(80.0f, 15.0f, 20.0f),
	                                    Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(far.actor == 0, "the torch 80 units beside the rat, now and before: nobody met");
	TorchPose invalid;
	const TorchTouch noTorch = TorchTouchOf(rat, invalid, invalid, Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(noTorch.actor == 0, "no torch shaft this frame: nobody met");
	Check(TorchTouchKind(ShoveSettings{}, noTorch) == ShoveKind::None, "and no blow");
}

void TestChop() {
	std::printf("A chop down onto a rat on the floor (the tester's miss)\n");
	const BladeBodies rat = RatBodies();
	// The frame before the head 40 units over the floor (18 over the rat's
	// back), now 20 (into it): the hand going down at 4.8 m/s, 2.25 of it
	// forward - the old rule's speed towards the middle across the ground.
	const NiPoint3 hand{0.0f, 2.25f, -4.24f};
	const TorchTouch t = TorchTouchOf(rat, Down(0.0f, 15.0f, 40.0f), Down(0.0f, 15.0f, 20.0f), Moving(hand), 0);
	Check(t.actor == kRat, "the rat met");
	Check(t.normal.z > 0.9f, "on its back: the way out of it is up");
	Check(Near(t.speedInto, 4.24f, 0.05f), "the torch's speed into it is the downward 4.24 m/s");
	Check(Near(t.handSpeed, 4.81f, 0.05f), "the hand's whole speed 4.81 m/s");
	ShoveSettings s;  // 3 and 6 m/s
	Check(TorchTouchKind(s, t) == ShoveKind::Light, "a stagger (the hand at 3 m/s or more, under 6)");
	Check(t.swept, "met between the two frames' poses (its head went 20 units in one frame)");
	Check(t.point.z >= 20.0f - 0.1f && t.point.z <= 26.0f, "where it met is at the rat's back, not deep inside");
}

void TestKinds() {
	std::printf("The blow by the speed into the body\n");
	const BladeBodies rat = RatBodies();
	ShoveSettings s;
	const TorchPose before = Down(0.0f, 15.0f, 40.0f);
	const TorchPose now = Down(0.0f, 15.0f, 20.0f);
	Check(TorchTouchKind(s, TorchTouchOf(rat, before, now, Moving(NiPoint3{0.0f, 0.0f, -2.0f}), 0)) == ShoveKind::None,
	      "the hand at 2 m/s, all into it: under the stagger's 3, no blow");
	Check(TorchTouchKind(s, TorchTouchOf(rat, before, now, Moving(NiPoint3{0.0f, 0.0f, -3.0f}), 0)) == ShoveKind::Light,
	      "the hand at 3 m/s into it: a stagger");
	Check(TorchTouchKind(s, TorchTouchOf(rat, before, now, Moving(NiPoint3{0.0f, 0.0f, -6.0f}), 0)) == ShoveKind::Hard,
	      "the hand at 6 m/s into it: a knockdown");
	const TorchTouch away = TorchTouchOf(rat, before, now, Moving(NiPoint3{0.0f, 0.0f, 5.0f}), 0);
	Check(away.actor == kRat && away.speedInto < 0.0f && TorchTouchKind(s, away) == ShoveKind::None,
	      "met while the hand goes up, away from it: no blow");
	const TorchTouch slide = TorchTouchOf(rat, before, now, Moving(NiPoint3{5.0f, 0.0f, 0.0f}), 0);
	Check(slide.actor == kRat && Near(slide.speedInto, 0.0f, 0.3f) && TorchTouchKind(s, slide) == ShoveKind::None,
	      "the hand fast but along its back (a slide, not a blow): no blow");
	// 5 m/s, 10 degrees into its back: a slide that dips in, under a quarter.
	const TorchTouch dip = TorchTouchOf(rat, before, now, Moving(NiPoint3{4.92f, 0.0f, -0.87f}), 0);
	Check(dip.actor == kRat && TorchTouchKind(s, dip) == ShoveKind::None,
	      "5 m/s along its back, 10 degrees into it: a slide, no blow");
	// The probe's chop (harness 2026-11-26 16:04): the hand at 4.4 m/s, 2.9 of it
	// into the rat - 49 degrees off its surface's normal.
	const TorchTouch angled = TorchTouchOf(rat, before, now, Moving(NiPoint3{0.0f, 3.31f, -2.9f}), 0);
	Check(angled.actor == kRat && Near(angled.speedInto, 2.9f, 0.05f) && TorchTouchKind(s, angled) == ShoveKind::Light,
	      "the hand at 4.4 m/s, 2.9 of it into the rat (a chop at an angle): a stagger by the hand's speed");
}

void TestTurn() {
	std::printf("The torch's turn carried out to where it met them\n");
	const BladeBodies rat = RatBodies();
	ShoveSettings s;
	// The torch upright beside the rat, held 20 units from its top (z 36), its
	// head swung in against the rat's right flank: the frame before 16 units
	// further right.
	const TorchPose before = Down(30.0f, 15.0f, 8.0f);
	const TorchPose now = Down(14.0f, 15.0f, 8.0f);
	// A flick of the wrist: the hand at 0.5 m/s, the torch turning at 15
	// radians a second about the forward axis, so the head below the hand
	// swings to the left, into the flank.
	TorchHand flick;
	flick.velocity = NiPoint3{-0.5f, 0.0f, 0.0f};
	flick.spin = NiPoint3{0.0f, 15.0f, 0.0f};
	const TorchTouch t = TorchTouchOf(rat, before, now, flick, 0);
	Check(t.actor == kRat && t.normal.x > 0.9f, "the rat's right flank met");
	Check(t.speedInto > 5.0f && TorchWentIn(t), "the head goes into it far faster than the hand (the turn carried out)");
	Check(Near(t.handSpeed, 0.5f, 0.01f) && TorchTouchKind(s, t) == ShoveKind::None,
	      "but a flick of the wrist, the hand at 0.5 m/s: no blow");
	TorchHand swing = flick;
	swing.velocity = NiPoint3{-4.0f, 0.0f, 0.0f};
	Check(TorchTouchKind(s, TorchTouchOf(rat, before, now, swing, 0)) == ShoveKind::Light,
	      "the arm's swing with the same turn, the hand at 4 m/s: a stagger");
	swing.velocity = NiPoint3{-6.5f, 0.0f, 0.0f};
	Check(TorchTouchKind(s, TorchTouchOf(rat, before, now, swing, 0)) == ShoveKind::Hard,
	      "the hand at 6.5 m/s: a knockdown");
	TorchHand back;
	back.velocity = NiPoint3{4.0f, 0.0f, 0.0f};
	const TorchTouch b = TorchTouchOf(rat, before, now, back, 0);
	Check(b.actor == kRat && !TorchWentIn(b) && TorchTouchKind(s, b) == ShoveKind::None,
	      "the hand at 4 m/s going away from the flank as the torch reaches it: no blow");
	Check(Near(TorchGripShare(NiPoint3{0.0f, -10.0f, 0.0f}, NiPoint3{0.0f, 30.0f, 0.0f}), 0.25f, 0.001f),
	      "the hand's share along a shaft from -10 to 30 through it: 0.25");
	Check(Near(TorchGripShare(NiPoint3{0.0f, 5.0f, 0.0f}, NiPoint3{0.0f, 30.0f, 0.0f}), 0.0f, 0.001f),
	      "a shaft wholly past the hand: its near end");
	Check(Near(TorchGripShare(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}), 0.0f, 0.001f),
	      "a shaft of no length: 0");
}

void TestSweep() {
	std::printf("A low sweep across the rat\n");
	const BladeBodies rat = RatBodies();
	// The torch's head 8 units over the floor, from 40 units to its right
	// to 40 to its left in one frame: neither pose touches it, the way
	// between does.
	const TorchTouch t =
		TorchTouchOf(rat, Down(40.0f, 15.0f, 8.0f), Down(-40.0f, 15.0f, 8.0f), Moving(NiPoint3{-5.0f, 0.0f, 0.0f}), 0);
	Check(t.actor == kRat && t.swept, "met between the two frames");
	Check(t.normal.x > 0.9f, "on its right flank: the way out of it is to the right");
	Check(Near(t.speedInto, 5.0f, 0.05f) && TorchTouchKind(ShoveSettings{}, t) == ShoveKind::Light,
	      "the hand's 5 m/s right to left all into it: a stagger");
	TorchPose jump = Down(400.0f, 15.0f, 8.0f);
	const TorchTouch far = TorchTouchOf(rat, jump, Down(-40.0f, 15.0f, 8.0f), Moving(NiPoint3{-5.0f, 0.0f, 0.0f}), 0);
	Check(far.actor == 0, "a jump of 440 units (a load, a teleport) is no sweep: nobody met");
}

void TestNewMeeting() {
	std::printf("Only a new meeting is a blow\n");
	const BladeBodies rat = RatBodies();
	const TorchTouch resting = TorchTouchOf(rat, Down(0.0f, 15.0f, 21.0f), Down(0.0f, 15.0f, 20.0f),
	                                        Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(resting.actor == 0, "the torch on the rat the frame before too (resting, pressed on): no new blow");
	TorchPose none;
	const TorchTouch first = TorchTouchOf(rat, none, Down(0.0f, 15.0f, 20.0f), Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(first.actor == kRat && !first.swept, "no pose the frame before (the torch just taken): met now");
	const TorchTouch now = TorchTouchOf(rat, Down(0.0f, 15.0f, 40.0f), Down(0.0f, 15.0f, 25.0f),
	                                    Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(now.actor == kRat && Near(now.point.z, 25.0f, 0.1f),
	      "a short step (under a sweep's step): met at this frame's pose");
}

void TestTwoBodies() {
	std::printf("Two bodies, and one cooling down\n");
	BladeBodies b = RatBodies();
	// A goblin's leg beside the rat, 20 units to its right.
	b.Add(NiPoint3{20.0f, 15.0f, 0.0f}, NiPoint3{20.0f, 15.0f, 50.0f}, 6.0f, kGoblin, BodyPart::LeftLeg);
	// A sweep from the right: the goblin's leg first, then the rat; the hand
	// right to left (into the goblin's right side and the rat's).
	const TorchTouch t =
		TorchTouchOf(b, Down(60.0f, 15.0f, 8.0f), Down(-40.0f, 15.0f, 8.0f), Moving(NiPoint3{-5.0f, 0.0f, 0.0f}), 0);
	Check(t.actor == kGoblin || t.actor == kRat, "one of the two met");
	const TorchTouch skipGoblin =
		TorchTouchOf(b, Down(60.0f, 15.0f, 8.0f), Down(-40.0f, 15.0f, 8.0f), Moving(NiPoint3{-5.0f, 0.0f, 0.0f}), kGoblin);
	Check(skipGoblin.actor == kRat, "the goblin cooling down from a blow: the rat is the one met");
	const TorchTouch skipRat = TorchTouchOf(RatBodies(), Down(0.0f, 15.0f, 40.0f), Down(0.0f, 15.0f, 20.0f),
	                                        Moving(NiPoint3{0.0f, 0.0f, -4.0f}), kRat);
	Check(skipRat.actor == 0, "only the rat there and it cooling down: nobody met");
	// Down onto both at once: the rat's back and the goblin's leg's top; the
	// hand going down and to the right - into the goblin's top harder.
	BladeBodies both = RatBodies();
	both.Add(NiPoint3{8.0f, 15.0f, 0.0f}, NiPoint3{8.0f, 15.0f, 18.0f}, 4.0f, kGoblin, BodyPart::LeftLeg);
	TorchPose before;
	before.valid = true;
	before.a = NiPoint3{-20.0f, 15.0f, 70.0f};
	before.b = NiPoint3{8.0f, 15.0f, 40.0f};
	TorchPose now = before;
	now.a.z -= 20.0f;
	now.b.z -= 20.0f;
	const TorchTouch hardest = TorchTouchOf(both, before, now, Moving(NiPoint3{0.0f, 0.0f, -4.0f}), 0);
	Check(hardest.actor != 0 && hardest.speedInto > 3.0f, "met someone at the hand's speed into them");
}

void TestCentre() {
	std::printf("The middle of someone's capsules\n");
	const BladeBodies rat = RatBodies();
	NiPoint3 c{0.0f, 0.0f, 0.0f};
	Check(CapsulesCentre(rat, kRat, c), "the rat's capsules found");
	Check(Near(c.x, 0.0f, 0.01f) && Near(c.y, 24.5f, 0.01f) && Near(c.z, 11.0f, 0.01f),
	      "the mean of their middles (0, 24.5, 11)");
	Check(!CapsulesCentre(rat, kGoblin, c), "nobody's capsules for another: none");
}

void TestBox() {
	std::printf("The swept box: bodies away from the torch are not tested\n");
	const BladeBodies rat = RatBodies();
	const TorchBox still = TorchSweptBox(Down(0.0f, 15.0f, 25.0f), Down(0.0f, 15.0f, 24.0f), 4.0f);
	Check(Near(still.low.z, 20.0f, 0.01f) && Near(still.high.z, 76.0f, 0.01f),
	      "a short step: this frame's shaft alone, grown by the margin (z 20 to 76)");
	const TorchBox swept = TorchSweptBox(Down(40.0f, 15.0f, 8.0f), Down(-40.0f, 15.0f, 8.0f), 4.0f);
	Check(Near(swept.low.x, -44.0f, 0.01f) && Near(swept.high.x, 44.0f, 0.01f),
	      "a sweep: both poses, grown by the margin (x -44 to 44)");
	Check(CapsulesInBox(rat, kRat, swept), "the rat under the sweep's way: in the box");
	const TorchBox above = TorchSweptBox(Down(0.0f, 15.0f, 90.0f), Down(0.0f, 15.0f, 88.0f), 4.0f);
	Check(!CapsulesInBox(rat, kRat, above), "the torch high over the rat: none of its capsules in the box");
	Check(!CapsulesInBox(rat, kGoblin, swept), "someone with no capsules: nothing in the box");
}

void TestNormalOnAxis() {
	std::printf("The torch through the axis of a round capsule\n");
	BladeBodies b;
	b.Add(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 60.0f}, 10.0f, kGoblin, BodyPart::Torso);
	// The shaft across the capsule's axis (x from -30 to 30 at z 30): its
	// nearest point lies on the axis, where no way out is known - the way
	// from the body's middle is taken instead; the hand straight in along y.
	TorchPose now;
	now.valid = true;
	now.a = NiPoint3{-30.0f, 0.0f, 30.0f};
	now.b = NiPoint3{30.0f, 0.0f, 30.0f};
	const TorchTouch t = TorchTouchOf(b, TorchPose{}, now, Moving(NiPoint3{0.0f, 4.0f, 0.0f}), 0);
	Check(t.actor == kGoblin, "met");
	Check(t.speedInto >= 3.9f, "no way out at the axis: the torch's whole speed counts as into them");
	Check(TorchTouchKind(ShoveSettings{}, t) == ShoveKind::Light, "and the hand at 4 m/s: a stagger");
}

}  // namespace

int main() {
	TestNothing();
	TestChop();
	TestKinds();
	TestTurn();
	TestSweep();
	TestNewMeeting();
	TestTwoBodies();
	TestCentre();
	TestBox();
	TestNormalOnAxis();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

// Checks the decisions about an arrow that sticks in the world
// (game/StuckArrow.h): which arrow counts as stuck - one the engine's grab
// cannot hold and the game itself takes by activating - and when a closed
// grip takes it. A wrong "stuck" either leaves the arrow where nothing can
// take it, or takes one that still flies, lies loose in the grab's own reach,
// or sits in a body that already got it into its inventory.

#include <cstdio>

#include "game/StuckArrow.h"

namespace {

using obvr::game::ArrowRead;
using obvr::game::ArrowTakeInput;
using obvr::game::ArrowTakeState;
using obvr::game::ArrowTakeVerdict;
using obvr::game::IsStuckArrow;
using obvr::game::StepArrowTake;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

ArrowRead Arrow(UInt32 state, bool haveImpact, UInt32 kind) {
	ArrowRead a;
	a.isArrow = true;
	a.state = state;
	a.haveImpact = haveImpact;
	a.kind = kind;
	return a;
}

void TestWhichArrowIsStuck() {
	std::printf("Which arrow is stuck\n");
	Check(!IsStuckArrow(ArrowRead{}), "not an arrow projectile at all: no");
	ArrowRead other = Arrow(obvr::game::kArrowStateAtRest, true, obvr::game::kArrowHitBody);
	other.isArrow = false;
	Check(!IsStuckArrow(other), "another reference with the same numbers: no");

	Check(IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, true, obvr::game::kArrowHitBody)),
	      "at rest in a wall, a post, a target: stuck");
	Check(IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, true, obvr::game::kArrowHitLand)),
	      "at rest in the ground: stuck");
	Check(IsStuckArrow(Arrow(obvr::game::kArrowStateFading, true, obvr::game::kArrowHitBody)),
	      "fading where it stuck: still to be taken, as the game's own activate takes it");
	Check(IsStuckArrow(Arrow(obvr::game::kArrowStateFading, true, obvr::game::kArrowHitLand)),
	      "the same in the ground");

	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateFlying, false, 0)), "flying: no");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateFlying, true, obvr::game::kArrowHitBody)),
	      "flying with a hit record left over: no");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateHit, true, obvr::game::kArrowHitBody)),
	      "the frame it hits, before the engine has set it into the wall: no");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, false, obvr::game::kArrowHitBody)),
	      "at rest without a hit record: nothing says where it sits - no");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateFading, false, 0)),
	      "grown old in flight and fading: no");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, true, obvr::game::kArrowHitActor)),
	      "in a body: left alone - the body may already hold it in its inventory");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, true, obvr::game::kArrowBounced)),
	      "bounced off and lying loose: the grab holds it like any item");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, true, obvr::game::kArrowFlightOver)),
	      "the flight ran out, lying loose: the same");
	Check(!IsStuckArrow(Arrow(obvr::game::kArrowStateAtRest, true, 5)), "a kind the engine does not have: no");
	Check(!IsStuckArrow(Arrow(4, true, obvr::game::kArrowHitBody)), "a state the engine does not have: no");
}

ArrowTakeInput Input(bool gripHeld, bool stuckInReach, bool free) {
	ArrowTakeInput in;
	in.gripHeld = gripHeld;
	in.stuckInReach = stuckInReach;
	in.free = free;
	return in;
}

void TestGripTakesOnce() {
	std::printf("A closed grip takes a stuck arrow once\n");
	ArrowTakeState s;
	ArrowTakeVerdict v = StepArrowTake(s, Input(false, true, true));
	Check(!v.takeNow && !v.blocksGrab, "the hand open at the arrow: nothing");
	v = StepArrowTake(s, Input(true, true, true));
	Check(v.takeNow && v.blocksGrab, "the grip closes: taken, and the hand grabs nothing else");
	v = StepArrowTake(s, Input(true, true, true));
	Check(!v.takeNow && v.blocksGrab, "still closed, another arrow beside it: not taken too");
	v = StepArrowTake(s, Input(true, false, true));
	Check(!v.takeNow && v.blocksGrab, "still closed, moved on to an apple: the grab stays off");
	v = StepArrowTake(s, Input(false, true, true));
	Check(!v.takeNow && !v.blocksGrab, "opened: free again");
	v = StepArrowTake(s, Input(true, true, true));
	Check(v.takeNow, "closed again: the next arrow");
}

void TestGripClosedBeforeTheArrow() {
	std::printf("A grip already closed that comes to the arrow\n");
	ArrowTakeState s;
	ArrowTakeVerdict v = StepArrowTake(s, Input(true, false, true));
	Check(!v.takeNow && !v.blocksGrab, "closed on nothing: the grab as ever");
	v = StepArrowTake(s, Input(true, true, true));
	Check(v.takeNow && v.blocksGrab, "the arrow comes into reach: taken, as an item would be grabbed");
}

void TestNotFree() {
	std::printf("The take waits while it may not run\n");
	ArrowTakeState s;
	ArrowTakeVerdict v = StepArrowTake(s, Input(true, true, false));
	Check(!v.takeNow && !v.blocksGrab, "something in the grab, a menu, dead: no take, and nothing held back");
	v = StepArrowTake(s, Input(true, true, false));
	Check(!v.takeNow && !v.blocksGrab, "and none the next frame");
	v = StepArrowTake(s, Input(true, true, true));
	Check(v.takeNow && v.blocksGrab, "free with the grip still closed at the arrow: taken then");
	v = StepArrowTake(s, Input(true, true, false));
	Check(!v.takeNow && v.blocksGrab, "the closing is spent whatever comes after");
}

void TestNothingStuck() {
	std::printf("Nothing stuck in reach\n");
	ArrowTakeState s;
	for (int frame = 0; frame < 3; ++frame) {
		const ArrowTakeVerdict v = StepArrowTake(s, Input(true, false, true));
		Check(!v.takeNow && !v.blocksGrab, "a grip held on something else: the arrow's step stays out of it");
	}
}

}  // namespace

int main() {
	TestWhichArrowIsStuck();
	TestGripTakesOnce();
	TestGripClosedBeforeTheArrow();
	TestNotFree();
	TestNothingStuck();
	std::printf(g_failures == 0 ? "all passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

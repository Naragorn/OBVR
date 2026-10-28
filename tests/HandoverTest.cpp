// Checks handing a thing from one hand to the other (game/HandoverLogic.h;
// Nadi, 2026-10-28, free-weapons-spec 5.14): which hand can wear what, and
// when a let go of the hand that wears it becomes a hand-over.

#include <cstdio>

#include "game/HandoverLogic.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestWearHand() {
	std::printf("The hand that can wear it\n");
	using game::HandoverThing;
	using game::WearHand;
	Check(game::WearHandOf(HandoverThing::OneHander) == WearHand::Weapon, "a one-hander: the weapon hand");
	Check(game::WearHandOf(HandoverThing::Bow) == WearHand::Other, "a bow: the other hand");
	Check(game::WearHandOf(HandoverThing::Shield) == WearHand::Other, "a shield: the other hand");
	Check(game::WearHandOf(HandoverThing::Torch) == WearHand::Other, "a torch: the other hand");
	Check(game::WearHandOf(HandoverThing::Item) == WearHand::None, "anything else: no hand wears it, both hold it");
	Check(game::WearHandOf(HandoverThing::TwoHander) == WearHand::None, "a two-hander: both hands, never handed over");
}

void TestAtLetGo() {
	std::printf("A let go becomes a hand-over\n");
	Check(game::HandOverAtLetGo(true, true, 4.0f, 10.0f), "let go, the other hand closed on it: handed over");
	Check(!game::HandOverAtLetGo(true, false, 4.0f, 10.0f), "the other hand open: a drop or a throw as before");
	Check(!game::HandOverAtLetGo(true, true, 25.0f, 10.0f), "the other hand closed, but away from it: a drop");
	Check(!game::HandOverAtLetGo(false, true, 0.0f, 10.0f), "no let go: nothing");
	Check(!game::HandOverAtLetGo(true, true, -1.0f, 10.0f), "where it is not known: a drop");
	Check(!game::HandOverAtLetGo(true, true, 0.0f, 0.0f), "reach 0 (switched off): a drop");
	Check(game::HandOverAtLetGo(true, true, 10.0f, 10.0f), "at the reach's edge: handed over");
}

void TestDistance() {
	std::printf("How far the hand is from the thing\n");
	const NiPoint3 grip{0.0f, 0.0f, 0.0f};
	const NiPoint3 tip{0.0f, 40.0f, 0.0f};
	Check(game::HandToSpanUnits(NiPoint3{3.0f, 20.0f, 0.0f}, grip, tip) > 2.99f &&
	          game::HandToSpanUnits(NiPoint3{3.0f, 20.0f, 0.0f}, grip, tip) < 3.01f,
	      "beside the blade: 3 units");
	Check(game::HandToSpanUnits(NiPoint3{0.0f, -4.0f, 0.0f}, grip, tip) > 3.99f, "past the pommel: from its end");
	Check(game::HandToSpanUnits(NiPoint3{1.0f, 1.0f, 0.0f}, grip, grip) > 1.41f, "a span of no length: the point");
}

void TestBack() {
	std::printf("Taken back by the hand that wears it\n");
	game::HandBackState st;
	game::HandBackInput in;
	in.heldRef = 0x1000;
	in.wearHandClosedNow = true;
	in.wearHandToThingUnits = 3.0f;
	in.reachUnits = 10.0f;
	in.grabbedRef = 0x1000;
	in.dt = 1.0f / 90.0f;
	game::HandBackVerdict v = game::StepHandBack(st, in);
	Check(v.releaseHold && v.takeRef == 0, "the wearing hand closes on it: the other hand's hold let go first");
	in.wearHandClosedNow = false;
	v = game::StepHandBack(st, in);
	Check(v.releaseHold && v.takeRef == 0, "the engine still holds it: waits, the hold still let go");
	in.grabbedRef = 0;
	v = game::StepHandBack(st, in);
	Check(v.takeRef == 0x1000 && !v.releaseHold, "the grab let go of it: taken, to be equipped");
	v = game::StepHandBack(st, in);
	Check(v.takeRef == 0 && !v.releaseHold, "once only");

	game::HandBackState far;
	in = game::HandBackInput{};
	in.heldRef = 0x1000;
	in.wearHandClosedNow = true;
	in.wearHandToThingUnits = 30.0f;
	in.reachUnits = 10.0f;
	in.grabbedRef = 0x1000;
	in.dt = 1.0f / 90.0f;
	v = game::StepHandBack(far, in);
	Check(!v.releaseHold && v.takeRef == 0, "the wearing hand closes away from it: nothing");
	in.heldRef = 0;
	in.wearHandToThingUnits = 1.0f;
	v = game::StepHandBack(far, in);
	Check(!v.releaseHold, "nothing held by the other hand: nothing");

	game::HandBackState stuck;
	in = game::HandBackInput{};
	in.heldRef = 0x1000;
	in.wearHandClosedNow = true;
	in.wearHandToThingUnits = 1.0f;
	in.reachUnits = 10.0f;
	in.grabbedRef = 0x1000;
	in.dt = 0.1f;
	game::StepHandBack(stuck, in);
	in.wearHandClosedNow = false;
	for (int i = 0; i < 12 && !v.gaveUp; ++i) {
		v = game::StepHandBack(stuck, in);
	}
	Check(v.gaveUp && !v.releaseHold && v.takeRef == 0, "the engine never lets go within a second: given up, nothing taken");
}

}  // namespace

int main() {
	TestWearHand();
	TestAtLetGo();
	TestDistance();
	TestBack();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

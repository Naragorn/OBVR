// Checks handing a thing from one hand to the other (game/HandoverLogic.h;
// Nadi, 2026-10-28, free-weapons-spec 5.14): which hand can wear what, and
// when a let go of the hand that wears it becomes a hand-over.

#include <cstdio>

#include "game/HandoverLogic.h"
#include "game/InventoryGrabLogic.h"

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
	Check(v.takeRef == 0 && v.releaseHold, "the grab let go of it: a frame for the grab physics' release first");
	v = game::StepHandBack(st, in);
	Check(v.takeRef == 0x1000 && !v.releaseHold, "the frame after: taken, to be equipped");
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
	Check(!game::HandBackUnderWay(stuck, v), "given up: no longer under way");
}

// While a hand-back is under way, the ordinary take at the grip keeps off:
// in the frame the hand-back waits for the grab physics' release, the
// weapon grab's own take took the sword, and the hand-back's take a frame
// later met a reference the activation had deleted - the game ended
// (handover-sword.txt 2026-10-30 12:09). And once it is done it is over: the
// flag stayed set after the take and kept the held things' throw off.
void TestUnderWay() {
	std::printf("A hand-back under way, frame by frame\n");
	game::HandBackState st;
	game::HandBackInput in;
	game::HandBackVerdict v;
	Check(!game::HandBackUnderWay(st, v), "nothing begun: not under way");
	in.heldRef = 0x1000;
	in.wearHandClosedNow = true;
	in.wearHandToThingUnits = 3.0f;
	in.reachUnits = 10.0f;
	in.grabbedRef = 0x1000;
	in.dt = 1.0f / 72.0f;
	v = game::StepHandBack(st, in);
	Check(game::HandBackUnderWay(st, v), "the wearing hand closed on it: under way");
	in.wearHandClosedNow = false;
	v = game::StepHandBack(st, in);
	Check(game::HandBackUnderWay(st, v), "the engine's grab still holds it: under way");
	in.grabbedRef = 0;
	v = game::StepHandBack(st, in);
	Check(v.takeRef == 0 && game::HandBackUnderWay(st, v),
	      "the grab let go, the frame of waiting: still under way - the ordinary take keeps off");
	v = game::StepHandBack(st, in);
	Check(v.takeRef == 0x1000 && game::HandBackUnderWay(st, v), "the take's frame: under way");
	in.heldRef = 0;
	v = game::StepHandBack(st, in);
	Check(!game::HandBackUnderWay(st, v), "the frame after the take: over");
}

void TestClosing() {
	std::printf("A hand closing on a thing the other hand holds\n");
	using game::InventoryHand;
	using game::InventoryItemKind;
	using game::HandoverAct;
	const auto act = [](InventoryItemKind k, InventoryHand h) { return game::ActOnClosing(game::ChooseInventoryTake(k, h)); };
	Check(act(InventoryItemKind::Weapon, InventoryHand::Right) == HandoverAct::EquipBack, "a sword, the weapon hand: equipped");
	Check(act(InventoryItemKind::Weapon, InventoryHand::Left) == HandoverAct::Pass, "a sword, the other hand: held there");
	Check(act(InventoryItemKind::Bow, InventoryHand::Left) == HandoverAct::EquipBack, "a bow, the other hand: equipped");
	Check(act(InventoryItemKind::Bow, InventoryHand::Right) == HandoverAct::Pass, "a bow, the weapon hand: held");
	Check(act(InventoryItemKind::Shield, InventoryHand::Left) == HandoverAct::EquipBack &&
	          act(InventoryItemKind::Torch, InventoryHand::Left) == HandoverAct::EquipBack,
	      "a shield or a torch, the other hand: equipped");
	Check(act(InventoryItemKind::Shield, InventoryHand::Right) == HandoverAct::Pass &&
	          act(InventoryItemKind::Torch, InventoryHand::Right) == HandoverAct::Pass,
	      "a shield or a torch, the weapon hand: held");
	Check(act(InventoryItemKind::Other, InventoryHand::Right) == HandoverAct::Pass &&
	          act(InventoryItemKind::Other, InventoryHand::Left) == HandoverAct::Pass,
	      "anything else: passed to whichever hand");
	Check(act(InventoryItemKind::Other, InventoryHand::None) == HandoverAct::None, "no hand: nothing");
}

// Anything in either hand (free-weapons-spec 5.16, step 4): a thing handed
// over switches the sides - the mirror - and stays the game's equipped thing.
void TestSwitchesSides() {
	std::printf("A handover switches the sides\n");
	using game::HandoverThing;
	Check(game::HandOverSwitchesSides(HandoverThing::OneHander, true), "the one-hander: the sides switch");
	Check(game::HandOverSwitchesSides(HandoverThing::Bow, false), "the bow: the sides switch");
	Check(game::HandOverSwitchesSides(HandoverThing::Torch, false) && game::HandOverSwitchesSides(HandoverThing::Shield, false),
	      "a torch or a shield to an empty weapon hand: the sides switch");
	Check(!game::HandOverSwitchesSides(HandoverThing::Torch, true) && !game::HandOverSwitchesSides(HandoverThing::Shield, true),
	      "to a weapon hand holding its weapon: no - one hand holds one thing (stage 2)");
	Check(!game::HandOverSwitchesSides(HandoverThing::Item, false), "any other item: passed as before, no switch");
	Check(game::HandOverSwitchesSides(HandoverThing::TwoHander, true), "a two-hander or a staff: the sides switch (from the two-hand hold)");
}

// Which let go of the weapon is a handover (free-weapons-spec 5.16, step 4):
// the other hand near it, or holding it in the two-hand hold anywhere on the
// handle - a two-hander's other hand sits a hand or more below the weapon
// hand, past the reach. A two-hander changes hands only from the hold.
void TestOfWeapon() {
	std::printf("A let go of the weapon becomes a handover\n");
	using game::HandoverThing;
	const float reach = 8.4f;
	Check(game::HandOverOfWeapon(HandoverThing::OneHander, false, true, 3.0f, reach), "a one-hander, the other hand closed near it: handed over");
	Check(!game::HandOverOfWeapon(HandoverThing::OneHander, false, true, 20.0f, reach), "a one-hander, the other hand closed away from it: a drop");
	Check(game::HandOverOfWeapon(HandoverThing::OneHander, true, true, 20.0f, reach),
	      "a one-hander in the two-hand hold, the other hand low on the handle: handed over");
	Check(game::HandOverOfWeapon(HandoverThing::TwoHander, true, true, 20.0f, reach),
	      "a two-hander or a staff in the two-hand hold: handed over");
	Check(!game::HandOverOfWeapon(HandoverThing::TwoHander, false, true, 3.0f, reach),
	      "a two-hander, the other hand closed near it but not in the hold: a drop");
	Check(!game::HandOverOfWeapon(HandoverThing::TwoHander, false, false, -1.0f, reach), "a two-hander, the other hand open: a drop");
	Check(!game::HandOverOfWeapon(HandoverThing::TwoHander, true, true, 20.0f, 0.0f),
	      "the handover off (reach 0): a drop, the hold or not");
	Check(game::HandOverOfWeapon(HandoverThing::Bow, false, true, 2.0f, reach), "the bow, the other hand closed near it: handed over");
	Check(!game::HandOverOfWeapon(HandoverThing::Bow, true, false, 20.0f, reach), "the bow: no two-hand hold counts");
	Check(!game::HandOverOfWeapon(HandoverThing::Item, true, true, 1.0f, reach), "anything else: not this way");
}

void TestTorchAtClose() {
	std::printf("The torch handed over when the weapon hand closes on it\n");
	const float reach = 8.4f;
	Check(game::TorchHandedAtClose(true, true, true, false, 3.0f, reach), "the empty weapon hand closed near the held torch: handed over");
	Check(!game::TorchHandedAtClose(false, true, true, false, 3.0f, reach), "the weapon hand still closed, not closing now: nothing");
	Check(!game::TorchHandedAtClose(true, false, true, false, 3.0f, reach), "no torch in the other hand: nothing");
	Check(!game::TorchHandedAtClose(true, true, false, false, 3.0f, reach), "the weapon hand busy (a held thing, a grab, a holster): nothing");
	Check(!game::TorchHandedAtClose(true, true, true, true, 3.0f, reach), "a weapon drawn in the weapon hand: nothing (one thing a hand)");
	Check(!game::TorchHandedAtClose(true, true, true, false, 20.0f, reach), "closed beyond the reach: nothing");
	Check(!game::TorchHandedAtClose(true, true, true, false, -1.0f, reach), "the distance not read: nothing");
	Check(!game::TorchHandedAtClose(true, true, true, false, 0.0f, 0.0f), "the handover off (reach 0): nothing");
}

void TestHeldInHand() {
	std::printf("Near a thing worn in a hand\n");
	Check(game::NearHeldInHandUnits(10.0f, 8.0f) > 1.99f && game::NearHeldInHandUnits(10.0f, 8.0f) < 2.01f,
	      "10 units from the hand that wears it, the thing 8 round: 2 off it");
	Check(game::NearHeldInHandUnits(5.0f, 8.0f) == 0.0f, "within its round: on it");
	Check(game::NearHeldInHandUnits(-1.0f, 8.0f) < 0.0f, "the hand not known: unknown");
}

}  // namespace

int main() {
	TestWearHand();
	TestAtLetGo();
	TestDistance();
	TestBack();
	TestUnderWay();
	TestClosing();
	TestHeldInHand();
	TestSwitchesSides();
	TestOfWeapon();
	TestTorchAtClose();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

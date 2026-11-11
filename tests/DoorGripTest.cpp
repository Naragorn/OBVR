// Checks doors by grip (vr/DoorGrip.h): the leaf's thin axis, the handle
// zone, the empty-hand rule, and every flow of the step - taken, opened by a
// pull and by a push, the grip let go before the threshold, a grip carried in,
// an item that goes first, the door gone, a menu up, switched off.

#include <cstdio>
#include <limits>

#include "vr/DoorGrip.h"

namespace {

using namespace obvr::vr;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

constexpr UInt32 kDoor = 0x1234;

// A door leaf 1 m wide (x), 4 cm thick (y), 2 m high (z), its foot at z 0.
DoorBoxMetres Leaf() { return DoorBoxMetres{NiPoint3{-0.5f, -0.02f, 0.0f}, NiPoint3{0.5f, 0.02f, 2.0f}}; }

void TestThinAxis() {
	std::printf("The leaf's thin axis\n");
	Check(DoorThinAxis(Leaf()) == 1, "a leaf thin along y: y");
	Check(DoorThinAxis(DoorBoxMetres{NiPoint3{-0.03f, -0.6f, 0.0f}, NiPoint3{0.03f, 0.6f, 2.2f}}) == 0,
	      "a leaf thin along x: x");
	Check(DoorThinAxis(DoorBoxMetres{NiPoint3{-0.5f, -0.5f, 0.0f}, NiPoint3{0.5f, 0.5f, 2.0f}}) == -1,
	      "as deep as wide (a cupboard, not a leaf): none");
	Check(DoorThinAxis(DoorBoxMetres{NiPoint3{-0.5f, -0.4f, 0.0f}, NiPoint3{0.5f, 0.4f, 2.0f}}) == -1,
	      "thicker than kDoorMostThickMetres: none");
	Check(DoorThinAxis(DoorBoxMetres{NiPoint3{-0.5f, -0.02f, 0.0f}, NiPoint3{0.5f, 0.02f, 0.4f}}) == -1,
	      "lower than kDoorLeastHighMetres (a hatch, a lid): none");
	Check(DoorThinAxis(DoorBoxMetres{NiPoint3{0.5f, 0.0f, 0.0f}, NiPoint3{-0.5f, 0.0f, 2.0f}}) == -1,
	      "an empty or turned-round box: none");
}

void TestZone() {
	std::printf("The handle zone\n");
	const DoorGripSettings s;
	float normal = 0.0f;
	Check(InHandleZone(NiPoint3{0.3f, 0.08f, 1.0f}, Leaf(), s, normal) && Near(normal, 0.08f),
	      "8 cm off the leaf at 1 m, inside its width: in, its normal coordinate given");
	Check(InHandleZone(NiPoint3{-0.45f, -0.06f, 1.2f}, Leaf(), s, normal) && Near(normal, -0.06f),
	      "the other face, the other edge: in (the hinge side is not known, the whole width counts)");
	Check(!InHandleZone(NiPoint3{0.3f, 0.30f, 1.0f}, Leaf(), s, normal), "30 cm off the leaf: out");
	Check(!InHandleZone(NiPoint3{0.3f, 0.05f, 0.3f}, Leaf(), s, normal), "at the knees: out");
	Check(!InHandleZone(NiPoint3{0.3f, 0.05f, 1.8f}, Leaf(), s, normal), "above the head: out");
	Check(!InHandleZone(NiPoint3{0.7f, 0.05f, 1.0f}, Leaf(), s, normal), "beside the leaf: out");
	const DoorBoxMetres low{NiPoint3{-0.4f, -0.02f, 0.0f}, NiPoint3{0.4f, 0.02f, 1.2f}};
	Check(InHandleZone(NiPoint3{0.0f, 0.05f, 0.7f}, low, s, normal),
	      "a low door (1.2 m): the band moves inside it, 0.7 m is in");
	Check(!InHandleZone(NiPoint3{0.0f, 0.05f, 1.0f}, DoorBoxMetres{NiPoint3{-0.5f, -0.5f, 0.0f},
	                                                               NiPoint3{0.5f, 0.5f, 2.0f}},
	                    s, normal),
	      "no leaf: never in");
	float lowZ = 0.0f;
	float highZ = 0.0f;
	HandleBand(Leaf(), s, lowZ, highZ);
	Check(Near(lowZ, 0.7f) && Near(highZ, 1.4f), "the band of a 2 m leaf: 0.7 to 1.4 m");
	HandleBand(low, s, lowZ, highZ);
	Check(Near(lowZ, 0.42f) && Near(highZ, 0.96f), "the band of a 1.2 m door: 35 to 80 % of its height");
}

void TestFreeHand() {
	std::printf("The empty hand\n");
	Check(HandFreeForDoor(true, false, false), "tracked, holding nothing, no weapon: free");
	Check(!HandFreeForDoor(false, false, false), "not tracked: not free");
	Check(!HandFreeForDoor(true, true, false), "holding something: not free");
	Check(!HandFreeForDoor(true, false, true), "a weapon readied: not free");
}

DoorGripFrame Ready() {
	DoorGripFrame f;
	f.enabled = true;
	f.candidate = kDoor;
	f.candidateLeft = false;
	f.candidateNormalMetres = 0.05f;
	f.handFree[0] = true;
	f.handFree[1] = true;
	return f;
}

void TestFlows() {
	const DoorGripSettings s;
	std::printf("The step: taken and opened\n");
	DoorGripState st;
	DoorGripFrame f = Ready();
	DoorGripVerdict v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None && st.phase == DoorGripPhase::Idle, "a hand at the door, no grip: nothing");
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::Take && v.ref == kDoor && !v.left && st.phase == DoorGripPhase::Holding,
	      "the grip closes there: taken (the tick)");
	Check(Near(st.startNormalMetres, 0.05f), "its normal coordinate kept");
	f.heldKnown = true;
	f.heldNormalMetres = 0.10f;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None && st.phase == DoorGripPhase::Holding, "5 cm along: still held");
	f.heldNormalMetres = 0.05f + s.moveMetres + 0.001f;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::Open && v.ref == kDoor && st.phase == DoorGripPhase::Done,
	      "past the threshold, pulled: opened (as A)");
	f.heldNormalMetres = 0.5f;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None && st.phase == DoorGripPhase::Done, "once only while the grip stays");
	f.gripDown[0] = false;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None && st.phase == DoorGripPhase::Idle, "the grip opened: idle again");

	std::printf("The step: pushed\n");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	StepDoorGrip(st, s, f);
	f.heldKnown = true;
	f.heldNormalMetres = 0.05f - s.moveMetres - 0.001f;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::Open, "past the threshold the other way: opened (which way it opens is not known)");

	std::printf("The step: let go, cancelled\n");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	StepDoorGrip(st, s, f);
	f.heldKnown = true;
	f.heldNormalMetres = 0.09f;
	f.gripDown[0] = false;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::LetGo && st.phase == DoorGripPhase::Idle, "the grip opened before the threshold: let go, shut");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	StepDoorGrip(st, s, f);
	f.heldGone = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::LetGo && st.phase == DoorGripPhase::Idle, "the door gone: let go");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	StepDoorGrip(st, s, f);
	f.heldGone = false;
	f.heldKnown = true;
	f.heldNormalMetres = 0.05f;
	f.menuUp = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::LetGo, "a menu came up: let go");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	StepDoorGrip(st, s, f);
	f.handFree[0] = false;
	f.heldKnown = true;
	f.heldNormalMetres = 0.5f;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::LetGo, "the hand took something meanwhile: let go, not opened");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	StepDoorGrip(st, s, f);
	f.heldKnown = false;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None && st.phase == DoorGripPhase::Holding,
	      "the held door unmeasured for a frame: held on, nothing decided");

	std::printf("The step: what is never taken\n");
	st = DoorGripState{};
	f = Ready();
	f.gripDown[0] = true;
	f.candidate = 0;
	StepDoorGrip(st, s, f);
	f.candidate = kDoor;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None && st.phase == DoorGripPhase::Idle,
	      "a grip carried to the door already closed: not taken (only a grip closing there)");
	st = DoorGripState{};
	f = Ready();
	f.itemFirst = true;
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None, "an item under the same hand goes first");
	st = DoorGripState{};
	f = Ready();
	f.handFree[0] = false;
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None, "a hand holding something: not taken");
	st = DoorGripState{};
	f = Ready();
	f.enabled = false;
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None, "switched off ([Hands] DoorsByGrip=0): nothing (A stays)");
	st = DoorGripState{};
	f = Ready();
	f.menuUp = true;
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None, "a menu up: not taken");
	st = DoorGripState{};
	f = Ready();
	f.candidate = 0;
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None, "no door at the hand (a non-door, nothing): nothing");

	std::printf("The step: the left hand\n");
	st = DoorGripState{};
	f = Ready();
	f.candidateLeft = true;
	f.gripDown[0] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::None, "the right grip, the door at the left hand: not taken");
	f.gripDown[1] = true;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::Take && v.left, "the left grip closing: taken by the left");
	f.gripDown[0] = false;
	f.heldKnown = true;
	f.heldNormalMetres = 0.05f + s.moveMetres + 0.01f;
	v = StepDoorGrip(st, s, f);
	Check(v.action == DoorGripAction::Open && v.left, "the right grip let go does not matter: the left opens it");

	std::printf("The threshold as used\n");
	Check(Near(DoorMoveMetresFor(0.12f), 0.12f), "0.12 m kept");
	Check(Near(DoorMoveMetresFor(0.01f), kDoorLeastMoveMetres), "under the least: the least");
	Check(Near(DoorMoveMetresFor(std::numeric_limits<float>::quiet_NaN()), 0.12f), "not a number: 0.12 m");
	Check(Near(DoorMoveMetresFor(2.0f), kDoorMostMoveMetres), "over the most: the most");
}

}  // namespace

int main() {
	TestThinAxis();
	TestZone();
	TestFreeHand();
	TestFlows();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

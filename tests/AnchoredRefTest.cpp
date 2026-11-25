// Checks what a closed grip may take into the engine's grab
// (game/AnchoredRef.h): a thing with a fixed part - a training dummy on its
// beam, a sign on its bracket - is not carried, because the hold would put
// its frame into the hand; a thing whose bodies are all free is, whatever
// their number; an actor always. And the answer kept while a grip stays on
// one reference: a wrong keep either walks the thing's nodes every frame or
// answers for another thing.

#include <cstdio>
#include <initializer_list>

#include "game/AnchoredRef.h"

namespace {

using obvr::game::BodyCensus;
using obvr::game::CarryMemo;
using obvr::game::CountBody;
using obvr::game::GripMayCarry;
using obvr::game::IsActorFormType;
using obvr::game::IsAnchoredThing;
using obvr::game::kCarryAskFrames;
using obvr::game::MayCarryKept;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// Havok's motion types as the engine numbers them: 1 to 5 the moving kinds
// (2 sphere inertia, 4 box inertia are what clutter and a dummy's parts have),
// 6 keyframed, 7 fixed.
BodyCensus Census(std::initializer_list<UInt32> motionTypes) {
	BodyCensus c;
	for (const UInt32 type : motionTypes) {
		CountBody(c, type);
	}
	return c;
}

void TestCensus() {
	std::printf("The census of a thing's bodies\n");
	BodyCensus c = Census({});
	Check(c.bodies == 0 && c.anchored == 0, "no body: nothing counted");
	c = Census({4});
	Check(c.bodies == 1 && c.anchored == 0, "one free body");
	c = Census({7});
	Check(c.bodies == 1 && c.anchored == 1, "a fixed body is anchored");
	c = Census({6});
	Check(c.bodies == 1 && c.anchored == 1, "a keyframed body is anchored");
	c = Census({1, 2, 3, 4, 5});
	Check(c.bodies == 5 && c.anchored == 0, "every moving kind is free");
	c = Census({0, 8, 255});
	Check(c.bodies == 3 && c.anchored == 0, "a kind the engine does not have is not taken for a fixed one");
	// ChainDoll01: 5 fixed (the beam, two supports, two feet), 18 hanging.
	c = Census({7, 7, 7, 7, 7, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4});
	Check(c.bodies == 23 && c.anchored == 5, "the training dummy: 23 bodies, 5 of them fixed");
}

void TestWhatTheGripMayCarry() {
	std::printf("What the grip may carry\n");
	Check(GripMayCarry(Census({4}), false), "a thing of one free body: carried");
	Check(GripMayCarry(Census({2, 2, 2, 2}), false), "an amulet's chain, all free: carried");
	Check(GripMayCarry(Census({}), false), "nothing read: carried, as before");
	Check(!GripMayCarry(Census({7, 4, 4}), false), "a fixed part and hanging ones (a dummy, a sign): not carried");
	Check(!GripMayCarry(Census({4, 4, 7}), false), "the fixed part read last, on a child node: not carried");
	Check(!GripMayCarry(Census({6, 4}), false), "a keyframed part: not carried");
	Check(GripMayCarry(Census({7}), false),
	      "a wall or the floor, all of it fixed: the key as ever - the engine refuses it, and takes the item on it");
	Check(GripMayCarry(Census({7, 7, 7}), false), "a room piece of several fixed bodies: the same");
	Check(GripMayCarry(Census({6}), false), "a door, an arrow stuck in wood (keyframed): the same");
	Check(!GripMayCarry(Census({6, 6, 4}), false), "keyframed parts and one free: not carried");
	Check(GripMayCarry(Census({7, 4}), true), "an actor is dragged whatever its bones are");
	Check(GripMayCarry(Census({4}), true), "a body on the ground: dragged");

	Check(!IsAnchoredThing(Census({})) && !IsAnchoredThing(Census({4, 4})) && !IsAnchoredThing(Census({7, 6})),
	      "nothing read, all free, all fixed: no frame with parts hanging from it");
	Check(IsAnchoredThing(Census({7, 4})) && IsAnchoredThing(Census({4, 6})),
	      "fixed or keyframed beside free: one");

	Check(IsActorFormType(0x23) && IsActorFormType(0x24), "a person and a creature are actors");
	Check(!IsActorFormType(0x22) && !IsActorFormType(0x25) && !IsActorFormType(0x1C) && !IsActorFormType(0),
	      "ammo, a levelled list, a static, nothing: not");
}

void TestTheAnswerKept() {
	std::printf("The answer kept while the grip stays on one thing\n");
	UInt32 asked = 0;
	UInt32 askedOf = 0;
	bool answer = false;
	const auto ask = [&](UInt32 ref) {
		++asked;
		askedOf = ref;
		return answer;
	};
	CarryMemo m;
	Check(!MayCarryKept(m, 0x1000, ask) && asked == 1 && askedOf == 0x1000 && m.fresh,
	      "the first frame on a thing: asked, and of that thing");
	answer = true;  // would be the answer now - not asked yet
	for (UInt32 frame = 0; frame < kCarryAskFrames; ++frame) {
		Check(!MayCarryKept(m, 0x1000, ask) && asked == 1 && !m.fresh, "the frames after: the kept answer");
	}
	Check(MayCarryKept(m, 0x1000, ask) && asked == 2 && m.fresh, "grown old: asked again, the new answer");
	answer = false;
	Check(!MayCarryKept(m, 0x2000, ask) && asked == 3 && askedOf == 0x2000 && m.fresh,
	      "another thing: asked at once");
	answer = true;
	Check(MayCarryKept(m, 0x1000, ask) && asked == 4 && askedOf == 0x1000,
	      "back to the first: asked again, never the other's answer");
	Check(MayCarryKept(m, 0x1000, ask) && asked == 4, "and kept from there");
}

}  // namespace

int main() {
	TestCensus();
	TestWhatTheGripMayCarry();
	TestTheAnswerKept();
	std::printf(g_failures == 0 ? "all passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}

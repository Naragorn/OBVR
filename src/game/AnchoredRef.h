#pragma once

// What a closed grip may take into the engine's grab, and what a hold may
// put into the hand: nothing with a fixed part (a Quest 2 tester, 2026-11-25:
// "Training dummies and some signs ... seem to give a connected beam of wood
// or iron that is a physics object, as if its part of the model and its
// breaking it").
//
// A training dummy, a heavy bag, a hinged sign, the prison's chains: one
// reference of many rigid bodies joined by constraints, a few of them fixed -
// the beam, the post, the bracket - and the rest hanging from those
// (ChainDoll01: 23 bodies, 5 fixed, 21 constraints; docs/holding-objects-spec.md,
// "Things with a fixed part"). The engine's grab takes one hanging part of
// it, which vanilla then tugs on its spring. OBVR's hold is written for a
// thing of one free body: it puts the reference's root node on the hand
// (game/HeldObject.cpp), gives the grabbed body the player's group and sets
// it to the root's pose when it is let go (game/GrabPhysics.cpp). On such a
// reference the root node is the frame: the beam was drawn in the hand, and
// the part let go was set to the frame's place. A fist is a closed grip, so
// every punch within the grab's reach could start it.
//
// The rule: a reference that has a fixed or keyframed rigid body under its
// node and free ones beside it - a frame with parts hanging from it - is
// anchored. The grip's grab key stays up on it - the hand's own bodies push
// its parts, a fist punches it - and a hold of it that began another way (the
// grab key itself) is left on the engine's spring. An actor is never anchored
// by this: a body is dragged as ever. A thing of several bodies that are all
// free (an amulet, a cuirass on the ground) is carried as ever too.
//
// A thing whose bodies are all fixed or keyframed - a wall, the floor, a door,
// an arrow stuck in wood - is not anchored in this sense: the engine's grab
// refuses it by itself, and the key has to go down all the same, because the
// pick through a hand at an item on the floor often meets the floor first and
// the engine's own grab then takes the item (weapon-pull.txt failed on
// 2026-11-25 with the key kept up there).
//
// Pure, covered by anchored_ref_test; the walk is game/AnchoredRef.cpp's.

#include "core/Types.h"
#include "game/GrabPhysics.h"

namespace obvr::game {

// The rigid bodies under a reference's node.
struct BodyCensus {
	UInt32 bodies = 0;    // read
	UInt32 anchored = 0;  // of them fixed or keyframed
};

inline void CountBody(BodyCensus& c, UInt32 motionType) {
	++c.bodies;
	if (motionType == kMotionTypeKeyframed || motionType == kMotionTypeFixed) {
		++c.anchored;
	}
}

// A person or a creature (xOBSE GameForms.h: NPC 0x23, creature 0x24).
inline bool IsActorFormType(UInt8 type) { return type == 0x23 || type == 0x24; }

// A frame with parts hanging from it: fixed or keyframed bodies and free ones
// in one reference.
inline bool IsAnchoredThing(const BodyCensus& c) { return c.anchored > 0 && c.bodies > c.anchored; }

// Whether the grip's grab key may go down on it, and a hold may place it in
// the hand. With no body read at all - nothing there, or the hands' bodies not
// yet made to tell a rigid body by - the answer is yes, as before.
inline bool GripMayCarry(const BodyCensus& c, bool actor) { return actor || !IsAnchoredThing(c); }

// The census of a reference, node by node (at most kCensusMaxNodes of them).
BodyCensus CensusBodiesOf(UInt32 ref);

// GripMayCarry of the reference as it is now.
bool RefMayBeCarried(UInt32 ref);

// The answer kept per reference while a grip is held on it: asked when the
// reference changes and again every kCarryAskFrames frames - a thing's bodies
// can change (an arrow lands), but not from one frame to the next.
inline constexpr UInt32 kCarryAskFrames = 15;

struct CarryMemo {
	UInt32 ref = 0;
	bool mayCarry = true;
	UInt32 framesLeft = 0;
	bool fresh = false;  // asked this frame, for a line in the log
};

template <typename Ask>
bool MayCarryKept(CarryMemo& m, UInt32 ref, Ask ask) {
	m.fresh = m.ref != ref || m.framesLeft == 0;
	if (m.fresh) {
		m.ref = ref;
		m.mayCarry = ask(ref);
		m.framesLeft = kCarryAskFrames;
	} else {
		--m.framesLeft;
	}
	return m.mayCarry;
}

}  // namespace obvr::game

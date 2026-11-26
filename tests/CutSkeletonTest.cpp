// game/CutSkeletonLogic.h: a cut on skeletons other than the human one (the
// tester, 2026-11-26: goblins kept their heads, a goblin's arm stretched), and
// the limb cut's shapes that must not go whole to the part.

#include <cstdio>

#include "game/CutSkeletonLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::game;

// Bodies by number, as the engine's bhk wrappers would be.
constexpr UInt32 kHead = 0x100;
constexpr UInt32 kNeck = 0x200;
constexpr UInt32 kSpine2 = 0x300;
constexpr UInt32 kJaw = 0x400;

void TestJoints() {
	std::printf("which joints held by the part's body are released\n");
	// The goblin's skeleton.nif (vanilla and the Linux ragdoll mod's): Bip01 Head holds
	// bhkRagdollConstraint(Head, Neck), Neck its own body hinged to Spine2. The cut looked for a joint to
	// Spine2 and released none: "2B009318's head cut - 0 neck joint(s) released" (OBVR.log 2026-11-26).
	Check(ReleasesPartJoint(kHead, kHead, kNeck, true, false), "the goblin's head to its neck's body: released");
	Check(ReleasesPartJoint(kHead, kHead, kSpine2, true, false), "a human head to Spine2: released, as before");
	Check(ReleasesPartJoint(kHead, kNeck, kHead, false, true), "the part's body as the joint's second: released");
	Check(!ReleasesPartJoint(kHead, kHead, kJaw, true, true),
	      "a joint to a body under the part (a jaw the head holds): kept, it goes with the part");
	Check(!ReleasesPartJoint(kHead, kJaw, kHead, true, true), "the same the other way round: kept");
	Check(!ReleasesPartJoint(kHead, kNeck, kSpine2, false, false), "a joint between two other bodies: kept");
	Check(!ReleasesPartJoint(kHead, kHead, kHead, true, true), "a joint of the part's body to itself: kept");
	Check(!ReleasesPartJoint(kHead, kHead, 0, true, false) && !ReleasesPartJoint(kHead, 0, kHead, false, true),
	      "a joint with no second body: kept");
	Check(!ReleasesPartJoint(0, 0, kNeck, false, false) && !ReleasesPartJoint(0, kHead, kNeck, false, false),
	      "the part has no body: nothing released");
}

void TestStump() {
	std::printf("the bone the stump follows\n");
	// The head hangs from Neck1 in the vanilla, the Windows and the Linux install's skeleton.nif, from Neck in
	// the goblin's (which has no Neck1), from Neck2 in a dog's (which has a Neck1 further down).
	Check(StumpBoneFrom(true, true, true) == StumpFrom::Parent, "the head: the bone it hangs from (Neck1 in a human)");
	Check(StumpBoneFrom(true, false, true) == StumpFrom::Parent, "the goblin's head, no Neck1: its parent, Neck");
	Check(StumpBoneFrom(true, true, false) == StumpFrom::Named, "the head's parent not read: the named Neck1");
	Check(StumpBoneFrom(true, false, false) == StumpFrom::Joint, "neither: the joint's body, as before");
	// The installed human skeletons put scale nodes between the limbs ("Scale L Hand" is the hand's parent).
	Check(StumpBoneFrom(false, true, true) == StumpFrom::Named, "a limb: the bone named for it, not a scale node");
	Check(StumpBoneFrom(false, false, true) == StumpFrom::Parent, "a limb whose named bone is missing: its parent");
	Check(StumpBoneFrom(false, false, false) == StumpFrom::Joint, "a limb with neither: the joint's body");
}

void TestJointBone() {
	std::printf("the body the part is joined to\n");
	Check(JointBoneFrom(true, true) == JointFrom::Named && JointBoneFrom(true, false) == JointFrom::Named,
	      "the named one (Spine2 for the head) when the skeleton has it");
	Check(JointBoneFrom(false, true) == JointFrom::StumpParent,
	      "an ogre, no Spine2: the bone its neck hangs from (Spine1, its head's joint)");
	Check(JointBoneFrom(false, false) == JointFrom::None, "neither: none - the cut is refused");
}

void TestNeckLow() {
	std::printf("the neck's low end for the blade\n");
	Check(NeckLowFrom(true, true) == NeckLow::Spine2 && NeckLowFrom(true, false) == NeckLow::Spine2,
	      "Spine2 when there (a human, a goblin): as before");
	Check(NeckLowFrom(false, true) == NeckLow::HeadGrandparent, "no Spine2 (an ogre): two bones up from the head");
	Check(NeckLowFrom(false, false) == NeckLow::None, "neither: no neck");
}

void TestBonesGoing() {
	std::printf("which bones of a part-side shape go with the part\n");
	// headCut, isPart, underPart, holdsPart, holdsOther, underStump, inOther, shapeHasOther
	Check(PartSideBoneFlies(false, true, false, false, false, false, false, true), "the part's own bone");
	Check(PartSideBoneFlies(false, false, true, false, false, false, false, true), "a bone under it (a finger)");
	Check(PartSideBoneFlies(false, false, false, true, false, false, false, true),
	      "a bone holding the part and not the other limb (the clavicle at an arm cut)");
	Check(PartSideBoneFlies(false, false, false, false, false, true, false, true),
	      "a twist beside the cut limb's chain");
	Check(!PartSideBoneFlies(false, false, false, true, true, false, false, true),
	      "the middle both limbs hang from (Spine2), the other limb in the shape: stays");
	Check(PartSideBoneFlies(false, false, false, true, true, false, false, false),
	      "the middle in a shape of the cut limb alone (a zombie's arm with Spine2): goes");
	Check(!PartSideBoneFlies(false, false, false, false, false, false, true, true), "the other limb's: stays");
	Check(!PartSideBoneFlies(false, false, false, false, false, false, false, true),
	      "a bone holding neither (the head at an arm cut): stays");
	Check(PartSideBoneFlies(true, false, false, false, false, false, false, false),
	      "a head cut: every other bone of the head's shapes goes to the head");
}

void TestSpanning() {
	std::printf("the vertices that would stretch between the part and the body\n");
	const CutWeight going[] = {{0, 0, 1.0f}, {1, 0, 0.6f}, {2, 0, 0.005f}, {9, 0, 0.5f}};
	const CutWeight staying[] = {{1, 0, 0.4f}, {2, 0, 0.995f}, {3, 0, 1.0f}};
	UInt8 marks[8] = {};
	CutBoneWeights bones[] = {{going, 4, true}, {staying, 3, false}};
	// vertex 1 hangs on both, 2 only by 0.5 % on the part's bone, 9 is past the shape's 8 vertices
	Check(VerticesSpanningCut(bones, 2, 8, marks, kSpanLeastWeight) == 1,
	      "one vertex on both sides by more than a hundredth: 1 (a sliver under it and a vertex past the shape not counted)");
	const CutWeight alsoGoing[] = {{1, 0, 0.3f}, {0, 0, 0.0f}};
	CutBoneWeights three[] = {{going, 4, true}, {alsoGoing, 2, true}, {staying, 3, false}};
	Check(VerticesSpanningCut(three, 3, 8, marks, kSpanLeastWeight) == 1, "a vertex counted once, whatever its bones");
	CutBoneWeights partOnly[] = {{going, 4, true}, {alsoGoing, 2, true}};
	Check(VerticesSpanningCut(partOnly, 2, 8, marks, kSpanLeastWeight) == 0, "bones going only (a hand's mesh): none");
	CutBoneWeights bodyOnly[] = {{staying, 3, false}};
	Check(VerticesSpanningCut(bodyOnly, 1, 8, marks, kSpanLeastWeight) == 0, "bones staying only: none");
	UInt8 dirty[8] = {3, 3, 3, 3, 3, 3, 3, 3};
	Check(VerticesSpanningCut(bodyOnly, 1, 8, dirty, kSpanLeastWeight) == 0 && dirty[4] == 0,
	      "the marks are zeroed first: a call before leaves nothing behind");
	const CutWeight nan[] = {{1, 0, -1.0f}};
	CutBoneWeights odd[] = {{going, 4, true}, {nan, 1, false}};
	Check(VerticesSpanningCut(odd, 2, 8, marks, kSpanLeastWeight) == 0, "a weight under nothing: not counted");
	Check(VerticesSpanningCut(bones, 0, 8, marks, kSpanLeastWeight) == 0 && VerticesSpanningCut(bones, 2, 0, marks, kSpanLeastWeight) == 0,
	      "no bones, no vertices: none");
}

void TestLimbSide() {
	std::printf("a limb cut's shape taken whole to the part only when nothing of it would stretch\n");
	// The goblin witch's ShamanChest at its right arm cut (OBVR.log 2026-11-26 line 25547, "Shamanchest(head,
	// 15/38)"): 15 of its 38 bones the arm's - 12 of them fingers - so the part's by its bones; its clavicle went to
	// the part, Spine2 and the neck stayed, and its shoulder's 56 vertices on both (pyffi, the vanilla
	// shamanchest.nif) stretched from the body to the arm on the ground.
	Check(ShapeSideFor(false, false, 15, 38, false) == ShapeSide::Part, "the robe by its bones: the part's (before)");
	Check(LimbShapeSideFor(ShapeSide::Part, false, false, SpanRead::Spans) == ShapeSide::Body,
	      "its vertices span the cut: the body's");
	Check(PartCopyWanted(false, LimbShapeSideFor(ShapeSide::Part, false, false, SpanRead::Spans), 15, false),
	      "and drawn again for the part, its sleeve with the arm");
	Check(LimbShapeSideFor(ShapeSide::Part, false, false, SpanRead::None) == ShapeSide::Part,
	      "a hand's mesh (the goblin's GoblinHandR, 0 of 1035 vertices on both): whole to the part");
	Check(LimbShapeSideFor(ShapeSide::Part, false, false, SpanRead::Unread) == ShapeSide::Body,
	      "a skin that did not read: the body's - drawn again is right for any shape");
	Check(LimbShapeSideFor(ShapeSide::Body, false, false, SpanRead::Spans) == ShapeSide::Body,
	      "the body's by its bones: stays the body's");
	Check(LimbShapeSideFor(ShapeSide::Part, true, false, SpanRead::Spans) == ShapeSide::Part,
	      "a head cut: its part side keeps nothing on the body, never changed");
	Check(LimbShapeSideFor(ShapeSide::Part, false, true, SpanRead::Spans) == ShapeSide::Part,
	      "a shape holding the cut's own node keeps the side it was given");
	std::printf("whether a shape's skin is read for it\n");
	Check(SpanWanted(ShapeSide::Part, false, false, true), "a limb cut, a skinned shape the part's by bones: read");
	// The first build read the rigid ones too - the witch's head and helmet, the part's by their bound - and
	// logged their skin unread at every rescan (dismember-goblin-witch, 2026-11-26 16:06).
	Check(!SpanWanted(ShapeSide::Part, false, false, false), "a rigid shape (no bones read, its side by its bound): not read");
	Check(!SpanWanted(ShapeSide::Body, false, false, true), "the body's by bones: not read");
	Check(!SpanWanted(ShapeSide::Part, true, false, true), "a head cut: not read");
	Check(!SpanWanted(ShapeSide::Part, false, true, true), "holding a cut node (a rescan): not read again");
	std::printf("whether its bones can span the cut\n");
	Check(SpanPossible(true, true), "bones going and staying: its weights are read");
	Check(!SpanPossible(true, false), "every bone going (a hand's mesh on a forearm cut): it goes whole");
	Check(!SpanPossible(false, true) && !SpanPossible(false, false), "no bone going: nothing to stretch");
}

}  // namespace

int main() {
	TestJoints();
	TestStump();
	TestJointBone();
	TestNeckLow();
	TestBonesGoing();
	TestSpanning();
	TestLimbSide();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}

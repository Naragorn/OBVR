#pragma once

// A cut on a skeleton that is not the human one (the tester, 2026-11-26:
// "beheadden für ratten goblins ... kam nix. bei goblins blieb der kopf dran";
// "infinity arme bei creatures also goblins"). Pure, covered by cut_skeleton_test;
// game/Dismember.cpp and game/CutJoints.cpp read the engine.
//
// Read with pyffi from every vanilla and Shivering Isles creature skeleton.nif
// and the human ones (docs/dismemberment-spec.md, "2026-11-26: creatures"):
// - the head's ragdoll joint is not always to Spine2: the goblin's, a bear's,
//   a rat's skull's go to their Neck's body, a sheep's to Neck1, a dog's or a
//   deer's to Neck2, an ogre's to Spine1 (it has no Spine2). In every skeleton
//   read, the part's own body holds its joint to the body it hangs from.
// - the goblin has no Neck1: its head hangs from Neck. The human skeletons'
//   head hangs from Neck1 (the vanilla, the Windows and the Linux install's
//   skeleton.nif), but their limbs from scale nodes ("Scale L Hand" is the
//   hand's parent there), so a limb's stump keeps the bone named for it.

#include "core/Types.h"
#include "game/CutSideLogic.h"

namespace obvr::game {

// A joint held in the part's own body's list is the cut's when it links the
// part's body to a body outside the part: `partBody` the part's own (0 when it
// has none), `a` and `b` the joint's two bodies (0 for none), `aInPart` and
// `bInPart` whether each is the part's or a body under it - those go with the
// part (a jaw's joint held by the head would stay).
inline bool ReleasesPartJoint(UInt32 partBody, UInt32 a, UInt32 b, bool aInPart, bool bInPart) {
	if (partBody == 0) {
		return false;
	}
	if (a == partBody) {
		return b != 0 && b != partBody && !bInPart;
	}
	if (b == partBody) {
		return a != 0 && !aInPart;
	}
	return false;
}

// The bone the stump's collapsed vertices follow: for the head the bone it
// hangs from (its parent - Neck1 in the human skeletons, Neck in the
// goblin's, Neck2 in a dog's), for a limb the bone named for it
// (DismemberLogic.h BonesOfCut), each falling back on the other, and on the
// joint's body when neither is there (the cut as it was before 2026-11-26).
enum class StumpFrom : UInt8 { Named, Parent, Joint };

inline StumpFrom StumpBoneFrom(bool head, bool namedThere, bool parentThere) {
	if (head) {
		return parentThere ? StumpFrom::Parent : (namedThere ? StumpFrom::Named : StumpFrom::Joint);
	}
	return namedThere ? StumpFrom::Named : (parentThere ? StumpFrom::Parent : StumpFrom::Joint);
}

// The body a cut's part is joined to, for the side of a shape read by its
// bound and the rescans: the bone named for it (Spine2 for the head), else the
// one the stump hangs from (an ogre has no Spine2: its head hangs from Neck,
// Neck from Spine1, its joint's body), else none - the cut is refused.
enum class JointFrom : UInt8 { Named, StumpParent, None };

inline JointFrom JointBoneFrom(bool namedThere, bool stumpParentThere) {
	if (namedThere) {
		return JointFrom::Named;
	}
	return stumpParentThere ? JointFrom::StumpParent : JointFrom::None;
}

// The neck's low end a blade is tested against (Spine2's origin to the
// head's): Spine2, else the bone two up from the head (an ogre's Spine1, a
// land dreugh's Spine03), else no neck.
enum class NeckLow : UInt8 { Spine2, HeadGrandparent, None };

inline NeckLow NeckLowFrom(bool spine2There, bool grandparentThere) {
	if (spine2There) {
		return NeckLow::Spine2;
	}
	return grandparentThere ? NeckLow::HeadGrandparent : NeckLow::None;
}

// A bone of a shape on the part's side that goes with the part: the part's own
// and what hangs under it, and the ones the swap points at the cut's part
// node (DismemberLogic.h SwapBoneOnPartSide); every other bone stays where the
// body is.
inline bool PartSideBoneFlies(bool headCut, bool isPart, bool underPart, bool holdsPart, bool holdsOther,
                              bool underStump, bool inOther, bool shapeHasOther) {
	return isPart || underPart ||
	       SwapBoneOnPartSide(headCut, isPart, underPart, holdsPart, holdsOther, underStump, inOther, shapeHasOther);
}

// One weight of NiSkinData's bone data as the engine keeps it (8 bytes: the
// vertex, two bytes, the weight; game/BodyView.cpp has where it was read), and
// one bone's list of them with whether that bone goes with the part.
struct CutWeight {
	UInt16 vertex = 0;
	UInt16 unused = 0;
	float weight = 0.0f;
};
static_assert(sizeof(CutWeight) == 8, "NiSkinData's weight is 8 bytes");

struct CutBoneWeights {
	const CutWeight* weights = nullptr;
	UInt32 count = 0;
	bool flies = false;
};

// The vertices of a shape that hang on both a bone that goes with the part
// and one that stays, each by at least `least` of its weight: taken whole to
// the part such a vertex stretches from the body to wherever the part flies.
// `marks` holds a byte a vertex, `vertexCount` of them, and is zeroed here; a
// weight naming a vertex past the count is not counted.
inline UInt32 VerticesSpanningCut(const CutBoneWeights* bones, UInt32 boneCount, UInt32 vertexCount, UInt8* marks,
                                  float least) {
	for (UInt32 v = 0; v < vertexCount; ++v) {
		marks[v] = 0;
	}
	UInt32 spanning = 0;
	for (UInt32 b = 0; b < boneCount; ++b) {
		const UInt8 side = bones[b].flies ? 1 : 2;
		for (UInt32 k = 0; k < bones[b].count; ++k) {
			const CutWeight& w = bones[b].weights[k];
			if (w.vertex >= vertexCount || !(w.weight >= least)) {
				continue;
			}
			const UInt8 before = marks[w.vertex];
			marks[w.vertex] = static_cast<UInt8>(before | side);
			spanning += before != 3 && marks[w.vertex] == 3 ? 1 : 0;
		}
	}
	return spanning;
}

// The least weight on each side that counts a vertex as hanging on both: a
// hundredth (a part thrown 2 m drags such a vertex 2 cm).
inline constexpr float kSpanLeastWeight = 0.01f;

// What reading a shape's skin for the span said.
enum class SpanRead : UInt8 { None, Spans, Unread };

// A limb cut's shape taken for the part's by its bones (ShapeSideFor) goes to
// the part whole - its bones that stay, the body's middle and the other limb's,
// with the body. When any of its vertices hangs on both, that shape would
// stretch from the body to the flying part (Nadi, 2026-11-26: "infinity arme
// bei creatures also goblins" - the goblin witch's robe, 15 of its 38 bones
// the right arm's, 12 of them fingers: its shoulder's 56 vertices on the
// clavicle and the upper arm and on Spine2 and the neck). Such a shape is the
// body's: its part collapses into the stump, and it is drawn again for the
// part (PartCopyWanted). So is one whose skin did not read - drawn again is
// right for any shape; whole to the part only for one that does not span.
// A head's part side sends every bone but the head's to the head, so nothing
// of it stays; a shape holding the cut's own node keeps the side it was given.
inline ShapeSide LimbShapeSideFor(ShapeSide byBones, bool headCut, bool holdsCutNode, SpanRead span) {
	if (headCut || holdsCutNode || byBones != ShapeSide::Part) {
		return byBones;
	}
	return span == SpanRead::None ? ShapeSide::Part : ShapeSide::Body;
}

// Whether a shape's skin needs reading for the span at all: only a skinned
// one on the part's side by its bones, on a limb cut, holding no cut node. A
// rigid shape (a goblin's head, a helmet: no bones read, its side by its
// bound) has nothing to stretch - the first build read those too and called
// their skin unread, 2026-11-26 16:06.
inline bool SpanWanted(ShapeSide byBones, bool headCut, bool holdsCutNode, bool skinned) {
	return !headCut && !holdsCutNode && skinned && byBones == ShapeSide::Part;
}

// And whether its bones can span the cut at all: some going with the part and
// some staying (a hand's own mesh on a forearm cut goes whole).
inline bool SpanPossible(bool anyFlies, bool anyStays) { return anyFlies && anyStays; }

// The head bone's names, asked in this order: "Bip01 Head", else "Bip01
// Skull" - the rat's, the clannfear's and the daedroth's head bone (pyffi on
// their skeleton.nif, docs/dismemberment-spec.md "2026-11-26: beasts"), whose
// neck was never found (the tester's rats: "its sweep on misses the neck").
// Null past the last.
inline const char* HeadBoneName(UInt32 i) {
	switch (i) {
	case 0: return "Bip01 Head";
	case 1: return "Bip01 Skull";
	default: return nullptr;
	}
}

// The vertices of a shape that hang on the bones going with the part (the
// head and what hangs under it) by at least half of their weight, summed:
// `sums` holds a UInt16 a vertex, `vertexCount` of them, zeroed here, each
// weight added in thousandths (rounded; saturating). A weight naming a vertex
// past the count, or not above 0, is not counted.
inline constexpr UInt32 kMostlyThousandths = 500;

inline UInt32 VerticesMostlyOnPart(const CutBoneWeights* bones, UInt32 boneCount, UInt32 vertexCount, UInt16* sums) {
	for (UInt32 v = 0; v < vertexCount; ++v) {
		sums[v] = 0;
	}
	UInt32 mostly = 0;
	for (UInt32 b = 0; b < boneCount; ++b) {
		if (!bones[b].flies) {
			continue;
		}
		for (UInt32 k = 0; k < bones[b].count; ++k) {
			const CutWeight& w = bones[b].weights[k];
			if (w.vertex >= vertexCount || !(w.weight > 0.0f)) {
				continue;
			}
			const UInt32 add = w.weight < 1.0f ? static_cast<UInt32>(w.weight * 1000.0f + 0.5f) : 1000u;
			const UInt32 before = sums[w.vertex];
			const UInt32 after = before + add < 0xFFFFu ? before + add : 0xFFFFu;
			sums[w.vertex] = static_cast<UInt16>(after);
			mostly += before < kMostlyThousandths && after >= kMostlyThousandths ? 1 : 0;
		}
	}
	return mostly;
}

// A head cut's shape of the body that holds part of the head, drawn again for
// the head (the tester, 2026-11-26: "go" to more creatures losing their
// heads). A troll's, an imp's, a deer's head is no mesh of its own but part of
// the body's one skinned mesh: the body's by its bones (a troll's 13 of 61),
// its head's vertices collapsed into the stump and only the rigid hair, horns
// or eyes flew. Such a shape is copied before the swap, as a limb's is
// (CutSideLogic.h PartCopyWanted), the copy keeping the head's bones and
// sending every other to the head's own node.
// Which shapes: on a creature, those whose vertices hang on the head bone
// or under it by at least half their weight, as a share of all its vertices
// - measured with pyffi (tools/nif-head-share.py) on every vanilla, Shivering
// Isles and installed creature mesh (docs/dismemberment-spec.md "2026-11-26:
// beasts"): a head in the body's mesh from 0.050 (the elytra, 219 of 4340
// vertices) and 0.060 (a mod's centurion) to 0.61; the rest of a body around
// a rigid head of its own from 0.003 to 0.029 (a boar's neck) - and to 0.13
// (a wolf's head's back and neck, the bear's, the minotaur's, the rat's skull
// and ears), which go with the head too. A fortieth (0.025): half the
// elytra's.
// Never on an NPC: its head is its own mesh (FaceGenFace, the head's side
// already), and no share tells a collar from a head - the middleclass/02
// shirt's collar is 0.151 (116 of 769), the installed robe hoods' (9 bones,
// the body's by the bone rule) 0.416, the Chorrol guard's helmet 0.87, the
// emperor's dress collar 0.58, a frost atronach's head 0.102. Their head
// vertices collapse into the stump as before.
inline constexpr UInt32 kHeadCopyShareParts = 40;  // a share of 1/40 or more

// Whether a shape's skin is read for its share of the head: a skinned shape
// of the body's side at a creature's head cut, holding no cut node (a rescan:
// the original's head bones point at the stump, the copy's other bones at the
// part), and no decal shape (PartCopyWanted).
inline bool HeadShareWanted(bool headCut, bool creature, ShapeSide side, bool holdsCutNode, bool skinned, bool decal) {
	return headCut && creature && side == ShapeSide::Body && !holdsCutNode && skinned && !decal;
}

// And whether the share read copies it: at least one vertex, and its share
// a fortieth or more (counted in whole numbers: 25 of 1000 is).
inline bool HeadShareCopies(UInt32 mostly, UInt32 vertices) {
	return mostly > 0 && mostly <= vertices && mostly * kHeadCopyShareParts >= vertices;
}

// The base form's type (TESForm +0x04, xOBSE GameForms.h FormType): a
// creature's (an NPC's is 0x23).
inline constexpr UInt8 kCreatureFormType = 0x24;

inline bool IsCreatureFormType(UInt8 type) { return type == kCreatureFormType; }

}  // namespace obvr::game

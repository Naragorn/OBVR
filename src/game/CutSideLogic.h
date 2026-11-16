#pragma once

// Two decisions of a standing cut (game/Dismember.cpp), pure, covered by
// dismember_test: which side of the cut a skinned shape is on, and what
// becomes of a cut whose check no longer holds.

#include "core/Types.h"
#include "game/DismemberLogic.h"

namespace obvr::game {

// A shape's side of a cut - the same at every reading. A cut's skins are read
// again every kCutRescanSeconds, and a shape swapped before no longer has the
// bones it had: the part's shapes' other bones point at the cut's part node,
// the body's shapes' part bones at its stump node. So a shape holding one of
// the cut's own nodes keeps the side that node says; only a shape holding
// neither (drawn since the cut: a blood decal, clothes put on) is taken by
// its bones (ShapeIsHeadSide) or, with no bones read, by its bound.
// Read by the ratio alone, FaceGenFace - the head and Neck1 of five bones, 2
// of 5 at the cut - was 1 of 5 at the next reading (Neck1 sent to the part's
// node), counted the body's, and its head bone went to the stump: the face
// collapsed to a point at the neck, the hair and the teeth - rigid, under the
// head bone - alone on the ground (Nadi, 2026-11-15: "nur ne frisur und
// zähne am boden"; "FaceGenFace(body, 1/5)" at every head cut in the log).
enum class ShapeSide : UInt8 { Body, Part };

inline ShapeSide ShapeSideFor(bool holdsPartNode, bool holdsStumpNode, UInt32 partBones, UInt32 bones,
                              bool boundSaysPart) {
	if (holdsPartNode) {
		return ShapeSide::Part;
	}
	if (holdsStumpNode) {
		return ShapeSide::Body;
	}
	if (bones > 0) {
		return ShapeIsHeadSide(partBones, bones) ? ShapeSide::Part : ShapeSide::Body;
	}
	return boundSaysPart ? ShapeSide::Part : ShapeSide::Body;
}

// A limb's own skin (always, no setting: Nadi, 2026-11-16). No mesh is cut: a shape is
// on one side whole, and a shape with few bones of the part - a foot 2 of a
// foot mesh's 8, a forearm 3 of a cuirass's 18 - is the body's, so the limb's
// vertices collapse into the stump and no skin lies on the ground with the
// part (Nadi, 2026-11-15: "bei anderen gliedern wohl auch so"; a foot would
// simply be gone). Such a shape is drawn a second time for the part: a copy
// taken before the swap, while its part bones are still the real ones, in
// which every other bone points at the cut's part node - the body's side of
// it collapsed at the part's origin, the part's vertices left. The original
// keeps the body and loses the part, as before.
// One copy a shape: once swapped, the original has no real bone of the part
// left (partBones counts the real ones only), and a copy itself holds the
// cut's part node, so it is the part's side at every later reading.
// Not for a head: the face is the part's own shape already, and a collar
// weighted on the head bone is no part of the head.
// Never one of the engine's decal shapes (IsDecalShapeName): the clone of a
// decal shape has a decal property with no decal in it, and the decal
// shader's draw (0x00805F20) takes the property's first decal unasked -
// 0x007EE1D0 answers 0 for an empty list, 0x00805FE4 reads +0x44 of it. The
// game crashed there at a hand cut off a body with three marks on its skin
// (Linux, 2026-11-16 14:06; the harness' cuts had no mark on the body). The
// marks on the limb's skin do not go with the limb.
inline bool PartCopyWanted(bool headCut, ShapeSide side, UInt32 partBones, bool decal) {
	return !headCut && !decal && side == ShapeSide::Body && partBones > 0;
}

// One of the engine's decal shapes by its name, as the engine's own decal
// call tells them (0x004CF1A0 skips "Decal..."; it names them
// "Decal:<the shape's name>").
inline bool IsDecalShapeName(const char* name) {
	return name != nullptr && name[0] == 'D' && name[1] == 'e' && name[2] == 'c' && name[3] == 'a' && name[4] == 'l';
}

// In the copy: the part's bone and what hangs under it stay, every other
// bone goes to the cut's part node.
inline bool CopyBoneToPartNode(bool isPart, bool underPart) { return !isPart && !underPart; }

// A cut whose check no longer holds. Each cut remembers one bone pointer it
// wrote; when that slot no longer holds it, the shape it was in is gone -
// the whole 3D rebuilt by a load (which may reuse the root's address), or
// only that shape: a cut corpse's clothes taken off. In the second case the
// actor's other shapes still point at the cut's nodes, and forgetting the
// cut zeroed those nodes under them - their vertices at the world's origin,
// the stretch from the neck to the horizon - before the cut was made again
// in whatever slot was free, the old pointers then another cut's nodes
// (Nadi, 2026-11-15: "wieder ein infinity stretch am nacken"; the log of
// 22:06, a head and an arm "forgotten" with the root unchanged).
//   Keep    the check holds
//   Reswap  the check gone, the root and the part's bones the same, shapes
//           still on the cut's nodes: the cut stands, its skins read again
//           at once and a new check taken
//   Forget  another root, the part's bones gone, or no shape on the cut's
//           nodes any more - nothing left that points at them
enum class CutFate : UInt8 { Keep, Reswap, Forget };

inline CutFate CutFateFor(bool rootSame, bool bonesThere, bool checkGone, bool shapesOnCutNodes) {
	if (!rootSame || !bonesThere) {
		return CutFate::Forget;
	}
	if (!checkGone) {
		return CutFate::Keep;
	}
	return shapesOnCutNodes ? CutFate::Reswap : CutFate::Forget;
}

}  // namespace obvr::game

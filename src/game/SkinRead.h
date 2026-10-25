#pragma once

// The shapes under a model and their skins, read as the engine holds them:
// the walk and the reading game/BodyView.h's worn-shapes probe and
// game/FittedCapsules.h share. NiGeometry's data at +0xB4 and skin instance
// at +0xB8; NiGeometryData's vertex count (UInt16) at +0x08 and vertices at
// +0x1C; NiSkinInstance's NiSkinData at +0x08 and bones at +0x14; NiSkinData's
// bone count at +0x40 and the bones' data at +0x44, 0x4C each: the
// skin-to-bone NiTransform at +0x00, the weights at +0x44 and their count, a
// UInt16, at +0x48 (BodyView.cpp has where each was read). Other actors'
// skins keep all of it on the CPU after the upload (worn-shapes.txt,
// 2026-10-25: 97 shapes of five NPCs read whole).

#include <vector>

#include "core/Types.h"
#include "game/BodyViewLogic.h"
#include "game/NiMath.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// A NiTriShape or NiTriStrips under a model, and whether it or anything
// above it is hidden.
struct ShapeUnder {
	const NiAVObject* geometry = nullptr;
	bool hidden = false;
};

// The shapes under `root`, in the tree's order, at most `most` of them.
void CollectShapesUnder(const NiAVObject* root, std::vector<ShapeUnder>& out, UInt32 most = 64);

struct SkinnedShape {
	const char* name = "?";
	bool strips = false;
	UInt32 data = 0;  // the NiGeometryData's address
	UInt32 vertexCount = 0;
	const NiPoint3* bind = nullptr;  // the vertices, in the skin's bind space
	UInt32 skin = 0;                 // the NiSkinInstance's address, 0 for a rigid shape
	std::vector<const NiAVObject*> boneNode;
	std::vector<const char*> boneName;
	std::vector<NiTransform> skinToBone;
	std::vector<SkinWeight> weights;
};

enum class SkinReadResult : UInt8 { Skinned, Rigid, Refused };

// Reads a shape's vertices and, when it has one, its skin: its bones, their
// skin-to-bone and their weights. Refused, with the reason in `why`, when
// any of it does not read; a rigid shape answers Rigid with its vertices.
// The weights are read, not checked: whether every vertex's add to one is
// the caller's (SkinWeightMisses).
SkinReadResult ReadSkinnedShape(const NiAVObject* geometry, SkinnedShape& out, char* why, UInt32 whySize);

// The readable name of an object (its NiObjectNET name), "?" when none.
const char* ShapeName(const NiAVObject* object);

}  // namespace obvr::game

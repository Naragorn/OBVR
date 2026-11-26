#include "game/CutSkinSpan.h"

#include "core/AddressSpace.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

constexpr UInt32 kGeometryData = 0xB4;
constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kDataVertexCount = 0x08;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kSkinDataBones = 0x44;
constexpr UInt32 kSkinBoneSize = 0x4C;
constexpr UInt32 kSkinBoneWeights = 0x44;
constexpr UInt32 kSkinBoneWeightCount = 0x48;
constexpr UInt32 kMaxBones = 128;
constexpr UInt32 kMaxVertices = 65536;  // a UInt16 count

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
UInt16 Read16(UInt32 a) { return *reinterpret_cast<const UInt16*>(a); }

// A block of `bytes` from `address` judged by its two ends (game/BodyView.cpp).
bool LooksLikeBlock(UInt32 address, UInt32 bytes) {
	return bytes > 0 && LooksLikeObject(address) && LooksLikeObject((address + bytes - 1) & ~3u);
}

}  // namespace

SpanRead ShapeSpansCut(UInt32 geometry, const bool* flies, UInt32 boneCount, UInt32& spanning) {
	spanning = 0;
	const UInt32 data = LooksLikeObject(geometry) ? Read(geometry + kGeometryData) : 0;
	const UInt32 skin = LooksLikeObject(geometry) ? Read(geometry + kGeometrySkinInstance) : 0;
	const UInt32 skinData = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
	if (!LooksLikeObject(data) || !LooksLikeObject(skinData) || boneCount == 0 || boneCount > kMaxBones ||
	    Read(skinData + kSkinDataBoneCount) != boneCount) {
		return SpanRead::Unread;
	}
	const UInt32 vertexCount = Read16(data + kDataVertexCount);
	const UInt32 boneDataAt = Read(skinData + kSkinDataBones);
	if (vertexCount == 0 || !LooksLikeBlock(boneDataAt, boneCount * kSkinBoneSize)) {
		return SpanRead::Unread;
	}
	// One byte a vertex for VerticesSpanningCut, kept: read at a cut, never per frame.
	static UInt8 s_marks[kMaxVertices];
	CutBoneWeights bones[kMaxBones];
	for (UInt32 b = 0; b < boneCount; ++b) {
		const UInt32 entry = boneDataAt + b * kSkinBoneSize;
		const UInt32 weightAt = Read(entry + kSkinBoneWeights);
		const UInt32 count = Read16(entry + kSkinBoneWeightCount);
		if (count > vertexCount || (count > 0 && !LooksLikeBlock(weightAt, count * sizeof(CutWeight)))) {
			return SpanRead::Unread;
		}
		bones[b].weights = count > 0 ? reinterpret_cast<const CutWeight*>(weightAt) : nullptr;
		bones[b].count = count;
		bones[b].flies = flies[b];
	}
	spanning = VerticesSpanningCut(bones, boneCount, vertexCount, s_marks, kSpanLeastWeight);
	return spanning > 0 ? SpanRead::Spans : SpanRead::None;
}

}  // namespace obvr::game

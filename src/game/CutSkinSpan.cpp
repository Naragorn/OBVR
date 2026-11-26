#include "game/CutSkinSpan.h"

#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/GameTypes.h"

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
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kMaxDepth = 48;

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
UInt16 Read16(UInt32 a) { return *reinterpret_cast<const UInt16*>(a); }

// A block of `bytes` from `address` judged by its two ends (game/BodyView.cpp).
bool LooksLikeBlock(UInt32 address, UInt32 bytes) {
	return bytes > 0 && LooksLikeObject(address) && LooksLikeObject((address + bytes - 1) & ~3u);
}

// `geometry`'s skin weights into `bones` (each flagged from `flies`), its
// vertex count into `vertexCount`; false when any of it does not read or the
// counts disagree. Nothing is copied: the weights are read where they lie.
bool ReadWeights(UInt32 geometry, const bool* flies, UInt32 boneCount, CutBoneWeights* bones, UInt32& vertexCount) {
	vertexCount = 0;
	const UInt32 data = LooksLikeObject(geometry) ? Read(geometry + kGeometryData) : 0;
	const UInt32 skin = LooksLikeObject(geometry) ? Read(geometry + kGeometrySkinInstance) : 0;
	const UInt32 skinData = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
	if (!LooksLikeObject(data) || !LooksLikeObject(skinData) || boneCount == 0 || boneCount > kMaxBones ||
	    Read(skinData + kSkinDataBoneCount) != boneCount) {
		return false;
	}
	vertexCount = Read16(data + kDataVertexCount);
	const UInt32 boneDataAt = Read(skinData + kSkinDataBones);
	if (vertexCount == 0 || !LooksLikeBlock(boneDataAt, boneCount * kSkinBoneSize)) {
		return false;
	}
	for (UInt32 b = 0; b < boneCount; ++b) {
		const UInt32 entry = boneDataAt + b * kSkinBoneSize;
		const UInt32 weightAt = Read(entry + kSkinBoneWeights);
		const UInt32 count = Read16(entry + kSkinBoneWeightCount);
		if (count > vertexCount || (count > 0 && !LooksLikeBlock(weightAt, count * sizeof(CutWeight)))) {
			return false;
		}
		bones[b].weights = count > 0 ? reinterpret_cast<const CutWeight*>(weightAt) : nullptr;
		bones[b].count = count;
		bones[b].flies = flies[b];
	}
	return true;
}

// Whether `bone` is `head` or hangs under it.
bool IsOrUnder(UInt32 bone, UInt32 head) {
	UInt32 at = bone;
	for (UInt32 i = 0; i < kMaxDepth && LooksLikeObject(at); ++i) {
		if (at == head) {
			return true;
		}
		at = reinterpret_cast<UInt32>(reinterpret_cast<const NiAVObject*>(at)->parent);
	}
	return false;
}

}  // namespace

SpanRead ShapeSpansCut(UInt32 geometry, const bool* flies, UInt32 boneCount, UInt32& spanning) {
	spanning = 0;
	CutBoneWeights bones[kMaxBones];
	UInt32 vertexCount = 0;
	if (!ReadWeights(geometry, flies, boneCount, bones, vertexCount)) {
		return SpanRead::Unread;
	}
	// One byte a vertex for VerticesSpanningCut, kept: read at a cut, never per frame.
	static UInt8 s_marks[kMaxVertices];
	spanning = VerticesSpanningCut(bones, boneCount, vertexCount, s_marks, kSpanLeastWeight);
	return spanning > 0 ? SpanRead::Spans : SpanRead::None;
}

HeadShare ShapeHeadShare(UInt32 geometry, const NiAVObject* head) {
	HeadShare share;
	const UInt32 skin = LooksLikeObject(geometry) ? Read(geometry + kGeometrySkinInstance) : 0;
	const UInt32 skinData = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
	const UInt32 list = LooksLikeObject(skin) ? Read(skin + kSkinInstanceBones) : 0;
	const UInt32 boneCount = LooksLikeObject(skinData) && LooksLikeObject(list) ? Read(skinData + kSkinDataBoneCount) : 0;
	if (boneCount == 0 || boneCount > kMaxBones) {
		share.read = false;
		return share;
	}
	bool onHead[kMaxBones] = {};
	bool any = false;
	for (UInt32 b = 0; b < boneCount; ++b) {
		onHead[b] = IsOrUnder(reinterpret_cast<const UInt32*>(list)[b], reinterpret_cast<UInt32>(head));
		any = any || onHead[b];
	}
	if (!any) {
		return share;  // no bone of the head: nothing of it to draw again, no weight read
	}
	CutBoneWeights bones[kMaxBones];
	if (!ReadWeights(geometry, onHead, boneCount, bones, share.vertices)) {
		share.read = false;
		return share;
	}
	// One UInt16 a vertex for VerticesMostlyOnPart, kept: read at a head cut, never per frame.
	static UInt16 s_sums[kMaxVertices];
	share.mostly = VerticesMostlyOnPart(bones, boneCount, share.vertices, s_sums);
	return share;
}

}  // namespace obvr::game

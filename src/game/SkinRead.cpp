#include "game/SkinRead.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

constexpr UInt32 kGeometryData = 0xB4;
constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kDataVertexCount = 0x08;
constexpr UInt32 kDataVertices = 0x1C;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kSkinDataBones = 0x44;
constexpr UInt32 kSkinBoneSize = 0x4C;
constexpr UInt32 kSkinBoneWeights = 0x44;
constexpr UInt32 kSkinBoneWeightCount = 0x48;
constexpr UInt32 kSkinWeightSize = 0x08;
constexpr UInt32 kMaxBones = 96;
constexpr UInt32 kMaxDepth = 16;
constexpr UInt16 kHiddenBit = 0x0001;

bool Looks(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool Looks(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
UInt16 Read16(UInt32 address) { return *reinterpret_cast<const UInt16*>(address); }

// A block of `bytes` bytes from `address` lies in readable memory, judged by
// its two ends.
bool LooksLikeBlock(UInt32 address, UInt32 bytes) {
	return bytes > 0 && Looks(address) && Looks((address + bytes - 1) & ~3u);
}

bool StartsWith(const char* text, const char* prefix) {
	for (; *prefix != '\0'; ++text, ++prefix) {
		if (*text != *prefix) {
			return false;
		}
	}
	return true;
}

void Collect(const NiAVObject* node, UInt32 depth, bool hiddenAbove, std::vector<ShapeUnder>& out, UInt32 most) {
	if (!Looks(node) || depth > kMaxDepth || out.size() >= most) {
		return;
	}
	const bool hidden = hiddenAbove || (node->flags & kHiddenBit) != 0;
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] != nullptr) {
				Collect(children[i], depth + 1, hidden, out, most);
			}
		}
		return;
	}
	if (StartsWith(className, "NiTriShape@") || StartsWith(className, "NiTriStrips@")) {
		out.push_back(ShapeUnder{node, hidden});
	}
}

}  // namespace

const char* ShapeName(const NiAVObject* object) {
	if (!Looks(object)) {
		return "?";
	}
	const char* const name = *reinterpret_cast<const char* const*>(reinterpret_cast<const UInt8*>(object) + 0x08);
	if (!Looks(name)) {
		return "?";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const unsigned char c = static_cast<unsigned char>(name[at]);
		if (c == '\0') {
			return at > 0 ? name : "?";
		}
		if (c < 0x20 || c > 0x7E) {
			return "?";
		}
	}
	return "?";
}

void CollectShapesUnder(const NiAVObject* root, std::vector<ShapeUnder>& out, UInt32 most) {
	out.clear();
	Collect(root, 0, false, out, most);
}

SkinReadResult ReadSkinnedShape(const NiAVObject* geometry, SkinnedShape& out, char* why, UInt32 whySize) {
	out = SkinnedShape{};
	if (why != nullptr && whySize > 0) {
		why[0] = '\0';
	}
	out.name = ShapeName(geometry);
	out.strips = Looks(geometry) && StartsWith(NiClassNameOf(geometry), "NiTriStrips@");
	const UInt32 g = reinterpret_cast<UInt32>(geometry);
	const UInt32 data = Looks(g) ? Read(g + kGeometryData) : 0;
	if (!Looks(data)) {
		std::snprintf(why, whySize, "its geometry data does NOT read (%08X)", data);
		return SkinReadResult::Refused;
	}
	out.data = data;
	out.vertexCount = Read16(data + kDataVertexCount);
	const UInt32 vertexAt = Read(data + kDataVertices);
	if (out.vertexCount == 0 || !LooksLikeBlock(vertexAt, out.vertexCount * 12)) {
		std::snprintf(why, whySize, "its vertices do NOT read (%u at %08X)", out.vertexCount, vertexAt);
		return SkinReadResult::Refused;
	}
	out.bind = reinterpret_cast<const NiPoint3*>(vertexAt);
	const UInt32 skin = Read(g + kGeometrySkinInstance);
	if (skin == 0) {
		return SkinReadResult::Rigid;
	}
	out.skin = skin;
	const UInt32 skinData = Looks(skin) ? Read(skin + kSkinInstanceData) : 0;
	const UInt32 boneAt = Looks(skin) ? Read(skin + kSkinInstanceBones) : 0;
	const UInt32 bones = Looks(skinData) ? Read(skinData + kSkinDataBoneCount) : 0;
	const UInt32 boneDataAt = Looks(skinData) ? Read(skinData + kSkinDataBones) : 0;
	if (bones == 0 || bones > kMaxBones || !LooksLikeBlock(boneAt, bones * 4) ||
	    !LooksLikeBlock(boneDataAt, bones * kSkinBoneSize)) {
		std::snprintf(why, whySize,
		              "its skin's bones do NOT read (skin %08X, data %08X, %u bones at %08X, their data at %08X)", skin,
		              skinData, bones, boneAt, boneDataAt);
		return SkinReadResult::Refused;
	}
	out.boneNode.resize(bones);
	out.boneName.resize(bones);
	out.skinToBone.resize(bones);
	for (UInt32 b = 0; b < bones; ++b) {
		const auto* const bone = reinterpret_cast<const NiAVObject*>(Read(boneAt + b * 4));
		// a live object's first word is its class's vtable: a freed bone's
		// is not, and its name read +0x08 as the float 1.0 and crashed the
		// hand fit on a blood decal of the first-person body (the
		// Performance session's crash run, 2026-10-27 17:05)
		if (!Looks(bone) || !addr::IsGameVtable(Read(reinterpret_cast<UInt32>(bone)))) {
			std::snprintf(why, whySize, "bone %u of %u does NOT read", b, bones);
			return SkinReadResult::Refused;
		}
		out.boneNode[b] = bone;
		out.boneName[b] = ShapeName(bone);
		const UInt32 entry = boneDataAt + b * kSkinBoneSize;
		out.skinToBone[b] = *reinterpret_cast<const NiTransform*>(entry);
		const UInt32 weightAt = Read(entry + kSkinBoneWeights);
		const UInt32 weightCount = Read16(entry + kSkinBoneWeightCount);
		if (weightCount > out.vertexCount ||
		    (weightCount > 0 && !LooksLikeBlock(weightAt, weightCount * kSkinWeightSize))) {
			std::snprintf(why, whySize, "the weights of bone %u \"%s\" do NOT read (%u at %08X)", b, out.boneName[b],
			              weightCount, weightAt);
			return SkinReadResult::Refused;
		}
		for (UInt32 k = 0; k < weightCount; ++k) {
			SkinWeight w;
			w.vertex = Read16(weightAt + k * kSkinWeightSize);
			w.bone = static_cast<UInt16>(b);
			w.weight = *reinterpret_cast<const float*>(weightAt + k * kSkinWeightSize + 4);
			if (w.vertex >= out.vertexCount) {
				std::snprintf(why, whySize, "a weight of bone %u \"%s\" names vertex %u of %u", b, out.boneName[b],
				              w.vertex, out.vertexCount);
				return SkinReadResult::Refused;
			}
			out.weights.push_back(w);
		}
	}
	return SkinReadResult::Skinned;
}

}  // namespace obvr::game

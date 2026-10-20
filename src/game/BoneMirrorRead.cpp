#include "game/BoneMirrorRead.h"

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "game/FirstPersonHide.h"

namespace obvr::game {

namespace {

// NiGeometry's skin instance; NiSkinInstance's data and bones; NiSkinData's
// bone count and bones' data (BodyView.cpp, ArmStump.cpp).
constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kSkinDataBones = 0x44;
constexpr UInt32 kSkinBoneSize = 0x4C;
constexpr UInt32 kMaxBones = 96;
constexpr UInt32 kMaxDepth = 12;
constexpr UInt16 kHiddenBit = 0x0001;
constexpr UInt32 kAllArmBones = 2 * kArmPartCount;

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool LooksLikeObject(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool LooksLikeBlock(UInt32 address, UInt32 bytes) {
	return bytes > 0 && LooksLikeObject(address) && LooksLikeObject((address + bytes - 1) & ~3u);
}

bool StartsWith(const char* text, const char* prefix) {
	if (text == nullptr || prefix == nullptr) {
		return false;
	}
	return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

const char* ReadableName(const void* object) {
	if (!LooksLikeObject(object)) {
		return "?";
	}
	const char* const name = *reinterpret_cast<const char* const*>(reinterpret_cast<const UInt8*>(object) + 0x08);
	if (!LooksLikeObject(name)) {
		return "?";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char c = name[at];
		if (c == '\0') {
			return at > 0 ? name : "(unnamed)";
		}
		if (c < 0x20 || c > 0x7E) {
			return "?";
		}
	}
	return "?";
}

struct Walk {
	NiAVObject* const* armBones;
	NiTransform skinToBone[kAllArmBones];
	bool have[kAllArmBones];
	UInt32 found;
	ArmBonePartnersRead* out;
};

void TakeFromShape(const NiAVObject* geometry, Walk& w) {
	++w.out->shapesWalked;
	const UInt32 g = reinterpret_cast<UInt32>(geometry);
	const UInt32 skin = Read(g + kGeometrySkinInstance);
	if (!LooksLikeObject(skin)) {
		return;
	}
	++w.out->shapesSkinned;
	const UInt32 skinData = Read(skin + kSkinInstanceData);
	const UInt32 boneAt = Read(skin + kSkinInstanceBones);
	if (!LooksLikeObject(skinData)) {
		return;
	}
	const UInt32 bones = Read(skinData + kSkinDataBoneCount);
	const UInt32 boneDataAt = Read(skinData + kSkinDataBones);
	if (bones == 0 || bones > kMaxBones || !LooksLikeBlock(boneAt, bones * 4) ||
	    !LooksLikeBlock(boneDataAt, bones * kSkinBoneSize)) {
		return;
	}
	bool took = false;
	for (UInt32 b = 0; b < bones; ++b) {
		const NiAVObject* const bone = reinterpret_cast<const NiAVObject*>(Read(boneAt + b * 4));
		for (UInt32 i = 0; i < kAllArmBones; ++i) {
			if (w.have[i] || w.armBones[i] == nullptr || w.armBones[i] != bone) {
				continue;
			}
			w.skinToBone[i] = *reinterpret_cast<const NiTransform*>(boneDataAt + b * kSkinBoneSize);
			w.have[i] = true;
			++w.found;
			took = true;
		}
	}
	if (took) {
		char* const shapes = w.out->shapes;
		const UInt32 used = static_cast<UInt32>(std::strlen(shapes));
		if (used < sizeof(w.out->shapes) - 8) {
			std::snprintf(shapes + used, sizeof(w.out->shapes) - used, "%s%s", used > 0 ? ", " : "",
			              ReadableName(geometry));
		}
	}
}

void WalkNode(const NiAVObject* node, UInt32 depth, Walk& w) {
	if (!LooksLikeObject(node) || depth > kMaxDepth || w.found == kAllArmBones) {
		return;
	}
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] != nullptr) {
				WalkNode(children[i], depth + 1, w);
			}
		}
		return;
	}
	if ((node->flags & kHiddenBit) != 0) {
		return;
	}
	if (StartsWith(className, "NiTriShape@") || StartsWith(className, "NiTriStrips@")) {
		TakeFromShape(node, w);
	}
}

}  // namespace

bool ReadArmBonePartners(const NiAVObject* bodyRoot, NiAVObject* const* armBones, ArmBonePartnersRead& out) {
	out = ArmBonePartnersRead{};
	if (!LooksLikeObject(bodyRoot) || armBones == nullptr) {
		out.why = "no body root to read the skins under";
		return false;
	}
	Walk w{};
	w.armBones = armBones;
	w.out = &out;
	WalkNode(bodyRoot, 0, w);
	if (!ArmPartnersFrom(w.skinToBone, w.have, out.partners)) {
		out.why = w.found == 0 ? "no skinned shape under the body carries an arm bone"
		                       : "not every arm bone is in a skin under the body";
		return false;
	}
	return true;
}

}  // namespace obvr::game

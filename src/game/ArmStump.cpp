#include "game/ArmStump.h"

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"

namespace obvr::game {
namespace {

// NiGeometry's skin instance (xOBSE NiObjects.h, "skinData" at +0xB8);
// NiSkinInstance's skin data (+0x08) and bone pointers (+0x14); NiSkinData's
// bone count (+0x40) (xOBSE NiGeometry.h).
constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kMaxBones = 64;

bool LooksLikeObject(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// OBVR's own nodes: a real bone's first bytes (its vtable and name, for
// anything that looks), no parent, a reference count nothing brings to
// zero, and a world transform OBVR writes. Never in a tree.
struct alignas(16) FakeNode {
	UInt8 bytes[0xC0];
};
FakeNode g_leftNode{};
FakeNode g_rightNode{};
constexpr UInt32 kMaxCentre = 12;  // stand-ins for real bones: the spine, the forearms
FakeNode g_centreNodes[kMaxCentre]{};
NiAVObject* g_centreReal[kMaxCentre] = {};
bool g_nodesMade = false;

NiAVObject* AsNode(FakeNode& f) { return reinterpret_cast<NiAVObject*>(f.bytes); }

void MakeFrom(FakeNode& fake, const NiAVObject* real) {
	std::memset(fake.bytes, 0, sizeof(fake.bytes));
	std::memcpy(fake.bytes, real, sizeof(NiAVObject));
	NiAVObject* const node = AsNode(fake);
	node->parent = nullptr;
	node->refCount = 0x40000000u;
	node->controller = nullptr;
	node->extraDataList = nullptr;
	node->extraDataListLen = 0;
	node->extraDataListCapacity = 0;
}

bool IsFake(UInt32 p) {
	if (p == reinterpret_cast<UInt32>(g_leftNode.bytes) || p == reinterpret_cast<UInt32>(g_rightNode.bytes)) {
		return true;
	}
	for (FakeNode& f : g_centreNodes) {
		if (p == reinterpret_cast<UInt32>(f.bytes)) {
			return true;
		}
	}
	return false;
}

// The skins swapped, and each one's own bone pointers to give back.
struct Swapped {
	UInt32 skin = 0;
	UInt32 count = 0;
	UInt32 original[kMaxBones] = {};
};
Swapped g_swapped[8];
UInt32 g_linesLeft = 12;

Swapped* Find(UInt32 skin) {
	for (Swapped& s : g_swapped) {
		if (s.skin == skin) {
			return &s;
		}
	}
	return nullptr;
}

Swapped* Track(UInt32 skin) {
	for (Swapped& s : g_swapped) {
		if (s.skin == 0) {
			s.skin = skin;
			return &s;
		}
	}
	return nullptr;
}

// The "Arms" geometries of the first-person model and their skins.
UInt32 ArmSkins(UInt32 (&skins)[8]) {
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return 0;
	}
	NiAVObject* nodes[8] = {};
	const UInt32 found = CollectNodesContaining(root, "Arms", nodes, 8);
	UInt32 n = 0;
	for (UInt32 i = 0; i < found; ++i) {
		// The shape itself, not a node that only has the word in its name.
		if (!NameIs(nodes[i]->name, "Arms")) {
			continue;
		}
		const UInt32 geometry = reinterpret_cast<UInt32>(nodes[i]);
		const UInt32 skin = Read(geometry + kGeometrySkinInstance);
		if (!mem::LooksLikeObjectAddress(skin)) {
			continue;
		}
		const UInt32 data = Read(skin + kSkinInstanceData);
		const UInt32 bones = Read(skin + kSkinInstanceBones);
		if (!mem::LooksLikeObjectAddress(data) || !mem::LooksLikeObjectAddress(bones)) {
			continue;
		}
		const UInt32 count = Read(data + kSkinDataBoneCount);
		if (count == 0 || count > kMaxBones) {
			continue;
		}
		skins[n++] = skin;
	}
	return n;
}

void GiveBack(Swapped& s) {
	UInt32* const bones = reinterpret_cast<UInt32*>(Read(s.skin + kSkinInstanceBones));
	for (UInt32 i = 0; i < s.count; ++i) {
		bones[i] = s.original[i];
	}
	s = Swapped{};
}

void GiveAllBack() {
	// Only skins still in the model are touched; one that went (an outfit
	// changed) went with its pointers.
	UInt32 skins[8];
	const UInt32 n = ArmSkins(skins);
	for (Swapped& s : g_swapped) {
		if (s.skin == 0) {
			continue;
		}
		bool present = false;
		for (UInt32 i = 0; i < n; ++i) {
			present = present || skins[i] == s.skin;
		}
		if (present) {
			GiveBack(s);
		} else {
			s = Swapped{};
		}
	}
}

NiAVObject* CentreNodeFor(NiAVObject* real) {
	for (UInt32 i = 0; i < kMaxCentre; ++i) {
		if (g_centreReal[i] == real) {
			return AsNode(g_centreNodes[i]);
		}
	}
	for (UInt32 i = 0; i < kMaxCentre; ++i) {
		if (g_centreReal[i] == nullptr) {
			g_centreReal[i] = real;
			MakeFrom(g_centreNodes[i], real);
			return AsNode(g_centreNodes[i]);
		}
	}
	return nullptr;
}

}  // namespace

bool StepForearmStumps(bool wanted) {
	if (!wanted) {
		GiveAllBack();
		return false;
	}
	NiAVObject* const leftForearm = FindFirstPersonNode("Bip01 L Forearm");
	NiAVObject* const rightForearm = FindFirstPersonNode("Bip01 R Forearm");
	NiAVObject* const leftHand = FindFirstPersonNode("Bip01 L Hand");
	NiAVObject* const rightHand = FindFirstPersonNode("Bip01 R Hand");
	NiAVObject* const leftClavicle = FindFirstPersonNode("Bip01 L Clavicle");
	NiAVObject* const rightClavicle = FindFirstPersonNode("Bip01 R Clavicle");
	if (!LooksLikeObject(leftForearm) || !LooksLikeObject(rightForearm) || !LooksLikeObject(leftClavicle) ||
	    !LooksLikeObject(rightClavicle) || !LooksLikeObject(leftHand) || !LooksLikeObject(rightHand)) {
		GiveAllBack();
		return false;
	}
	if (!g_nodesMade) {
		MakeFrom(g_leftNode, leftForearm);
		MakeFrom(g_rightNode, rightForearm);
		g_nodesMade = true;
	}
	// Each side: the wrist (the hand bone), the way back to the elbow, and
	// where the short stump ends on it.
	const NiPoint3 leftWrist = leftHand->worldTransform.pos;
	const NiPoint3 rightWrist = rightHand->worldTransform.pos;
	const NiPoint3 leftAxis = UnitFromTo(leftWrist, leftForearm->worldTransform.pos);
	const NiPoint3 rightAxis = UnitFromTo(rightWrist, rightForearm->worldTransform.pos);
	const NiPoint3 leftElbow = StumpEnd(leftWrist, leftForearm->worldTransform.pos);
	const NiPoint3 rightElbow = StumpEnd(rightWrist, rightForearm->worldTransform.pos);
	AsNode(g_leftNode)->worldTransform = CollapseAt(leftElbow);
	AsNode(g_rightNode)->worldTransform = CollapseAt(rightElbow);
	const NiPoint3 across = UnitFromTo(leftClavicle->worldTransform.pos, rightClavicle->worldTransform.pos);
	const NiPoint3 centre = (leftClavicle->worldTransform.pos + rightClavicle->worldTransform.pos) * 0.5f;

	UInt32 skins[8];
	const UInt32 n = ArmSkins(skins);
	UInt32 swappedNow = 0;
	for (UInt32 k = 0; k < n; ++k) {
		const UInt32 skin = skins[k];
		const UInt32 count = Read(Read(skin + kSkinInstanceData) + kSkinDataBoneCount);
		UInt32* const bones = reinterpret_cast<UInt32*>(Read(skin + kSkinInstanceBones));
		Swapped* s = Find(skin);
		const bool fresh = s == nullptr;
		if (fresh) {
			s = Track(skin);
			if (s == nullptr) {
				continue;
			}
			s->count = count;
			for (UInt32 i = 0; i < count; ++i) {
				s->original[i] = IsFake(bones[i]) ? 0 : bones[i];
			}
		}
		UInt32 left = 0, right = 0, centreBones = 0, forearmBones = 0;
		for (UInt32 i = 0; i < s->count; ++i) {
			NiAVObject* const real = reinterpret_cast<NiAVObject*>(s->original[i]);
			if (!LooksLikeObject(real)) {
				continue;
			}
			switch (StumpRoleOf(real->name)) {
			case StumpRole::LeftUpper:
				bones[i] = reinterpret_cast<UInt32>(AsNode(g_leftNode));
				++left;
				break;
			case StumpRole::RightUpper:
				bones[i] = reinterpret_cast<UInt32>(AsNode(g_rightNode));
				++right;
				break;
			case StumpRole::LeftForearm:
			case StumpRole::RightForearm: {
				const bool isLeft = StumpRoleOf(real->name) == StumpRole::LeftForearm;
				NiAVObject* const node = CentreNodeFor(real);
				if (node != nullptr) {
					node->worldTransform = SquashAlong(real->worldTransform, isLeft ? leftWrist : rightWrist,
					                                   isLeft ? leftAxis : rightAxis);
					bones[i] = reinterpret_cast<UInt32>(node);
					++forearmBones;
				}
				break;
			}
			case StumpRole::Centre: {
				NiAVObject* const node = CentreNodeFor(real);
				if (node != nullptr) {
					node->worldTransform = CentreStump(real->worldTransform, leftElbow, rightElbow, across, centre);
					bones[i] = reinterpret_cast<UInt32>(node);
					++centreBones;
				}
				break;
			}
			case StumpRole::Keep:
				bones[i] = s->original[i];
				break;
			}
		}
		++swappedNow;
		if (fresh && g_linesLeft > 0) {
			--g_linesLeft;
			OBVR_LOG("Hand bones: forearm stump - the Arms skin %08X (%u bones): %u left and %u right "
			         "above the elbow collapsed into the elbows, %u shared (spine) mapped to them, %u forearm bones squashed to %.0f %%; stump ends "
			         "at (%.1f %.1f %.1f) and (%.1f %.1f %.1f)",
			         skin, s->count, left, right, centreBones, forearmBones,
			         static_cast<double>(kStumpForearmShare * 100.0f), static_cast<double>(leftElbow.x),
			         static_cast<double>(leftElbow.y), static_cast<double>(leftElbow.z),
			         static_cast<double>(rightElbow.x), static_cast<double>(rightElbow.y),
			         static_cast<double>(rightElbow.z));
		}
	}
	if (swappedNow == 0 && g_linesLeft > 0) {
		--g_linesLeft;
		OBVR_LOG("Hand bones: forearm stump wanted, but no \"Arms\" shape with a skin in the first-person "
		         "model - nothing to show");
	}
	return swappedNow > 0;
}

}  // namespace obvr::game

#include "game/DismemberCaps.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "core/AddressSpace.h"
#include "core/BranchDecode.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/SkinRead.h"

// The wound caps' engine side, out of game/Dismember.cpp (its size ratchet):
// the neck plug loaded, cloned, skinned to a bone and hung under a root, and
// which way it faces.

namespace obvr::game {
namespace {

constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kMaxBones = 128;

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

// The wound caps (Nadi, 2026-10-31: "4. go"): vanilla's zombie neck plug
// (`meshes\creatures\zombie\neckplug.nif`: two shapes, flesh and bone, skinned
// to one bone, "Bip01 Head", at the neck's end of the head) loaded by the
// engine's model loader (0x00439EB0 on 0x00B33A1C, thiscall, ret 0x10),
// cloned (0x00700900 - the loader's copy is shared), its bone pointed at
// `bone`, hung under the actor's root (vt+0x84) and updated (0x00707610,
// 0x00707370) - ../OBNG/knowledge/skinned-part-attach.md. Once on the head
// (it caps the head's underside) and once on the stump's own bone (capSide:
// the head's place and turn at the cut, following the neck). 0 when refused.
constexpr UInt32 kModelLoader = 0x00B33A1C;
constexpr UInt32 kLoadModel = 0x00439EB0;
constexpr UInt32 kCloneObject = 0x00700900;
constexpr UInt32 kUpdateUpward = 0x00707610;
constexpr UInt32 kUpdateDownward = 0x00707370;
constexpr UInt32 kNodeAttachSlot = 0x84;
const char* const kCapModel = "Creatures\\Zombie\\NeckPlug.nif";
const UInt8 kLoadModelBytes[] = {0x6A, 0xFF, 0x68, 0x08, 0xC9, 0x9A, 0x00, 0x64};
const UInt8 kCloneBytes[] = {0x6A, 0xFF, 0x68, 0xE8, 0x93, 0x9C, 0x00, 0x64};
const UInt8 kUpdateUpwardBytes[] = {0x6A, 0xFF, 0x68, 0x7B, 0x96, 0x9C, 0x00, 0x64};
const UInt8 kUpdateDownwardBytes[] = {0xD9, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xF1, 0x8B};

using LoadModelFn = UInt32(__fastcall*)(void* loader, void* edx, const char* path, UInt32 a, UInt32 b, UInt32 c);
using CloneFn = UInt32(__fastcall*)(void* source, void* edx);
using AttachChildFn = void(__fastcall*)(void* node, void* edx, void* child, UInt32 firstAvailable);
using UpdateUpwardFn = void(__fastcall*)(void* node, void* edx);
using UpdateDownwardFn = void(__fastcall*)(void* node, void* edx, float time, UInt32 flags);

// One of the caps' functions: as read, another plugin's jump at its entry
// (called through it, game/DismemberLogic.h CapEntryOf), or neither; a
// differing entry is named with the module its jump goes to.
CapEntry CheckCapEntry(UInt32 address, const UInt8* bytes, UInt32 size) {
	const bool asRead = mem::Verify(address, bytes, size);
	mem::RelativeBranch branch;
	const bool jump = !asRead && mem::DecodeRelativeBranch(reinterpret_cast<const UInt8*>(address), address, branch) &&
	                  branch.isJump;
	if (!asRead) {
		mem::ReportForeignCode("Dismember", address);
	}
	return CapEntryOf(asRead, jump);
}

bool CapsVerified() {
	static int verified = 0;
	if (verified == 0) {
		const CapEntry entries[] = {
			CheckCapEntry(kLoadModel, kLoadModelBytes, sizeof(kLoadModelBytes)),
			CheckCapEntry(kCloneObject, kCloneBytes, sizeof(kCloneBytes)),
			CheckCapEntry(kUpdateUpward, kUpdateUpwardBytes, sizeof(kUpdateUpwardBytes)),
			CheckCapEntry(kUpdateDownward, kUpdateDownwardBytes, sizeof(kUpdateDownwardBytes)),
		};
		UInt32 detoured = 0;
		bool callable = true;
		for (const CapEntry e : entries) {
			detoured += e == CapEntry::Detoured ? 1u : 0u;
			callable = callable && CapEntryCallable(e);
		}
		verified = callable ? 1 : -1;
		OBVR_LOG("Dismember: the model loader, clone and update %s",
		         !callable       ? "NOT as read - no caps"
		         : detoured > 0  ? "as read but for another plugin's jump at their entry, called through it - the cuts get caps"
		                         : "as read - the cuts get caps");
	}
	return verified > 0;
}

// Every skinned shape under `node`: its bones pointed at `bone`. The shapes counted.
UInt32 PointCapBones(const NiAVObject* node, UInt32 bone, UInt32 depth) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node)) || depth > 8) {
		return 0;
	}
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		UInt32 shapes = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			shapes += PointCapBones(children[i], bone, depth + 1);
		}
		return shapes;
	}
	if (className == nullptr ||
	    (std::strncmp(className, "NiTriShape@", 11) != 0 && std::strncmp(className, "NiTriStrips@", 12) != 0)) {
		return 0;
	}
	const UInt32 skin = Read(reinterpret_cast<UInt32>(node) + kGeometrySkinInstance);
	const UInt32 data = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
	const UInt32 bones = LooksLikeObject(skin) ? Read(skin + kSkinInstanceBones) : 0;
	if (!LooksLikeObject(data) || !LooksLikeObject(bones)) {
		return 0;
	}
	const UInt32 count = Read(data + kSkinDataBoneCount);
	if (count == 0 || count > kMaxBones) {
		return 0;
	}
	for (UInt32 i = 0; i < count; ++i) {
		reinterpret_cast<UInt32*>(bones)[i] = bone;
	}
	return 1;
}

}  // namespace

UInt32 AttachCap(UInt32 root, const NiAVObject* bone, char* why, UInt32 whySize) {
	const UInt32 loader = Read(kModelLoader);
	if (!CapsVerified() || !LooksLikeObject(loader) || !LooksLikeObject(root)) {
		std::snprintf(why, whySize, "no loader or root");
		return 0;
	}
	const UInt32 shared =
		reinterpret_cast<LoadModelFn>(kLoadModel)(reinterpret_cast<void*>(loader), nullptr, kCapModel, 0, 0, 1);
	if (!LooksLikeObject(shared)) {
		std::snprintf(why, whySize, "%s not loaded", kCapModel);
		return 0;
	}
	const UInt32 cap = reinterpret_cast<CloneFn>(kCloneObject)(reinterpret_cast<void*>(shared), nullptr);
	if (!LooksLikeObject(cap)) {
		std::snprintf(why, whySize, "not cloned");
		return 0;
	}
	const UInt32 shapes = PointCapBones(reinterpret_cast<const NiAVObject*>(cap), reinterpret_cast<UInt32>(bone), 0);
	if (shapes == 0) {
		std::snprintf(why, whySize, "no skinned shape in it");
		return 0;
	}
	const UInt32 attach = Read(Read(root) + kNodeAttachSlot);
	reinterpret_cast<AttachChildFn>(attach)(reinterpret_cast<void*>(root), nullptr, reinterpret_cast<void*>(cap), 1);
	reinterpret_cast<UpdateUpwardFn>(kUpdateUpward)(reinterpret_cast<void*>(cap), nullptr);
	reinterpret_cast<UpdateDownwardFn>(kUpdateDownward)(reinterpret_cast<void*>(cap), nullptr, 0.0f, 0);
	std::snprintf(why, whySize, "%u shape(s)", shapes);
	return cap;
}

// Which way a cap faces, in its bone's space: its shapes' vertices and
// normals (NiGeometryData +0x20, BodyView.cpp) under their skin-to-bone.
CapFacing ReadCapFacing(UInt32 cap) {
	CapFacing f;
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(reinterpret_cast<const NiAVObject*>(cap), shapes, 8);
	for (const ShapeUnder& s : shapes) {
		SkinnedShape shape;
		char why[64] = "";
		if (ReadSkinnedShape(s.geometry, shape, why, sizeof(why)) != SkinReadResult::Skinned || shape.skinToBone.empty()) {
			continue;
		}
		const UInt32 normals = Read(shape.data + 0x20);
		if (normals < 0x10000) {
			continue;
		}
		AddCapFacing(f, shape.bind, reinterpret_cast<const NiPoint3*>(normals), shape.vertexCount, shape.skinToBone[0]);
	}
	return f;
}

}  // namespace obvr::game

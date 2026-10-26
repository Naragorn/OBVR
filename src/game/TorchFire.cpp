#include "game/TorchFire.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/TorchGroundLogic.h"
#include "game/GameCamera.h"

#include <cstdio>
#include <cstring>

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// The engine's functions, each checked by its first bytes before a call
// (objdump of Oblivion.exe 1.2.0.416, 2026-10-26).
constexpr UInt32 kRefSetFireOut = 0x0046AB60;
constexpr UInt8 kRefSetFireOutBytes[] = {0x80, 0x7C, 0x24, 0x04, 0x00, 0x74, 0x0A};
constexpr UInt32 kNodePutFlamesOn = 0x004E26F0;
constexpr UInt8 kNodePutFlamesOnBytes[] = {0x51, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x85, 0xFF};
constexpr UInt32 kLightAttachLight = 0x004B1600;
constexpr UInt8 kLightAttachLightBytes[] = {0x6A, 0xFF, 0x68, 0xCE, 0x2F, 0x9B, 0x00};

constexpr UInt8 kFormTypeLight = 0x1A;
constexpr UInt32 kLightFlagsOffset = 0x7C;
constexpr UInt32 kLightFlagOffByDefault = 0x20;

const char* NameOf(const NiAVObject* object) {
	return LooksLikeObject(reinterpret_cast<UInt32>(object)) && object->name != nullptr ? object->name : "";
}

void ReadNode(const NiAVObject* node, UInt32 depth, TorchFireReading& out) {
	if (depth > 12 || !LooksLikeObject(reinterpret_cast<UInt32>(node))) {
		return;
	}
	const char* const name = NameOf(node);
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(node, count);
	if (std::strncmp(name, "FlameNode", 9) == 0) {
		++out.flameNodes;
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			out.flames += children[i] != nullptr ? 1u : 0u;
		}
	} else if (std::strcmp(name, "FlameCap") == 0) {
		out.capFound = true;
		out.capShown = (node->flags & 1) == 0;
	} else if (std::strcmp(name, "AttachLight") == 0) {
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] == nullptr) {
				continue;
			}
			if (out.lightsUnder == 0) {
				std::snprintf(out.lightClass, sizeof(out.lightClass), "%s", NiClassNameOf(children[i]));
			}
			++out.lightsUnder;
		}
	}
	for (UInt32 i = 0; children != nullptr && i < count; ++i) {
		if (children[i] != nullptr) {
			ReadNode(children[i], depth + 1, out);
		}
	}
}

}  // namespace

bool ReadTorchFire(UInt32 ref, TorchFireReading& out) {
	out = TorchFireReading{};
	if (!LooksLikeObject(ref)) {
		return false;
	}
	out.formFlags = Read(ref + addr::kFormFlagsOffset);
	const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
	if (LooksLikeObject(base) && *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) == kFormTypeLight) {
		out.lightFlags = Read(base + kLightFlagsOffset);
	}
	const UInt32 node = Read(ref + addr::kRefNiNodeOffset);
	if (!LooksLikeObject(node)) {
		return false;
	}
	out.haveNode = true;
	const auto* const root = reinterpret_cast<const NiAVObject*>(node);
	out.position = root->worldTransform.pos;
	out.hidden = (root->flags & 1) != 0;
	ReadNode(root, 0, out);
	return true;
}

bool IgniteDroppedTorch(UInt32 ref, bool putFlamesOn, char* why, UInt32 whySize) {
	const auto say = [&](const char* text) {
		if (why != nullptr && whySize > 0) {
			std::snprintf(why, whySize, "%s", text);
		}
		return false;
	};
	if (!LooksLikeObject(ref)) {
		return say("no reference");
	}
	const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
	if (!LooksLikeObject(base) || *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) != kFormTypeLight) {
		return say("its base is no light");
	}
	const UInt32 node = Read(ref + addr::kRefNiNodeOffset);
	if (!LooksLikeObject(node)) {
		return say("no 3D yet");
	}
	if (!mem::Verify(kRefSetFireOut, kRefSetFireOutBytes, sizeof(kRefSetFireOutBytes)) ||
	    !mem::Verify(kNodePutFlamesOn, kNodePutFlamesOnBytes, sizeof(kNodePutFlamesOnBytes)) ||
	    !mem::Verify(kLightAttachLight, kLightAttachLightBytes, sizeof(kLightAttachLightBytes))) {
		return say("the engine's fire functions are not the ones read");
	}
	using SetFireOutFn = void(__thiscall*)(UInt32 ref, bool out);
	using PutFlamesOnFn = bool(__cdecl*)(UInt32 node);
	using AttachLightFn = void(__thiscall*)(UInt32 light, UInt32 ref, UInt32 node);
	reinterpret_cast<SetFireOutFn>(kRefSetFireOut)(ref, false);
	TorchFireReading now;
	ReadTorchFire(ref, now);
	if (putFlamesOn && now.flames == 0) {
		reinterpret_cast<PutFlamesOnFn>(kNodePutFlamesOn)(node);
		ReadTorchFire(ref, now);
	}
	if (now.lightsUnder == 0) {
		// Torch02 is "off by default" (its flags 0x2B, torch-drop-lit.txt
		// 2026-10-26 08:41), and for such a light 0x004B1600 makes a light
		// only when the reference's vtable +0x190 says yes - an actor's does,
		// a plain reference's answers false (0x0069D990) - and marks the fire
		// out again. So the flag is off for this one call, and back after.
		UInt32* const lightFlags = reinterpret_cast<UInt32*>(base + kLightFlagsOffset);
		const UInt32 was = *lightFlags;
		*lightFlags = was & ~kLightFlagOffByDefault;
		reinterpret_cast<AttachLightFn>(kLightAttachLight)(base, ref, node);
		*lightFlags = was;
	}
	UpdateNodeTransforms(reinterpret_cast<NiAVObject*>(node));
	return true;
}

bool ScanTorchesInPlayerCell(TorchCellScan& out) {
	constexpr UInt32 kCellObjectListOffset = 0x48;  // as NearbyItems.cpp walks it
	constexpr UInt32 kFormDeletedOrDisabled = 0x20 | 0x800;
	constexpr UInt32 kMaxRefs = 8192;
	out = TorchCellScan{};
	const UInt32 player = Read(addr::kPlayerPointer);
	const UInt32 cell = LooksLikeObject(player) ? Read(player + addr::kRefParentCellOffset) : 0;
	if (!LooksLikeObject(cell)) {
		return false;
	}
	out.cell = Read(cell + 0x0C);
	// The first entry is inline in the cell; the rest are linked.
	UInt32 entry = cell + kCellObjectListOffset;
	for (UInt32 walked = 0; entry != 0 && walked < kMaxRefs; ++walked) {
		const UInt32 ref = Read(entry);
		const UInt32 next = Read(entry + 4);
		entry = LooksLikeObject(next) ? next : 0;
		if (!LooksLikeObject(ref) || ref == player) {
			continue;
		}
		const UInt32 formFlags = Read(ref + addr::kFormFlagsOffset);
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		if ((formFlags & kFormDeletedOrDisabled) != 0 || !LooksLikeObject(base) ||
		    *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) != kFormTypeLight) {
			continue;
		}
		TorchOnGround t;
		t.lightFlags = Read(base + kLightFlagsOffset);
		t.formFlags = formFlags;
		if ((t.lightFlags & kLightFlagCarried) == 0) {
			continue;
		}
		++out.carried;
		// The cheap reads first: the tree is walked only for OBVR's.
		t.haveNode = true;
		t.flames = 1;
		if (TorchOnGroundNeeds(t) == TorchGroundNeed::NotOurs) {
			continue;
		}
		++out.ours;
		TorchFireReading r;
		ReadTorchFire(ref, r);
		t.haveNode = r.haveNode;
		t.flames = r.flames;
		t.lightsUnder = r.lightsUnder;
		const TorchGroundNeed need = TorchOnGroundNeeds(t);
		if (need == TorchGroundNeed::Wait) {
			++out.waiting;
		} else if (need == TorchGroundNeed::Light && IgniteDroppedTorch(ref, false, nullptr, 0)) {
			if (out.relit++ == 0) {
				out.firstRelit = Read(ref + 0x0C);
			}
		}
	}
	return true;
}

}  // namespace obvr::game

#include "game/OwnedNode.h"

#include <intrin.h>

#include "core/AddressSpace.h"
#include "core/BranchDecode.h"
#include "core/Memory.h"
#include "game/DismemberLogic.h"
#include "game/GameAddresses.h"

namespace obvr::game {

NiAVObject* CloneNode(const NiAVObject* source) {
	using CloneFn = NiAVObject*(__thiscall*)(const NiAVObject* self);
	return reinterpret_cast<CloneFn>(addr::kNiObjectClone)(source);
}

void HoldNode(NiAVObject* node) { _InterlockedIncrement(reinterpret_cast<volatile long*>(&node->refCount)); }

void ReleaseNode(NiAVObject* node) {
	if (_InterlockedDecrement(reinterpret_cast<volatile long*>(&node->refCount)) == 0) {
		using DestroyFn = void(__thiscall*)(NiAVObject* self, bool freeThis);
		const auto destroy = *reinterpret_cast<DestroyFn*>(node->vtable);
		destroy(node, true);
	}
}

void AddChildNode(NiAVObject* parent, NiAVObject* child) {
	using AddFn = void(__thiscall*)(NiAVObject* self, NiAVObject* child, UInt32 firstFree);
	const auto add = *reinterpret_cast<AddFn*>(reinterpret_cast<UInt8*>(parent->vtable) + addr::kNiNodeAddObjectSlot);
	add(parent, child, 1);
}

void RemoveChildNode(NiAVObject* parent, NiAVObject* child) {
	using RemoveFn = NiAVObject**(__thiscall*)(NiAVObject* self, NiAVObject** out, NiAVObject* child);
	const auto remove =
		*reinterpret_cast<RemoveFn*>(reinterpret_cast<UInt8*>(parent->vtable) + addr::kNiNodeRemoveObjectSlot);
	NiAVObject* removed = nullptr;
	remove(parent, &removed, child);
	if (removed != nullptr) {
		ReleaseNode(removed);
	}
}

namespace {

constexpr UInt32 kModelLoader = 0x00B33A1C;
constexpr UInt32 kLoadModel = 0x00439EB0;
constexpr UInt32 kUpdateEffects = 0x00707980;
constexpr UInt32 kUpdateProperties = 0x00707610;
constexpr UInt32 kUpdate = 0x00707370;
// Their first bytes as read in Oblivion.exe 1.2.0.416 (the clone's and the
// loader's as game/DismemberCaps.cpp reads them).
const UInt8 kLoadModelBytes[] = {0x6A, 0xFF, 0x68, 0x08, 0xC9, 0x9A, 0x00, 0x64};
const UInt8 kCloneBytes[] = {0x6A, 0xFF, 0x68, 0xE8, 0x93, 0x9C, 0x00, 0x64};
const UInt8 kUpdateEffectsBytes[] = {0x6A, 0xFF, 0x68, 0xF0, 0xE1, 0x9C, 0x00, 0x64};
const UInt8 kUpdatePropertiesBytes[] = {0x6A, 0xFF, 0x68, 0x7B, 0x96, 0x9C, 0x00, 0x64};
const UInt8 kUpdateBytes[] = {0xD9, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xF1, 0x8B};

// As read, or another plugin's jump at the entry - called through it, as the
// wound caps call theirs (game/DismemberCaps.cpp; OBNG detours 0x00707370,
// harness kept-on-body 2026-11-20). Anything else is refused.
bool Checked(UInt32 address, const UInt8* bytes, UInt32 size) {
	const bool asRead = mem::Verify(address, bytes, size);
	mem::RelativeBranch branch;
	const bool jump = !asRead && mem::DecodeRelativeBranch(reinterpret_cast<const UInt8*>(address), address, branch) &&
	                  branch.isJump;
	if (!asRead) {
		mem::ReportForeignCode("OwnedNode", address);
	}
	return CapEntryCallable(CapEntryOf(asRead, jump));
}

bool LoaderAsRead() {
	static int verified = 0;
	if (verified == 0) {
		verified = Checked(kLoadModel, kLoadModelBytes, sizeof(kLoadModelBytes)) &&
		                   Checked(addr::kNiObjectClone, kCloneBytes, sizeof(kCloneBytes))
		               ? 1
		               : -1;
	}
	return verified > 0;
}

bool UpdatesAsRead() {
	static int verified = 0;
	if (verified == 0) {
		verified = Checked(kUpdateEffects, kUpdateEffectsBytes, sizeof(kUpdateEffectsBytes)) &&
		                   Checked(kUpdateProperties, kUpdatePropertiesBytes, sizeof(kUpdatePropertiesBytes)) &&
		                   Checked(kUpdate, kUpdateBytes, sizeof(kUpdateBytes))
		               ? 1
		               : -1;
	}
	return verified > 0;
}

}  // namespace

NiAVObject* LoadModelCopy(const char* path, const char*& why) {
	const UInt32 loader = *reinterpret_cast<const UInt32*>(kModelLoader);
	if (!LoaderAsRead() || !mem::LooksLikeObjectAddress(loader)) {
		why = "the model loader not as read";
		return nullptr;
	}
	using LoadFn = UInt32(__fastcall*)(void* loader, void* edx, const char* path, UInt32 a, UInt32 b, UInt32 c);
	const UInt32 shared = reinterpret_cast<LoadFn>(kLoadModel)(reinterpret_cast<void*>(loader), nullptr, path, 0, 0, 1);
	if (!mem::LooksLikeObjectAddress(shared)) {
		why = "the model did not load";
		return nullptr;
	}
	NiAVObject* const copy = CloneNode(reinterpret_cast<const NiAVObject*>(shared));
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(copy))) {
		why = "the model could not be cloned";
		return nullptr;
	}
	HoldNode(copy);
	return copy;
}

bool SettleNewNode(NiAVObject* node) {
	if (!UpdatesAsRead()) {
		return false;
	}
	using NoArgFn = void(__fastcall*)(NiAVObject* self, void* edx);
	using UpdateFn = void(__fastcall*)(NiAVObject* self, void* edx, float time, UInt32 flags);
	reinterpret_cast<NoArgFn>(kUpdateEffects)(node, nullptr);
	reinterpret_cast<NoArgFn>(kUpdateProperties)(node, nullptr);
	reinterpret_cast<UpdateFn>(kUpdate)(node, nullptr, 0.0f, 0);
	return true;
}

}  // namespace obvr::game

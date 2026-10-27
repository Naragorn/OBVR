#include "game/HolsterSlots.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

UInt32 g_ids[vr::kHolsterSlotCount] = {};
bool g_loaded = false;

constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl, as QuickKeyPages.cpp
constexpr UInt8 kFormTypeArmor = 0x14;           // xOBSE GameForms.h kFormType_Armor
constexpr UInt8 kFormTypeLight = 0x1A;           // kFormType_Light, as game/TorchGrab.h

}  // namespace

UInt32 (&HolsterSlotIds())[vr::kHolsterSlotCount] { return g_ids; }

UInt8* HolsterSlotForm(UInt32 id) {
	if (id == 0) {
		return nullptr;
	}
	using LookupFn = UInt32(__cdecl*)(UInt32 id);
	const UInt32 form = reinterpret_cast<LookupFn>(kLookupFormById)(id);
	return mem::LooksLikeObjectAddress(form) ? reinterpret_cast<UInt8*>(form) : nullptr;
}

UInt32 FormIdOf(const UInt8* form) {
	return form != nullptr ? *reinterpret_cast<const UInt32*>(form + addr::kFormIdOffset) : 0;
}

vr::SlotItem SlotItemOfForm(const UInt8* form) {
	if (form == nullptr) {
		return vr::SlotItem::Nothing;
	}
	const UInt8 type = form[addr::kFormTypeOffset];
	if (type == addr::kFormTypeWeapon) {
		return vr::SlotItemOf(vr::KindOfWeaponType(form[addr::kWeaponTypeOffset]));
	}
	if (type == kFormTypeArmor) {
		return vr::SlotItem::Shield;
	}
	if (type == kFormTypeLight) {
		return vr::SlotItem::Torch;
	}
	return vr::SlotItem::Nothing;
}

bool HolsterSlotsLoaded() { return g_loaded; }

void SetHolsterSlotsLoaded(bool loaded) { g_loaded = loaded; }

}  // namespace obvr::game

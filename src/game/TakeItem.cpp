#include "game/TakeItem.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/MeleeHits.h"
#include "game/NearbyItems.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

bool LooksLikeCode(UInt32 address) {
	return address >= addr::kTextStart && address < addr::kTextEnd;
}

// A virtual off an object's table, refused unless the table and the entry
// look right: a wrong slot would call something else with these arguments.
UInt32 VirtualAt(UInt32 object, UInt32 slotOffset) {
	if (!LooksLikeObject(object)) {
		return 0;
	}
	const UInt32 vtable = Read(object);
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 entry = Read(vtable + slotOffset);
	return LooksLikeCode(entry) ? entry : 0;
}

}  // namespace

bool RefIsItem(UInt32 ref, bool* isBook) {
	const UInt8 type = RefBaseFormType(ref);
	if (isBook != nullptr) {
		*isBook = type == addr::kFormTypeBook;
	}
	return type != 0 && IsHandItemType(type);
}

TakeResult TakeIntoInventory(UInt32 ref, UInt32* ownerId) {
	if (ownerId != nullptr) {
		*ownerId = 0;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return TakeResult::NoPlayer;
	}
	bool isBook = false;
	if (!RefIsItem(ref, &isBook)) {
		return TakeResult::NotAnItem;
	}
	if (ownerId != nullptr) {
		using OwnerFn = UInt32(__fastcall*)(UInt32 self, void* edx);
		const UInt32 owner = reinterpret_cast<OwnerFn>(addr::kRefOwner)(ref, nullptr);
		*ownerId = LooksLikeObject(owner) ? Read(owner + addr::kFormIdOffset) : 0;
	}
	if (isBook) {
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		const UInt8 flags = *reinterpret_cast<const UInt8*>(base + addr::kBookFlagsOffset);
		if ((flags & addr::kBookCantBeTaken) != 0) {
			return TakeResult::CannotTake;
		}
		const UInt32 pickUp = VirtualAt(player, addr::kActorPickUpSlot);
		if (pickUp == 0) {
			return TakeResult::NoPlayer;
		}
		using CountFn = SInt16(__fastcall*)(UInt32 extraList, void* edx);
		const SInt16 count =
			reinterpret_cast<CountFn>(addr::kRefExtraCount)(ref + addr::kRefExtraListOffset, nullptr);
		using PickUpFn = void(__fastcall*)(UInt32 actor, void* edx, UInt32 ref, SInt32 count,
		                                   UInt32 arg);
		reinterpret_cast<PickUpFn>(pickUp)(player, nullptr, ref, count, 0);
		return TakeResult::Taken;
	}
	// As the player's activate control calls it (0x0067318A).
	using ActivateFn = UInt8(__fastcall*)(UInt32 self, void* edx, UInt32 activator, UInt32 a2,
	                                      UInt32 a3, UInt32 a4);
	const UInt8 done =
		reinterpret_cast<ActivateFn>(addr::kRefActivate)(ref, nullptr, player, 0, 0, 1);
	return done != 0 ? TakeResult::Taken : TakeResult::Refused;
}

namespace {

// The poison test's first bytes, as read: `push esi; mov esi, ecx; cmp
// dword [esi+8], 0; jne +0Ah`.
constexpr UInt8 kAllHostileBytes[] = {0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x08, 0x00, 0x75, 0x0A};

bool IsPoisonForm(UInt32 base) {
	const UInt8* const code = reinterpret_cast<const UInt8*>(addr::kEffectListAllHostile);
	for (UInt32 i = 0; i < sizeof(kAllHostileBytes); ++i) {
		if (code[i] != kAllHostileBytes[i]) {
			return true;  // cannot be told: treated as a poison, not drunk
		}
	}
	using AllHostileFn = UInt8(__fastcall*)(UInt32 list, void* edx);
	return reinterpret_cast<AllHostileFn>(addr::kEffectListAllHostile)(base + addr::kMagicItemEffectListOffset,
	                                                                     nullptr) != 0;
}

}  // namespace

bool RefIsEdible(UInt32 ref, bool* isPoison) {
	if (isPoison != nullptr) {
		*isPoison = false;
	}
	const UInt8 type = RefBaseFormType(ref);
	if (!IsEdibleType(type)) {
		return false;
	}
	if (type == addr::kFormTypePotion) {
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		if (!LooksLikeObject(base) || IsPoisonForm(base)) {
			if (isPoison != nullptr) {
				*isPoison = true;
			}
			return false;
		}
	}
	return true;
}

bool EatItem(UInt32 baseForm) {
	if (!LooksLikeObject(baseForm)) {
		return false;
	}
	const UInt8 type = *reinterpret_cast<const UInt8*>(baseForm + addr::kFormTypeOffset);
	if (!IsEdibleType(type) || (type == addr::kFormTypePotion && IsPoisonForm(baseForm))) {
		return false;
	}
	return EquipItemForm(reinterpret_cast<UInt8*>(baseForm), false);
}

UInt32 GrabbedRef() {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	const UInt32 ref = Read(player + addr::kPlayerGrabbedRefOffset);
	return LooksLikeObject(ref) ? ref : 0;
}

const char* TakeResultName(TakeResult result) {
	switch (result) {
	case TakeResult::Taken:
		return "taken";
	case TakeResult::Refused:
		return "refused by the game (its activation answered no)";
	case TakeResult::CannotTake:
		return "not taken - a book marked as not to be taken";
	case TakeResult::NotAnItem:
		return "not taken - not an item";
	case TakeResult::NoPlayer:
		return "not taken - the player could not be reached";
	}
	return "?";
}

}  // namespace obvr::game

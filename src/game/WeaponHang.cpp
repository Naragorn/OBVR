#include "game/WeaponHang.h"

#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kProcessHangWeaponSlot = 0x150;
constexpr UInt32 kProcessHandNodeSlot = 0x118;
// The left hand's node (the "Torch" node), which the hang takes instead
// while the process's byte +0xF4 is set - the bow (0x00656270 at 0x006562AD).
constexpr UInt32 kProcessLeftHandNodeSlot = 0x11C;
constexpr UInt32 kProcessLeftHandedOffset = 0xF4;
constexpr UInt32 kProcessSheathNodeSlot = 0x124;
constexpr UInt32 kProcessSetOnHandSlot = 0x308;
constexpr UInt32 kProcessHangWeapon = 0x00656270;
constexpr UInt32 kPlayerBipedFor = 0x006600D0;
constexpr UInt32 kPlayerAnimDataFor = 0x0065D750;
// 0x00656270's entry: sub esp,30h; push ebx; push ebp; push esi; push edi;
// mov ebp,ecx.
const UInt8 kHangWeaponBytes[] = {0x83, 0xEC, 0x30, 0x53, 0x55, 0x56, 0x57, 0x8B, 0xE9};

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool Looks(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Slot(UInt32 object, UInt32 slot) { return Read(Read(object) + slot); }

UInt32 PlayerFor(UInt32 player, UInt32 function, bool firstPerson) {
	using ForFn = UInt32(__fastcall*)(UInt32 self, void* edx, UInt32 firstPerson);
	return reinterpret_cast<ForFn>(function)(player, nullptr, firstPerson ? 1 : 0);
}

UInt32 ProcessNode(UInt32 process, UInt32 slot, UInt32 biped) {
	using NodeFn = UInt32(__fastcall*)(UInt32 self, void* edx, UInt32 biped);
	return reinterpret_cast<NodeFn>(Slot(process, slot))(process, nullptr, biped);
}

UInt16 Children(UInt32 node) {
	if (!Looks(node)) {
		return 0;
	}
	UInt16 hung = 0;
	const UInt32 array = Read(node + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(node + addr::kNiChildCountOffset);
	for (UInt16 i = 0; Looks(array) && i < count && i < 64; ++i) {
		hung += Looks(Read(array + 4u * i)) ? 1 : 0;
	}
	return hung;
}

// One person's hang, onto the hand or back onto the sheath; whether its
// hand holds a model after it.
bool HangFor(UInt32 process, UInt32 player, bool firstPerson, bool toHand, UInt16& sheathBefore,
             UInt16& handBefore) {
	const UInt32 biped = PlayerFor(player, kPlayerBipedFor, firstPerson);
	const UInt32 animData = PlayerFor(player, kPlayerAnimDataFor, firstPerson);
	const UInt32 handSlot =
		*reinterpret_cast<const UInt8*>(process + kProcessLeftHandedOffset) != 0 ? kProcessLeftHandNodeSlot
		                                                                          : kProcessHandNodeSlot;
	sheathBefore = Children(ProcessNode(process, kProcessSheathNodeSlot, biped));
	handBefore = Children(ProcessNode(process, handSlot, biped));
	using HangFn = void(__fastcall*)(UInt32 self, void* edx, UInt32 toHand, UInt32 biped, UInt32 animData,
	                                 UInt32 actor);
	reinterpret_cast<HangFn>(kProcessHangWeapon)(process, nullptr, toHand ? 1 : 0, biped, animData, player);
	return Children(ProcessNode(process, handSlot, biped)) > 0;
}

}  // namespace

WeaponHangReport HangPlayerWeapon(bool toHand) {
	WeaponHangReport r;
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!Looks(player)) {
		r.why = "no player";
		return r;
	}
	const UInt32 process = Read(player + addr::kMobileProcessOffset);
	if (!Looks(process) || !Looks(Read(process))) {
		r.why = "no process";
		return r;
	}
	if (Slot(process, kProcessHangWeaponSlot) != kProcessHangWeapon ||
	    !mem::Verify(kProcessHangWeapon, kHangWeaponBytes, sizeof(kHangWeaponBytes))) {
		r.why = "the process's hang is not what 1.2.0.416 has (0x00656270)";
		return r;
	}
	using SetFn = void(__fastcall*)(UInt32 self, void* edx, UInt32 value);
	reinterpret_cast<SetFn>(Slot(process, kProcessSetOnHandSlot))(process, nullptr, toHand ? 1 : 0);
	r.called = true;
	r.toHand = toHand;
	r.firstOnHand = HangFor(process, player, true, toHand, r.firstSheathBefore, r.firstHandBefore);
	r.thirdOnHand = HangFor(process, player, false, toHand, r.thirdSheathBefore, r.thirdHandBefore);
	return r;
}

WeaponHangReport HangPlayerWeaponOnHand() { return HangPlayerWeapon(true); }

WeaponHangReport HangPlayerWeaponOnSheath() { return HangPlayerWeapon(false); }

}  // namespace obvr::game

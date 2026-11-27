#include "game/ForearmLines.h"

#include "core/AddressSpace.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"

namespace obvr::game {
namespace {

using GetterFn = void*(__fastcall*)(void* self, void* edx, UInt32 arg);

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool& LastRead() {
	static bool read = false;
	return read;
}

bool Arm(const char* forearm, const char* knuckle, NiPoint3& a, NiPoint3& b) {
	const NiAVObject* const from = FindFirstPersonNode(forearm);
	const NiAVObject* const to = FindFirstPersonNode(knuckle);
	if (from == nullptr || to == nullptr) {
		return false;
	}
	a = from->worldTransform.pos;
	b = to->worldTransform.pos;
	return (b - a).LengthSquared() > 1.0f;
}

}  // namespace

ForearmLines ReadForearmLines() {
	ForearmLines l;
	SInt32 type = -1;
	const bool fists = EquippedWeaponForm(&type) == nullptr && ReadPlayerWeaponState() == WeaponState::Drawn;
	if (fists) {
		l.rightValid = Arm("Bip01 R Forearm", "Bip01 R Finger2", l.rightA, l.rightB);
		l.leftValid = Arm("Bip01 L Forearm", "Bip01 L Finger2", l.leftA, l.leftB);
	}
	LastRead() = l.rightValid && l.leftValid;
	return l;
}

bool ForearmLinesRead() { return LastRead(); }

bool AttackerArmedOrUnplaced(UInt32 attacker) {
	if (!mem::LooksLikeObjectAddress(attacker)) {
		return true;
	}
	const UInt32 process = Read(attacker + addr::kMobileProcessOffset);
	if (!mem::LooksLikeObjectAddress(process)) {
		return true;
	}
	const UInt32 vtable = Read(process);
	if (!mem::LooksLikeObjectAddress(vtable)) {
		return true;
	}
	const UInt32 getter = Read(vtable + addr::kProcessVtableEquippedWeaponOffset);
	if (getter < addr::kTextStart || getter >= addr::kTextEnd) {
		return true;
	}
	return reinterpret_cast<GetterFn>(getter)(reinterpret_cast<void*>(process), nullptr, 1) != nullptr;
}

}  // namespace obvr::game

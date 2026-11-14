#include "game/HintReads.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "vr/HintLogic.h"

namespace obvr::game {

namespace {

constexpr UInt8 kFormTypeNpc = 0x23;            // NPC_ (xOBSE GameForms.h kFormType_NPC)
constexpr UInt32 kNpcClassOffset = 0x104;       // TESNPC::npcClass
constexpr UInt32 kClassFlagsOffset = 0x60;      // TESClass::classFlags
constexpr UInt32 kClassFlagGuard = 0x2;         // TESClass::kFlag_Guard
constexpr UInt32 kGetActorValueFSlot = 0x288;   // float, current (KnockdownRoll.cpp)
constexpr UInt32 kAvResponsibility = 0x24;

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
bool LooksLikeObject(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

bool ReadValue(UInt32 actor, UInt32 av, float& now) {
	const UInt32 vtable = Read(actor);
	const UInt32 slot = LooksLikeObject(vtable) ? Read(vtable + kGetActorValueFSlot) : 0;
	if (!LooksLikeObject(slot)) {
		return false;
	}
	using GetFloatFn = float(__thiscall*)(void* actor, UInt32 av);
	now = reinterpret_cast<GetFloatFn>(slot)(reinterpret_cast<void*>(actor), av);
	return now == now;
}

}  // namespace

bool ActorIsLawful(UInt32 actor) {
	const UInt32 base = LooksLikeObject(actor) ? Read(actor + addr::kRefBaseFormOffset) : 0;
	if (!LooksLikeObject(base)) {
		return false;
	}
	const bool npc = *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) == kFormTypeNpc;
	if (!npc) {
		return false;
	}
	const UInt32 cls = Read(base + kNpcClassOffset);
	const bool guard = LooksLikeObject(cls) && (Read(cls + kClassFlagsOffset) & kClassFlagGuard) != 0;
	float responsibility = 0.0f;
	if (!ReadValue(actor, kAvResponsibility, responsibility)) {
		responsibility = 0.0f;
	}
	return vr::HintLawfulFoe(true, guard, responsibility);
}

}  // namespace obvr::game

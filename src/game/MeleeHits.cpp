#include "game/MeleeHits.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/MeleeHit.h"
#include "game/PlayerAim.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

bool LooksLikeCode(UInt32 address) {
	return address >= addr::kTextStart && address < addr::kTextEnd;
}

// A virtual read off an object's table, refused unless the table and the
// entry look like what they should be: a wrong slot would call some other
// function with these arguments.
UInt32 VirtualAt(const void* object, UInt32 slotOffset) {
	if (!LooksLikeObject(object)) {
		return 0;
	}
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(object);
	if (!mem::LooksLikeObjectAddress(vtable)) {
		return 0;
	}
	const UInt32 entry = *reinterpret_cast<const UInt32*>(vtable + slotOffset);
	return LooksLikeCode(entry) ? entry : 0;
}

using ThisFn = void* (__fastcall*)(void* self, void* edx);
using ThisFloatFn = float (__fastcall*)(void* self, void* edx);
using ThisBoolArgFn = UInt8 (__fastcall*)(void* self, void* edx, UInt32 arg);
using ThisPtrArgFn = void* (__fastcall*)(void* self, void* edx, UInt32 arg);
using ReachFn = float (__cdecl*)(float reach);
using AttackHandlingFn = void (__fastcall*)(void* self, void* edx, UInt32 powerAttack,
                                            void* arrowRef, void* target);

struct ListNode {
	void* data;
	ListNode* next;
};

UInt8* PlayerOrNull() {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return nullptr;
	}
	// The player it is, and the table the hit function is read from holds
	// the function this build was read for - or none of this applies.
	if (*reinterpret_cast<const UInt32*>(player) != addr::kVtblPlayerCharacter) {
		return nullptr;
	}
	if (VirtualAt(player, addr::kActorVtableAttackHandlingOffset) != addr::kAttackHandling) {
		return nullptr;
	}
	return player;
}

// The equipped weapon through the process, the way the hit function asks
// for it. Null for fists - and for a process that cannot be read, which the
// caller tells apart by the answer.
bool EquippedWeapon(UInt8* player, UInt8** weaponOut) {
	*weaponOut = nullptr;
	auto* const process = *reinterpret_cast<UInt8* const*>(player + addr::kMobileProcessOffset);
	if (!LooksLikeObject(process)) {
		return false;
	}
	const UInt32 getter = VirtualAt(process, addr::kProcessVtableEquippedWeaponOffset);
	if (getter == 0) {
		return false;
	}
	auto* const entry =
		static_cast<UInt8*>(reinterpret_cast<ThisPtrArgFn>(getter)(process, nullptr, 1));
	if (entry == nullptr) {
		return true;
	}
	if (!LooksLikeObject(entry)) {
		return false;
	}
	auto* const weapon = *reinterpret_cast<UInt8* const*>(entry + addr::kEntryDataTypeOffset);
	if (weapon != nullptr && !LooksLikeObject(weapon)) {
		return false;
	}
	*weaponOut = weapon;
	return true;
}

SInt32 WeaponTypeOf(const UInt8* weapon) {
	if (weapon == nullptr) {
		return static_cast<SInt32>(WeaponTypeCode::None);
	}
	return *reinterpret_cast<const signed char*>(weapon + addr::kWeaponTypeOffset);
}

// The reach in game units, the hit function's own arithmetic: the weapon's
// reach through the setting, or the hand's reach, times the actor's scale.
bool ReachUnits(UInt8* player, const UInt8* weapon, float* out) {
	float reach = 0.0f;
	if (weapon != nullptr) {
		const float raw = *reinterpret_cast<const float*>(weapon + addr::kWeaponReachOffset);
		if (!(raw > 0.0f && raw < 100.0f)) {
			return false;
		}
		reach = reinterpret_cast<ReachFn>(addr::kReachInUnits)(raw);
	} else {
		const UInt32 handReach = VirtualAt(player, addr::kActorVtableHandReachOffset);
		if (handReach == 0) {
			return false;
		}
		reach = reinterpret_cast<ThisFloatFn>(handReach)(player, nullptr);
	}
	const UInt32 getScale = VirtualAt(player, addr::kActorVtableGetScaleOffset);
	if (getScale == 0) {
		return false;
	}
	const float scale = reinterpret_cast<ThisFloatFn>(getScale)(player, nullptr);
	if (scale > 0.0f) {
		reach *= scale;
	}
	if (!(reach > 0.0f && reach < 4096.0f)) {
		return false;
	}
	*out = reach;
	return true;
}

const char* SettingNameOrEmpty(UInt32 valueAddress) {
	const char* const name = *reinterpret_cast<const char* const*>(valueAddress + 4);
	if (!LooksLikeObject(name)) {
		return "";
	}
	for (UInt32 at = 0; at < 48; ++at) {
		if (name[at] == '\0') {
			return at > 0 ? name : "";
		}
		if (name[at] < 0x20 || name[at] > 0x7E) {
			return "";
		}
	}
	return "";
}

bool IsActorObject(const void* object) {
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(object);
	return vtable == addr::kVtblCharacter || vtable == addr::kVtblCreature;
}

bool ActorIsDead(void* actor) {
	const UInt32 isDead = VirtualAt(actor, addr::kActorVtableIsDeadOffset);
	if (isDead == 0) {
		return true;  // unreadable: leave it alone
	}
	return (reinterpret_cast<ThisBoolArgFn>(isDead)(actor, nullptr, 0) & 1) != 0;
}

bool ActorBound(void* actor, NiBound* out) {
	const UInt32 getNode = VirtualAt(actor, addr::kActorVtableGetNiNodeOffset);
	if (getNode == 0) {
		return false;
	}
	const auto* const node =
		static_cast<const UInt8*>(reinterpret_cast<ThisFn>(getNode)(actor, nullptr));
	if (!LooksLikeObject(node)) {
		return false;
	}
	*out = *reinterpret_cast<const NiBound*>(node + addr::kNiAVObjectWorldBoundOffset);
	const float radius = out->radius;
	if (!(radius >= 0.0f && radius < 4096.0f)) {
		return false;
	}
	return true;
}

SwingLedger g_ledger;
bool g_armedReported = false;
bool g_refusedReported = false;
UInt32 g_strikeLinesLeft = 20;

}  // namespace

bool MeleeInHand(SInt32* weaponType) {
	if (weaponType != nullptr) {
		*weaponType = static_cast<SInt32>(WeaponTypeCode::None);
	}
	UInt8* const player = PlayerOrNull();
	if (player == nullptr) {
		return false;
	}
	UInt8* weapon = nullptr;
	if (!EquippedWeapon(player, &weapon)) {
		return false;
	}
	const SInt32 type = WeaponTypeOf(weapon);
	if (weaponType != nullptr) {
		*weaponType = type;
	}
	return WeaponIsSwung(type) && ReadPlayerWeaponState() == WeaponState::Drawn;
}

UInt32 StrikeByMotion(const MotionStrike& strike) {
	UInt8* const player = PlayerOrNull();
	if (player == nullptr) {
		if (!g_refusedReported) {
			g_refusedReported = true;
			OBVR_LOG("Hands: strikes by motion refused - the player's table does not hold the "
			         "hit function this build was read for");
		}
		return 0;
	}
	UInt8* weapon = nullptr;
	if (!EquippedWeapon(player, &weapon) || !WeaponIsSwung(WeaponTypeOf(weapon)) ||
	    ReadPlayerWeaponState() != WeaponState::Drawn) {
		return 0;
	}
	float reach = 0.0f;
	if (!ReachUnits(player, weapon, &reach)) {
		return 0;
	}

	// The camera's frame, the space the hand's offset is measured from -
	// the same the bones are pinned in.
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr || !LooksLikeObject(root->parent)) {
		return 0;
	}
	const NiAVObject* const camera = root->parent;
	const Blade blade = BladeInWorld(camera->worldTransform.rot, camera->worldTransform.pos,
	                                 strike.handRotation, strike.handOffsetUnits, reach);

	if (!g_armedReported) {
		g_armedReported = true;
		OBVR_LOG("Hands: strikes by motion armed - weapon type %d, reach %.1f units (the reach "
		         "setting is \"%s\" = %.1f), within %.2f of a body's bound plus %.1f units",
		         WeaponTypeOf(weapon), reach, SettingNameOrEmpty(addr::kCombatDistanceSetting),
		         *reinterpret_cast<const float*>(addr::kCombatDistanceSetting), strike.boundFactor,
		         strike.padUnits);
	}

	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	auto* node = static_cast<ListNode*>(
		reinterpret_cast<ThisPtrArgFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	UInt32 struck = 0;
	for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
		if (!LooksLikeObject(node)) {
			break;
		}
		void* const actor = node->data;
		node = node->next;
		if (actor == nullptr || actor == player || !LooksLikeObject(actor) || !IsActorObject(actor)) {
			continue;
		}
		if (ActorIsDead(actor)) {
			continue;
		}
		NiBound bound;
		if (!ActorBound(actor, &bound)) {
			continue;
		}
		if (!BladeStrikes(blade, bound.center, bound.radius, strike.boundFactor, strike.padUnits)) {
			continue;
		}
		if (!LedgerAdmits(g_ledger, strike.swingSerial, actor)) {
			continue;
		}
		reinterpret_cast<AttackHandlingFn>(addr::kAttackHandling)(player, nullptr,
		                                                          strike.heavy ? 1u : 0u, nullptr,
		                                                          actor);
		++struck;
		if (g_strikeLinesLeft > 0) {
			--g_strikeLinesLeft;
			OBVR_LOG("Hands: the blade struck %08X (%s, bound radius %.0f, %.0f units from its "
			         "centre) - %s attack, swing %u",
			         reinterpret_cast<UInt32>(actor),
			         *reinterpret_cast<const UInt32*>(actor) == addr::kVtblCreature ? "creature"
			                                                                          : "character",
			         bound.radius, SegmentPointDistance(blade.base, blade.tip, bound.center),
			         strike.heavy ? "heavy" : "light", strike.swingSerial);
		}
	}
	return struck;
}

void ForgetStrikes() { g_ledger = SwingLedger{}; }

}  // namespace obvr::game

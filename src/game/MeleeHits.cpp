#include "game/MeleeHits.h"

#include <cstdlib>
#include <limits>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/MeleeHit.h"
#include "game/PlayerAim.h"
#include "game/ShoveLogic.h"

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

SwingLedger g_ledger[2];  // per hand: the weapon hand, the other
// The weapon type the armed line was last written for: again on each change,
// the first run only ever said "fists" (type -1).
SInt32 g_armedType = -1000;
UInt32 g_armedLinesLeft = 8;
bool g_refusedReported = false;
UInt32 g_strikeLinesLeft = 20;

// The actor's GetAnimData (vtable +0x164, xOBSE GameObjects.h; the player's
// 0x0065D720 answers its first-person data in first person) and the word whose
// low byte is the current attack group (animsMapKey[3], +0x42).
constexpr UInt32 kActorVtableAnimDataOffset = 0x164;
constexpr UInt32 kAnimDataAttackGroupOffset = 0x42;
// vr::kAnimGroupAttackLight - AttackLeft, so a light strike carries no power group.
constexpr UInt8 kAnimGroupLightStrike = 0x14;

// Hands `actor` to the engine's melee hit, read as `group` (0 leaves the
// attacker's): AttackHandling takes the power attack's direction from the
// low byte of the attacker's AnimData +0x42 (0x005FF355, 0x0060028E) - set
// for the call, put back after unless the engine wrote its own meanwhile.
// The target's health as the actor values have it (GetActorValue_F, vtable
// +0x288, as PlayerTeleport reads fatigue; health is actor value 8): what a
// strike took, for the log. NaN when it cannot be read.
constexpr UInt32 kActorVtableActorValueF = 0x288;
constexpr UInt32 kActorValueHealth = 8;

float HealthOf(void* actor) {
	const UInt32 slot = VirtualAt(actor, kActorVtableActorValueF);
	if (slot == 0) {
		return std::numeric_limits<float>::quiet_NaN();
	}
	using GetFloatFn = float(__thiscall*)(void* actor, UInt32 av);
	return reinterpret_cast<GetFloatFn>(slot)(actor, kActorValueHealth);
}

void ApplyStrike(UInt8* player, void* actor, bool heavy, UInt8 group, UInt32 serial) {
	const float healthBefore = HealthOf(actor);
	UInt16* groupWord = nullptr;
	UInt16 groupBefore = 0;
	UInt16 groupWritten = 0;
	if (group != 0) {
		const UInt32 animDataOf = VirtualAt(player, kActorVtableAnimDataOffset);
		UInt8* const animData =
			animDataOf != 0 ? static_cast<UInt8*>(reinterpret_cast<ThisFn>(animDataOf)(player, nullptr)) : nullptr;
		if (LooksLikeObject(animData)) {
			groupWord = reinterpret_cast<UInt16*>(animData + kAnimDataAttackGroupOffset);
			groupBefore = *groupWord;
			groupWritten = static_cast<UInt16>((groupBefore & 0xFF00u) | group);
			*groupWord = groupWritten;
		}
	}
	reinterpret_cast<AttackHandlingFn>(addr::kAttackHandling)(player, nullptr, heavy ? 1u : 0u, nullptr, actor);
	if (groupWord != nullptr && *groupWord == groupWritten) {
		*groupWord = groupBefore;
	}
	static UInt32 linesLeft = 30;
	if (linesLeft > 0) {
		--linesLeft;
		OBVR_LOG("Hands: struck %08X - %s attack (group %02X), swing %u; health %.1f -> %.1f",
		         reinterpret_cast<UInt32>(actor), heavy ? "power" : "light", static_cast<UInt32>(group), serial,
		         static_cast<double>(healthBefore), static_cast<double>(HealthOf(actor)));
	}
}

// Bodies met before their swing was a power attack, held to its end
// (MeleeHit.h, SettleHeldStrike).
struct HeldBody {
	UInt32 hand = 0;
	void* actor = nullptr;
	UInt32 serial = 0;
};
constexpr UInt32 kMaxHeldBodies = 8;
HeldBody g_held[kMaxHeldBodies];

void HoldStrike(void* actor, UInt32 serial, UInt32 hand) {
	for (HeldBody& h : g_held) {
		if (h.actor == nullptr) {
			h.actor = actor;
			h.serial = serial;
			h.hand = hand;
			return;
		}
	}
}

}  // namespace

UInt8* EquippedWeaponForm(SInt32* type) {
	*type = static_cast<SInt32>(WeaponTypeCode::None);
	UInt8* const player = PlayerOrNull();
	UInt8* weapon = nullptr;
	if (player == nullptr || !EquippedWeapon(player, &weapon) || weapon == nullptr) {
		return nullptr;
	}
	*type = WeaponTypeOf(weapon);
	return weapon;
}

bool WeaponWeightOf(const UInt8* weapon, float* weight) {
	if (weapon == nullptr) {
		return false;
	}
	const float raw = *reinterpret_cast<const float*>(weapon + addr::kWeaponWeightOffset);
	if (!(raw >= 0.0f && raw < 1000.0f)) {
		return false;
	}
	*weight = raw;
	return true;
}

// Equipping without the item's up/down sound, as xOBSE's EquipItemSilent
// does (Commands_Inventory.cpp, OverrideGameSounds_Execute): the `jne` at the
// start of the sound picker 0x005E96E0 ("char* GetItemUpDownSound(TESForm*
// item, bool up, arg2)") nopped for the call, so it answers no sound for any
// item, and put back after. The bytes are checked first; anything else there
// and the equip keeps its sound.
namespace {

constexpr UInt32 kItemSoundBranch = 0x005E96E7;
constexpr UInt8 kItemSoundBranchBytes[2] = {0x75, 0x06};
constexpr UInt8 kItemSoundNops[2] = {0x90, 0x90};

class SilentItemSounds {
public:
	SilentItemSounds() {
		m_patched = mem::Verify(kItemSoundBranch, kItemSoundBranchBytes, 2) &&
		            mem::SafeWrite(kItemSoundBranch, kItemSoundNops, 2);
	}
	~SilentItemSounds() {
		if (m_patched) {
			mem::SafeWrite(kItemSoundBranch, kItemSoundBranchBytes, 2);
		}
	}
	bool Patched() const { return m_patched; }

private:
	bool m_patched = false;
};

void ReportSilence(bool patched) {
	static bool s_said = false;
	if (!s_said) {
		s_said = true;
		OBVR_LOG("Hands: weapons change without the item sound (%s)",
		         patched ? "the sound picker at 005E96E7 nopped for the call"
		                 : "NOT - the sound picker is not what 1.2.0.416 has there");
	}
}

}  // namespace

bool EquipWeaponForm(UInt8* weapon) {
	UInt8* const player = PlayerOrNull();
	if (player == nullptr || !LooksLikeObject(weapon)) {
		return false;
	}
	using EquipItemFn = void(__fastcall*)(UInt8* actor, void* edx, UInt8* item, UInt32 count,
	                                      void* extraData, UInt32 unk3, bool lockEquip);
	const SilentItemSounds silent;
	ReportSilence(silent.Patched());
	reinterpret_cast<EquipItemFn>(addr::kActorEquipItem)(player, nullptr, weapon, 1, nullptr, 1,
	                                                     false);
	return true;
}

bool UnequipWeapon() {
	UInt8* const player = PlayerOrNull();
	UInt8* weapon = nullptr;
	if (player == nullptr || !EquippedWeapon(player, &weapon) || weapon == nullptr) {
		return false;
	}
	// As the game's UnequipItem command: the worn stack first, then the
	// unequip with it (addr::kActorUnequipItem).
	using ChangesFn = void*(__fastcall*)(void* extraList, void* edx);
	using FindWornFn = void*(__fastcall*)(void* changes, void* edx, UInt8* item, UInt32 zero);
	void* const changes = reinterpret_cast<ChangesFn>(addr::kGetContainerChanges)(
		player + addr::kActorExtraListOffset, nullptr);
	if (!LooksLikeObject(changes)) {
		return false;
	}
	void* const worn =
		reinterpret_cast<FindWornFn>(addr::kFindWornExtraData)(changes, nullptr, weapon, 0);
	if (!LooksLikeObject(worn)) {
		return false;
	}
	// Its return value did not say whether it worked (2026-09-27: it read
	// false for an unequip the game carried out), so the call being made is
	// what is reported; whether the slot empties is watched by vr::StepFist.
	using UnequipFn = void(__fastcall*)(UInt8* actor, void* edx, UInt8* item, UInt32 count,
	                                    void* extraData, UInt32 unk3, bool lock, UInt32 unk5);
	const SilentItemSounds silent;
	ReportSilence(silent.Patched());
	reinterpret_cast<UnequipFn>(addr::kActorUnequipItem)(player, nullptr, weapon, 1, worn, 0, false,
	                                                     0);
	return true;
}

UInt32 EquippedWeaponFormId() {
	UInt8* const player = PlayerOrNull();
	UInt8* weapon = nullptr;
	if (player == nullptr || !EquippedWeapon(player, &weapon) || weapon == nullptr) {
		return 0;
	}
	return *reinterpret_cast<const UInt32*>(weapon + 0x0C);
}

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

bool PlayerIsDead() {
	UInt8* const player = PlayerOrNull();
	if (player == nullptr) {
		return false;
	}
	const UInt32 isDead = VirtualAt(player, addr::kActorVtableIsDeadOffset);
	if (isDead == 0) {
		return false;
	}
	return (reinterpret_cast<ThisBoolArgFn>(isDead)(player, nullptr, 0) & 1) != 0;
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
	if (!EquippedWeapon(player, &weapon) || ReadPlayerWeaponState() != WeaponState::Drawn) {
		return 0;
	}
	const StrikeKind kind = StrikeKindFor(strike.arrow, strike.hand, weapon != nullptr, WeaponTypeOf(weapon));
	if (kind == StrikeKind::None) {
		return 0;
	}
	const bool arrow = kind == StrikeKind::Arrow;
	float reach = 0.0f;
	if (arrow) {
		reach = math::Sqrt((strike.arrowHead - strike.arrowNock).LengthSquared());
	} else if (!ReachUnits(player, weapon, &reach)) {
		return 0;
	}

	// The world camera the hand's offset is measured from. It was the
	// first-person root's parent, whose world transform is not the world
	// the bodies stand in: every swing of the 2026-09-25 evening run walked
	// all the living, bounded actors (282 entries on one swing) and found
	// not one within 2048 units of the blade. The grab by reach, which
	// works, measures from the camera the eyes are built on; so does this.
	if (!strike.cameraValid) {
		return 0;
	}
	const Blade blade = arrow ? ArrowBladeInWorld(strike.cameraRotation, strike.cameraPosition, strike.handOffsetUnits,
	                                              strike.arrowGrip, strike.arrowNock, strike.arrowHead)
	                          : BladeInWorld(strike.cameraRotation, strike.cameraPosition, strike.handRotation,
	                                         strike.handOffsetUnits, reach);

	if (WeaponTypeOf(weapon) != g_armedType && g_armedLinesLeft > 0) {
		g_armedType = WeaponTypeOf(weapon);
		--g_armedLinesLeft;
		OBVR_LOG("Hands: strikes by motion armed - weapon type %d, reach %.1f units (the reach "
		         "setting is \"%s\" = %.1f), within %.2f of a body's bound plus %.1f units",
		         WeaponTypeOf(weapon), reach, SettingNameOrEmpty(addr::kCombatDistanceSetting),
		         *reinterpret_cast<const float*>(addr::kCombatDistanceSetting), strike.boundFactor,
		         strike.padUnits);
	}

	// A swing that struck nothing, said once when the next swing starts: how
	// close the blade came to the nearest living body and how close it had to
	// come. No run has logged a strike yet ("the blade struck" never appears),
	// and whether the blade misses by a hand's width or by metres is the
	// first thing to know.
	static UInt32 s_missSerial = 0;
	static bool s_missStruck = false;
	static UInt32 s_missBodies = 0;
	// Where the walk lost them: list entries seen, of those Characters or
	// Creatures, alive, with a readable bound - and the first other table.
	static UInt32 s_missEntries = 0, s_missActors = 0, s_missAlive = 0, s_missBounded = 0;
	static UInt32 s_missOtherVtable = 0;
	static float s_missNearest = -1.0f;
	static float s_missNeeded = 0.0f;
	static NiPoint3 s_missCentre{0.0f, 0.0f, 0.0f};
	static float s_missReach = 0.0f;
	static SInt32 s_missType = 0;
	static UInt32 s_missLinesLeft = 20;
	if (strike.swingSerial != s_missSerial) {
		if (s_missSerial != 0 && !s_missStruck && s_missLinesLeft > 0) {
			--s_missLinesLeft;
			OBVR_LOG("Hands: swing %u struck nothing - weapon type %d, reach %.0f; list entries %u, "
			         "actors %u, alive %u, bounded %u (first other table %08X); %u bodies "
			         "near, the nearest %.0f units from the blade, %.0f needed",
			         s_missSerial, s_missType, s_missReach, s_missEntries, s_missActors, s_missAlive,
			         s_missBounded, s_missOtherVtable, s_missBodies, s_missNearest,
			         s_missNeeded);
		}
		s_missSerial = strike.swingSerial;
		s_missStruck = false;
		s_missBodies = 0;
		s_missEntries = s_missActors = s_missAlive = s_missBounded = 0;
		s_missOtherVtable = 0;
		s_missNearest = -1.0f;
	}
	s_missReach = reach;
	s_missType = WeaponTypeOf(weapon);

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
		if (actor == nullptr || actor == player || !LooksLikeObject(actor)) {
			continue;
		}
		++s_missEntries;
		if (!IsActorObject(actor)) {
			if (s_missOtherVtable == 0) {
				s_missOtherVtable = *reinterpret_cast<const UInt32*>(actor);
			}
			continue;
		}
		++s_missActors;
		if (ActorIsDead(actor)) {
			continue;
		}
		++s_missAlive;
		NiBound bound;
		if (!ActorBound(actor, &bound)) {
			continue;
		}
		++s_missBounded;
		{
			const float distance = SegmentPointDistance(blade.base, blade.tip, bound.center);
			if (distance == distance) {  // any finite distance: how far is the finding
				++s_missBodies;
				if (s_missNearest < 0.0f || distance < s_missNearest) {
					s_missNearest = distance;
					s_missNeeded = bound.radius * strike.boundFactor + strike.padUnits;
					s_missCentre = bound.center;
				}
			}
		}
		if (!BladeStrikes(blade, bound.center, bound.radius, strike.boundFactor, strike.padUnits)) {
			continue;
		}
		if (!LedgerAdmits(g_ledger[strike.hand & 1u], strike.swingSerial, actor)) {
			continue;
		}
		++struck;
		s_missStruck = true;
		const bool now = strike.heavy || arrow;
		if (g_strikeLinesLeft > 0) {
			--g_strikeLinesLeft;
			OBVR_LOG("Hands: the %s met %08X (%s, bound radius %.0f, %.0f units from its "
			         "centre) - swing %u, %s",
			         arrow ? "arrow in the hand" : "blade", reinterpret_cast<UInt32>(actor),
			         *reinterpret_cast<const UInt32*>(actor) == addr::kVtblCreature ? "creature"
			                                                                          : "character",
			         bound.radius, SegmentPointDistance(blade.base, blade.tip, bound.center), strike.swingSerial,
			         arrow  ? "a stab: struck now"
			         : now ? "a power attack already: struck now"
			               : "held to the swing's end");
		}
		if (now) {
			ApplyStrike(player, actor, strike.heavy, strike.attackGroup, strike.swingSerial);
		} else {
			HoldStrike(actor, strike.swingSerial, strike.hand & 1u);
		}
	}
	// A stab is one thrust, not a swing with a next one to report it: said at
	// once, a few times, how near it came.
	static UInt32 s_stabMissLines = 12;
	if (arrow && struck == 0 && s_stabMissLines > 0) {
		--s_stabMissLines;
		OBVR_LOG("Hands: the arrow in the hand thrust and met nobody - the nearest body %.0f units from it, %.0f "
		         "needed (its centre %.0f %.0f %.0f, the arrow %.0f %.0f %.0f to %.0f %.0f %.0f; the camera %.0f %.0f "
		         "%.0f, the hand %.0f %.0f %.0f from it) - swing %u",
		         static_cast<double>(s_missNearest), static_cast<double>(s_missNeeded),
		         static_cast<double>(s_missCentre.x), static_cast<double>(s_missCentre.y),
		         static_cast<double>(s_missCentre.z), static_cast<double>(blade.base.x),
		         static_cast<double>(blade.base.y), static_cast<double>(blade.base.z), static_cast<double>(blade.tip.x),
		         static_cast<double>(blade.tip.y), static_cast<double>(blade.tip.z),
		         static_cast<double>(strike.cameraPosition.x), static_cast<double>(strike.cameraPosition.y),
		         static_cast<double>(strike.cameraPosition.z), static_cast<double>(strike.handOffsetUnits.x),
		         static_cast<double>(strike.handOffsetUnits.y), static_cast<double>(strike.handOffsetUnits.z),
		         strike.swingSerial);
	}
	return struck;
}

void SettleHeldStrikes(UInt32 currentSerial, bool swingActive, bool swingPower, bool endedPower, UInt8 powerGroup,
                       UInt32 hand) {
	UInt8* const player = PlayerOrNull();
	for (HeldBody& h : g_held) {
		if (h.actor == nullptr || h.hand != (hand & 1u)) {
			continue;
		}
		const HeldStrike verdict = SettleHeldStrike(h.serial, currentSerial, swingActive, swingPower, endedPower);
		if (verdict == HeldStrike::Wait) {
			continue;
		}
		// Still a living actor? (It may have died or gone since.)
		if (player != nullptr && LooksLikeObject(h.actor) && IsActorObject(h.actor) && !ActorIsDead(h.actor)) {
			const bool power = verdict == HeldStrike::Power;
			ApplyStrike(player, h.actor, power, power ? powerGroup : kAnimGroupLightStrike, h.serial);
		}
		h = HeldBody{};
	}
}

void ForgetStrikes() {
	g_ledger[0] = SwingLedger{};
	g_ledger[1] = SwingLedger{};
}

}  // namespace obvr::game

namespace obvr::game {
namespace {

// The engine's weapon sound (0x006AF880, cdecl, 9 dwords, the caller
// cleans up): with no target it plays the swish - WPNSwishHand for no
// weapon, else Small/Medium/Large by the weapon's speed
// (bUseSpeedForWeaponSwish) - as a 3D sound at the actor. Vanilla's own miss
// call: (actor, 0.0, 0.0, 0, weaponType or -1, -1, -1, 0, 0) at
// 0x005FEC7D..0x005FEC95. Read 2026-09-29.
constexpr UInt32 kWeaponSound = 0x006AF880;
constexpr UInt8 kWeaponSoundBytes[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0xEC, 0x24, 0x53, 0x33, 0xDB};
int g_weaponSoundVerified = -1;
UInt32 g_swishLines = 4;

}  // namespace

bool PlaySwingSwish() {
	if (g_weaponSoundVerified < 0) {
		g_weaponSoundVerified = mem::Verify(kWeaponSound, kWeaponSoundBytes, sizeof(kWeaponSoundBytes)) ? 1 : 0;
		if (g_weaponSoundVerified == 0) {
			OBVR_LOG("Hands: the weapon sound function at %08X is not the bytes read - swings stay silent",
			         kWeaponSound);
		}
	}
	UInt8* const player = PlayerOrNull();
	if (g_weaponSoundVerified != 1 || player == nullptr) {
		return false;
	}
	SInt32 type = static_cast<SInt32>(WeaponTypeCode::None);
	if (!MeleeInHand(&type)) {
		return false;
	}
	using WeaponSoundFn = void(__cdecl*)(void* actor, float a, float b, void* target, SInt32 weaponType,
	                                     SInt32 armour, SInt32 shield, UInt32 c, UInt32 d);
	reinterpret_cast<WeaponSoundFn>(kWeaponSound)(player, 0.0f, 0.0f, nullptr, type, -1, -1, 0, 0);
	if (g_swishLines > 0) {
		--g_swishLines;
		OBVR_LOG("Hands: a swing's swish (weapon type %d)", type);
	}
	return true;
}

}  // namespace obvr::game

namespace obvr::game {

void* LivingActorAt(const NiPoint3& point, float factor, float padUnits, NiPoint3* centreOut,
                    float heightFactor) {
	UInt8* const player = PlayerOrNull();
	if (player == nullptr) {
		return nullptr;
	}
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	auto* node = static_cast<ListNode*>(
		reinterpret_cast<ThisPtrArgFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	void* best = nullptr;
	float bestSquared = 0.0f;
	for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
		if (!LooksLikeObject(node)) {
			break;
		}
		void* const actor = node->data;
		node = node->next;
		if (actor == nullptr || actor == player || !LooksLikeObject(actor) || !IsActorObject(actor) ||
		    ActorIsDead(actor)) {
			continue;
		}
		NiBound bound;
		if (!ActorBound(actor, &bound)) {
			continue;
		}
		// A ball round the bound's centre, or with a height factor an upright
		// column (HandAtBody), nearest across the ground.
		const float reach = bound.radius * factor + padUnits;
		const NiPoint3 off = point - bound.center;
		const bool column = heightFactor > 0.0f;
		const float d = column ? off.x * off.x + off.y * off.y : off.LengthSquared();
		const bool at = column ? HandAtBody(point, bound.center, bound.radius, factor, heightFactor, padUnits)
		                       : d <= reach * reach;
		if (!at || (best != nullptr && d >= bestSquared)) {
			continue;
		}
		best = actor;
		bestSquared = d;
		if (centreOut != nullptr) {
			*centreOut = bound.center;
		}
	}
	return best;
}

void* ActorUnderRay(const NiPoint3& origin, const NiPoint3& direction, float maxUnits, float coneCos,
                    NiPoint3* centreOut) {
	UInt8* const player = PlayerOrNull();
	const float dirLength = math::Sqrt(direction.LengthSquared());
	if (player == nullptr || !(dirLength > 1.0e-6f)) {
		return nullptr;
	}
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	auto* node = static_cast<ListNode*>(
		reinterpret_cast<ThisPtrArgFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	void* best = nullptr;
	float bestSquared = 0.0f;
	for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
		if (!LooksLikeObject(node)) {
			break;
		}
		void* const actor = node->data;
		node = node->next;
		if (actor == nullptr || actor == player || !LooksLikeObject(actor) || !IsActorObject(actor) ||
		    ActorIsDead(actor)) {
			continue;
		}
		NiBound bound;
		if (!ActorBound(actor, &bound)) {
			continue;
		}
		const NiPoint3 to = bound.center - origin;
		const float dSquared = to.LengthSquared();
		const float toLength = math::Sqrt(dSquared);
		if (!(toLength > 1.0e-3f) || toLength > maxUnits || (best != nullptr && dSquared >= bestSquared)) {
			continue;
		}
		// The angle from the ray to the bound's edge: the cone is passed when
		// the ray runs within the bound's half-angle of its centre.
		float cosine = (to.x * direction.x + to.y * direction.y + to.z * direction.z) / (toLength * dirLength);
		cosine = cosine > 1.0f ? 1.0f : (cosine < -1.0f ? -1.0f : cosine);
		const float angle = math::Atan2(math::Sqrt(1.0f - cosine * cosine), cosine);
		const float half = toLength > bound.radius ? math::Asin(bound.radius / toLength) : 3.14159f;
		const float miss = angle > half ? angle - half : 0.0f;
		const float coneAngle = math::Atan2(math::Sqrt(1.0f - coneCos * coneCos), coneCos);
		if (miss > coneAngle) {
			continue;
		}
		best = actor;
		bestSquared = dSquared;
		if (centreOut != nullptr) {
			*centreOut = bound.center;
		}
	}
	return best;
}

}  // namespace obvr::game

namespace obvr::game {
namespace {

// The player's combat line (PlayerCharacter's vtable +0x308, 0x006608A0,
// thiscall(player, TESObjectREFR* target, UInt32 combat topic index, bool
// interrupt), ret 0Ch; nothing while sneaking). Vanilla says the PowerAttack
// topic (index 10) as a power attack starts, when a roll under
// fCombatSpeakPowerAttackChance (the setting's value at 0x00B36F40) lands
// (0x0065EF10). Read 2026-09-29.
constexpr UInt32 kPlayerSayCombat = 0x006608A0;
constexpr UInt8 kPlayerSayCombatBytes[] = {0x51, 0x56, 0x8B, 0xF1, 0xE8};
constexpr UInt32 kPowerAttackSpeakChance = 0x00B36F40;
constexpr UInt32 kCombatTopicPowerAttack = 10;
int g_sayVerified = -1;
UInt32 g_gruntLines = 6;

}  // namespace

bool PlayPowerAttackGrunt() {
	if (g_sayVerified < 0) {
		g_sayVerified = mem::Verify(kPlayerSayCombat, kPlayerSayCombatBytes, sizeof(kPlayerSayCombatBytes)) ? 1 : 0;
		if (g_sayVerified == 0) {
			OBVR_LOG("Hands: the player's combat line at %08X is not the bytes read - no grunts", kPlayerSayCombat);
		}
	}
	UInt8* const player = PlayerOrNull();
	if (g_sayVerified != 1 || player == nullptr) {
		return false;
	}
	float chance = *reinterpret_cast<const float*>(kPowerAttackSpeakChance);
	if (!(chance >= 0.0f && chance <= 1.0f)) {
		chance = 1.0f;
	}
	const float roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const bool said = roll <= chance;
	if (said) {
		using SayFn = void(__thiscall*)(void* player, void* target, UInt32 topic, bool interrupt);
		reinterpret_cast<SayFn>(kPlayerSayCombat)(player, player, kCombatTopicPowerAttack, false);
	}
	if (g_gruntLines > 0) {
		--g_gruntLines;
		OBVR_LOG("Hands: a power attack's grunt %s (the game's chance %.2f)", said ? "said" : "skipped by the roll",
		         static_cast<double>(chance));
	}
	return said;
}

}  // namespace obvr::game

namespace obvr::game {

bool PlayerWearsShield() {
	UInt8* const player = PlayerOrNull();
	auto* const process =
		player != nullptr ? *reinterpret_cast<UInt8* const*>(player + addr::kMobileProcessOffset) : nullptr;
	const UInt32 getter = LooksLikeObject(process) ? VirtualAt(process, addr::kProcessVtableEquippedShieldOffset) : 0;
	static int reported = -1;
	if (getter != addr::kEquippedShieldGetter) {
		// Not the table read: the raised hand keeps blocking as before.
		if (reported != 2) {
			reported = 2;
			OBVR_LOG("Hands: the shield getter could not be read (%08X) - the raised left hand blocks as before",
			         getter);
		}
		return true;
	}
	const void* const entry = reinterpret_cast<ThisPtrArgFn>(getter)(process, nullptr, 1);
	const bool worn = entry != nullptr;
	if ((worn ? 1 : 0) != reported) {
		reported = worn ? 1 : 0;
		OBVR_LOG("Hands: %s - the raised left hand %s", worn ? "a shield worn" : "no shield worn",
		         worn ? "blocks" : "does not block");
	}
	return worn;
}

}  // namespace obvr::game

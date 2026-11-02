#include "game/KnockdownRoll.h"

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/ImpactLogic.h"

namespace obvr::game {
namespace {

// The settings 0x005475D0 and its helpers read: a GameSetting is its value
// at +0 and its name at +4 (the constructors at 0x005E88B0..0x005E89A0 and
// 0x005EA7F0 pass both; read 2026-11-02, OBNG knowledge/hit-reactions.md).
struct GmstSlot {
	UInt32 address;
	const char* name;
};
constexpr GmstSlot kAgilBase{0x00B36F80, "fKnockdownAgilBase"};
constexpr GmstSlot kAgilMult{0x00B36F88, "fKnockdownAgilMult"};
constexpr GmstSlot kDamageBase{0x00B36F90, "fKnockdownDamageBase"};
constexpr GmstSlot kDamageMult{0x00B36F98, "fKnockdownDamageMult"};
constexpr GmstSlot kChance{0x00B36FA0, "fKnockdownChance"};
constexpr GmstSlot kLuckBase{0x00B37388, "iActorLuckSkillBase"};
constexpr GmstSlot kLuckMult{0x00B37390, "fActorLuckSkillMult"};
constexpr GmstSlot kFatigueBase{0x00B37530, "fFatigueBase"};
constexpr GmstSlot kFatigueMult{0x00B37538, "fFatigueMult"};

// Actor values (xOBSE obse_command_doc: Strength 0, Agility 3, Luck 7,
// Fatigue 10, HandToHand 17), read as the enemy bars read health: current
// through vtable +0x288 (float), base through 0x005F1910.
constexpr UInt32 kAvStrength = 0;
constexpr UInt32 kAvAgility = 3;
constexpr UInt32 kAvLuck = 7;
constexpr UInt32 kAvFatigue = 10;
constexpr UInt32 kAvHandToHand = 17;
constexpr UInt32 kGetActorValueFSlot = 0x288;
constexpr UInt32 kGetBaseActorValue = 0x005F1910;

bool LooksLikeObject(UInt32 a) { return mem::LooksLikeObjectAddress(a); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

bool SlotIs(const GmstSlot& s) {
	const UInt32 name = Read(s.address + 4);
	return LooksLikeObject(name) && std::strcmp(reinterpret_cast<const char*>(name), s.name) == 0;
}

// The game's settings now, or vanilla's when a slot is not the one read.
bool ReadGmst(KnockdownGmst& g) {
	const GmstSlot floats[] = {kAgilBase, kAgilMult, kDamageBase, kDamageMult, kChance, kLuckMult, kFatigueBase, kFatigueMult};
	for (const GmstSlot& s : floats) {
		if (!SlotIs(s)) {
			return false;
		}
	}
	if (!SlotIs(kLuckBase)) {
		return false;
	}
	auto f = [](const GmstSlot& s) { return *reinterpret_cast<const float*>(s.address); };
	g.agilBase = f(kAgilBase);
	g.agilMult = f(kAgilMult);
	g.damageBase = f(kDamageBase);
	g.damageMult = f(kDamageMult);
	g.chance = f(kChance);
	g.luckBase = *reinterpret_cast<const SInt32*>(kLuckBase.address);
	g.luckMult = f(kLuckMult);
	g.fatigueBase = f(kFatigueBase);
	g.fatigueMult = f(kFatigueMult);
	return true;
}

bool ReadValue(UInt32 actor, UInt32 av, float& now) {
	if (!LooksLikeObject(actor) || !LooksLikeObject(Read(actor + addr::kRefBaseFormOffset))) {
		return false;
	}
	const UInt32 vtable = Read(actor);
	const UInt32 slot = LooksLikeObject(vtable) ? Read(vtable + kGetActorValueFSlot) : 0;
	if (!LooksLikeObject(slot)) {
		return false;
	}
	using GetFloatFn = float(__thiscall*)(void* actor, UInt32 av);
	now = reinterpret_cast<GetFloatFn>(slot)(reinterpret_cast<void*>(actor), av);
	return now == now;
}

float BaseValue(UInt32 actor, UInt32 av) {
	using GetBaseFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	return static_cast<float>(reinterpret_cast<GetBaseFn>(kGetBaseActorValue)(reinterpret_cast<void*>(actor), av));
}

// A number 0..1, as the engine's `rand() % 100 / 100` is: a linear
// congruential draw, enough for a fight.
float Draw() {
	static UInt32 s_seed = 0x6C8E9CF5u;
	s_seed = s_seed * 1664525u + 1013904223u;
	return static_cast<float>((s_seed >> 8) % 100u) / 100.0f;
}

}  // namespace

ShoveKind RollKnockdown(void* actor, ShoveKind kind, bool vanilla) {
	if (kind != ShoveKind::Hard || !vanilla) {
		return kind;
	}
	static UInt32 s_lines = 24;
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	const UInt32 player = Read(addr::kPlayerPointer);
	KnockdownGmst g;
	const bool gameSettings = ReadGmst(g);
	if (!gameSettings) {
		g = KnockdownGmst{};
	}
	float agility = 0.0f;
	float luck = 0.0f;
	float fatigue = 0.0f;
	float strength = 0.0f;
	float handToHand = 0.0f;
	if (!ReadValue(a, kAvAgility, agility) || !ReadValue(a, kAvLuck, luck) || !ReadValue(a, kAvFatigue, fatigue) ||
	    !ReadValue(player, kAvStrength, strength) || !ReadValue(player, kAvHandToHand, handToHand)) {
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Impact: %08X's values unread - the knockdown stands, unrolled", a);
		}
		return kind;
	}
	const float fatigueBase = BaseValue(a, kAvFatigue);
	const float ratio = fatigueBase > 0.0f ? fatigue / fatigueBase : 1.0f;
	const float damage = FistDamage(strength, handToHand, true);
	const float chance = VanillaKnockdownChance(g, agility, luck, ratio, damage);
	const float roll = Draw();
	const bool stands = KnockdownStands(true, chance, roll);
	if (s_lines > 0) {
		--s_lines;
		OBVR_LOG("Impact: the knockdown on %08X rolled %.2f against vanilla's %.3f (%s; agility %.0f, luck %.0f, "
		         "fatigue %.0f%%, the fist's power damage %.1f from strength %.0f and hand to hand %.0f) - %s",
		         a, static_cast<double>(roll), static_cast<double>(chance),
		         gameSettings ? "the game's settings" : "the settings unread - vanilla's values",
		         static_cast<double>(agility), static_cast<double>(luck), static_cast<double>(ratio * 100.0f),
		         static_cast<double>(damage), static_cast<double>(strength), static_cast<double>(handToHand),
		         stands ? "down they go" : "a stagger instead");
	}
	return stands ? ShoveKind::Hard : ShoveKind::Light;
}

}  // namespace obvr::game

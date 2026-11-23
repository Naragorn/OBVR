#include "game/SpellStrength.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kApplyEffectiveness = 0x0068E170;
constexpr UInt32 kSiteEffectiveness = 0x0069B466;
constexpr UInt32 kQueueProjectile = 0x00673A90;
constexpr UInt32 kSiteQueued = 0x0069F6A7;
constexpr UInt32 kApplyAtTarget = 0x0069BA90;
constexpr UInt32 kSitesHit[4] = {0x00695617, 0x0069730A, 0x0069C5E9, 0x0069CD4A};
constexpr UInt32 kActorValueMagicka = 9;
constexpr UInt32 kActorChangeValueSlot = 0x2A4;

constexpr UInt32 kBolts = 8;

struct Bolt {
	const void* projectile = nullptr;
	float factor = 1.0f;
};

struct Strength {
	float armed = 1.0f;  // the factor while a charged release is made; 1 otherwise
	Bolt bolts[kBolts];
	UInt32 nextBolt = 0;
	UInt32 lines = 24;
};

Strength& State() {
	static Strength s;
	return s;
}

bool Budget() {
	Strength& s = State();
	if (s.lines == 0) {
		return false;
	}
	--s.lines;
	return true;
}

// The call at `site` to `target` taken to `hook`: only when it is a direct call to it as read.
bool TakeCall(UInt32 site, UInt32 target, UInt32 hook) {
	const UInt8* const at = reinterpret_cast<const UInt8*>(site);
	if (at[0] != 0xE8 || site + 5 + *reinterpret_cast<const UInt32*>(at + 1) != target) {
		return false;
	}
	const UInt32 to = hook - (site + 5);
	return mem::SafeWrite(site + 1, &to, sizeof(to));
}

}  // namespace

void __fastcall OBVR_ChargedEffectiveness(void* effect, void* edx, float effectiveness) {
	const float factor = State().armed;
	const float* const values = reinterpret_cast<const float*>(static_cast<const UInt8*>(effect) + 0x18);
	const float magnitudeBefore = values[0];
	using ApplyFn = void(__fastcall*)(void*, void*, float);
	reinterpret_cast<ApplyFn>(kApplyEffectiveness)(effect, edx, effectiveness * factor);
	if (factor != 1.0f && Budget()) {
		OBVR_LOG("Spell strength: an effect of the charged spell - the caster's effectiveness %.2f times %.2f; "
		         "magnitude %.1f made %.1f, duration %.1f",
		         static_cast<double>(effectiveness), static_cast<double>(factor), static_cast<double>(magnitudeBefore),
		         static_cast<double>(values[0]), static_cast<double>(values[1]));
	}
}

UInt32 __fastcall OBVR_ChargedBoltQueued(void* manager, void* edx, void* projectile, UInt32 a, UInt32 b, UInt32 c,
                                         UInt32 d) {
	Strength& s = State();
	for (Bolt& bolt : s.bolts) {
		if (bolt.projectile == projectile) {
			bolt = Bolt{};  // the address of a bolt gone, used again
		}
	}
	if (s.armed != 1.0f && projectile != nullptr) {
		s.bolts[s.nextBolt] = Bolt{projectile, s.armed};
		s.nextBolt = (s.nextBolt + 1) % kBolts;
		if (Budget()) {
			OBVR_LOG("Spell strength: the charged spell's bolt %08X noted at %.2f", reinterpret_cast<UInt32>(projectile),
			         static_cast<double>(s.armed));
		}
	}
	using QueueFn = UInt32(__fastcall*)(void*, void*, void*, UInt32, UInt32, UInt32, UInt32);
	return reinterpret_cast<QueueFn>(kQueueProjectile)(manager, edx, projectile, a, b, c, d);
}

void __fastcall OBVR_ChargedBoltApplies(void* caster, void* edx, UInt32 item, UInt32 target, UInt32 x, UInt32 y,
                                        UInt32 z, void* projectile, UInt32 w6, UInt32 w7, float multiplier,
                                        UInt32 w9) {
	float factor = 1.0f;
	for (const Bolt& bolt : State().bolts) {
		if (bolt.projectile == projectile && projectile != nullptr) {
			factor = bolt.factor;
		}
	}
	if (factor != 1.0f && Budget()) {
		OBVR_LOG("Spell strength: the charged bolt %08X reached its target %08X - its effectiveness multiplier %.2f "
		         "times %.2f",
		         reinterpret_cast<UInt32>(projectile), target, static_cast<double>(multiplier),
		         static_cast<double>(factor));
	}
	using ApplyFn = void(__fastcall*)(void*, void*, UInt32, UInt32, UInt32, UInt32, UInt32, void*, UInt32, UInt32,
	                                  float, UInt32);
	reinterpret_cast<ApplyFn>(kApplyAtTarget)(caster, edx, item, target, x, y, z, projectile, w6, w7,
	                                          multiplier * factor, w9);
}

void InstallSpellStrength() {
	const bool effectiveness = TakeCall(kSiteEffectiveness, kApplyEffectiveness, reinterpret_cast<UInt32>(&OBVR_ChargedEffectiveness));
	const bool queued = TakeCall(kSiteQueued, kQueueProjectile, reinterpret_cast<UInt32>(&OBVR_ChargedBoltQueued));
	UInt32 hits = 0;
	for (const UInt32 site : kSitesHit) {
		hits += TakeCall(site, kApplyAtTarget, reinterpret_cast<UInt32>(&OBVR_ChargedBoltApplies)) ? 1 : 0;
	}
	OBVR_LOG("Spell strength: the release's effectiveness %s, the bolts' queue %s, %u of 4 bolt hits taken",
	         effectiveness ? "taken" : "NOT taken (not as read)", queued ? "taken" : "NOT taken (not as read)", hits);
}

void ArmChargedRelease(float factor) { State().armed = factor > 1.0f ? factor : 1.0f; }

void DisarmChargedRelease() { State().armed = 1.0f; }

float PlayerSpellCost(const void* item) {
	UInt8* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (item == nullptr || player == nullptr) {
		return -1.0f;
	}
	const UInt8* const list = static_cast<const UInt8*>(item) + 0x0C;
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(list);
	if (!addr::IsGameVtable(vtable)) {
		return -1.0f;
	}
	using CostFn = float(__fastcall*)(const void* self, void* edx, void* actor);
	return reinterpret_cast<CostFn>(*reinterpret_cast<const UInt32*>(vtable))(list, nullptr, player);
}

void TakePlayerMagicka(float amount) {
	UInt8* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (player == nullptr || !(amount > 0.0f)) {
		return;
	}
	const UInt32 slot = (*reinterpret_cast<const UInt32* const*>(player))[kActorChangeValueSlot / 4];
	using ChangeFn = void(__fastcall*)(void* actor, void* edx, UInt32 av, float change, UInt32 source);
	reinterpret_cast<ChangeFn>(slot)(player, nullptr, kActorValueMagicka, -amount, 0);
}

}  // namespace obvr::game

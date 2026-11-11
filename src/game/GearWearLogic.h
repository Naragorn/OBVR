#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BodyPart.h"
#include "game/NiMath.h"

namespace obvr::game {

// Armor and weapon wear (docs/break-armor-spec.md). The decisions, over plain
// values; the engine side is game/GearWear.cpp.
//
// What the engine does, read 2026-11-11 in Oblivion.exe 1.2.0.416:
// - Every hit in the attack handling (0x005FEBF0) wears the attacker's
//   weapon by its base damage times fDamageToWeaponPercentage (0x00547240,
//   the setting's value at 0x00B36E10) and one armor piece of the struck one
//   by fDamageToArmorPercentage (0x00547260, value 0x00B36E18). The piece is
//   a random worn one, weighted by slot (0x005E5A00, called at 0x005FFC1B).
// - The wear lands in the actor's item-damage function (vtable +0x2C4 ->
//   0x005F3870; the player's 0x0065FF10 calls it): the item's health less
//   the amount, under 1 it is 0. At 0 a drawn weapon or a shield is dropped
//   into the world by an NPC (vtable +0x2C8, a lootable item) and unequipped
//   by the player (0x005F2E70); any other armor stays worn with no rating.

// Oblivion's biped slots (TESBipedModelForm's order, as xOBSE names them).
inline constexpr SInt32 kSlotNone = -1;
inline constexpr SInt32 kSlotHead = 0;
inline constexpr SInt32 kSlotUpperBody = 2;
inline constexpr SInt32 kSlotLowerBody = 3;
inline constexpr SInt32 kSlotHand = 4;
inline constexpr SInt32 kSlotFoot = 5;

// The slots a struck part wears, the first that holds a piece: a blow on an
// arm wears the gauntlets, else the cuirass over it; on a leg the greaves,
// else the boots; the neck the helmet, else the cuirass.
struct WearSlots {
	SInt32 first = kSlotNone;
	SInt32 second = kSlotNone;
};

inline WearSlots ArmorSlotsForPart(BodyPart part) {
	switch (part) {
	case BodyPart::Head: return {kSlotHead, kSlotNone};
	case BodyPart::Neck: return {kSlotHead, kSlotUpperBody};
	case BodyPart::LeftArm:
	case BodyPart::RightArm: return {kSlotHand, kSlotUpperBody};
	case BodyPart::LeftLeg:
	case BodyPart::RightLeg: return {kSlotLowerBody, kSlotFoot};
	case BodyPart::Torso:
	case BodyPart::Column:
	default: return {kSlotUpperBody, kSlotNone};
	}
}

// What the player's own piece does at 0 health ([Hands] PlayerGearBreaksOff).
enum class GearBreakMode : UInt8 {
	Vanilla = 0,    // as the engine does it: armor stays worn, a drawn weapon or shield is unequipped
	Knock = 1,      // knocked off and gone, as an enemy's
	Inventory = 2,  // unequipped into the inventory, broken and repairable (the default)
};

inline GearBreakMode GearBreakModeOf(UInt32 value) {
	if (value == 0) {
		return GearBreakMode::Vanilla;
	}
	return value == 1 ? GearBreakMode::Knock : GearBreakMode::Inventory;
}

struct GearWearSettings {
	float armorWearMult = 1.0f;   // 1 the engine's own wear, up to 10
	float weaponWearMult = 1.0f;  // the same for weapons
	UInt32 playerGearBreaksOff = 2;
	float knockFlightSeconds = 2.5f;  // a knocked-off piece flies this long, then is gone
	float helmetBlowShare = 0.25f;    // a hard blunt blow takes a helmet under this share of its health off
};

inline constexpr float kWearMultMin = 1.0f;
inline constexpr float kWearMultMax = 10.0f;

inline float ClampWearMult(float m) {
	if (!(m == m) || m < kWearMultMin) {
		return kWearMultMin;
	}
	return m > kWearMultMax ? kWearMultMax : m;
}

// The engine's wear settings scaled: the base is what the setting held when
// it was last not our own write - the game's data, a mod, a console's
// setgs - so a changed multiplier or a changed base is taken at the next
// frame and the scale never compounds.
struct GmstScale {
	bool have = false;
	float base = 0.0f;
	float written = 0.0f;
};

inline float StepGmstScale(GmstScale& s, float current, float mult) {
	if (!s.have || current != s.written) {
		s.have = true;
		s.base = current;
	}
	s.written = s.base * ClampWearMult(mult);
	return s.written;
}

// What happens to a piece the wear brought to 0.
enum class ZeroAction : UInt8 {
	Leave,         // nothing more
	KnockDropped,  // the engine dropped it into the world: sent flying, then gone
	DropAndKnock,  // dropped by us, sent flying, then gone
	ToInventory,   // unequipped, kept in the inventory
};

// `stillWorn`: the piece is still equipped; `inInventory`: still carried
// (worn or not); `engineDropped`: the engine made a world item of it.
inline ZeroAction ZeroHealthAction(bool isPlayer, GearBreakMode mode, bool stillWorn, bool inInventory,
                                   bool engineDropped) {
	if (!isPlayer) {
		if (engineDropped) {
			return ZeroAction::KnockDropped;
		}
		return stillWorn ? ZeroAction::DropAndKnock : ZeroAction::Leave;
	}
	switch (mode) {
	case GearBreakMode::Knock: return inInventory ? ZeroAction::DropAndKnock : ZeroAction::Leave;
	case GearBreakMode::Inventory: return stillWorn ? ZeroAction::ToInventory : ZeroAction::Leave;
	case GearBreakMode::Vanilla:
	default: return ZeroAction::Leave;
	}
}

// A hard blunt blow on the head takes a damaged helmet off before 0.
inline bool HelmetKnockedByBlow(bool blunt, bool power, float healthShare, float threshold) {
	return blunt && power && healthShare > 0.0f && healthShare < threshold;
}

// How a knocked-off piece leaves: along the blow, across the ground, and up.
inline NiPoint3 KnockOffVelocity(const NiPoint3& blow, float unitsPerSecond) {
	const float across = math::Sqrt(blow.x * blow.x + blow.y * blow.y);
	if (!(across > 0.001f)) {
		return NiPoint3{0.0f, 0.0f, unitsPerSecond};
	}
	return NiPoint3{blow.x / across * unitsPerSecond, blow.y / across * unitsPerSecond, unitsPerSecond * 0.5f};
}

// A knocked-off piece's way out, each frame: sent once its body is there,
// disabled when the flight is over (its body or not), then deleted the frame
// after - never left lying as an item.
struct KnockedPiece {
	float age = 0.0f;
	bool sent = false;
	bool disabled = false;
};

struct KnockStep {
	bool send = false;
	bool disable = false;
	bool remove = false;
};

inline KnockStep StepKnockedPiece(KnockedPiece& k, float dt, bool haveBody, float flightSeconds) {
	KnockStep out;
	if (k.disabled) {
		out.remove = true;
		return out;
	}
	k.age += dt > 0.0f ? dt : 0.0f;
	if (!k.sent && haveBody) {
		k.sent = true;
		out.send = true;
	}
	if (k.age >= flightSeconds) {
		k.disabled = true;
		out.disable = true;
	}
	return out;
}

}  // namespace obvr::game

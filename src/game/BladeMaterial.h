#pragma once

// What a blade meets on a body (docs/blade-penetration-spec.md 3): the
// outermost layer where it touches - the skin, clothing, light or heavy
// armour - read per shape from the worn item it belongs to, carried per
// vertex into the fit and per bin into the capsules' radial fields
// (game/RadialField.h, game/RadialFieldBuild.h).
//
// Read 2026-11-08:
// - a worn item's 3D hangs under a node named for its slot and its form ID
//   in brackets, "UpperBody (0001C6D1)" (game/NodeNameList.h
//   IsWornItemNode); a shape under none is the body's or the head's own;
// - armour is form type 0x14, clothing 0x16 (xOBSE GameForms.h, DeepWiki
//   llde/xOBSE); both carry a TESBipedModelForm (the engine's dynamic cast,
//   kRttiTESBipedModelForm) whose flags byte at +0x06 holds heavy armour in
//   bit 7; armour's rating is a UInt16 at TESObjectARMO+0xE4. In our EXE
//   checked by the harness log of known armour (the spec, step 1).
//
// Pure; blade_material_test.

#include "core/Types.h"

namespace obvr::game {

enum class BodyMaterial : UInt8 {
	Unknown = 0,  // nothing read (a guessed capsule, a field without classes)
	Skin = 1,     // the body's or head's own shape, a creature's
	Cloth = 2,    // clothing
	Light = 3,    // light armour (leather, fur, chain, elven, glass ...)
	Heavy = 4,    // heavy armour (iron, steel, dwarven, ebony, daedric ...)
};
inline constexpr UInt32 kBodyMaterialCount = 5;

inline const char* BodyMaterialName(BodyMaterial m) {
	switch (m) {
	case BodyMaterial::Skin:
		return "skin";
	case BodyMaterial::Cloth:
		return "cloth";
	case BodyMaterial::Light:
		return "light armour";
	case BodyMaterial::Heavy:
		return "heavy armour";
	default:
		return "unknown";
	}
}

inline constexpr UInt8 kWornTypeArmor = 0x14;
inline constexpr UInt8 kWornTypeClothing = 0x16;
inline constexpr UInt32 kBipedFlagsOffset = 0x06;  // in TESBipedModelForm
inline constexpr UInt8 kBipedHeavyArmorBit = 0x80;
inline constexpr UInt32 kArmorRatingOffset = 0xE4;  // in TESObjectARMO

// The material of a shape by the worn item it hangs under: none (`wornId`
// 0) the body's own skin; armour light or heavy by its flag; clothing; any
// other worn form - read as nothing known.
inline BodyMaterial MaterialOfWorn(UInt32 wornId, bool formRead, UInt8 formType, bool heavyFlag) {
	if (wornId == 0) {
		return BodyMaterial::Skin;
	}
	if (!formRead) {
		return BodyMaterial::Unknown;
	}
	if (formType == kWornTypeArmor) {
		return heavyFlag ? BodyMaterial::Heavy : BodyMaterial::Light;
	}
	if (formType == kWornTypeClothing) {
		return BodyMaterial::Cloth;
	}
	return BodyMaterial::Unknown;
}

// The armour's rating as the gate reads it, in whole points held to a byte:
// the engine keeps it a hundred times over (xOBSE GetArmorAR divides by 100
// - to be checked against the harness log, the spec step 1).
inline constexpr UInt32 kArmorRatingScale = 100;
inline UInt8 ArmorPointsOf(UInt16 raw) {
	const UInt32 points = static_cast<UInt32>(raw) / kArmorRatingScale;
	return static_cast<UInt8>(points > 255u ? 255u : points);
}

// The form ID in a worn item node's name, "<slot> (<eight hex digits>)":
// 0 for any other name.
inline UInt32 WornItemIdOf(const char* name) {
	if (name == nullptr) {
		return 0;
	}
	const char* p = name;
	while (*p != '\0' && *p != '(') {
		++p;
	}
	if (*p != '(' || p == name || p[-1] != ' ') {
		return 0;
	}
	++p;
	UInt32 id = 0;
	UInt32 digits = 0;
	for (; digits < 8; ++digits, ++p) {
		const char c = *p;
		UInt32 v = 0;
		if (c >= '0' && c <= '9') {
			v = static_cast<UInt32>(c - '0');
		} else if (c >= 'A' && c <= 'F') {
			v = static_cast<UInt32>(c - 'A' + 10);
		} else if (c >= 'a' && c <= 'f') {
			v = static_cast<UInt32>(c - 'a' + 10);
		} else {
			return 0;
		}
		id = (id << 4) | v;
	}
	return *p == ')' && p[1] == '\0' ? id : 0;
}

// The material most of a capsule's vertices have, for its bins the mesh gave
// no vertex: counts per material, the largest; Unknown with none.
inline BodyMaterial MostCommonMaterial(const UInt32 (&counts)[kBodyMaterialCount]) {
	UInt32 best = 0;
	for (UInt32 m = 1; m < kBodyMaterialCount; ++m) {
		if (counts[m] > counts[best]) {
			best = m;
		}
	}
	return counts[best] > 0 ? static_cast<BodyMaterial>(best) : BodyMaterial::Unknown;
}

}  // namespace obvr::game

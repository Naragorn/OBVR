#pragma once

// Model B of free-weapons-spec decision 5, at the hips: what a hip place keeps
// (game/HolsterSlots.h) and is not in use hangs there on the body (Nadi,
// 2026-11-20: "go 3. Sichtbar gefüllte Holster"; 2026-10-27: the hips only -
// the back is not seen in first person). A one-handed weapon hangs as the game
// hangs its sheathed weapon; a shield, a torch (unlit) and a potion kept there
// hang at the same spot in a pose of their own (KeptHipLocalFor; "go alle",
// 2026-11-20). The equipped weapon is the game's own: it hangs on the body's
// "SideWeapon" node, at the left hip or mirrored to the right one
// (vr/SheathMirror.h); a worn shield is on the arm, a worn torch in the hand.
// What a hip keeps and is not in use is in the pack, so the game shows nothing
// of it; OBVR hangs a copy of its model there (game/KeptOnBody.cpp) and takes
// it away when the place, the slot or the pack change.
//
// What a hip shows (KeptShownAt):
//   * nothing with [Hands] ShowKeptWeapons off, outside Full VR or with the
//     places off;
//   * nothing for an empty place (a potion the place keeps beside no item -
//     vr/PotionBelt.h - is shown), or one keeping a two-hander or a bow (they
//     do not go at a hip);
//   * nothing for the equipped weapon, a worn shield or torch: the game shows
//     those itself;
//   * nothing when the pack no longer holds it (sold, given away: the place
//     forgets it only when a reach fails);
//   * nothing where the game's own sheath hangs: an equipped one-hander kept
//     at no place hangs at the game's hip, and its scabbard stays there while
//     it is drawn - two weapons in one spot.
//
// Where it hangs (KeptHipLocal): under the sheath node's parent (Bip01
// Pelvis), at the game's sheath node's own transform for the left hip and its
// mirror for the right (MirroredAcrossLocalZ, as the body mirrors the game's
// node). The node is read as it stands; the body may have mirrored it already
// this frame (StepBodySheaths), so which side it is on is read too.
//
// Pure, covered by kept_on_body_test.

#include "game/BodyIkLogic.h"
#include "vr/Holster.h"

namespace obvr::game {

inline constexpr UInt32 kKeptHipCount = 2;  // 0 the left hip, 1 the right

inline vr::HolsterSlot KeptHipSlot(UInt32 hip) { return hip == 0 ? vr::HolsterSlot::HipLeft : vr::HolsterSlot::HipRight; }

enum class KeptShown : UInt8 { Shown, Off, Empty, NotForAHip, Equipped, NotInPack, GameSheathThere };

// What a hip shows, for its model and its pose.
enum class KeptKind : UInt8 { Nothing, Weapon, Shield, Torch, Potion };

inline const char* KeptShownName(KeptShown k) {
	switch (k) {
	case KeptShown::Shown:
		return "shown";
	case KeptShown::Off:
		return "off";
	case KeptShown::Empty:
		return "the place keeps nothing";
	case KeptShown::NotForAHip:
		return "nothing that goes at a hip";
	case KeptShown::Equipped:
		return "in use, the game's own";
	case KeptShown::NotInPack:
		return "no longer in the pack";
	case KeptShown::GameSheathThere:
		return "the game's sheath hangs there";
	}
	return "?";
}

struct KeptOnBodyInput {
	bool enabled = false;                                   // ShowKeptWeapons, Full VR, the places on
	UInt32 kept[vr::kHolsterSlotCount] = {};                // each place's base form id, 0 none
	vr::SlotItem item[vr::kHolsterSlotCount] = {};          // what that form is
	bool inPack[vr::kHolsterSlotCount] = {};                // at least one of it in the pack
	bool worn[vr::kHolsterSlotCount] = {};                  // a shield or torch kept there is worn
	UInt32 potion[vr::kHolsterSlotCount] = {};              // the potion each place keeps (vr/PotionBelt.h)
	bool potionInPack[vr::kHolsterSlotCount] = {};          // at least one of it in the pack
	UInt32 equippedWeapon = 0;                              // the weapon slot's base form id, 0 none
	vr::HolsterSlot gameSheathHip = vr::HolsterSlot::None;  // where the game's own sheath hangs (GameSheathHip)
};

// The hip the game's sheath node hangs at: a one-hander equipped, the left
// hip or, mirrored by the body (vr::SheathMirror::side), the right; None for
// any other weapon or none. None too while the sheath is hidden (the one-hander
// is kept at a shoulder, vr::HideHipSheath): the hip's own copy may show then.
inline vr::HolsterSlot GameSheathHip(vr::EquippedKind equipped, bool sideMirrored, bool hidden = false) {
	if (equipped != vr::EquippedKind::OneHand || hidden) {
		return vr::HolsterSlot::None;
	}
	return sideMirrored ? vr::HolsterSlot::HipRight : vr::HolsterSlot::HipLeft;
}

inline KeptKind KeptKindOf(vr::SlotItem item) {
	switch (item) {
	case vr::SlotItem::OneHand:
		return KeptKind::Weapon;
	case vr::SlotItem::Shield:
		return KeptKind::Shield;
	case vr::SlotItem::Torch:
		return KeptKind::Torch;
	default:
		return KeptKind::Nothing;
	}
}

inline KeptShown KeptShownAt(const KeptOnBodyInput& in, vr::HolsterSlot slot) {
	const UInt32 i = static_cast<UInt32>(slot);
	if (!in.enabled) {
		return KeptShown::Off;
	}
	if (!vr::SlotIsHip(slot) || (in.kept[i] == 0 && in.potion[i] == 0)) {
		return KeptShown::Empty;
	}
	const bool potion = in.kept[i] == 0;
	if (!potion && KeptKindOf(in.item[i]) == KeptKind::Nothing) {
		return KeptShown::NotForAHip;
	}
	if (!potion && (in.item[i] == vr::SlotItem::OneHand ? in.kept[i] == in.equippedWeapon : in.worn[i])) {
		return KeptShown::Equipped;
	}
	if (!(potion ? in.potionInPack[i] : in.inPack[i])) {
		return KeptShown::NotInPack;
	}
	if (in.gameSheathHip == slot) {
		return KeptShown::GameSheathThere;
	}
	return KeptShown::Shown;
}

// The base form each hip shows, 0 nothing.
struct KeptOnBody {
	UInt32 form[kKeptHipCount] = {};
	KeptKind kind[kKeptHipCount] = {KeptKind::Nothing, KeptKind::Nothing};
};

inline KeptOnBody KeptOnBodyFor(const KeptOnBodyInput& in) {
	KeptOnBody out;
	for (UInt32 hip = 0; hip < kKeptHipCount; ++hip) {
		const vr::HolsterSlot slot = KeptHipSlot(hip);
		const UInt32 i = static_cast<UInt32>(slot);
		if (KeptShownAt(in, slot) == KeptShown::Shown) {
			const bool potion = in.kept[i] == 0;
			out.form[hip] = potion ? in.potion[i] : in.kept[i];
			out.kind[hip] = potion ? KeptKind::Potion : KeptKindOf(in.item[i]);
		}
	}
	return out;
}

// The local transform under the sheath node's parent a hip's kept weapon hangs
// at. `sheathLocal` is the game's sheath node's local as read, `sheathRightOf`
// how far it lies to the body's right (RightOfAcross): the game hangs it at
// the left hip, so a node read on the right has been mirrored by the body and
// is turned back first.
inline NiTransform KeptHipLocal(const NiTransform& sheathLocal, float sheathRightOf, vr::HolsterSlot hip) {
	const NiTransform left = sheathRightOf > 0.0f ? MirroredAcrossLocalZ(sheathLocal) : sheathLocal;
	return hip == vr::HolsterSlot::HipRight ? MirroredAcrossLocalZ(left) : left;
}

// How far out from the sheath's spot a shield's grip and a potion's middle
// sit, units: the shield's plate clear of the thigh, the bottle beside it.
inline constexpr float kKeptShieldOutUnits = 2.0f;
inline constexpr float kKeptPotionOutUnits = 4.0f;

// The pose of each kind at the left hip, in the sheath's parent's axes (its
// local z the body's left, BodyIkLogic.h), mirrored for the right as the
// weapon is. A weapon and a torch hang as the game's sheathed weapon: both
// run along their own y from the grip (iron longsword y -12 to 68, torch02 y
// -14 to 34, read with pyffi 2026-11-20), the torch's head down as a mace's.
// A shield's plate lies across its own x and z with its face to -y (iron,
// elven, daedric, fur shields: y -14 to 7 against 42 to 70 across, read the
// same day): its -y turned outward, its x along the sword's way down - it
// hangs from its grip at the hip down the thigh, slanted as the sword. The
// sword's way down is the sheath's own y (its column 1), taken square to the
// body's left-right. A potion stands along its own z (potion01 z -8 to 8): its
// z turned to `up`, the world's up in the sheath parent's axes when the copy
// is hung (the sword's way down tilts some 45 degrees back - a potion along
// it lay on its side, kept-on-body-potion.txt 2026-11-20); `up` unusable, the
// sword's way up instead.
inline NiTransform KeptHipLocalFor(KeptKind kind, const NiTransform& sheathLocal, float sheathRightOf,
                                   vr::HolsterSlot hip, const NiPoint3& up = NiPoint3{0.0f, 0.0f, 0.0f}) {
	if (kind != KeptKind::Shield && kind != KeptKind::Potion) {
		return KeptHipLocal(sheathLocal, sheathRightOf, hip);
	}
	const NiTransform left = sheathRightOf > 0.0f ? MirroredAcrossLocalZ(sheathLocal) : sheathLocal;
	NiPoint3 down{left.rot.data[0][1], left.rot.data[1][1], 0.0f};
	const float length = math::Sqrt(down.LengthSquared());
	down = length > 1.0e-4f ? down * (1.0f / length) : NiPoint3{-1.0f, 0.0f, 0.0f};
	const bool right = hip == vr::HolsterSlot::HipRight;
	NiTransform t;
	t.scale = left.scale;
	if (kind == KeptKind::Shield) {
		// Built at the left hip and mirrored (a proper turn: the plate's -y
		// stays outward, its x along the way down).
		const NiPoint3 out{0.0f, 0.0f, 1.0f};
		const NiPoint3 x = down;
		const NiPoint3 y = out * -1.0f;
		const NiPoint3 z{x.y * y.z - x.z * y.y, x.z * y.x - x.x * y.z, x.x * y.y - x.y * y.x};  // x cross y
		t.rot.data[0][0] = x.x;
		t.rot.data[1][0] = x.y;
		t.rot.data[2][0] = x.z;
		t.rot.data[0][1] = y.x;
		t.rot.data[1][1] = y.y;
		t.rot.data[2][1] = y.z;
		t.rot.data[0][2] = z.x;
		t.rot.data[1][2] = z.y;
		t.rot.data[2][2] = z.z;
		t.pos = left.pos + out * kKeptShieldOutUnits;
		return right ? MirroredAcrossLocalZ(t) : t;
	}
	// A potion is built at its own hip: mirrored, its z would point down.
	const NiPoint3 out{0.0f, 0.0f, right ? -1.0f : 1.0f};
	NiPoint3 u{up.x, up.y, 0.0f};
	const float uLength = math::Sqrt(u.LengthSquared());
	const NiPoint3 z = uLength > 1.0e-3f ? u * (1.0f / uLength) : down * -1.0f;
	const NiPoint3 x = out;
	const NiPoint3 y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};  // z cross x
	t.rot.data[0][0] = x.x;
	t.rot.data[1][0] = x.y;
	t.rot.data[2][0] = x.z;
	t.rot.data[0][1] = y.x;
	t.rot.data[1][1] = y.y;
	t.rot.data[2][1] = y.z;
	t.rot.data[0][2] = z.x;
	t.rot.data[1][2] = z.y;
	t.rot.data[2][2] = z.z;
	t.pos = NiPoint3{left.pos.x, left.pos.y, right ? -left.pos.z : left.pos.z} + out * kKeptPotionOutUnits;
	return t;
}

}  // namespace obvr::game

#pragma once

// Model B of free-weapons-spec decision 5, at the hips: a one-handed weapon a
// hip place keeps (game/HolsterSlots.h) and that is not the equipped one hangs
// there on the body, as the game hangs its sheathed weapon (Nadi, 2026-11-20:
// "go 3. Sichtbar gefüllte Holster"; 2026-10-27: the hips only - the back is
// not seen in first person). The equipped weapon is the game's own: it hangs
// on the body's "SideWeapon" node, at the left hip or mirrored to the right
// one (vr/SheathMirror.h). A weapon kept at a hip is in the pack, not worn,
// so the game shows nothing of it; OBVR hangs a copy of its model there
// (game/KeptOnBody.cpp) and takes it away when the place, the slot or the
// pack change.
//
// What a hip shows (KeptShownAt):
//   * nothing with [Hands] ShowKeptWeapons off, outside Full VR or with the
//     places off;
//   * nothing for an empty place, or one keeping a shield or a torch (only
//     the weapons here; their look at a hip is not decided);
//   * nothing for the equipped weapon: the game hangs that one itself;
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

enum class KeptShown : UInt8 { Shown, Off, Empty, NotAOneHander, Equipped, NotInPack, GameSheathThere };

inline const char* KeptShownName(KeptShown k) {
	switch (k) {
	case KeptShown::Shown:
		return "shown";
	case KeptShown::Off:
		return "off";
	case KeptShown::Empty:
		return "the place keeps nothing";
	case KeptShown::NotAOneHander:
		return "not a one-handed weapon";
	case KeptShown::Equipped:
		return "the equipped weapon, the game's own";
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
	UInt32 equippedWeapon = 0;                              // the weapon slot's base form id, 0 none
	vr::HolsterSlot gameSheathHip = vr::HolsterSlot::None;  // where the game's own sheath hangs (GameSheathHip)
};

// The hip the game's sheath node hangs at: a one-hander equipped, the left
// hip or, mirrored by the body (vr::SheathMirror::side), the right; None for
// any other weapon or none.
inline vr::HolsterSlot GameSheathHip(vr::EquippedKind equipped, bool sideMirrored) {
	if (equipped != vr::EquippedKind::OneHand) {
		return vr::HolsterSlot::None;
	}
	return sideMirrored ? vr::HolsterSlot::HipRight : vr::HolsterSlot::HipLeft;
}

inline KeptShown KeptShownAt(const KeptOnBodyInput& in, vr::HolsterSlot slot) {
	const UInt32 i = static_cast<UInt32>(slot);
	if (!in.enabled) {
		return KeptShown::Off;
	}
	if (!vr::SlotIsHip(slot) || in.kept[i] == 0) {
		return KeptShown::Empty;
	}
	if (in.item[i] != vr::SlotItem::OneHand) {
		return KeptShown::NotAOneHander;
	}
	if (in.kept[i] == in.equippedWeapon) {
		return KeptShown::Equipped;
	}
	if (!in.inPack[i]) {
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
};

inline KeptOnBody KeptOnBodyFor(const KeptOnBodyInput& in) {
	KeptOnBody out;
	for (UInt32 hip = 0; hip < kKeptHipCount; ++hip) {
		const vr::HolsterSlot slot = KeptHipSlot(hip);
		if (KeptShownAt(in, slot) == KeptShown::Shown) {
			out.form[hip] = in.kept[static_cast<UInt32>(slot)];
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

}  // namespace obvr::game

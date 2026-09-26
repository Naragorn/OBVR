#pragma once

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Where a held object sits in the hand (docs/holding-objects-spec.md, parts
// 2 and 4).
//
// Part 2, the grab point in the palm. The engine's spring is a
// bhkMouseSpringAction (vtable 0xA3D524, built at 0x0066D84C): it pulls one
// point of the body - the point the grab's ray hit, kept in body space
// (0x0066D7D8..0x0066D828) - towards a target the update writes each frame
// (0x008B8A10, hk+0x30). So the point that was touched goes wherever the
// target is. The target was the controller's tracked origin, which sits on
// the tracking ring above the fingers; now it is the palm of the pinned hand
// bone: the bone's origin (the wrist) moved along its own x, the way a
// Bip01 hand runs along its fingers.
//
// Part 4, small objects fixed in the hand. The spring holds no rotation and
// lags behind a moving hand, so a potion hangs and swings. For small things
// - by form type and by the size of their scene bound - the object's scene
// node is placed each frame, after physics and before the draw, rigidly on
// the hand: turned with the wrist the way it was turned against the hand
// when the hold began, the touched point in the palm. The physics body
// still follows the spring to the same palm, so letting go leaves the
// object about where it was seen.

// The palm: the hand bone's origin moved `alongUnits` along its own x.
inline NiPoint3 PalmPoint(const NiMatrix33& handRot, const NiPoint3& handPos, float alongUnits) {
	return handPos + handRot * NiPoint3{alongUnits, 0.0f, 0.0f};
}

// The palm's distance from the wrist on a Bip01 hand, metres.
constexpr float kPalmAlongMetres = 0.07f;

// xOBSE GameForms.h's FormType values of what is held in one hand.
inline constexpr UInt8 kFormApparatus = 0x13;
inline constexpr UInt8 kFormBook = 0x15;  // scrolls are books
inline constexpr UInt8 kFormIngredient = 0x19;
inline constexpr UInt8 kFormMisc = 0x1B;
inline constexpr UInt8 kFormAmmo = 0x22;
inline constexpr UInt8 kFormSoulGem = 0x26;
inline constexpr UInt8 kFormKey = 0x27;
inline constexpr UInt8 kFormPotion = 0x28;
inline constexpr UInt8 kFormSigilStone = 0x2A;

// A bound radius no larger than this is small: 0.3 m.
constexpr float kSmallObjectMaxRadiusUnits = 21.0f;

inline bool IsSmallHeldObject(UInt8 formType, float boundRadiusUnits) {
	switch (formType) {
	case kFormApparatus:
	case kFormBook:
	case kFormIngredient:
	case kFormMisc:
	case kFormAmmo:
	case kFormSoulGem:
	case kFormKey:
	case kFormPotion:
	case kFormSigilStone:
		break;
	default:
		return false;
	}
	return boundRadiusUnits > 0.0f && boundRadiusUnits <= kSmallObjectMaxRadiusUnits;
}

// The object against the hand, taken when the hold begins: its rotation
// relative to the hand's, and the touched point in its own space (unscaled).
struct HeldAttachment {
	NiMatrix33 relativeRot = NiMatrix33::Identity();
	NiPoint3 pivotLocal{0.0f, 0.0f, 0.0f};
};

inline HeldAttachment CaptureAttachment(const NiMatrix33& handRot, const NiMatrix33& objectRot,
                                        const NiPoint3& objectPos, float objectScale,
                                        const NiPoint3& touchedWorld) {
	HeldAttachment a;
	a.relativeRot = InverseRotation(handRot) * objectRot;
	const float scale = objectScale > 0.0f ? objectScale : 1.0f;
	a.pivotLocal = (InverseRotation(objectRot) * (touchedWorld - objectPos)) * (1.0f / scale);
	return a;
}

// Where the object goes this frame: turned with the hand, the touched point
// on the palm.
struct HeldPose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
};

inline HeldPose AttachedPose(const NiMatrix33& handRot, const NiPoint3& palm,
                             const HeldAttachment& a, float objectScale) {
	HeldPose p;
	p.rot = handRot * a.relativeRot;
	const float scale = objectScale > 0.0f ? objectScale : 1.0f;
	p.pos = palm - p.rot * (a.pivotLocal * scale);
	return p;
}

// Whether a new hold is placed on the hand. The in-hand mode (`attachAll`)
// places everything, at the touched point or else the middle; the levitated
// mode only small things, and only with a touched point to hold them by.
// Either way the object keeps the turn it had in the world when the grip
// closed and turns with the wrist from there (CaptureAttachment): holding it
// the way the sword is held was tried and dropped - an object should stay as
// it lay when picked up (2026-09-26).
inline bool AttachesInHand(bool attachAll, bool isSmall, bool haveTouched) {
	return attachAll || (isSmall && haveTouched);
}

// What one frame of holding knows about the hand.
struct HeldHand {
	bool rightHand = true;
	float palmAlongUnits = 0.0f;
	// Where the weapon's grip is (the right hand's Weapon node), when known:
	// the held point goes there instead of the palm.
	bool haveGripPoint = false;
	NiPoint3 gripPoint{0.0f, 0.0f, 0.0f};
};

// Once per frame in the draw pass, after the hands are pinned: fixes a small
// held object in the palm of the hand that holds it. `holding` is the
// engine holding something for `rightHand`'s hand; `touched` the point the
// grab's ray hit when the hold began (valid with haveTouched).
// `attachAll` is the in-hand mode (Hands.LevitateObjects off): every held
// object sits in the hand, not only small ones.
void StepHeldObject(bool enabled, bool holding, const HeldHand& hand, bool haveTouched,
                    const NiPoint3& touched, bool attachAll = false);

}  // namespace obvr::game

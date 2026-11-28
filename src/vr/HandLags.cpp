#include "vr/HandMode.h"

#include "core/MathFns.h"
#include "vr/HeadOffset.h"
#include "vr/HeldItemWeight.h"
#include "vr/OneHandTwoHander.h"
#include "vr/WeaponWeight.h"

namespace obvr::vr {
namespace {

// A drawn pose of the room put relative to the head the way the controller's
// own is (HandMode::Update): its rotation and its offset from the eyes in
// game units, in the game's convention.
void DrawnFromHead(const HandModeFrame& f, const WeaponLagVerdict& drawn, NiMatrix33& rotation, NiPoint3& offsetUnits) {
	const Quaternion relative = (f.head.Conjugate() * drawn.orientation).Normalized();
	rotation = ToMatrix(FromOpenXR(relative));
	offsetUnits = OffsetFromPose(f.head, drawn.position, f.headPosition, 1.0f) * f.unitsPerMetre;
}

}  // namespace

// Each hand as it is drawn (vr/WeaponWeight.h, vr/HeldItemWeight.h): behind its
// controller by what it holds, lagged in the room, then put relative to the
// head the way the controller's own pose is. Neither lags while the hands are
// being adjusted: the fit measures the controller.
//
// The weapon hand: a swung weapon or a staff with a weight - the frame carries
// 0 for the fists, a bow and nothing drawn. Both hands on the handle stiffen
// it; a two-hander in one hand is heavier (vr/OneHandTwoHander.h), never a
// staff (twoHanderSwung is false for it).
//
// The off hand: the drawn bow, the worn shield or the torch (the frame's
// offHandWeight), not in a menu, with the tablet, or on a weapon's handle
// (vr::OffHandLags). Its velocity is the drawn hand's while it lags.
void HandMode::StepHandLags(const HandModeFrame& f, const HandSettings& s, HandModeResult& r) {
	const bool adjusting = s.adjustHands || f.adjustingHands;
	{
		const bool weaponInHand = WeaponHandLags(f.weaponWeight, f.right.valid && f.firstPerson, adjusting);
		const float oneHanded =
			f.twoHanderSwung && !f.bothHandsOnHandle ? PercentMore(s.oneHandTwoHander.weightPercent) : 1.0f;
		// Exhausted, the lag is made heavier (game/ExhaustionLogic.h).
		const WeaponLagTuning tuning = WeaponLagExhausted(
			WeaponLagHeavier(
				WeaponLagFor(f.weaponWeight, s.weaponWeightPercent, f.bothHandsOnHandle, s.weaponSwingThrough),
				oneHanded),
			f.exhaustionEase, f.exhaustionFactor);
		const WeaponLagVerdict drawn =
			StepWeaponLag(m_weaponLag, weaponInHand, tuning, f.right.orientation, f.right.position, f.dtSeconds);
		r.weaponLagging = drawn.lagging;
		r.weaponGapMetres = drawn.gapMetres;
		r.weaponGapDegrees = drawn.gapRadians * math::kRadiansToDegrees;
		r.weaponHandRotation = r.rightHandRotation;
		r.weaponHandOffsetUnits = r.rightHandOffsetUnits;
		if (r.rightHandValid && drawn.lagging) {
			DrawnFromHead(f, drawn, r.weaponHandRotation, r.weaponHandOffsetUnits);
		}
	}
	{
		OffHandLagGate gate;
		gate.weight = f.offHandWeight;
		gate.handValid = f.left.valid && f.firstPerson;
		gate.adjusting = adjusting;
		gate.menuMode = f.menuMode;
		gate.tabletUp = f.tablet.up;
		gate.onHandle = f.leftGripOnHandle;
		const WeaponLagTuning tuning = OffHandLagTuning(f.offHandWeight, s.weaponWeightPercent, s.weaponSwingThrough,
		                                                f.exhaustionEase, f.exhaustionFactor);
		const WeaponLagVerdict drawn =
			StepWeaponLag(m_offHandLag, OffHandLags(gate), tuning, f.left.orientation, f.left.position, f.dtSeconds);
		r.offHandLagging = drawn.lagging;
		r.offHandGapMetres = drawn.gapMetres;
		r.offHandGapDegrees = drawn.gapRadians * math::kRadiansToDegrees;
		r.offHandRotation = r.leftHandRotation;
		r.offHandOffsetUnits = r.leftHandOffsetUnits;
		r.offHandVelocity = r.leftVelocity;
		if (r.leftHandValid && drawn.lagging) {
			DrawnFromHead(f, drawn, r.offHandRotation, r.offHandOffsetUnits);
			r.offHandVelocity = OffsetFromPose(f.head, m_offHandLag.velocity, NiPoint3{}, 1.0f);
		}
	}
}

}  // namespace obvr::vr

#include "camera/hands/Swings.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/BowVisual.h"
#include "game/FatigueCost.h"
#include "game/MeleeHit.h"
#include "game/MeleeHits.h"
#include "game/PlayerTeleport.h"
#include "test/HandScriptRuntime.h"
#include "vr/Archery.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {
void StepSwingSounds(SwingsState& state, const HandStep& step, const HandShared& shared) {
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& handResult = shared.hand;
	// A swing that has become a power attack grunts once (vr::GruntDue), as
	// vanilla does when its power attack starts.
	{
		if (active && !menuIsUp && handResult.rightHandValid &&
		    vr::GruntDue(handResult.strikeByMotion, handResult.swingHeavy, handResult.swingSerial, state.lastGrunted)) {
			state.lastGrunted = handResult.swingSerial;
			game::PlayPowerAttackGrunt();
		}
	}
	// Each swing that may strike swishes once, as it starts (game::SwishDue)
	// - once it is long enough to be a swing and not a twitch
	// ([Hands] SwingMinMetres).
	{
		if (active && !menuIsUp && handResult.rightHandValid &&
		    game::SwishDue(handResult.strikeByMotion, handResult.swingActive && handResult.swingLongEnough,
		                   handResult.swingSerial, state.lastSwished)) {
			state.lastSwished = handResult.swingSerial;
			game::PlaySwingSwish();
		}
	}
	
	// The weapon's lag, measured: the largest gap of each of the first swings
	// with a lag, since the headset's feel cannot be read from here. Watched
	// for half a second past the swing as well: the swing-through comes when
	// the hand has stopped.
	{
		const bool watching = handResult.weaponLagging && (handResult.swingActive || state.gapAfter > 0);
		const bool newSwing = handResult.weaponLagging && handResult.swingActive && handResult.swingSerial != state.gapSerial;
		if (state.gapSerial != 0 && (!watching || newSwing)) {
			if (state.gapLines > 0) {
				--state.gapLines;
				OBVR_LOG("Weapon weight: swing %u - the weapon was up to %.2f m and %.0f degrees from the hand, "
				         "within half a second of it",
				         state.gapSerial, static_cast<double>(state.gapMetres), static_cast<double>(state.gapDegrees));
			}
			state.gapSerial = 0;
			state.gapAfter = 0;
		}
		if (watching) {
			if (newSwing) {
				state.gapSerial = handResult.swingSerial;
				state.gapMetres = 0.0f;
				state.gapDegrees = 0.0f;
			}
			state.gapAfter = handResult.swingActive ? 45 : state.gapAfter - 1;
			state.gapMetres = handResult.weaponGapMetres > state.gapMetres ? handResult.weaponGapMetres : state.gapMetres;
			state.gapDegrees = handResult.weaponGapDegrees > state.gapDegrees ? handResult.weaponGapDegrees : state.gapDegrees;
		}
	}
	
}

void StepStrikes(SwingsState& state, const HandStep& step, const HandShared& shared, const SwingsLinks& links) {
	const Config& config = step.config;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& handResult = shared.hand;
	
	// The strike by motion: while the right hand is swinging a drawn melee
	// weapon, the blade is tested against the bodies near the player and each
	// one it passes through is handed to the engine's hit function - once per
	// swing, heavy when the swing has been fast enough. Not in a menu, not in
	// third person (no hand pose there), and only while the hand is tracked -
	// and not with a blade that went through a wall until it is back out
	// (links.bladeThrough: the tester chose "let go" over "stuck", 2026-10-09),
	// nor a fist that did.
	if (active && handResult.strikeByMotion && handResult.swingActive && handResult.swingLongEnough &&
	    handResult.rightHandValid && !menuIsUp && !links.bladeThrough && !links.weaponHandThrough && !links.halfSwordGrip) {
		game::MotionStrike strike;
		strike.swingSerial = handResult.swingSerial;
		strike.heavy = handResult.swingHeavy;
		strike.attackGroup = handResult.swingHeavy ? vr::PowerAttackGroup(handResult.powerDirection) : vr::kAnimGroupAttackLight;
		// Struck at the touch: fast enough there, a power attack of the
		// swing's direction so far (game::StrikeAtContact).
		strike.speedNow = handResult.swingSpeedNow;
		strike.powerSpeed = config.hands.powerStrikeSpeed;
		strike.powerGroup = vr::PowerAttackGroup(handResult.powerDirection);
		strike.swingMetres = handResult.swingTravelledMetres;
		// Held with both hands, the blade is where the two hands turned it.
		// The DRAWN hand, behind the controller by the weapon's weight
		// (vr/WeaponWeight.h): the hit lands where the weapon is seen.
		strike.handRotation = handResult.weaponHandRotation;
		strike.handOffsetUnits = handResult.weaponHandOffsetUnits;
		strike.cameraValid = shared.cyclopeanCameraValid;
		strike.cameraRotation = shared.cyclopeanCamera.rot;
		strike.cameraPosition = shared.cyclopeanCamera.pos;
		strike.boundFactor = config.hands.hitBoundFactor;
		strike.padUnits = config.hands.hitPadUnits;
		// The hit by touch against the capsules the weapon's step collected
		// this frame (none with a bare fist: the hands' step's then).
		strike.byTouch = config.hands.hitsByTouch;
		strike.capsules = links.bladeBodies.count > 0 ? &links.bladeBodies : &links.handBodies;
		// The drawn blade's own span for the touch - the weapon's geometry at
		// the drawn hand, as the contact step held it.
		if (links.bladeSpan.span.valid) {
			game::BladePose drawn;
			drawn.rot = shared.cyclopeanCamera.rot * handResult.weaponHandRotation;
			drawn.pos = shared.cyclopeanCamera.pos +
			            shared.cyclopeanCamera.rot * handResult.weaponHandOffsetUnits;
			strike.touchSpanValid = true;
			strike.touchBase = game::BladePointAt(drawn, links.bladeSpan.span, 0.0f);
			strike.touchTip = game::BladePointAt(drawn, links.bladeSpan.span, 1.0f);
		}
		game::StrikeByMotion(strike);
	}
	// The arrow in the drawing hand, with the bow drawn, stabs the same way
	// (vr::ArrowStabs): the arrow as it is seen is the blade - only while it is
	// thrust head first, not swung (game::IsArrowThrust).
	{
		// Measured by the bow's own step, once a frame, between its places of
		// the arrow (this runs more often than that step moves it).
		game::MotionStrike strike;
		float cosine = -2.0f;
		const bool seen = game::ArrowInHandWorld(strike.arrowGrip, strike.arrowNock, strike.arrowHead, cosine);
		const bool thrust = seen && cosine >= game::kArrowThrustMinAlong;
		// Each swing with the arrow, once: whether it was a thrust or a swing.
		if (active && handResult.arrowStrike && handResult.swingActive && cosine > -1.5f && handResult.swingSerial != state.saidSerial &&
		    state.thrustLines > 0) {
			state.saidSerial = handResult.swingSerial;
			--state.thrustLines;
			OBVR_LOG("Hands: the arrow in the hand %s (its head's way %.2f along it, %.2f needed) - swing %u",
			         thrust ? "thrust - it stabs" : "swung, not thrust - no stab", static_cast<double>(cosine),
			         static_cast<double>(game::kArrowThrustMinAlong), handResult.swingSerial);
		}
		if (active && handResult.arrowStrike && handResult.swingActive && handResult.rightHandValid && !menuIsUp && thrust) {
			strike.arrow = true;
			strike.handRotation = handResult.rightHandRotation;
			strike.handOffsetUnits = handResult.rightHandOffsetUnits;
			strike.swingSerial = handResult.swingSerial;
			strike.attackGroup = vr::kAnimGroupAttackLight;
			strike.cameraValid = shared.cyclopeanCameraValid;
			strike.cameraRotation = shared.cyclopeanCamera.rot;
			strike.cameraPosition = shared.cyclopeanCamera.pos;
			strike.boundFactor = config.hands.hitBoundFactor;
			strike.padUnits = config.hands.hitPadUnits;
			game::StrikeByMotion(strike);
		}
	}
	// The left fist, the same way: with the fists up both hands punch - not
	// one that went through a wall until it is back out.
	if (active && handResult.leftStrikeByMotion && handResult.leftSwingActive && handResult.leftSwingLongEnough &&
	    handResult.leftHandValid && !menuIsUp &&
	    !links.offHandThrough) {
		game::MotionStrike strike;
		strike.hand = 1;
		strike.swingSerial = handResult.leftSwingSerial;
		strike.heavy = handResult.leftSwingHeavy;
		strike.attackGroup =
			handResult.leftSwingHeavy ? vr::PowerAttackGroup(handResult.leftPowerDirection) : vr::kAnimGroupAttackLight;
		strike.speedNow = handResult.leftSwingSpeedNow;
		strike.powerSpeed = config.hands.powerStrikeSpeed;
		strike.powerGroup = vr::PowerAttackGroup(handResult.leftPowerDirection);
		strike.swingMetres = handResult.leftSwingTravelledMetres;
		strike.handRotation = handResult.leftHandRotation;
		strike.handOffsetUnits = handResult.leftHandOffsetUnits;
		strike.cameraValid = shared.cyclopeanCameraValid;
		strike.cameraRotation = shared.cyclopeanCamera.rot;
		strike.cameraPosition = shared.cyclopeanCamera.pos;
		strike.boundFactor = config.hands.hitBoundFactor;
		strike.padUnits = config.hands.hitPadUnits;
		strike.byTouch = config.hands.hitsByTouch;
		strike.capsules = &links.handBodies;
		game::StrikeByMotion(strike);
	}
	{
		if (active && !menuIsUp && handResult.leftHandValid) {
			if (game::SwishDue(handResult.leftStrikeByMotion, handResult.leftSwingActive && handResult.leftSwingLongEnough,
			                   handResult.leftSwingSerial,
			                   state.lastLeftSwished)) {
				state.lastLeftSwished = handResult.leftSwingSerial;
				game::PlaySwingSwish();
			}
			if (vr::GruntDue(handResult.leftStrikeByMotion, handResult.leftSwingHeavy, handResult.leftSwingSerial,
			                 state.lastLeftGrunted)) {
				state.lastLeftGrunted = handResult.leftSwingSerial;
				game::PlayPowerAttackGrunt();
			}
		}
		// On a budget of its own: the right hand's swings spent the shared one
		// before the first left-fist line (the tester's session, 2026-10-14).
		if (handResult.leftSwing != vr::SwingVerdict::None && handResult.leftStrikeByMotion && state.leftSwingLines > 0) {
			--state.leftSwingLines;
			OBVR_LOG("Hands: a left-fist %s%s swing",
			         handResult.leftSwing == vr::SwingVerdict::Heavy ? vr::PowerDirectionName(handResult.leftPowerDirection)
			                                                     : "",
			         handResult.leftSwing == vr::SwingVerdict::Heavy ? " power" : "light");
		}
	}
	
	// Each swing that may strike by motion costs fatigue once it ends, as
	// vanilla's attack does (game::SwingFatigueFor): the weapon hand with its
	// weapon's weight, the other hand as a fist.
	for (UInt32 hand = 0; hand < 2; ++hand) {
		const bool weaponHand = hand == 0;
		const vr::SwingVerdict verdict = weaponHand ? handResult.swing : handResult.leftSwing;
		if (verdict == vr::SwingVerdict::None) {
			continue;
		}
		SInt32 weaponType = -1;
		UInt8* const weapon = weaponHand ? game::EquippedWeaponForm(&weaponType) : nullptr;
		float weight = 0.0f;
		const bool weightKnown = weapon != nullptr && game::WeaponWeightOf(weapon, &weight);
		const game::SwingFatigue charge = game::SwingFatigueFor(
			weaponHand ? handResult.strikeByMotion : handResult.leftStrikeByMotion, true,
			verdict == vr::SwingVerdict::Heavy, active && !menuIsUp, weapon == nullptr, weightKnown, weight);
		if (!charge.due) {
			continue;
		}
		float before = 0.0f;
		float base = 0.0f;
		const bool readBefore = game::ReadPlayerFatigue(before, base);
		float cost = 0.0f;
		const bool charged = game::ChargeSwingFatigue(charge.weight, charge.power, &cost);
		float after = 0.0f;
		const bool readAfter = game::ReadPlayerFatigue(after, base);
		if (state.fatigueLines > 0) {
			--state.fatigueLines;
			char dearer[64] = "";
			if (game::FatigueCostScale() > 1.0f) {
				std::snprintf(dearer, sizeof(dearer), ", x%.2f - a two-hander in one hand",
				              static_cast<double>(game::FatigueCostScale()));
			}
			OBVR_LOG("Fatigue: a %s %s swing cost %.2f (weight %.1f%s)%s; fatigue %.1f -> %.1f",
			         weaponHand ? (weapon != nullptr ? "weapon" : "right-fist") : "left-fist",
			         charge.power ? "power" : "light", static_cast<double>(cost), static_cast<double>(charge.weight),
			         dearer, charged ? "" : " - NOT charged", readBefore ? static_cast<double>(before) : -1.0,
			         readAfter ? static_cast<double>(after) : -1.0);
		}
	}
	
}

}  // namespace obvr::camera::hands

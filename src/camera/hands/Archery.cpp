#include "camera/hands/Archery.h"

#include "core/Log.h"
#include "game/BowDrawSound.h"
#include "game/BowRelease.h"
#include "game/ConsoleLine.h"
#include "game/GameAddresses.h"
#include "game/GameSound.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

void StepArchery(ArcheryState& state, const HandStep& step, const HandShared& shared,
                 const vr::HandModeFrame& frame, game::BowVisualInput& bowVisual, float deltaSeconds) {
	const Config& config = step.config;
	const vr::HandModeResult& handResult = shared.hand;
	const vr::ArcheryVerdict& a = handResult.archery;
	if (a.took || a.nocked || a.drawStarted || a.loosed || a.dropped) {
		OBVR_LOG("Hands: bow by hand - %s (the hands %.2f m apart)%s",
		         a.took ? "an arrow taken from the quiver"
		         : a.nocked ? "the arrow nocked"
		         : a.drawStarted ? "drawn - the engine's draw begins"
		         : a.loosed ? "loosed"
		                    : "the arrow dropped, no shot",
		         static_cast<double>(a.handsApartMetres),
		         a.loosed || a.drawStarted ? (handResult.arrowAimValid ? ", along the arrow's line" : ", no line")
		                                   : "");
	}
	// What is seen of it: the arrow in the hand and on the string, the
	// string where the hand pulls it; the engine's own shot keeps the
	// string at rest until it is over.
	bowVisual.active = config.hands.archery.enabled && frame.inWorld && !frame.menuMode &&
	                     frame.equipped == vr::EquippedKind::Bow && frame.weaponSeen == vr::WeaponSeen::Drawn;
	// The draw sound in its parts, the engine's whole one kept quiet
	// meanwhile (game/BowDrawSound.h).
	game::SetBowDrawSoundByHand(bowVisual.active);
	{
		const vr::BowSoundCue cue = vr::BowSoundsFor(a);
		if (cue.nock) {
			game::PlayBowDrawPart(game::BowDrawPart::Nock);
		}
		if (cue.stretch) {
			game::PlayBowDrawPart(game::BowDrawPart::Stretch);
		}
		if (cue.stopStretch) {
			game::StopBowDrawPart(game::BowDrawPart::Stretch);
		}
	}
	{
		const SInt32 action = game::ReadPlayerAction();
		const vr::ReleaseSnap snap =
			vr::StepReleaseSnap(state.releaseSnap, a.loosed, action, state.releaseSnapped, deltaSeconds);
		if (snap == vr::ReleaseSnap::ToAttach) {
			game::SnapBowDrawToAttach();
		}
		state.releaseSnapped = snap == vr::ReleaseSnap::ToHold && game::SnapBowDrawToHold();
		// How long from the let-go to the arrow leaving (the action 3): the
		// tester's "pause", measured.
		if (a.loosed) {
			state.sinceLoose = 0.0f;
		} else if (state.sinceLoose >= 0.0f) {
			state.sinceLoose += deltaSeconds;
			if (action == addr::kActionAttackFollowThrough || state.sinceLoose > 3.0f) {
				if (state.leftLines > 0) {
					--state.leftLines;
					OBVR_LOG("Hands: bow by hand - %s %.3f s after the let-go",
					         action == addr::kActionAttackFollowThrough ? "the arrow left" : "NO ARROW",
					         static_cast<double>(state.sinceLoose));
				}
				state.sinceLoose = -1.0f;
			}
		}
		// The power from the draw: the string's weight last frame (the
		// picture is placed after this), left to the game when there is
		// none.
		float drawWeight = 0.0f;
		const bool haveWeight = game::BowDrawWeight(drawWeight);
		float weight = 0.0f;
		if (vr::StepBowPower(state.bowPower, a.state == vr::ArrowState::Drawing, haveWeight, drawWeight, a.loosed,
		                     game::ReadPlayerAction(), weight)) {
			game::WriteBowPowerForDraw(weight);
		}
		if (a.loosed) {
			OBVR_LOG("Hands: bow by hand - loosed at a draw of %.2f%s", static_cast<double>(state.bowPower.weight),
			         state.bowPower.holding ? "" : " (no draw read - the game's power)");
		}
	}
	bowVisual.arrow = vr::ArrowShownFor(a.state);
	bowVisual.nockGuide = a.nockGuide;
	bowVisual.string =
		vr::StepBowString(state.bowString, a.state, a.loosed, game::ReadPlayerAction() >= 0, deltaSeconds);
	bowVisual.rightHandBone = config.hands.rightHandBone;
	bowVisual.dtSeconds = deltaSeconds;
	// The player's action while the bow is drawn by hand: 4 and 5 the draw,
	// 3 the follow-through after an arrow left (game::kAction*), so a
	// cancelled draw shows as a draw that never reaches 3.
	{
		const SInt32 action = game::ReadPlayerAction();
		if (bowVisual.active && action != state.action && state.actionLines > 0) {
			--state.actionLines;
			OBVR_LOG("Hands: bow by hand - the player's action %d -> %d", static_cast<int>(state.action),
			         static_cast<int>(action));
		}
		// An eased draw is watched until the engine's action ends: the
		// follow-through (3) is an arrow that left after all.
		if (a.eased) {
			state.watchEase = true;
		} else if (state.watchEase && action != state.action) {
			if (action == addr::kActionAttackFollowThrough) {
				OBVR_LOG("Hands: bow by hand - the eased draw loosed its arrow anyway (action 3)");
				state.watchEase = false;
			} else if (action < 0) {
				OBVR_LOG("Hands: bow by hand - the eased draw ended with no arrow loosed");
				state.watchEase = false;
			}
		}
		state.action = action;
	}
	// Eased back from a draw: the engine's shot taken back the way the
	// DenockArrow mods do it - the unequip group played while the attack
	// is still held (vr::kDenockHoldSeconds), then back to the idle once
	// it is let go (github.com/dannywarren/Oblivion-DenockArrowToo,
	// src/DenockArrowScript.txt).
	// The cancel's end: back to the idle, and the quiver's arrows shown
	// again - the draw's Attach hid one of them (0x005FD0D6) and no arrow
	// was spent (game::RefreshQuiverArrows).
	if (a.denockDone) {
		const bool asked = game::RequestConsoleLine("player.playgroup idle 1");
		const bool refreshed = game::RefreshQuiverArrows();
		OBVR_LOG("Hands: bow by hand - the draw's cancel through: the control let go, back to the idle, the "
		         "quiver %s (action %d)%s",
		         refreshed ? "shown again" : "NOT refreshed", static_cast<int>(game::ReadPlayerAction()),
		         asked ? "" : " - the script line could not be run");
	}
	if (a.eased || a.unnocked || a.stowed) {
		const bool asked = !a.eased || game::RequestConsoleLine("player.playgroup unequip 1");
		OBVR_LOG("Hands: bow by hand - %s (pull %.2f m, %.2f m off the bow's line along %.2f %.2f %.2f, action "
		         "%d)%s",
		         a.eased       ? "eased back: the draw taken back, the arrow back in the hand"
		         : a.unnocked  ? "the arrow off the string, in the hand"
		                       : "the arrow put back in the quiver",
		         static_cast<double>(a.pullMetres), static_cast<double>(a.offLineMetres),
		         static_cast<double>(a.bowAxis.x), static_cast<double>(a.bowAxis.y), static_cast<double>(a.bowAxis.z),
		         static_cast<int>(game::ReadPlayerAction()), asked ? "" : " - the script line could not be run");
	}
	// The quiver's sounds: the game's own for ammunition taken up and put
	// down (ITMAmmoUp 0008B095, ITMAmmoDown 0008B096, Oblivion.esm; the
	// tester, 2026-10-01: a sound at the take - vanilla has none, its draw
	// has only WPNBowDraw and bowShoot).
	if (a.took || a.stowed) {
		const UInt32 form = a.took ? vr::kSoundFormAmmoUp : vr::kSoundFormAmmoDown;
		const bool played = game::PlaySoundForm(form);
		if (state.soundLines > 0) {
			--state.soundLines;
			OBVR_LOG("Hands: bow by hand - the quiver's sound (%08X) %s", form,
			         played ? "played" : "COULD NOT BE PLAYED");
		}
	}
}

}  // namespace obvr::camera::hands

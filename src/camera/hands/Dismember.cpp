#include "camera/hands/Dismember.h"

#include <cstring>

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/BladeContactLogic.h"
#include "game/GameAddresses.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/WalkersLink.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

// A blunt blow (a kick, a bash, a pommel, a hilt) takes the death from the
// blade: the waiting cut is forgotten and their neck held a moment
// (game::ForgetPendingCutFor, BluntGuardHolds).
static void NoteBluntBlow(DismemberState& state, float stepSeconds) {
	const UInt32 blunt = game::TakeBluntBlowActor();
	if (blunt == 0) {
		state.bluntAge += stepSeconds;
		return;
	}
	if (state.killingBlow.armed && state.killingBlow.actor == blunt) {
		OBVR_LOG("Dismember: a blunt blow on %08X - the blade's waiting cut forgotten", blunt);
	}
	game::ForgetPendingCutFor(state.killingBlow, blunt);
	state.bluntActor = blunt;
	state.bluntAge = 0.0f;
}

void StepDismember(DismemberState& state, const HandStep& step, const HandShared& /*shared*/,
                   const vr::HandModeFrame& frame, const DismemberLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	game::StepCuts(config.hands.dismember, dt == dt && dt > 0.0f ? dt : 0.0f);
	if (test::HandScriptMarkedThisFrame()) {
		game::LogCuts(test::HandScriptMarkName());
	}
	// [Debug] TestCut: at the mark "cut", the nearest dead one's head not yet cut, no blade.
	if (config.hands.testCut && test::HandScriptMarkedThisFrame() &&
	    std::strcmp(test::HandScriptMarkName(), "cut") == 0) {
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		if (game::PlayerWorldPosition(feet)) {
			game::NeckOf necks[8];
			const UInt32 n = game::CollectNecks(feet, 400.0f, necks, 8);
			UInt32 best = 8;
			for (UInt32 i = 0; i < n; ++i) {
				if (necks[i].dead && !game::HeadIsCut(necks[i].actor) &&
				    (best == 8 || (necks[i].high - feet).LengthSquared() < (necks[best].high - feet).LengthSquared())) {
					best = i;
				}
			}
			char why[96] = "no dead actor within 400 units";
			const bool cut =
				best < 8 && game::CutHead(necks[best].actor, NiPoint3{60.0f, 0.0f, 120.0f}, why, sizeof(why));
			OBVR_LOG("Dismember: test cut at the mark - %s (%u actors near)", cut ? "cut" : why, n);
		}
	}
	// [Debug] TestCut: at the mark "cut_hand", the nearest dead one's left hand.
	if (config.hands.testCut && test::HandScriptMarkedThisFrame() &&
	    std::strcmp(test::HandScriptMarkName(), "cut_hand") == 0) {
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		if (game::PlayerWorldPosition(feet)) {
			game::NeckOf necks[8];
			const UInt32 n = game::CollectNecks(feet, 400.0f, necks, 8);
			UInt32 best = 8;
			for (UInt32 i = 0; i < n; ++i) {
				if (necks[i].dead && !game::PartIsCut(necks[i].actor, game::CutPart::LeftHand) &&
				    (best == 8 || (necks[i].high - feet).LengthSquared() < (necks[best].high - feet).LengthSquared())) {
					best = i;
				}
			}
			char why[96] = "no dead actor within 400 units";
			const bool cut = best < 8 && game::CutPartOff(necks[best].actor, game::CutPart::LeftHand,
			                                              NiPoint3{0.0f, 40.0f, 60.0f}, why, sizeof(why));
			OBVR_LOG("Dismember: test hand cut at the mark - %s (%u actors near)", cut ? "cut" : why, n);
		}
	}
	const game::CutSettings& cuts = config.hands.dismember;
	const float stepSeconds = dt == dt && dt > 0.0f ? dt : 0.0f;
	NoteBluntBlow(state, stepSeconds);
	// The killing blow: a living neck cut, waiting for the death (StepPendingCut).
	if (state.killingBlow.armed) {
		const UInt32 actor = state.killingBlow.actor;
		const NiPoint3 push = state.killingBlow.push;
		const float waited = state.killingBlow.age;
		const game::PendingStep pending =
			game::StepPendingCut(state.killingBlow, stepSeconds, cuts.enabled && game::LoadedActorIsDead(actor));
		if (pending == game::PendingStep::Cut) {
			char why[96] = "";
			const bool cut = !game::HeadIsCut(actor) && game::CutHead(actor, push, why, sizeof(why));
			OBVR_LOG("Dismember: %08X died of the blow through its neck %.2f s after it - %s%s", actor,
			         static_cast<double>(waited + stepSeconds), cut ? "the killing blow cut" : "refused: ",
			         cut ? "" : why);
			if (cut) {
				backend.Pulse(!links.rolesSwapped, 0.12f, 60.0f, 1.0f);
			}
		} else if (pending == game::PendingStep::Expired) {
			OBVR_LOG("Dismember: %08X lived past the blow through its neck - no cut", actor);
		}
	}
	if (cuts.enabled && active && !menuIsUp && frame.inWorld && links.drawnWeaponValid) {
		const NiPoint3 grip = links.drawnWeaponGrip;
		const NiPoint3 tip = links.drawnWeaponTip;
		if (test::HandScriptMarkedThisFrame()) {
			// For the harness: where the blade stands against the nearest neck.
			game::NeckOf near1[8];
			const UInt32 m = game::CollectNecks(tip, 300.0f, near1, 8);
			for (UInt32 i = 0; i < m && i < 2; ++i) {
				OBVR_LOG("Dismember: at the mark %s the blade %.0f %.0f %.0f to %.0f %.0f %.0f, %08X's neck %.0f %.0f "
				         "%.0f to %.0f %.0f %.0f (%s)",
				         test::HandScriptMarkName(), static_cast<double>(grip.x), static_cast<double>(grip.y),
				         static_cast<double>(grip.z), static_cast<double>(tip.x), static_cast<double>(tip.y),
				         static_cast<double>(tip.z), near1[i].actor, static_cast<double>(near1[i].low.x),
				         static_cast<double>(near1[i].low.y), static_cast<double>(near1[i].low.z),
				         static_cast<double>(near1[i].high.x), static_cast<double>(near1[i].high.y),
				         static_cast<double>(near1[i].high.z), near1[i].dead ? "dead" : "alive");
			}
		}
		if (state.haveBefore && stepSeconds > 0.0f) {
			const float upm = config.tracker.unitsPerMetre > 0.0f ? config.tracker.unitsPerMetre : 70.0f;
			const float speed = math::Sqrt((tip - state.tipBefore).LengthSquared()) / stepSeconds / upm;
			game::NeckOf necks[8];
			const UInt32 n = game::CollectNecks(tip, 160.0f, necks, 8);
			SInt32 type = -1;
			const UInt8* const weapon = game::EquippedWeaponForm(&type);
			const bool edge = game::WeaponHasEdge(type, game::WeaponModelPath(weapon));
			// The killing strikes since the frame before: decided by the part
			// the blade touched, the body flying off before the sweep reaches
			// the neck (game/DismemberLogic.h PartTakesHead).
			game::KillingStrike killed[4];
			const UInt32 kills = game::TakeKillingStrikes(killed, 4);
			for (UInt32 i = 0; i < kills; ++i) {
				const game::BodyPart part = static_cast<game::BodyPart>(killed[i].part);
				game::CutInput in;
				in.enabled = true;
				in.alreadyCut = game::HeadIsCut(killed[i].actor);
				in.dead = true;
				in.edge = edge;
				bool swept = false;
				for (UInt32 j = 0; j < state.necksBeforeCount; ++j) {
					const game::NeckOf& was = state.necksBefore[j];
					if (was.actor == killed[i].actor &&
					    game::KillingSweepMeetsNeck(was.low, was.high, cuts.neckRadiusUnits, state.gripBefore,
					                                state.tipBefore, grip, tip, stepSeconds, game::kKillingSweepSeconds)) {
						swept = true;
					}
				}
				in.onNeck = game::PartTakesHead(part) || swept;
				in.speedMetres = speed;
				const game::CutVerdict verdict = game::DecideCut(cuts, in);
				char why[96] = "";
				const NiPoint3 push = (tip - state.tipBefore) * (0.5f / stepSeconds);
				const bool cut =
					verdict == game::CutVerdict::Cut && game::CutHead(killed[i].actor, push, why, sizeof(why));
				if (state.cutLines > 0) {
					--state.cutLines;
					OBVR_LOG("Dismember: the killing strike on %08X's %s at %.1f m/s (%s, %s) - %s%s%s",
					         killed[i].actor, game::BodyPartName(part), static_cast<double>(speed),
					         edge ? "an edge" : "no edge",
					         swept ? "its sweep on through the neck" : "its sweep on misses the neck",
					         cut ? "the killing blow cut" : game::CutVerdictName(verdict),
					         verdict == game::CutVerdict::Cut && !cut ? " refused: " : "",
					         verdict == game::CutVerdict::Cut && !cut ? why : "");
				}
				if (cut) {
					state.killingBlow.armed = false;
					backend.Pulse(!links.rolesSwapped, 0.12f, 60.0f, 1.0f);
				}
				// A blunt killing blow to the head: crushed, nothing cut -
				// Walkers ends the walker as by a cut (DismemberLogic.h).
				if (game::TellsHeadCrushed(edge, part, in.alreadyCut)) {
					const UInt32 formId =
						*reinterpret_cast<const UInt32*>(killed[i].actor + addr::kFormIdOffset);
					const bool told = formId != 0 && game::SendWalkersHead(formId, game::kHeadCrushed);
					OBVR_LOG("Dismember: the killing blow crushed %08X's head (no edge) - Walkers %s", killed[i].actor,
					         told ? "told" : "not told");
				}
			}
			for (UInt32 i = 0; i < n; ++i) {
				const game::NeckOf& k = necks[i];
				if (game::BluntGuardHolds(state.bluntActor, state.bluntAge, k.actor) || !game::NeckTouched(k.low, k.high, cuts.neckRadiusUnits, state.gripBefore, state.tipBefore, grip, tip)) {
					continue;
				}
				game::CutInput in;
				in.enabled = true;
				in.alreadyCut = game::HeadIsCut(k.actor);
				in.dead = k.dead;
				in.edge = edge;
				in.onNeck = true;
				in.speedMetres = speed;
				const game::CutVerdict verdict = game::DecideCut(cuts, in);
				char why[96] = "";
				const NiPoint3 push = stepSeconds > 0.0f ? (tip - state.tipBefore) * (0.5f / stepSeconds) : NiPoint3{};
				const bool cut =
					verdict == game::CutVerdict::Cut && game::CutHead(k.actor, push, why, sizeof(why));
				if (game::ArmsKillingBlow(cuts, in) && !(state.killingBlow.armed && state.killingBlow.actor == k.actor)) {
					game::ArmPendingCut(state.killingBlow, k.actor, push);
				}
				if ((k.actor != state.lastActor || verdict != state.lastVerdict || cut) && state.cutLines > 0) {
					--state.cutLines;
					OBVR_LOG("Dismember: the blade through %08X's neck at %.1f m/s (%s, %s) - %s%s%s", k.actor,
					         static_cast<double>(speed), k.dead ? "dead" : "alive",
					         edge ? "an edge" : "no edge", game::CutVerdictName(verdict),
					         verdict == game::CutVerdict::Cut && !cut ? " refused: " : "",
					         verdict == game::CutVerdict::Cut && !cut ? why : "");
				}
				state.lastActor = k.actor;
				state.lastVerdict = verdict;
				if (cut) {
					backend.Pulse(!links.rolesSwapped, 0.12f, 60.0f, 1.0f);
				}
			}
			// The hands at their wrists (Nadi, 2026-10-31: "7 ... go"): the same
			// rule on the forearm's last stretch (DismemberLogic.h WristSegment).
			for (UInt32 i = 0; i < n; ++i) {
				const game::NeckOf& k = necks[i];
				for (UInt32 side = 0; side < 2; ++side) {
					if (!k.hasWrist[side]) {
						continue;
					}
					NiPoint3 low{};
					NiPoint3 high{};
					game::WristSegment(k.wristLow[side], k.wristHigh[side], low, high);
					if (!game::NeckTouched(low, high, game::kWristRadiusUnits, state.gripBefore, state.tipBefore, grip, tip)) {
						continue;
					}
					const game::CutPart part = side == 0 ? game::CutPart::LeftHand : game::CutPart::RightHand;
					game::CutInput in;
					in.enabled = true;
					in.alreadyCut = game::PartIsCut(k.actor, part);
					in.dead = k.dead;
					in.edge = edge;
					in.onNeck = true;
					in.speedMetres = speed;
					const game::CutVerdict verdict = game::DecideCut(cuts, in);
					char why[96] = "";
					const NiPoint3 push = (tip - state.tipBefore) * (0.5f / stepSeconds);
					const bool cut = verdict == game::CutVerdict::Cut &&
					                 game::CutPartOff(k.actor, part, push, why, sizeof(why));
					if (state.wristLines > 0 && (cut || verdict != game::CutVerdict::Cut)) {
						--state.wristLines;
						OBVR_LOG("Dismember: the blade through %08X's %s wrist at %.1f m/s (%s, %s) - %s%s", k.actor,
						         side == 0 ? "left" : "right", static_cast<double>(speed), k.dead ? "dead" : "alive",
						         edge ? "an edge" : "no edge", game::CutVerdictName(verdict), cut ? "" : why);
					}
					if (cut) {
						backend.Pulse(!links.rolesSwapped, 0.12f, 60.0f, 1.0f);
					}
				}
			}
		}
		state.gripBefore = grip;
		state.tipBefore = tip;
		state.haveBefore = true;
		state.necksBeforeCount = game::CollectNecks(tip, 160.0f, state.necksBefore, 8);
	} else {
		state.haveBefore = false;
	}
	if (!state.haveBefore) {
		game::KillingStrike drained[4];
		game::TakeKillingStrikes(drained, 4);
	}
}

}  // namespace obvr::camera::hands

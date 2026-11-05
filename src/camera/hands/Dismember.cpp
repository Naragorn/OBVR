#include "camera/hands/Dismember.h"

#include <cstring>

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/BladeContactLogic.h"
#include "game/CampKill.h"
#include "game/GameAddresses.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/SkillCredit.h"
#include "game/WalkersLink.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

// A cut's input for `actor`: on, its part's state, essential or not (an
// essential one never loses a part), and the blade's edge and speed; on
// the neck or the wrist - the caller says otherwise. `limb`: a hand, an arm
// or a leg (DismemberLimbMinSpeed); a two-handed blade drawn cuts easier.
static game::CutInput CutInputOf(UInt32 actor, bool alreadyCut, bool dead, bool edge, float speed, bool limb) {
	SInt32 type = -1;
	game::EquippedWeaponForm(&type);
	game::CutInput in;
	in.limb = limb;
	in.twoHanded = type == game::kCutWeaponBladeTwoHand;
	in.enabled = true;
	in.alreadyCut = alreadyCut;
	in.essential = game::ActorIsEssential(actor);
	in.dead = dead;
	in.edge = edge;
	in.onNeck = true;
	in.speedMetres = speed;
	return in;
}

// The extra skill a cut or a crushed head trains (game/StrikeSkillLogic.h,
// PlanDismemberBonus), with the drawn weapon's skill.
static void CreditCut(const Config& config, UInt32 actor, game::DismemberBonus what) {
	SInt32 type = -1;
	game::EquippedWeaponForm(&type);
	game::CreditDismemberBonus(reinterpret_cast<void*>(actor), what, type, config.hands.dismemberHeadSkillUses,
	                           config.hands.dismemberHandSkillUses);
}

// A cut landed: felt in the weapon hand, and its extra skill.
static void CutLanded(vr::OpenVRBackend& backend, bool rolesSwapped, const Config& config, UInt32 actor,
                      game::DismemberBonus what) {
	backend.Pulse(!rolesSwapped, 0.12f, 60.0f, 1.0f);
	CreditCut(config, actor, what);
}

// A blunt killing blow to the head: crushed, nothing cut - Walkers ends the
// walker as by a cut (DismemberLogic.h), and its extra Blunt.
static void CrushHead(const Config& config, UInt32 actor) {
	const UInt32 formId = *reinterpret_cast<const UInt32*>(actor + addr::kFormIdOffset);
	const bool told = formId != 0 && game::SendWalkersHead(formId, game::kHeadCrushed);
	CreditCut(config, actor, game::DismemberBonus::HeadCrushed);
	OBVR_LOG("Dismember: the killing blow crushed %08X's head (no edge) - Walkers %s", actor, told ? "told" : "not told");
}

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

// A limb's part on the body side a killing strike touched (an arm's capsule:
// its forearm and the whole arm; a leg's: its lower leg), in the order tried.
static UInt32 LimbsOfBodyPart(game::BodyPart part, game::CutPart* out) {
	switch (part) {
	case game::BodyPart::LeftArm:
		out[0] = game::CutPart::LeftForearm;
		out[1] = game::CutPart::LeftArm;
		return 2;
	case game::BodyPart::RightArm:
		out[0] = game::CutPart::RightForearm;
		out[1] = game::CutPart::RightArm;
		return 2;
	case game::BodyPart::LeftLeg:
		out[0] = game::CutPart::LeftLowerLeg;
		return 1;
	case game::BodyPart::RightLeg:
		out[0] = game::CutPart::RightLowerLeg;
		return 1;
	default: return 0;
	}
}

// A limb cut by the blade: its verdict (DismemberLogic.h DecideCut, the joint
// for the neck), the cut, the log and the hand's pulse. Answers whether it was cut.
static bool CutLimb(DismemberState& state, vr::OpenVRBackend& backend, const DismemberLinks& links,
                    const Config& config, UInt32 actor, game::CutPart part, bool dead, bool edge, float speed,
                    const NiPoint3& push, const char* how) {
	const game::CutInput in = CutInputOf(actor, game::PartIsCut(actor, part), dead, edge, speed, part != game::CutPart::Head);
	const game::CutVerdict verdict = game::DecideCut(config.hands.dismember, in);
	char why[96] = "";
	const bool cut = verdict == game::CutVerdict::Cut && game::CutPartOff(actor, part, push, why, sizeof(why));
	if (state.wristLines > 0 && (cut || verdict != game::CutVerdict::Cut)) {
		--state.wristLines;
		OBVR_LOG("Dismember: %s %08X's %s at %.1f m/s (%s, %s) - %s%s", how, actor, game::CutJointName(part),
		         static_cast<double>(speed), dead ? "dead" : "alive", edge ? "an edge" : "no edge",
		         cut ? game::CutPartName(part) : game::CutVerdictName(verdict), cut ? " off" : why);
	}
	if (cut) {
		CutLanded(backend, links.rolesSwapped, config, actor, game::DismemberBonus::HandOff);
	}
	return cut;
}

// The limbs at their joints (Nadi, 2026-11-05: "go 4"): the hands' rule on
// each joint's stretch (DismemberLogic.h JointSegment), one cut per actor a frame.
static void CutLimbsBySweep(DismemberState& state, vr::OpenVRBackend& backend, const DismemberLinks& links,
                            const Config& config, const game::NeckOf* necks, UInt32 n, bool edge, float speed,
                            const NiPoint3& grip, const NiPoint3& tip, float stepSeconds) {
	const NiPoint3 push = (tip - state.tipBefore) * (0.5f / stepSeconds);
	for (UInt32 i = 0; i < n; ++i) {
		const game::NeckOf& k = necks[i];
		for (UInt32 p = 0; p < game::kCutPartCount; ++p) {
			const game::CutPart part = static_cast<game::CutPart>(p);
			if (!k.hasLimb[p]) {
				continue;
			}
			NiPoint3 low{};
			NiPoint3 high{};
			game::JointSegment(part, k.limbJoint[p], k.limbNext[p], low, high);
			if (game::NeckTouched(low, high, game::LimbRadiusUnits(part), state.gripBefore, state.tipBefore, grip, tip) &&
			    CutLimb(state, backend, links, config, k.actor, part, k.dead, edge, speed, push, "the blade through")) {
				break;
			}
		}
	}
}

// A killing strike on an arm or a leg: the limb whose joint the blade's sweep
// on passes through, as it stood the frame before (the body flung off by the
// hit; the neck's rule, KillingSweepMeetsNeck).
static void CutLimbOfKillingStrike(DismemberState& state, vr::OpenVRBackend& backend, const DismemberLinks& links,
                                   const Config& config, UInt32 actor, game::BodyPart touched, bool edge, float speed,
                                   const NiPoint3& grip, const NiPoint3& tip, float stepSeconds) {
	game::CutPart parts[2];
	const UInt32 count = LimbsOfBodyPart(touched, parts);
	for (UInt32 j = 0; j < state.necksBeforeCount; ++j) {
		const game::NeckOf& was = state.necksBefore[j];
		for (UInt32 q = 0; was.actor == actor && q < count; ++q) {
			const UInt32 p = static_cast<UInt32>(parts[q]);
			NiPoint3 low{};
			NiPoint3 high{};
			game::JointSegment(parts[q], was.limbJoint[p], was.limbNext[p], low, high);
			if (was.hasLimb[p] &&
			    game::KillingSweepMeetsNeck(low, high, game::LimbRadiusUnits(parts[q]), state.gripBefore, state.tipBefore,
			                                grip, tip, stepSeconds, game::kKillingSweepSeconds) &&
			    CutLimb(state, backend, links, config, actor, parts[q], true, edge, speed,
			            (tip - state.tipBefore) * (0.5f / stepSeconds), "the killing strike through")) {
				return;
			}
		}
	}
}

// [Debug] TestCut at a mark: the nearest dead one's part named by the mark
// ("cut_hand" the left hand - "cut_hand_far" thrown far -, "cut_forearm" the
// left forearm, "cut_arm" the right arm, "cut_leg" the left lower leg).
static void TestCutAtMark(const Config& config) {
	if (!config.hands.testCut || !test::HandScriptMarkedThisFrame()) {
		return;
	}
	struct MarkPart {
		const char* mark;
		const char* label;  // the log's word, "hand" as the hand's line always had it
		game::CutPart part;
		NiPoint3 push;
	};
	static const MarkPart kMarks[] = {
		{"cut_hand", "hand", game::CutPart::LeftHand, NiPoint3{0.0f, 40.0f, 60.0f}},
		// Thrown far, so that what stays at the wrist shows (Nadi, 2026-11-05).
		{"cut_hand_far", "hand", game::CutPart::LeftHand, NiPoint3{0.0f, 300.0f, 250.0f}},
		{"cut_forearm", "forearm", game::CutPart::LeftForearm, NiPoint3{0.0f, 40.0f, 60.0f}},
		{"cut_arm", "arm", game::CutPart::RightArm, NiPoint3{0.0f, 40.0f, 60.0f}},
		{"cut_leg", "leg", game::CutPart::LeftLowerLeg, NiPoint3{0.0f, 40.0f, 40.0f}},
	};
	for (const MarkPart& m : kMarks) {
		if (std::strcmp(test::HandScriptMarkName(), m.mark) != 0) {
			continue;
		}
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		if (!game::PlayerWorldPosition(feet)) {
			return;
		}
		game::NeckOf necks[8];
		const UInt32 n = game::CollectNecks(feet, 400.0f, necks, 8);
		UInt32 best = 8;
		for (UInt32 i = 0; i < n; ++i) {
			if (necks[i].dead && !game::PartIsCut(necks[i].actor, m.part) &&
			    (best == 8 || (necks[i].high - feet).LengthSquared() < (necks[best].high - feet).LengthSquared())) {
				best = i;
			}
		}
		char why[96] = "no dead actor within 400 units";
		const bool cut = best < 8 && game::CutPartOff(necks[best].actor, m.part, m.push, why, sizeof(why));
		OBVR_LOG("Dismember: test %s cut at the mark - %s (%u actors near)", m.label, cut ? "cut" : why, n);
		return;
	}
}

void StepDismember(DismemberState& state, const HandStep& step, const HandShared& /*shared*/,
                   const vr::HandModeFrame& frame, const DismemberLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	game::StepCuts(config.hands.dismember, dt == dt && dt > 0.0f ? dt : 0.0f);
	game::StepWalkersCampKills(config.hands.dismember.enabled);
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
	TestCutAtMark(config);
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
				CutLanded(backend, links.rolesSwapped, config, actor, game::DismemberBonus::HeadOff);
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
				game::CutInput in = CutInputOf(killed[i].actor, game::HeadIsCut(killed[i].actor), true, edge, speed, false);
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
					CutLanded(backend, links.rolesSwapped, config, killed[i].actor, game::DismemberBonus::HeadOff);
				}
				// A blunt killing blow to the head: crushed, nothing cut -
				// Walkers ends the walker as by a cut (DismemberLogic.h).
				if (game::TellsHeadCrushed(edge, part, in.alreadyCut)) {
					CrushHead(config, killed[i].actor);
				}
				CutLimbOfKillingStrike(state, backend, links, config, killed[i].actor, part, edge, speed, grip, tip, stepSeconds);
			}
			for (UInt32 i = 0; i < n; ++i) {
				const game::NeckOf& k = necks[i];
				if (game::BluntGuardHolds(state.bluntActor, state.bluntAge, k.actor) || !game::NeckTouched(k.low, k.high, cuts.neckRadiusUnits, state.gripBefore, state.tipBefore, grip, tip)) {
					continue;
				}
				const game::CutInput in = CutInputOf(k.actor, game::HeadIsCut(k.actor), k.dead, edge, speed, false);
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
					CutLanded(backend, links.rolesSwapped, config, k.actor, game::DismemberBonus::HeadOff);
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
					const game::CutInput in = CutInputOf(k.actor, game::PartIsCut(k.actor, part), k.dead, edge, speed, true);
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
						CutLanded(backend, links.rolesSwapped, config, k.actor, game::DismemberBonus::HandOff);
					}
				}
			}
			CutLimbsBySweep(state, backend, links, config, necks, n, edge, speed, grip, tip, stepSeconds);
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

#include "camera/hands/TorchBlow.h"

#include <cstdio>

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/GameSound.h"
#include "game/MeleeHits.h"
#include "game/Shove.h"
#include "game/TorchBlowLogic.h"
#include "platform/Win32Min.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {
namespace {

// Oblivion.esm's hand-to-hand hit (SOUN WPNHitHandX, read 2026-10-09 with
// the other weapon sounds): the torch's blow is heard as the off hand's punch.
constexpr UInt32 kSoundFormHitHand = 0x0000C3CA;

void Buzz(const TorchBlowLinks& links) {
	if (links.pulse != nullptr) {
		links.pulse(vr::HandDeviceForRole(false, links.rolesSwapped), 0.05f, 150.0f, 1.0f);
	}
}

// The push's point: 30 units back from where the blow met along the hand's
// way across the ground.
NiPoint3 PushFrom(const NiPoint3& at, const NiPoint3& velocity) {
	const float across = math::Sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
	if (!(across > 0.001f)) {
		return at;
	}
	return at - NiPoint3{velocity.x / across, velocity.y / across, 0.0f} * 30.0f;
}

void NoteTouchCost(TorchBlowState& s, long long ticks) {
	++s.costFrames;
	s.costTicks += static_cast<double>(ticks);
	s.costLongest = ticks > s.costLongest ? ticks : s.costLongest;
	if (s.costFrames < 600) {
		return;
	}
	static const long long perSecond = ReadPerformanceFrequency();
	if (s.costLines > 0 && perSecond > 0) {
		--s.costLines;
		const double us = 1.0e6 / static_cast<double>(perSecond);
		OBVR_LOG("Torch touch: %u frames with the torch and people near - %.1f us a frame on the mean, %.1f the longest",
		         s.costFrames, s.costTicks * us / static_cast<double>(s.costFrames),
		         static_cast<double>(s.costLongest) * us);
	}
	s.costFrames = 0;
	s.costTicks = 0.0;
	s.costLongest = 0;
}

}  // namespace

bool TorchBlowHeldFor(const TorchBlowState& s, const void* actor) {
	for (const HeldTorchBlow& h : s.held) {
		if (h.actor != nullptr && h.actor == actor) {
			return true;
		}
	}
	return false;
}

void SettleTorchBlows(TorchBlowState& s, const game::ShoveSettings& settings, const TorchBlowLinks& links, float dt) {
	const vr::HandModeResult& hand = links.hand;
	for (HeldTorchBlow& h : s.held) {
		if (h.actor == nullptr) {
			continue;
		}
		h.seconds += dt;
		const bool endedPower = hand.leftSwing == vr::SwingVerdict::Heavy;
		const game::ShoveKind kind = game::SettleTorchBlow(h.serial, hand.leftSwingSerial, hand.leftSwingActive,
		                                                   hand.leftSwingHeavy, endedPower);
		if (kind == game::ShoveKind::None) {
			continue;
		}
		const HeldTorchBlow held = h;
		h = HeldTorchBlow{};
		const bool there = game::RefStillThere(held.print) && game::ActorStillLiving(held.actor);
		if (s.heldLines > 0) {
			--s.heldLines;
			OBVR_LOG("Shove: the torch's blow on %08X held %.2f s - its swing %s: %s", held.print.ref,
			         static_cast<double>(held.seconds),
			         held.serial != hand.leftSwingSerial ? "long gone"
			         : hand.leftSwingActive              ? "grew into a power one"
			         : endedPower                        ? "ended a power one"
			                                             : "ended short of a power one",
			         !there                          ? "they are gone or dead - nothing lands"
			         : kind == game::ShoveKind::Hard ? "knocked down"
			                                         : "staggered");
		}
		if (there && game::StrikeAndShove(held.actor, kind, held.from, held.centre,
		                                  game::AtImpactSpeed(settings, held.speed), game::BlowKind::Torch, true, true)) {
			game::StartShoveCooldown(links.cooldown, held.actor, settings.cooldownSeconds);
		}
	}
}

void LandTorchBlow(TorchBlowState& s, const Config& config, const TorchBlowLinks& links, void* actor,
                   game::ShoveKind bySpeed, const NiPoint3& at, const NiPoint3& from, const NiPoint3& centre,
                   float speed, bool byTouch) {
	const game::ShoveSettings& settings = config.hands.shove;
	const vr::HandModeResult& hand = links.hand;
	char who[96];
	if (byTouch) {
		std::snprintf(who, sizeof(who), "the torch met %08X, the hand at %.1f m/s", reinterpret_cast<UInt32>(actor),
		              static_cast<double>(speed));
	} else {
		std::snprintf(who, sizeof(who), "the left hand at %.1f m/s towards %08X", static_cast<double>(speed),
		              reinterpret_cast<UInt32>(actor));
	}
	// A stagger by the speed, a knockdown only with a power swing
	// (TorchKnockdownPowerOnly) - a blow met before the swing is a power one
	// held to its end. A blow, never a slap, whatever the fingers do round it
	// (the harness's open hand on the torch slapped the face and set off Put
	// it in its Place's slap).
	game::ShoveKind kind = bySpeed;
	bool slotsFull = false;
	const game::TorchBlow blow = game::TorchBlowAtContact(config.hands.torchKnockdownPowerOnly, bySpeed,
	                                                      hand.leftSwingActive, hand.leftSwingHeavy,
	                                                      hand.leftSwing == vr::SwingVerdict::Heavy);
	if (blow.hold) {
		HeldTorchBlow* slot = nullptr;
		for (HeldTorchBlow& h : s.held) {
			if (h.actor == nullptr) {
				slot = &h;
				break;
			}
		}
		if (slot != nullptr) {
			slot->actor = actor;
			slot->print = game::TakeRefFingerprint(reinterpret_cast<UInt32>(actor));
			slot->serial = hand.leftSwingSerial;
			slot->from = from;
			slot->centre = centre;
			slot->speed = speed;
			slot->metres = hand.leftSwingTravelledMetres;
			slot->seconds = 0.0f;
			game::StartShoveCooldown(links.cooldown, actor, settings.cooldownSeconds);
			// The blow is heard and felt as it lands; what it does waits.
			game::PlaySoundFormAt(kSoundFormHitHand, reinterpret_cast<UInt32>(actor), at);
			Buzz(links);
			if (s.heldLines > 0) {
				--s.heldLines;
				OBVR_LOG("Shove: %s - the torch's blow held to the swing's end (swing %u, %.2f m so far, a power one "
				         "from %.2f m; by the speed alone %s)",
				         who, hand.leftSwingSerial, static_cast<double>(hand.leftSwingTravelledMetres),
				         static_cast<double>(vr::PowerMetresFor(hand.leftPowerDirection, config.hands.gestures)),
				         bySpeed == game::ShoveKind::Hard ? "hard" : "light");
			}
			return;
		}
		kind = game::ShoveKind::Light;  // four held already: a stagger now
		slotsFull = true;
	} else {
		kind = blow.kind;
	}
	if (!game::StrikeAndShove(actor, kind, from, centre, game::AtImpactSpeed(settings, speed), game::BlowKind::Torch,
	                          true, true)) {
		return;
	}
	game::StartShoveCooldown(links.cooldown, actor, settings.cooldownSeconds);
	game::PlaySoundFormAt(kSoundFormHitHand, reinterpret_cast<UInt32>(actor), at);
	Buzz(links);
	OBVR_LOG("Shove: %s - %s%s%s", who, kind == game::ShoveKind::Hard ? "hard" : "light",
	         byTouch ? "" : " (the off hand's punch, a weapon drawn)",
	         !config.hands.torchKnockdownPowerOnly ? " - the torch, by the speed alone"
	         : slotsFull                           ? " - the torch, four blows held already: a stagger"
	         : kind == game::ShoveKind::Hard       ? " - the torch in a power swing"
	         : bySpeed == game::ShoveKind::Hard    ? " - the torch, no swing running: a stagger, not the speed's knockdown"
	                                               : " - the torch, no swing running");
}

void StepTorchTouch(TorchBlowState& s, const Config& config, const TorchBlowLinks& links,
                    const game::BladeBodies& bodies, bool byTouch, const game::TorchPose& now,
                    const game::TorchHand& hand, bool allowed, float dt) {
	const game::TorchPose before = s.before;
	s.before = byTouch ? now : game::TorchPose{};
	s.quietSeconds = s.quietSeconds > dt ? s.quietSeconds - dt : 0.0f;
	if (!byTouch || !allowed || !now.valid || bodies.count == 0) {
		return;
	}
	const UInt32 skip =
		links.cooldown.secondsLeft > 0.0f ? reinterpret_cast<UInt32>(links.cooldown.actor) : 0u;
	const long long started = ReadPerformanceCounter();
	const game::TorchTouch t = game::TorchTouchOf(bodies, before, now, hand, skip);
	NoteTouchCost(s, ReadPerformanceCounter() - started);
	void* const actor = reinterpret_cast<void*>(t.actor);
	if (t.actor == 0 || TorchBlowHeldFor(s, actor)) {
		return;
	}
	const game::ShoveSettings& settings = config.hands.shove;
	const game::ShoveKind kind = game::TorchTouchKind(settings, t);
	if (kind == game::ShoveKind::None) {
		// Met, and no blow: why, now and then (the 2026-11-26 log's refusals
		// named the shove's rule - "a weapon is readied" - for a torch that was
		// only too slow).
		if (s.touchLines > 0 && s.quietSeconds <= 0.0f) {
			--s.touchLines;
			s.quietSeconds = 1.0f;
			OBVR_LOG("Shove: the torch met %08X at its %s (%.0f %.0f %.0f, the way out of it %.2f %.2f %.2f), the hand "
			         "at %.1f m/s, the torch there at %.1f m/s, %.1f of it into them%s - %s",
			         t.actor, game::BodyPartName(t.part), static_cast<double>(t.point.x), static_cast<double>(t.point.y),
			         static_cast<double>(t.point.z), static_cast<double>(t.normal.x), static_cast<double>(t.normal.y),
			         static_cast<double>(t.normal.z), static_cast<double>(t.handSpeed), static_cast<double>(t.speedThere),
			         static_cast<double>(t.speedInto), t.swept ? " (between two frames)" : "",
			         !game::TorchWentIn(t) ? "not into them (along them or away): no blow"
			                               : "the hand under the stagger's speed: no blow");
		}
		return;
	}
	NiPoint3 centre = t.point;
	game::CapsulesCentre(bodies, t.actor, centre);
	if (s.touchLines > 0) {
		--s.touchLines;
		OBVR_LOG("Shove: the torch went into %08X at its %s (%.0f %.0f %.0f, the way out of it %.2f %.2f %.2f) - the torch "
		         "there at %.1f m/s, %.1f of it into them%s",
		         t.actor, game::BodyPartName(t.part), static_cast<double>(t.point.x), static_cast<double>(t.point.y),
		         static_cast<double>(t.point.z), static_cast<double>(t.normal.x), static_cast<double>(t.normal.y),
		         static_cast<double>(t.normal.z), static_cast<double>(t.speedThere), static_cast<double>(t.speedInto),
		         t.swept ? " (between two frames)" : "");
	}
	LandTorchBlow(s, config, links, actor, kind, t.point, PushFrom(t.point, hand.velocity), centre, t.handSpeed, true);
}

}  // namespace obvr::camera::hands

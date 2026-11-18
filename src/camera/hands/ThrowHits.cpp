#include "camera/hands/ThrowHits.h"

#include "camera/hands/ThrowNoiseRun.h"
#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/BounceBackLogic.h"
#include "game/GameAddresses.h"
#include "game/GrabPhysics.h"
#include "game/KnockWatch.h"
#include "game/MeleeHits.h"
#include "game/RefFingerprint.h"
#include "game/Shove.h"
#include "game/ThrowLogic.h"
#include "game/ThrowNoise.h"
#include "game/TorchGrab.h"
#include "vr/HintLogic.h"

namespace obvr::camera::hands {
namespace {

// The thing followed, and what it was as it left the hand
// (game/RefFingerprint.h): one picked up again in flight is deleted, and its
// memory can be another's.
struct ThrowRun {
	game::ThrowFlight flight;
	game::RefFingerprint print;
};

ThrowRun& Throw() {
	static ThrowRun run;
	return run;
}

// Gear on its way back to the hand, and the hands as this frame read them.
struct BounceRun {
	game::BounceFlight flight;
	game::RefFingerprint print;
	bool left = false;  // to the other hand (a torch, the bow)
	NiTransform camera{};
	bool cameraValid = false;
	bool valid[2] = {false, false};  // right, left
	NiPoint3 offset[2] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
};

BounceRun& Bounce() {
	static BounceRun run;
	return run;
}

bool BounceHand(const BounceRun& run, bool left, NiPoint3& hand) {
	const int side = left ? 1 : 0;
	if (!run.cameraValid || !run.valid[side]) {
		return false;
	}
	hand = run.camera.pos + run.camera.rot * run.offset[side];
	return true;
}

// Each frame: the hands kept for this frame's hit, the thing on its way back
// steered.
void StepBounceBack(const Config& config, const BounceHands& hands, float dt, bool inWorld) {
	BounceRun& run = Bounce();
	run.camera = hands.camera;
	run.cameraValid = hands.cameraValid;
	run.valid[0] = hands.hand.rightHandValid;
	run.valid[1] = hands.hand.leftHandValid;
	run.offset[0] = hands.hand.rightHandOffsetUnits;
	run.offset[1] = hands.hand.leftHandOffsetUnits;
	game::BounceFlight& flight = run.flight;
	if (flight.ref == 0) {
		return;
	}
	const float perMetre = config.tracker.unitsPerMetre;
	const UInt32 ref = flight.ref;
	const bool there = inWorld && game::RefStillThere(run.print);
	const UInt32 node = there ? *reinterpret_cast<const UInt32*>(ref + addr::kRefNiNodeOffset) : 0;
	const UInt32 body = there ? game::RefBodyOf(ref) : 0;
	NiPoint3 hand{0.0f, 0.0f, 0.0f};
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	// Caught by the grab or the pull, it is the engine's grab's body.
	const bool present = mem::LooksLikeObjectAddress(node) && body != 0 && body != game::HeldBody() &&
	                     game::BodyVelocityUnits(body, velocity) && BounceHand(run, run.left, hand);
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	if (present) {
		const float* const p = reinterpret_cast<const float*>(node + addr::kNodeWorldTranslateOffset);
		at = NiPoint3{p[0], p[1], p[2]};
	}
	const UInt32 steeredBefore = flight.steered;
	const float startMetres = flight.startMetres;
	const game::BounceStep st =
		game::StepBounce(flight, config.hands.throwHit.bounceBack, present, at, velocity, hand, dt, perMetre);
	if (st.steer) {
		game::SendBodyOff(body, st.velocity);
		static UInt32 s_traceLines = 120;
		if (config.shoveTrace && s_traceLines > 0) {
			--s_traceLines;
			OBVR_LOG("Bounce back trace: %08X at %.1f %.1f %.1f, %.2f m from the hand, moving %.0f units/s, set to "
			         "%.0f",
			         ref, static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
			         static_cast<double>(math::Sqrt((hand - at).LengthSquared()) / perMetre),
			         static_cast<double>(math::Sqrt(velocity.LengthSquared())),
			         static_cast<double>(math::Sqrt(st.velocity.LengthSquared())));
		}
	}
	if (st.end != game::BounceEnd::None) {
		static UInt32 s_lines = 16;
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Bounce back: %08X - %s; %u frame(s) steered, %.2f m from the hand at the hit, the nearest "
			         "%.2f m",
			         ref, game::BounceEndName(st.end), steeredBefore, static_cast<double>(startMetres),
			         static_cast<double>(flight.nearestMetres));
		}
	}
}

// At a throw's hit: gear (a weapon, a torch) sent back to its hand - the
// bow's and the torch's is the other hand. True when it was sent.
bool BeginBounceBack(const Config& config, UInt32 ref, UInt32 base, bool weapon, SInt32 weaponType,
                     const NiPoint3& at, float hitMetresPerSecond) {
	BounceRun& run = Bounce();
	const game::BounceBackSettings& settings = config.hands.throwHit.bounceBack;
	const bool torch = !weapon && mem::LooksLikeObjectAddress(base) &&
	                   *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) == game::kFormTypeLightCode;
	const game::BounceThing thing = weapon ? game::BounceThing::Weapon
	                                : torch ? game::BounceThing::Torch
	                                        : game::BounceThing::Other;
	if (!game::BounceBackFor(settings, thing)) {
		return false;
	}
	const bool left = torch || weaponType == 5;  // the bow is held in the other hand, and so is the torch
	NiPoint3 hand{0.0f, 0.0f, 0.0f};
	const UInt32 body = game::RefBodyOf(ref);
	const bool handRead = BounceHand(run, left, hand);
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	static UInt32 s_lines = 16;
	if (body == 0 || !handRead ||
	    !game::BeginBounce(run.flight, settings, ref, at, hand, hitMetresPerSecond, config.tracker.unitsPerMetre,
	                       velocity) ||
	    !game::SendBodyOff(body, velocity)) {
		run.flight = game::BounceFlight{};
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Bounce back: %08X NOT sent back - %s", ref,
			         body == 0 ? "no body" : !handRead ? "the hand not tracked" : "the body not written");
		}
		return false;
	}
	run.print = game::TakeRefFingerprint(ref);
	run.left = left;
	if (s_lines > 0) {
		--s_lines;
		OBVR_LOG("Bounce back: %08X (%s) sent back to the %s hand %.2f m off - %.2f m/s, %.2f s on its arc, the hit "
		         "at %.1f m/s",
		         ref, torch ? "a torch" : "a weapon", left ? "other" : "weapon",
		         static_cast<double>(run.flight.startMetres),
		         static_cast<double>(math::Sqrt(velocity.LengthSquared()) / config.tracker.unitsPerMetre),
		         static_cast<double>(run.flight.flightSeconds), static_cast<double>(hitMetresPerSecond));
	}
	return true;
}

// [Debug] ShoveTrace: each frame of a thrown thing's flight fast enough to
// hit - where it is, and the nearest living actor's bound within a wide
// reach (none in reach of a hit when `actor` is null).
void TraceThrowFrame(const Config& config, const game::ThrowFlight& flight, const NiPoint3& at, const void* actor) {
	static UInt32 s_lines = 60;
	if (!config.shoveTrace || s_lines == 0) {
		return;
	}
	--s_lines;
	NiPoint3 nearest{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	const void* const around = game::LivingActorAt(at, 1.0f, 300.0f, &nearest, 1.0f, &radius);
	OBVR_LOG("Throw trace: at %.1f %.1f %.1f, %.2f m/s - %s; the nearest within 300 units %s %.1f %.1f %.1f "
	         "(bound %.1f, %.1f units off)",
	         static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
	         static_cast<double>(flight.speed), actor != nullptr ? "IN someone's bound" : "in no one's bound",
	         around != nullptr ? "at" : "none, at", static_cast<double>(nearest.x), static_cast<double>(nearest.y),
	         static_cast<double>(nearest.z), static_cast<double>(radius),
	         static_cast<double>(math::Sqrt((at - nearest).LengthSquared())));
}

}  // namespace

void StepThrowHits(const Config& config, const BounceHands& hands, HintsState& hints, game::ShoveCooldown& cooldown,
                   float dt, bool inWorld) {
	StepBounceBack(config, hands, dt, inWorld);
	game::ThrowFlight& flight = Throw().flight;
	// A thrown object, else a weapon or torch thrown from the hand (its body
	// just sent off; one waiting stays for the next frame).
	const UInt32 object = game::TakeJustThrown();
	const UInt32 thrown = object != 0 ? object : game::TakeSentOff();
	StartThrowNoiseFor(thrown, config);
	// A weapon or torch sent off is a throw; an object only at a throw's speed.
	const float letGo = game::LastLetGoUnitsPerSecond() / config.tracker.unitsPerMetre;
	hints.throws += thrown != 0 && (object == 0 || vr::HintThrowCounts(letGo)) ? 1u : 0u;
	StepThrowNoiseFor(config, dt, inWorld);
	game::StepKnockWatch(dt, config.shoveTrace && inWorld);
	if (thrown != 0 && config.hands.throwHit.enabled) {
		game::StartThrowFlight(flight, thrown);
		Throw().print = game::TakeRefFingerprint(thrown);
		static UInt32 s_throwLines = 8;
		if (s_throwLines > 0) {
			--s_throwLines;
			OBVR_LOG("Throw: %08X thrown - watched for whom it hits", thrown);
		}
	}
	if (flight.ref == 0) {
		return;
	}
	if (!game::RefStillThere(Throw().print)) {
		flight = game::ThrowFlight{};
		return;
	}
	const UInt32 node = inWorld ? *reinterpret_cast<const UInt32*>(flight.ref + addr::kRefNiNodeOffset) : 0;
	if (!mem::LooksLikeObjectAddress(node)) {
		flight = game::ThrowFlight{};
		return;
	}
	const float* const p = reinterpret_cast<const float*>(node + addr::kNodeWorldTranslateOffset);
	const NiPoint3 at{p[0], p[1], p[2]};
	const bool hadLast = flight.haveLast;
	const NiPoint3 before = flight.last;
	if (!game::StepThrowFlight(flight, config.hands.throwHit, at, dt, config.tracker.unitsPerMetre)) {
		return;
	}
	const game::ShoveKind kind = game::ThrowHitFor(config.hands.throwHit, flight.speed);
	if (kind == game::ShoveKind::None) {
		return;
	}
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	void* const actor = game::LivingActorAt(at, 0.5f, 4.0f, &centre, 1.0f);
	TraceThrowFrame(config, flight, at, actor);
	if (actor == nullptr || !game::ShoveAllowed(cooldown, actor)) {
		return;
	}
	const float speed = flight.speed;
	// A weapon thrown hits with its own skill, anything else as a fist
	// (game/StrikeSkillLogic.h).
	const UInt32 base = *reinterpret_cast<const UInt32*>(flight.ref + addr::kRefBaseFormOffset);
	const bool weapon = mem::LooksLikeObjectAddress(base) &&
	                    *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) == addr::kFormTypeWeapon;
	const SInt32 weaponType = weapon ? *reinterpret_cast<const UInt8*>(base + addr::kWeaponTypeOffset) : -1;
	if (game::StrikeAndShove(actor, kind, hadLast ? before : at, centre, game::AtImpactSpeed(config.hands.shove, speed),
	                         weapon ? game::BlowKind::ThrownWeapon : game::BlowKind::ThrownThing, false, false,
	                         weaponType)) {
		game::StartShoveCooldown(cooldown, actor, config.hands.shove.cooldownSeconds);
		OBVR_LOG("Throw: %08X hit %08X at %.1f m/s - %s", flight.ref, reinterpret_cast<UInt32>(actor),
		         static_cast<double>(speed), kind == game::ShoveKind::Hard ? "knocked down" : "staggered");
		hints.bounces += BeginBounceBack(config, flight.ref, base, weapon, weaponType, at, speed) ? 1u : 0u;
		flight = game::ThrowFlight{};  // one hit per throw
	}
}

}  // namespace obvr::camera::hands

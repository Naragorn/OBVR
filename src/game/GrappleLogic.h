#pragma once

// The off hand in a fight, as in Blade & Sorcery (docs/physical-combat-spec.md,
// section 8; the tester, 2026-10-09: "man kann mit der off hand gegner packen
// oder hauen wie in blade & sorcery", and asked whether to note it for later:
// "ne starte jetzt"). The off hand is the one without the weapon - the left,
// the right for a left-handed player (vr::AssignHandRoles swaps the roles
// before anything here sees them).
//
// - The strike. With a blade or a blunt weapon drawn, the off hand - its grip
//   loose, not on a two-hander's handle - driven fast into someone is the
//   shove's blow (ShoveLogic.h): from ShoveSpeed they stagger and are pushed,
//   from ShoveHardSpeed they are knocked down. A fist is a punch; an open
//   hand the shove or the slap it is with the weapons away. With the weapons
//   away the hands are the shove's and the fists' already.
// - The grab. The off hand's grip closed at someone fighting - within one of
//   the capsules on their bones (BladeContactLogic.h) and kGrappleReachUnits
//   more - holds them: they stagger, their legs stop (SlowApproach.h,
//   StandStill), the hand drags them across the ground, and held by the
//   head, the neck or the weapon arm their blows do not land (game/Parry).
//   Let go at speed they are thrown - knocked down or staggered by the
//   hand's speed, as a shove; they break free after GrabHoldSeconds, and a
//   hand dragged far from them (they are stuck on something) loses them.
// - The ragdoll hold (the tester, 2026-10-12/13: "gegner greifen und werfen
//   ... mit ragdoll irgendwie", "npc ragdoll halten und mit kraft
//   wegschubsen oder sogar auf dem boden zu schubsen"). Grabbed by the
//   torso, an arm or a leg they go down into their ragdoll - the engine's
//   own knockdown with no push - and the part under the hand is held: its
//   Havok body driven to the hand each frame (game/RagdollHold), the rest
//   of them hanging from it through the ragdoll's joints. One already
//   lying knocked down is taken as they lie. Let go fast the held part is
//   thrown with the hand's speed times GrabThrowStrength - away, or along
//   the ground; let go slow they drop. The hold ends when the engine's
//   get-up takes them, after GrabHoldSeconds, with the body too far from
//   the hand, or with them dead or gone. The head and the neck keep the
//   standing hold (B&S's choke). [Hands] GrabRagdoll.
//
// Pure, covered by grapple_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct GrappleSettings {
	bool strikes = true;         // [Hands] OffHandStrikes
	bool grabs = true;           // [Hands] OffHandGrabs
	float holdSeconds = 4.0f;    // [Hands] GrabHoldSeconds: then they break free
	float fatigue = 15.0f;       // [Hands] GrabFatigue, spent at the grab (the light shove's)
	bool ragdoll = true;         // [Hands] GrabRagdoll: the torso, an arm or a leg grabbed takes them down into the ragdoll
	float throwStrength = 1.5f;  // [Hands] GrabThrowStrength: the hand's speed times this on the held part, let go
	float weight = 1.0f;         // [Hands] GrabWeight: the held ragdoll's mass against the hand's pull; 0 weightless
};

// GrabThrowStrength as used: 0 (no throw, they drop) to 5; anything else
// the default 1.5.
inline float GrappleThrowStrength(float setting) {
	return setting >= 0.0f && setting <= 5.0f ? setting : 1.5f;
}

// ------------------------------------------------------- the ragdoll's weight
// The tester (2026-10-13, the ragdoll hold seen in the headset): "geben wir
// den gehaltenen gegnern noch gewicht". The held part's body is no longer
// given the velocity that closes the gap whatever it takes (an unlimited
// force): the change of its velocity each Havok step is cut to a budget -
// a force, in the ragdoll's own mass units (skeleton.nif's bhkRigidBodies: a
// hand 15, a forearm 30, an upper arm 40, the whole body about 530) times
// Havok units a second squared - over the body's mass inverse. Havok's
// joints carry the rest of the body on the held part and its gravity pulls
// it down, so a heavy chain lags behind the hand and sags under it, and one
// the budget cannot lift stays on the ground and is dragged. The budget
// comes from the player's Strength: at 100 it lifts kGrabLiftMassAtHundred
// against the world's gravity, at 50 half that; GrabWeight divides it (2:
// twice as heavy), 0 is no limit (the hold as first built).

// GrabWeight as used: 0 (weightless) to 5; anything else 1.
inline float GrappleWeight(float setting) {
	return setting >= 0.0f && setting <= 5.0f ? setting : 1.0f;
}

inline constexpr float kGrabLiftMassAtHundred = 300.0f;
inline constexpr float kGrabGravityFallback = 9.8f;
inline constexpr float kGrabStrengthLeast = 10.0f;
inline constexpr float kGrabStrengthMost = 200.0f;

// The pull's budget - a force in Havok's terms (mass times Havok units a
// second squared) - from the player's Strength (counted from 10 to 200),
// GrabWeight and the world's gravity (a positive number; not one, or 0:
// Havok's own 9.8). 0 with the weight 0: no limit.
inline float GrapplePullBudget(float strength, float weight, float gravityHavok) {
	const float w = GrappleWeight(weight);
	if (!(w > 0.0f)) {
		return 0.0f;
	}
	float s = strength >= kGrabStrengthLeast ? strength : kGrabStrengthLeast;
	if (s > kGrabStrengthMost) {
		s = kGrabStrengthMost;
	}
	const float g = gravityHavok > 0.01f && gravityHavok < 1000.0f ? gravityHavok : kGrabGravityFallback;
	return kGrabLiftMassAtHundred * g * (s / 100.0f) / w;
}

// What the budget lifts, in the ragdoll's mass units: the budget over the
// gravity; 0 for no limit or no gravity.
inline float GrappleLiftMass(float budget, float gravityHavok) {
	return budget > 0.0f && gravityHavok > 0.01f ? budget / gravityHavok : 0.0f;
}

struct PullVerdict {
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // the velocity to set, Havok units a second
	bool injected = true;                 // false: Havok has not stepped since the last set - the same velocity again
	bool capped = false;                  // the change wanted was cut to the budget
	float accel = 0.0f;                   // the change given over the time, Havok units a second squared
};

// The velocity to set on the held body. `now` is the body's velocity read
// back, `wanted` the drive's own (the gap closed in a frame), `massInv` the
// body's, `budget` the pull's (0: no limit), `seconds` the time since the
// last change was given. The change is given once per Havok step: with the
// body's velocity still exactly the one set last (`stepped` false), Havok
// has not stepped, and the same velocity goes again - except after a set of
// nothing, which a body at rest on the ground reads back whether Havok
// stepped or not. A body whose mass does not read, a frame without time or
// no budget: the wanted velocity, no cap.
inline PullVerdict PullLimitedVelocity(const NiPoint3& now, const NiPoint3& wanted, float massInv, float budget,
                                       float seconds, bool stepped, const NiPoint3& lastSet) {
	PullVerdict v;
	if (!stepped) {
		v.velocity = lastSet;
		v.injected = false;
		return v;
	}
	v.velocity = wanted;
	if (!(budget > 0.0f) || !(massInv > 0.0f) || !(seconds > 0.0f)) {
		return v;
	}
	const NiPoint3 d = wanted - now;
	const float change = math::Sqrt(d.LengthSquared());
	const float most = budget * massInv * seconds;
	v.accel = change / seconds;
	if (change > most && change > 0.0f) {
		v.velocity = now + d * (most / change);
		v.capped = true;
		v.accel = budget * massInv;
	}
	return v;
}

// Whether the velocity read back is still exactly the one set - then Havok
// has not stepped in between (PullLimitedVelocity's `stepped` is the
// opposite). A set of nothing is never "still the same": a body at rest on
// the ground reads back nothing after a step too, and the hold must not
// stick to it.
inline bool VelocityStillTheSet(bool haveLast, const NiPoint3& now, const NiPoint3& lastSet) {
	if (!haveLast) {
		return false;
	}
	if (lastSet.x == 0.0f && lastSet.y == 0.0f && lastSet.z == 0.0f) {
		return false;
	}
	return now.x == lastSet.x && now.y == lastSet.y && now.z == lastSet.z;
}

// The hand this far outside a capsule still takes hold (about 9 cm; the
// capsules are thin on the arms).
inline constexpr float kGrappleReachUnits = 6.0f;
// The hand this far across the ground from where it wants them: lost.
inline constexpr float kGrappleLoseUnits = 40.0f;
// The drag: each frame they are placed where the hand wants them (SetPos's
// way, their controller placed too - game::PlaceActorAt), at most 2 m a
// second of the way. The hit's knockback through their character proxy
// was tried first (2026-10-10, hand script grapple.txt): over a single
// frame it did not move them at all, and re-issued over 0.3 s a frame it
// moved them a few units while the stagger carried them off - the hold
// was lost in 0.6 s both times.
inline constexpr float kGrappleDragUnitsPerSecond = 140.0f;

// GrabHoldSeconds as used: 0.5 to 30 s; anything else the default 4.
inline float GrappleHoldSeconds(float setting) {
	return setting >= 0.5f && setting <= 30.0f ? setting : 4.0f;
}

// The off hand's strike with a weapon drawn: the shove's thresholds, a fist
// or an open hand alike. None without a blade or blunt weapon drawn (the
// fists and the weapons away are the hands' own already), with the hand
// busy (its grip held, on a two-hander's handle), or too slow.
inline ShoveKind OffHandStrikeFor(const GrappleSettings& g, const ShoveSettings& shove, bool meleeWeaponDrawn,
                                  bool offHandFree, const ShoveHand& hand) {
	if (!g.strikes || !meleeWeaponDrawn || !offHandFree || !hand.valid || hand.gripHeld) {
		return ShoveKind::None;
	}
	if (!(hand.towardsSpeed >= shove.speed)) {
		return ShoveKind::None;
	}
	return hand.towardsSpeed >= shove.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

// Where the hand takes hold: the nearest capsule whose surface is within
// `reachUnits` of the hand (inside it counts as 0 or less).
struct GrappleTake {
	bool found = false;
	UInt32 actor = 0;
	BodyPart part = BodyPart::Column;
	float gap = 0.0f;  // the hand from the capsule's surface, units
};

inline GrappleTake GrappleTakeAt(const BladeBodies& bodies, const NiPoint3& hand, float reachUnits) {
	GrappleTake best;
	for (UInt32 i = 0; i < bodies.count; ++i) {
		const BladeBodyCapsule& c = bodies.cap[i];
		const float gap = PointSegmentDistance(hand, c.a, c.b) - c.radius;
		if (!(gap <= reachUnits)) {
			continue;
		}
		if (!best.found || gap < best.gap) {
			best.found = true;
			best.actor = c.actor;
			best.part = c.part;
			best.gap = gap;
		}
	}
	return best;
}

// Whether a hold there stops their blows: the head, the neck, or the arm
// that holds the weapon - the right; Oblivion's actors draw it in the right
// hand.
inline bool GrappleStopsBlows(BodyPart part) {
	return part == BodyPart::Head || part == BodyPart::Neck || part == BodyPart::RightArm;
}

enum class GrappleEnd : UInt8 {
	None,
	LetGo,       // the grip opened, the hand slow
	Thrown,      // opened at ShoveSpeed: staggered and pushed along the hand's motion (a ragdoll: the held part sent off)
	ThrownDown,  // opened at ShoveHardSpeed: knocked down along it
	BrokeFree,   // held GrabHoldSeconds
	Lost,        // the hand dragged too far from them
	Gone,        // dead, unreadable, the mode or the setting off, the hand busy
	StoodUp,     // a ragdoll hold: the engine's get-up took them (or they were never down)
};

inline const char* GrappleEndName(GrappleEnd e) {
	switch (e) {
	case GrappleEnd::LetGo: return "let go";
	case GrappleEnd::Thrown: return "thrown - staggered and pushed";
	case GrappleEnd::ThrownDown: return "thrown down";
	case GrappleEnd::BrokeFree: return "broke free";
	case GrappleEnd::Lost: return "lost - the hand too far from them";
	case GrappleEnd::Gone: return "gone - dead, out of reach, or the hand busy";
	case GrappleEnd::StoodUp: return "stood up - the engine's get-up took them";
	default: return "held";
	}
}

// --------------------------------------------------------- the ragdoll hold
//
// docs/physical-combat-spec.md, section 8's proposal, built 2026-10-13.

enum class GrappleHold : UInt8 {
	Standing,  // staggered, the legs held, placed where the hand wants them
	Ragdoll,   // knocked down into the ragdoll, the held part's body driven to the hand
};

inline const char* GrappleHoldName(GrappleHold h) { return h == GrappleHold::Ragdoll ? "ragdoll" : "standing"; }

// The parts a ragdoll hold takes: the torso, the arms, the legs. The head
// and the neck stay the standing hold - B&S's choke, the one held upright
// and their blows stopped; the column (a creature read without the bones)
// the standing hold too - a creature may have no ragdoll.
inline bool GrapplePartRagdolls(BodyPart part) {
	return part == BodyPart::Torso || part == BodyPart::LeftArm || part == BodyPart::RightArm ||
	       part == BodyPart::LeftLeg || part == BodyPart::RightLeg;
}

// Which hold a grab takes: the ragdoll with the setting on, a part it takes
// and a Havok body found on the part's bone (`bodyFound`); the standing hold
// otherwise.
inline GrappleHold GrappleHoldFor(const GrappleSettings& g, BodyPart part, bool bodyFound) {
	return g.ragdoll && bodyFound && GrapplePartRagdolls(part) ? GrappleHold::Ragdoll : GrappleHold::Standing;
}

// The engine's knock state (HighProcess +0x11C; its step 0x006545E0, read
// 2026-10-13): 0 standing; 2 the knockdown's lead-in, the ragdoll on; 1 down
// - the get-up comes 2.4 s after the process's time stamp, or 5.4 s with the
// body's root ("Bip01 NonAccum") more than 16 units off its proxy; 3 and 4
// the collapse's and the ragdoll-less knock's own; 5 the get-up animation
// (the ragdoll still on); 6 its end. Down: 1 to 4. Getting up: 5 and 6.
inline bool KnockStateDown(SInt32 state) { return state >= 1 && state <= 4; }
inline bool KnockStateGettingUp(SInt32 state) { return state == 5 || state == 6; }

// The engine's get-up clock: seconds after the process's time stamp, and the
// lift of the body's root above the proxy that stretches the shorter wait
// to the longer one (the constants at 0x00A72D40 and 0x00A72D38 times the
// timescale - real seconds whatever the timescale - and 0x005F0270's 16).
inline constexpr float kKnockGetUpSeconds = 2.4f;
inline constexpr float kKnockGetUpLiftedSeconds = 5.4f;
inline constexpr float kKnockGetUpLiftUnits = 16.0f;

// Whether the hold stamps the process's time anew this frame, so the
// engine's get-up waits from now: only while they lie (state 1; in the
// lead-in the engine stamps itself, and a get-up under way is left alone),
// and only every so often - the stamp is the engine's own call, made each
// time the knockdown is.
inline constexpr float kKnockRestampSeconds = 0.5f;

inline bool KnockRestampDue(SInt32 state, float sinceLastStamp) {
	return state == 1 && sinceLastStamp >= kKnockRestampSeconds;
}

// The grip opened: how they leave the hand, by its speed across the ground
// (m/s) - the shove's thresholds.
inline GrappleEnd GrappleReleaseFor(const ShoveSettings& shove, float speedAcross) {
	if (speedAcross >= shove.hardSpeed) {
		return GrappleEnd::ThrownDown;
	}
	return speedAcross >= shove.speed ? GrappleEnd::Thrown : GrappleEnd::LetGo;
}

// A grip squeezed just before the hand gets there still takes hold: for
// this long after it closed (a lunge squeezes first).
inline constexpr float kGrappleTakeGraceSeconds = 0.3f;

struct GrappleState {
	UInt32 actor = 0;  // held, 0 nobody
	BodyPart part = BodyPart::Column;
	GrappleHold hold = GrappleHold::Standing;
	float seconds = 0.0f;
	// Where they stood from the hand at the grab, across the ground: the
	// hand keeps them there (the standing hold).
	NiPoint3 offset{0.0f, 0.0f, 0.0f};
	bool gripWas = false;
	float closedFor = 0.0f;  // seconds the grip has been closed; past the grace after an end
};

// A ragdoll hold whose knockdown the engine refused: the standing hold
// instead, from the same grab.
inline void GrappleHoldDowngrade(GrappleState& s) { s.hold = GrappleHold::Standing; }

struct GrappleInput {
	// [Hands] OffHandGrabs is in the settings; this is the rest: Full VR in
	// the world, the off hand free (not on a handle, no shield on it).
	bool allowed = false;
	bool handValid = false;
	bool grip = false;                         // the off hand's grip down
	NiPoint3 hand{0.0f, 0.0f, 0.0f};           // the off hand, world
	NiPoint3 handVelocity{0.0f, 0.0f, 0.0f};   // m/s, world
	// When the grip closes: what the hand is at, whether they are fighting
	// (alive, in combat, in a high process), and where they stand.
	GrappleTake take;
	bool takeFights = false;
	NiPoint3 takeAt{0.0f, 0.0f, 0.0f};
	// For the ragdoll hold, at the take: whether they stand (knock state 0,
	// so they are knocked down first) or already lie knocked down (taken as
	// they lie), and whether a Havok body was found on the part's bone. One
	// neither standing nor down (getting up) is refused.
	bool takeStanding = true;
	bool takeDown = false;
	bool takeBody = false;
	// The one held, this frame: alive, near, and - the standing hold -
	// standing; where they stand.
	bool heldReadable = false;
	NiPoint3 heldAt{0.0f, 0.0f, 0.0f};
	// The one held as a ragdoll, this frame: their knock state, and their
	// held part's body - whether it still reads, and where its centre is.
	SInt32 heldKnockState = 0;
	bool heldBodyValid = false;
	NiPoint3 heldBodyAt{0.0f, 0.0f, 0.0f};
	float dtSeconds = 0.0f;
};

struct GrappleVerdict {
	bool started = false;
	bool refused = false;  // the grip closed at someone not fighting (or down, for the standing hold)
	GrappleEnd end = GrappleEnd::None;
	UInt32 actor = 0;
	BodyPart part = BodyPart::Column;
	GrappleHold hold = GrappleHold::Standing;
	bool knock = false;                    // a ragdoll start: knock them down first (the engine's, no push, from the hand)
	NiPoint3 drag{0.0f, 0.0f, 0.0f};       // standing, held: units across the ground to move them this frame
	NiPoint3 driveTo{0.0f, 0.0f, 0.0f};    // ragdoll, held: where the hand wants the held part's body, world
	float gap = 0.0f;                      // units from where the hand wants them (across the ground; a ragdoll: the body from the hand)
	float seconds = 0.0f;                  // how long they have been held, at an end the whole hold
	float speed = 0.0f;                    // the hand at the end, m/s (across the ground; a ragdoll: in the air)
	NiPoint3 direction{0.0f, 0.0f, 0.0f};  // the hand's motion then, a unit
	NiPoint3 throwVelocity{0.0f, 0.0f, 0.0f};  // ragdoll, thrown: the held part's new velocity, m/s
};

inline GrappleVerdict StepGrapple(GrappleState& s, const GrappleSettings& g, const ShoveSettings& shove,
                                  const GrappleInput& in) {
	GrappleVerdict v;
	const bool closed = in.grip && !s.gripWas;
	s.gripWas = in.grip;
	const float dt = in.dtSeconds > 0.0f ? in.dtSeconds : 0.0f;
	s.closedFor = !in.grip ? 0.0f : closed ? 0.0f : s.closedFor + dt;
	// Closing now, or closed within the grace and the hand arriving.
	const bool taking = in.grip && s.closedFor <= kGrappleTakeGraceSeconds;
	if (s.actor != 0) {
		v.actor = s.actor;
		v.part = s.part;
		v.hold = s.hold;
		if (!g.grabs || !in.allowed || !in.handValid || !in.heldReadable) {
			v.end = GrappleEnd::Gone;
		} else if (!in.grip) {
			if (s.hold == GrappleHold::Ragdoll) {
				// The held part leaves the hand with the hand's speed, in the
				// air: away, up, or along the ground as the hand went.
				const float speed = math::Sqrt(in.handVelocity.LengthSquared());
				v.speed = speed;
				if (speed > 1e-4f) {
					v.direction = in.handVelocity * (1.0f / speed);
				}
				// GrabThrowStrength 0: no throw, they drop (the body's own
				// motion left alone, not set to nothing).
				const float strength = GrappleThrowStrength(g.throwStrength);
				v.end = speed >= shove.speed && strength > 0.0f ? GrappleEnd::Thrown : GrappleEnd::LetGo;
				if (v.end == GrappleEnd::Thrown) {
					v.throwVelocity = in.handVelocity * strength;
				}
			} else {
				const float across =
					math::Sqrt(in.handVelocity.x * in.handVelocity.x + in.handVelocity.y * in.handVelocity.y);
				v.speed = across;
				if (across > 1e-4f) {
					v.direction = NiPoint3{in.handVelocity.x / across, in.handVelocity.y / across, 0.0f};
				}
				v.end = GrappleReleaseFor(shove, across);
			}
		} else if (s.hold == GrappleHold::Ragdoll) {
			s.seconds += dt;
			if (!KnockStateDown(in.heldKnockState)) {
				v.end = GrappleEnd::StoodUp;
			} else if (s.seconds >= GrappleHoldSeconds(g.holdSeconds)) {
				v.end = GrappleEnd::BrokeFree;
			} else if (!in.heldBodyValid) {
				v.end = GrappleEnd::Gone;
			} else {
				const float gap = math::Sqrt((in.hand - in.heldBodyAt).LengthSquared());
				v.gap = gap;
				if (gap > kGrappleLoseUnits) {
					v.end = GrappleEnd::Lost;
				} else {
					v.driveTo = in.hand;
				}
			}
		} else {
			s.seconds += dt;
			const NiPoint3 d{in.hand.x + s.offset.x - in.heldAt.x, in.hand.y + s.offset.y - in.heldAt.y, 0.0f};
			const float gap = math::Sqrt(d.x * d.x + d.y * d.y);
			v.gap = gap;
			if (s.seconds >= GrappleHoldSeconds(g.holdSeconds)) {
				v.end = GrappleEnd::BrokeFree;
			} else if (gap > kGrappleLoseUnits) {
				v.end = GrappleEnd::Lost;
			} else {
				const float most = kGrappleDragUnitsPerSecond * dt;
				v.drag = gap > most && gap > 0.0f ? d * (most / gap) : d;
			}
		}
		v.seconds = s.seconds;
		if (v.end != GrappleEnd::None) {
			s.actor = 0;
			s.seconds = 0.0f;
			s.hold = GrappleHold::Standing;
			// The next hold wants the grip closed anew.
			s.closedFor = kGrappleTakeGraceSeconds + 1.0f;
		}
		return v;
	}
	if (!taking || !g.grabs || !in.allowed || !in.handValid || !in.take.found) {
		return v;
	}
	v.actor = in.take.actor;
	v.part = in.take.part;
	v.hold = GrappleHoldFor(g, in.take.part, in.takeBody);
	// Nobody not fighting; the standing hold takes nobody down, the ragdoll
	// hold nobody getting up.
	const bool takes = v.hold == GrappleHold::Ragdoll ? in.takeStanding || in.takeDown : in.takeStanding;
	if (!in.takeFights || !takes) {
		v.refused = true;
		return v;
	}
	s.actor = in.take.actor;
	s.part = in.take.part;
	s.hold = v.hold;
	s.seconds = 0.0f;
	s.offset = NiPoint3{in.takeAt.x - in.hand.x, in.takeAt.y - in.hand.y, 0.0f};
	v.started = true;
	v.knock = v.hold == GrappleHold::Ragdoll && in.takeStanding;
	return v;
}

// Where a throw pushes from: behind them along the hand's motion, so the
// shove's push and knockback (which go away from that point) carry them
// along it. Their centre itself when the hand did not move.
inline NiPoint3 GrappleThrowFrom(const NiPoint3& centre, const NiPoint3& direction) {
	return NiPoint3{centre.x - direction.x * 30.0f, centre.y - direction.y * 30.0f, centre.z};
}

}  // namespace obvr::game

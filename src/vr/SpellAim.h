#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

// The spell from the casting hand (Nadi, 2026-10-28: "das projektil muss aus
// der casting hand (whl linke oder?) kommen statt irgendwo von rechts wie
// jetzt ... mit linker hand hochheben dann kommt das crosshair um zielen
// können und dann linke hand trigger drücken dann kommt das projektil aus der
// linken hand dahin wo das crosshair ist").
//
// The casting hand is the other hand - the left, or the right for a
// left-hander - the hand whose trigger holds the cast and whose palm carries
// the spell's hand effect (CameraHook PlaceMagicNodeAtCastingHand). With a
// spell that reaches a target ready:
//
//   * the crosshair: shown on that hand's ray while the hand is raised
//     (CastingHandRaised) or a cast is under way, and gone when it is let
//     down again;
//   * the aim: while a cast is under way the spell leaves along that hand's
//     ray (game/AimAtSource.h: the player's rotation swapped for the length
//     of the engine call that makes the projectile);
//   * the origin: the projectile starts at that hand's palm. The engine takes
//     a caster's start from the world position of its "magicNode"
//     (0x0069A060 reads [node+0x88..0x90] of the node the caster's virtual
//     +0x24, 0x005EDC40, finds by name in the first- or third-person
//     skeleton); game::SetSpellOrigin moves that node there for the call.
//     Any spell cast from the hand - a touch or self spell makes no
//     projectile, so it costs nothing there.

// The widest range among a spell's effects (the EffectItem's range: 0 self,
// 1 touch, 2 target - xOBSE GameMagicEffects.h). Unknown: not read.
enum class SpellReach : UInt8 { Unknown, Self, Touch, Target };

struct SpellAimSettings {
	bool enabled = true;                // [Hands] SpellFromCastingHand
	float raiseBelowEyesMetres = 0.45f;  // [Hands] SpellRaiseBelowEyesMetres
	float raiseAheadMetres = 0.15f;      // [Hands] SpellRaiseAheadMetres
};

// Whether the casting hand is held up to aim: no lower than belowEyesMetres
// under the eyes, and at least aheadMetres ahead of them along the view
// levelled to the horizon - looking down at the raised hand keeps it ahead.
// handFromEyes is the hand minus the eyes in the world, in metres, z up.
inline bool CastingHandRaised(const NiPoint3& handFromEyes, const NiPoint3& viewForward, float belowEyesMetres,
                              float aheadMetres) {
	const float flatLength = math::Sqrt(viewForward.x * viewForward.x + viewForward.y * viewForward.y);
	if (!(flatLength > 1.0e-3f)) {
		return false;
	}
	const float ahead = (handFromEyes.x * viewForward.x + handFromEyes.y * viewForward.y) / flatLength;
	return ahead >= aheadMetres && handFromEyes.z >= -belowEyesMetres;
}

// A hand's ray as the aim the source swap takes (game::AimSourcePose): the
// turn from the head's heading, from the ray in the head's frame (the game's
// axes, x right, y forward) the way the grab's reach is turned
// (ReachDirection: atan2(-x, y)), and the sine of its pitch in the world,
// positive up. False for a ray with no heading or no length.
inline bool AimAlong(const NiPoint3& headRelative, const NiPoint3& world, float& yawTurn, float& sinPitch) {
	const float flat = headRelative.x * headRelative.x + headRelative.y * headRelative.y;
	const float lengthSquared = world.LengthSquared();
	if (!(flat > 1.0e-6f) || !(lengthSquared > 1.0e-6f)) {
		return false;
	}
	yawTurn = math::Atan2(-headRelative.x, headRelative.y);
	float s = world.z / math::Sqrt(lengthSquared);
	if (s > 1.0f) {
		s = 1.0f;
	} else if (s < -1.0f) {
		s = -1.0f;
	}
	sinPitch = s;
	return true;
}

struct SpellAimInput {
	bool fullVr = false;
	bool firstPerson = false;
	bool menuUp = false;
	// The casting hand tracked, its ray and palm known this frame.
	bool castingHandValid = false;
	// The active spell's widest reach.
	SpellReach reach = SpellReach::Unknown;
	// The cast control held or its cast still under way (the cast window).
	bool castActive = false;
	// CastingHandRaised for the casting hand.
	bool raised = false;
};

struct SpellAimVerdict {
	bool crosshair = false;  // the crosshair on the casting hand's ray
	bool aim = false;        // the spell leaves along that ray
	bool origin = false;     // the projectile starts at that palm
};

// The aim and the origin only while a cast is under way: the swap and the
// node are read by every engine call around them, and a swing of the weapon
// hand with a target spell merely ready must keep its own heading.
inline SpellAimVerdict StepSpellAim(const SpellAimInput& in, const SpellAimSettings& s) {
	SpellAimVerdict v;
	if (!s.enabled || !in.fullVr || !in.firstPerson || in.menuUp || !in.castingHandValid) {
		return v;
	}
	const bool target = in.reach == SpellReach::Target;
	v.crosshair = target && (in.raised || in.castActive);
	v.aim = target && in.castActive;
	v.origin = in.castActive;
	return v;
}

}  // namespace obvr::vr

#pragma once

// The body IK's twist bones and the hand's roll on the forearm, pure
// (game/BodyIk.cpp SolveArm, game/ArmRests.h): measured from the standing
// idle's rest and written also where a skeleton hangs its twist bones under
// nodes of its own.

#include "game/BodyIkLogic.h"

namespace obvr::game {

// A twist bone's local for a roll about its limb's x. The twist may hang
// under its limb through nodes of a skeleton's own ("ReScale R Forearm"
// between "Bip01 R Forearm" and "Bip01 R ForearmTwist", the harness's and
// Nadi's skeleton, 2026-11-18): `between` is the turn from the limb's frame to
// the twist's parent's (the locals of the nodes between, the identity when
// the limb is the parent), `restOnLimb` the twist's rest in the limb's frame.
inline NiMatrix33 TwistLocalFor(const NiMatrix33& between, float roll, const NiMatrix33& restOnLimb) {
	return ik::Transposed(between) * (TurnAboutX(roll) * restOnLimb);
}

// A twist's local, as read, in its limb's frame - the rest TwistLocalFor takes.
inline NiMatrix33 TwistRestOnLimb(const NiMatrix33& between, const NiMatrix33& local) { return between * local; }

// The hand's roll on the forearm the IK solved: the target's turn about the
// forearm's x less the hand's at rest on it - the rest, not the animation's
// hand of the frame, so an animation turning the wrist (a take, an equip, a
// draw) does not roll the drawn forearm under a hand held still.
inline float HandRollOnForearm(const NiMatrix33& forearmRot, const NiMatrix33& handRot, const NiMatrix33& handRestLocal) {
	return math::WrapAngle(TwistAboutX(ik::Transposed(forearmRot) * handRot) - TwistAboutX(handRestLocal));
}

}  // namespace obvr::game

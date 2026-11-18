#pragma once

// The arms' rests for the body IK (game/BodyIk.cpp): the upper arm's and the
// hand's locals and the twist bones' turns in their limb's frame, read once
// from the standing idle with the trunk's pose. The IK measures the arms'
// rolls from here, not from the animation's frame: read from the animation,
// a take, an equip or a draw - whose animation turns the wrist - rolled the
// drawn forearm and its twist up to 40 degrees a frame with the hand held
// still (Nadi, Windows headset 2026-11-16: "arme flackern weiterhin wenn ich
// eine waffe ausrüste/aufnehme"; harness weapon-pickup-wobble, 2026-11-18).

#include "game/GameTypes.h"

namespace obvr::game {

struct ArmRests {
	NiMatrix33 upper{};       // the upper arm's local under the clavicle
	NiMatrix33 hand{};        // the hand's local under the forearm
	NiMatrix33 upperTwist{};  // in the upper arm's frame (TwistRestOnLimb)
	NiMatrix33 foreTwist{};   // in the forearm's frame
};

// The turn from `limb`'s frame to the frame of `node`'s parent: the locals of
// the nodes between ("ReScale R Forearm" between the forearm and its twist,
// on the harness's skeleton and Nadi's), the identity when `limb` is the
// parent. False when `node` does not hang under `limb` within three nodes.
bool TurnBetween(const NiAVObject* limb, const NiAVObject* node, NiMatrix33& out);

// An arm's rests from its bones as they stand: upper arm, its twist,
// forearm, its twist, hand. False when one is missing or a twist does not
// hang under its limb.
bool TakeArmRests(const NiAVObject* upper, const NiAVObject* upperTwist, const NiAVObject* fore,
                  const NiAVObject* foreTwist, const NiAVObject* hand, ArmRests& out);

}  // namespace obvr::game

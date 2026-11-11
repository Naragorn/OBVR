#pragma once

// The stoppers' ids and who may be a body for the hands and the blade, moved
// out of game/BladeContactLogic.h (the size ratchet) - see its "the
// stoppers" section for what they are.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kStopperOwnShield = 0xFFFFFFF1u;
inline constexpr UInt32 kStopperOwnBlade = 0xFFFFFFF2u;
inline constexpr UInt32 kStopperFoeBladeBit = 1u;
inline constexpr UInt32 kStopperFoeShieldBit = 2u;

// Whether an actor may be taken as a body for the hands and the blade to
// stop at, strike or shove: not the player, and not the horse the player
// rides. On the horse the hands sit inside its capsule all the time, and the
// stops' hold and the strikes on it set the horse and its rider down anew -
// both headings and the place jumping at once, 8 to 34 degrees and up to 110
// units, the engine's camera on its spine swinging with it (horse-ride.txt,
// 2026-10-19: gone with the stops and the shove off, there with them on).
// `mount` is 0 on foot.
inline bool ActorMayBeABody(UInt32 actor, UInt32 player, UInt32 mount) {
	return actor != 0 && actor != player && (mount == 0 || actor != mount);
}

inline UInt32 FoeBladeId(UInt32 actor) { return actor | kStopperFoeBladeBit; }
inline UInt32 FoeShieldId(UInt32 actor) { return actor | kStopperFoeShieldBit; }

inline bool IsStopperId(UInt32 id) {
	return id == kStopperOwnShield || id == kStopperOwnBlade || (id & 3u) != 0;
}

}  // namespace obvr::game

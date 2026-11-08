#pragma once

// The impact's speeds handed to every way of striking (vr/HandMode.h
// HandSettings), kept apart from the settings themselves. Pure.

#include "vr/HandMode.h"

namespace obvr::vr {

// One speed for every blunt blow (game/ImpactLogic.h): the impact's stagger
// and knockdown speeds handed to each way of striking - the shove and the
// off hand (with the torch and the grapple's throw), the shield bash, the
// pommel, the half-sword's hilt and a thrown thing - and the vanilla roll
// to the shove's settings, which every one of them lands through.
inline void ApplyImpact(HandSettings& h) {
	const game::ImpactSettings& i = h.impact;
	h.shove.speed = i.staggerSpeed;
	h.shove.hardSpeed = i.knockdownSpeed;
	h.shove.vanillaKnockdown = i.vanillaKnockdown;
	h.shieldBash.speed = i.staggerSpeed;
	h.shieldBash.hardSpeed = i.knockdownSpeed;
	h.pommel.speed = i.staggerSpeed;
	h.pommel.hardSpeed = i.knockdownSpeed;
	h.halfSword.hiltHardSpeed = i.knockdownSpeed;
	h.throwHit.staggerSpeed = i.staggerSpeed;
	h.throwHit.knockSpeed = i.knockdownSpeed;
}

}  // namespace obvr::vr

#pragma once

#include "core/Types.h"
#include "game/BlockAngleLogic.h"
#include "game/ParryLogic.h"

namespace obvr::game {

// Arrows by the shield's contact rule (game/ArrowShieldLogic.h has the
// reading of the projectile's impact 0x0060A640 and the decision). The call
// to the Block check at 0x0060AD01 goes through a stub: with the shield by
// contact, an arrow at the player is judged against the disc - on it, the
// shield's impact (the arrow attached to the shield's node); beside it, the
// body's - and the verdict is kept for that projectile until the hit
// handler's cone takes it (TakeArrowVerdict). Not the player, or the shield
// not by contact: the original call, so vanilla's test is unchanged.
void InstallArrowShield();

// Once a frame (from game/BlockCone's SetBlockGuard): the player's guard -
// the disc in the world and ShieldGuardPose - and the frame's count for the
// kept verdicts' age.
void SetArrowShieldGuard(const BlockGuard& guard);

// The verdict kept from this projectile's impact, taken once; None for any
// other attacker (a melee blow, an arrow never judged).
KeptArrow TakeArrowVerdict(UInt32 attacker);

}  // namespace obvr::game

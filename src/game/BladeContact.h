#pragma once

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The Havok world as the blade's contact step asks it
// (game/BladeContactLogic.h): the world's ray pick (PickWorldSegment) cast
// as layer 23 with the player's group - the quiet layer, whose row OBVR
// keeps without the character controllers while the hand bodies run, so a
// living person's coarse capsule does not stop the blade; the step passes
// one anyway when it is met (an ignored kind). The player's own group -
// the controller, the hand bodies, a held object - is never met (the filter
// rule 0x008A7F70, step 4). What the pick met is a kind by its body's
// motion and layer (ContactKindOf, ReadPickBody).
class HavokBladeWorld {
public:
	BladeRayHit Cast(const NiPoint3& from, const NiPoint3& to);

	UInt32 casts = 0;     // asked this frame
	UInt32 notBodies = 0; // met something that is no rigid body (ignored)
	UInt32 lastLayer = 0; // the layer of the last fixed body met, for the log
	UInt32 lastMotion = 0;
};

// The blade's kicks, handed to WorldPush's push; answers how many moved.
UInt32 KickBladeKicks(const BladeKicks& kicks);

}  // namespace obvr::game

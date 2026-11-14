#include "game/CorpseTouch.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/ActorRosterLogic.h"
#include "game/Dismember.h"
#include "game/GameAddresses.h"
#include "vr/WalkersDisguise.h"

namespace obvr::game {
namespace {

// How far round the hands a body's head is looked for: past a lying body's
// length beyond the reach.
constexpr float kLookBeyondUnits = 140.0f;
constexpr UInt32 kBodiesMost = 16;

}  // namespace

void DeadBodiesUnderHands(const NiPoint3 hands[2], const bool valid[2], float reach, UInt32 out[2]) {
	out[0] = 0;
	out[1] = 0;
	if (!valid[0] && !valid[1]) {
		return;
	}
	const NiPoint3 around = valid[0] && valid[1] ? (hands[0] + hands[1]) * 0.5f : (valid[0] ? hands[0] : hands[1]);
	NeckOf bodies[kBodiesMost];
	const UInt32 n = CollectNecks(around, reach + kLookBeyondUnits, bodies, kBodiesMost, NeckParts::NeckOnly);
	static UInt32 s_lines = 40;
	for (UInt32 h = 0; h < 2; ++h) {
		if (!valid[h]) {
			continue;
		}
		float nearest = -1.0f;
		for (UInt32 i = 0; i < n; ++i) {
			if (!mem::LooksLikeObjectAddress(bodies[i].actor)) {
				continue;
			}
			if (!bodies[i].dead) {
				// A living one under the hand - knocked down, knocked out - is no
				// corpse (ActorRosterLogic.h LifeStateIsDeadBody).
				static UInt32 s_livingLines = 12;
				if (s_livingLines > 0 && vr::HandInBody(hands[h], bodies[i].low, bodies[i].high, reach)) {
					--s_livingLines;
					OBVR_LOG("Walkers disguise: the %s hand in %08X, who lives (life state %u) - no corpse",
					         h == 0 ? "left" : "right", *reinterpret_cast<const UInt32*>(bodies[i].actor + addr::kFormIdOffset),
					         *reinterpret_cast<const UInt32*>(bodies[i].actor + kActorLifeStateOffset));
				}
				continue;
			}
			const NiPoint3 down = bodies[i].low - (bodies[i].high - bodies[i].low);
			const float d = math::Sqrt(vr::SegmentDistanceSquared(hands[h], down, bodies[i].high));
			nearest = nearest < 0.0f || d < nearest ? d : nearest;
			if (vr::HandInBody(hands[h], bodies[i].low, bodies[i].high, reach)) {
				out[h] = *reinterpret_cast<const UInt32*>(bodies[i].actor + addr::kFormIdOffset);
				break;
			}
		}
		if (out[h] == 0 && nearest >= 0.0f && s_lines > 0) {
			--s_lines;
			OBVR_LOG("Walkers disguise: the %s hand %.0f units from the nearest dead body's trunk (%.0f counts)",
			         h == 0 ? "left" : "right", static_cast<double>(nearest), static_cast<double>(reach));
		}
	}
}

}  // namespace obvr::game

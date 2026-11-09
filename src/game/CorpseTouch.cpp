#include "game/CorpseTouch.h"

#include "core/AddressSpace.h"
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
	const UInt32 n = CollectNecks(around, reach + kLookBeyondUnits, bodies, kBodiesMost);
	for (UInt32 h = 0; h < 2; ++h) {
		if (!valid[h]) {
			continue;
		}
		for (UInt32 i = 0; i < n; ++i) {
			if (!bodies[i].dead || !mem::LooksLikeObjectAddress(bodies[i].actor) ||
			    !vr::HandInBody(hands[h], bodies[i].low, bodies[i].high, reach)) {
				continue;
			}
			out[h] = *reinterpret_cast<const UInt32*>(bodies[i].actor + addr::kFormIdOffset);
			break;
		}
	}
}

}  // namespace obvr::game

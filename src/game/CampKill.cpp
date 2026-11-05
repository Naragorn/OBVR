#include "game/CampKill.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ActorRoster.h"
#include "game/Dismember.h"
#include "game/GameAddresses.h"
#include "game/WalkersLink.h"

namespace obvr::game {
namespace {

// LookupFormByID (xOBSE obse/GameAPI.cpp: 0x0046B250, cdecl, the form or null).
constexpr UInt32 kLookupFormById = 0x0046B250;

UInt32 FormByRefId(UInt32 refId) {
	using LookupFn = UInt32(__cdecl*)(UInt32 id);
	const UInt32 form = reinterpret_cast<LookupFn>(kLookupFormById)(refId);
	return mem::LooksLikeObjectAddress(form) ? form : 0;
}

}  // namespace

void StepWalkersCampKills(bool enabled) {
	static UInt32 s_lines = 24;
	for (UInt32 refId = TakeWalkersCampKill(); refId != 0; refId = TakeWalkersCampKill()) {
		char why[96] = "";
		bool cut = false;
		const UInt32 actor = enabled ? FormByRefId(refId) : 0;
		bool dead = false;
		if (!enabled) {
			std::snprintf(why, sizeof(why), "dismemberment off");
		} else if (actor == 0) {
			std::snprintf(why, sizeof(why), "no such reference");
		} else if (!ActorInWorld(actor, dead)) {
			std::snprintf(why, sizeof(why), "no actor with its 3D in the world");
		} else if (!dead) {
			std::snprintf(why, sizeof(why), "not dead");
		} else if (HeadIsCut(actor)) {
			std::snprintf(why, sizeof(why), "its head is off already");
		} else {
			cut = CutHead(actor, NiPoint3{0.0f, 0.0f, 60.0f}, why, sizeof(why));
		}
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Walkers: 'WGCT' %08X - killed by a refugee, %s%s", refId, cut ? "its head cut" : "NOT cut: ",
			         cut ? "" : why);
		}
	}
}

}  // namespace obvr::game

#include "game/HoldClaims.h"

#include "core/Log.h"
#include "game/Parry.h"
#include "game/SlowApproach.h"

namespace obvr::game {
namespace {

HoldClaims& Claims() {
	static HoldClaims c;
	return c;
}

const char* OwnerName(HoldOwner o) { return o == HoldOwner::Grab ? "the grab" : "the blade"; }

void Apply(const HoldClaims& c) {
	const HoldEffect e = HoldEffectOf(c);
	HoldStill(e.still);
	SetHeldFoe(e.foe);
}

}  // namespace

bool ClaimHold(HoldOwner owner, UInt32 actor, bool blows) {
	HoldClaims& c = Claims();
	const bool taken = Claim(c, owner, actor, blows);
	if (!taken && actor != 0) {
		OBVR_LOG("Hold: %08X not held by %s - the grab holds %08X", actor, OwnerName(owner), c.grab.actor);
	}
	Apply(c);
	return taken;
}

void ReleaseHold(HoldOwner owner) {
	HoldClaims& c = Claims();
	const UInt32 had = ClaimOf(c, owner).actor;
	Release(c, owner);
	const HoldOwner other = owner == HoldOwner::Grab ? HoldOwner::Blade : HoldOwner::Grab;
	const HoldEffect e = HoldEffectOf(c);
	if (had != 0 && e.still == had) {
		OBVR_LOG("Hold: %08X let go by %s, still held - by %s%s", had, OwnerName(owner), OwnerName(other),
		         e.foe == had ? ", his blows stopped" : ", his blows land again");
	}
	Apply(c);
}

UInt32 HeldBy(HoldOwner owner) { return ClaimOf(Claims(), owner).actor; }

}  // namespace obvr::game

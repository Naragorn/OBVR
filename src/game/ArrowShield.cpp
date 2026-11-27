#include "game/ArrowShield.h"

#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/ArrowShieldLogic.h"
#include "game/GameAddresses.h"
#include "game/StuckArrow.h"

namespace obvr::game {
namespace {

// The block check the stub stands in front of (the hit handler's too).
constexpr UInt32 kBlockCheck = 0x005E5670;
// A reference's position (posX, posY, posZ), as the impact reads the
// target's at 0x0060AD31.
constexpr UInt32 kRefPositionOffset = 0x2C;

// The frame's guard (SetArrowShieldGuard), the verdicts kept, the frame's
// count, the log lines left.
struct ArrowShieldState {
	BlockGuard guard;
	ArrowVerdicts kept;
	UInt32 frame = 0;
	UInt32 lines = 24;
	UInt32 expiredLines = 6;
	bool installed = false;
};
ArrowShieldState& State() {
	static ArrowShieldState s;
	return s;
}

const char* AnswerName(ArrowAtShield a) {
	switch (a) {
	case ArrowAtShield::Blocked: return "blocked, the shield's impact";
	case ArrowAtShield::Through: return "THROUGH, the body's impact";
	case ArrowAtShield::AsRaised: return "not judged - the game's test, as raised";
	default: return "vanilla";
	}
}

ArrowShieldInput InputFor(UInt32 target, UInt32 projectile, const float* point) {
	ArrowShieldInput in;
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	in.targetIsPlayer = player != 0 && target == player;
	in.byContact = State().guard.shieldPose.byContact;
	in.raised = State().guard.shieldPose.raised;
	in.discValid = State().guard.shieldValid;
	in.disc.centre = State().guard.shieldCentre;
	in.disc.normal = State().guard.shieldNormal;
	in.disc.radius = State().guard.shieldRadius;
	in.disc.halfThick = State().guard.shieldHalfThick;
	in.pointValid = point != nullptr;
	if (in.pointValid) {
		in.point = NiPoint3{point[0], point[1], point[2]};
	}
	in.fromValid = mem::LooksLikeObjectAddress(projectile);
	if (in.fromValid) {
		const float* const pos = reinterpret_cast<const float*>(projectile + kRefPositionOffset);
		in.from = NiPoint3{pos[0], pos[1], pos[2]};
	}
	return in;
}

// Called by the stub at 0x0060AD01, inside the Havok contact callback: no
// allocation, a line only on a judged arrow, capped.
UInt32 __cdecl OnArrowAtTarget(UInt32 target, UInt32 projectile, const float* point) {
	if (!State().guard.shieldPose.byContact || !mem::LooksLikeObjectAddress(projectile) ||
	    *reinterpret_cast<const UInt32*>(projectile) != kArrowProjectileVtable) {
		return static_cast<UInt32>(ArrowAtShield::Vanilla);
	}
	const ArrowShieldVerdict v = ArrowAtShieldFor(InputFor(target, projectile, point));
	if (v.answer == ArrowAtShield::Blocked || v.answer == ArrowAtShield::Through) {
		State().kept.Add(projectile, v.answer == ArrowAtShield::Blocked, State().frame);
	}
	if (v.answer != ArrowAtShield::Vanilla && State().lines > 0) {
		--State().lines;
		OBVR_LOG("Block angle: an arrow %08X at the shield by contact - %s%s, its point %.0f units off the disc's "
		         "middle (radius %.0f), %.0f off its plane (half thickness %.0f) - %s",
		         projectile, v.why, v.bySegment ? " (by its flight)" : "", static_cast<double>(v.radialUnits),
		         static_cast<double>(State().guard.shieldRadius), static_cast<double>(v.alongUnits),
		         static_cast<double>(State().guard.shieldHalfThick), AnswerName(v.answer));
	}
	return static_cast<UInt32>(v.answer);
}

}  // namespace

void InstallArrowShield() {
	if (!mem::Verify(kArrowBlockCheckCall, kArrowBlockCheckCallBytes, sizeof(kArrowBlockCheckCallBytes))) {
		OBVR_LOG("Block: the arrow impact's block check at %08X is not `call 005E5670` - left alone; an arrow at "
		         "the shield by contact shows the body's impact",
		         kArrowBlockCheckCall);
		mem::ReportForeignCode("Block", kArrowBlockCheckCall);
		return;
	}
	auto* const stub = static_cast<UInt8*>(mem::AllocExecutable(64));
	const UInt32 base = reinterpret_cast<UInt32>(stub);
	if (stub == nullptr ||
	    BuildArrowShieldStub(stub, 64, base, reinterpret_cast<UInt32>(&OnArrowAtTarget), kBlockCheck) !=
	        kArrowShieldStubSize) {
		OBVR_LOG("Block: no stub for the arrow impact's block check - arrows at the shield show the body's impact");
		return;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), kArrowBlockCheckCall, base) != sizeof(patch) ||
	    !mem::SafeWrite(kArrowBlockCheckCall, patch, sizeof(patch))) {
		OBVR_LOG("Block: could not reroute the arrow impact's block check at %08X", kArrowBlockCheckCall);
		return;
	}
	State().installed = true;
	OBVR_LOG("Block: the arrow impact's block check (0060AD01) goes through a stub - with the shield by contact an "
	         "arrow is judged against the disc");
}

void SetArrowShieldGuard(const BlockGuard& guard) {
	State().guard = guard;
	++State().frame;
	const UInt32 dropped = State().kept.Expire(State().frame);
	if (dropped > 0 && State().expiredLines > 0) {
		--State().expiredLines;
		OBVR_LOG("Block angle: %u arrow verdict(s) not taken by the shield's cone within %u frames (the hit "
		         "handler did not ask with that arrow, or the engine blocked by itself)",
		         dropped, kArrowVerdictFrames);
	}
}

KeptArrow TakeArrowVerdict(UInt32 attacker) {
	bool blocked = false;
	if (!State().installed || !State().kept.Take(attacker, blocked)) {
		return KeptArrow::None;
	}
	return blocked ? KeptArrow::Blocked : KeptArrow::Through;
}

}  // namespace obvr::game

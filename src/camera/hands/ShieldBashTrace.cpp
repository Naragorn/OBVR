#include "camera/hands/ShieldBashTrace.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/BashApproachLogic.h"
#include "game/BashContactLogic.h"
#include "game/MeleeHits.h"
#include "game/ShieldBashLogic.h"
#include "game/ShieldBashWatch.h"
#include "game/ShoveLogic.h"

namespace obvr::camera::hands {
namespace {

// How far from someone's bound the shield is still near them for the watch.
constexpr float kNearPad = 200.0f;

// The frame as the bash saw it against the one the shield met, else the
// nearest one near.
game::BashAttemptFrame FrameOf(const ShieldBashLook& look, float speed, float controllerSpeed) {
	game::BashAttemptFrame f;
	f.speed = speed;
	f.controllerSpeed = controllerSpeed;
	f.landed = look.landed;
	const BashTouch& t = look.touch;
	const NiPoint3& at = t.disc.centre;
	NiPoint3 centre = t.centre;
	game::BashColumn column = t.column;
	void* actor = t.actor;
	if (actor == nullptr) {
		float radius = 0.0f;
		actor = game::LivingActorAt(at, 1.0f, kNearPad, &centre, -1.0f, &radius);
		column = game::BashColumnOf(centre, radius);
	}
	if (actor == nullptr) {
		return f;
	}
	f.near = true;
	f.actor = reinterpret_cast<UInt32>(actor);
	f.inColumn = t.actor != nullptr;
	f.byContact = f.inColumn && t.byContact;
	f.cooling = f.inColumn && look.cooling;
	f.handBusy = look.handBusy;
	// The bash's speed is the controller's approach into their body (StepShieldBash); this frame's own speed towards
	// their middle, across the ground, and its angle beside it, for the comparison.
	f.approach = f.inColumn ? t.approach : ShieldBashApproachInto(column);
	f.gap = f.inColumn ? t.gap : game::DiscColumnGap(t.disc, column).gap;
	f.towards = game::BashSpeedTowards(look.controllerVelocity, at, centre);
	f.towardsGround = game::SpeedTowards(look.controllerVelocity, at, centre);
	f.degrees = game::TravelDegreesOffMiddle(look.controllerVelocity, at, centre);
	const NiPoint3 off = at - centre;
	f.across = math::Sqrt(off.x * off.x + off.y * off.y);
	f.up = off.z;
	f.columnWide = column.wide;
	f.columnHigh = column.high;
	return f;
}

const char* MetWords(const game::BashAttemptFrame& b) {
	return !b.inColumn ? "not" : b.byContact ? "by the contact step" : "by its disc";
}

}  // namespace

void TraceShieldBash(const ShieldBashLook& look) {
	static game::BashAttemptState s_attempt;
	static UInt32 s_lines = 60;
	const float speed = math::Sqrt(look.velocity.LengthSquared());
	const float controllerSpeed = math::Sqrt(look.controllerVelocity.LengthSquared());
	if (!s_attempt.open && !look.landed && speed < game::kBashAttemptSpeed &&
	    controllerSpeed < game::kBashAttemptSpeed) {
		return;  // a slow shield and no attempt open: nothing read
	}
	const game::BashAttemptSummary r = game::StepBashAttempt(s_attempt, FrameOf(look, speed, controllerSpeed));
	if (!r.ended || s_lines == 0) {
		return;
	}
	--s_lines;
	const game::BashAttemptFrame& b = r.best;
	OBVR_LOG("Shield bash attempt: %s - %u frames, the shield up to %.1f m/s (its controller %.1f); closest at %08X: "
	         "the approach %.1f m/s into their body over the last %.2f s, judged (the bash wants %.1f); the shield met "
	         "them %s - its disc %.0f units from their column; in that frame alone the controller %.1f m/s towards "
	         "their middle (%.1f across the ground alone), %.0f degrees off the line to it, %.0f units across and %.0f "
	         "up from their middle (the column %.0f wide, %.0f high)",
	         game::BashAttemptEndWords(r.end), r.frames, static_cast<double>(r.mostSpeed),
	         static_cast<double>(r.mostControllerSpeed), b.actor, static_cast<double>(b.approach),
	         static_cast<double>(game::kBashApproachSeconds), static_cast<double>(look.gateSpeed), MetWords(b),
	         static_cast<double>(b.gap), static_cast<double>(b.towards), static_cast<double>(b.towardsGround),
	         static_cast<double>(b.degrees), static_cast<double>(b.across), static_cast<double>(b.up),
	         static_cast<double>(b.columnWide), static_cast<double>(b.columnHigh));
}

}  // namespace obvr::camera::hands

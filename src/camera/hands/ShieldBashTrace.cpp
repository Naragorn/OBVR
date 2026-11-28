#include "camera/hands/ShieldBashTrace.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/MeleeHits.h"
#include "game/ShieldBashLogic.h"
#include "game/ShieldBashWatch.h"
#include "game/ShoveLogic.h"

namespace obvr::camera::hands {
namespace {

// The column StepShieldBash asks for (game::LivingActorAt(at, 0.5, 8, ..., 1)):
// half the bound's radius plus 8 across, the radius plus 8 above and below.
constexpr float kColumnSide = 0.5f;
constexpr float kColumnPad = 8.0f;
// How far from someone's bound the shield is still near them for the watch.
constexpr float kNearPad = 200.0f;

// The frame as the bash saw it against the one in the column, else the
// nearest one near.
game::BashAttemptFrame FrameOf(const ShieldBashLook& look, float speed, float controllerSpeed) {
	game::BashAttemptFrame f;
	f.speed = speed;
	f.controllerSpeed = controllerSpeed;
	f.landed = look.landed;
	NiPoint3 centre = look.centre;
	float radius = 0.0f;
	void* actor = look.actor;
	if (actor != nullptr) {
		radius = game::ActorBoundRadius(actor);
	} else {
		actor = game::LivingActorAt(look.at, 1.0f, kNearPad, &centre, -1.0f, &radius);
	}
	if (actor == nullptr) {
		return f;
	}
	f.near = true;
	f.actor = reinterpret_cast<UInt32>(actor);
	f.inColumn = look.actor != nullptr;
	f.cooling = f.inColumn && look.cooling;
	f.handBusy = look.handBusy;
	// The bash's speed is the controller's (StepShieldBash): judged in 3D, and across the ground for the comparison.
	f.towards = f.inColumn ? look.towards : game::BashSpeedTowards(look.controllerVelocity, look.at, centre);
	f.towardsGround = game::SpeedTowards(look.controllerVelocity, look.at, centre);
	const NiPoint3 off = look.at - centre;
	f.across = math::Sqrt(off.x * off.x + off.y * off.y);
	f.up = off.z;
	f.columnWide = radius * kColumnSide + kColumnPad;
	f.columnHigh = radius + kColumnPad;
	return f;
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
	         "the controller %.1f m/s towards their middle, judged (%.1f across the ground alone; the bash wants %.1f), "
	         "%.0f units across and %.0f up from their middle (the column %.0f wide, %.0f high)",
	         game::BashAttemptEndWords(r.end), r.frames, static_cast<double>(r.mostSpeed),
	         static_cast<double>(r.mostControllerSpeed), b.actor, static_cast<double>(b.towards),
	         static_cast<double>(b.towardsGround), static_cast<double>(look.gateSpeed), static_cast<double>(b.across),
	         static_cast<double>(b.up), static_cast<double>(b.columnWide), static_cast<double>(b.columnHigh));
}

}  // namespace obvr::camera::hands

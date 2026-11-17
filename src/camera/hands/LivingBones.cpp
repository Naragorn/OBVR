#include "camera/hands/LivingBones.h"

#include "core/Config.h"
#include "game/BladeContactLogic.h"
#include "game/BodyGive.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/WalkerGrip.h"
#include "game/WalkersLink.h"
#include "test/HandScriptRuntime.h"
#include "vr/Holster.h"

namespace obvr::camera::hands {
namespace {

// The controllers in the world for Walkers' grip (game/WalkerGrip.h): the
// cyclopean camera times each controller's offset and rotation.
game::HeldHands HandsForWalkers(const LivingLinks& links) {
	game::HeldHands hands;
	if (!links.cameraValid) {
		return hands;
	}
	const NiMatrix33& camRot = links.camera.rot;
	const NiPoint3& camPos = links.camera.pos;
	for (int at = 0; at < 2; ++at) {
		const bool right = at == 1;
		hands.valid[at] = right ? links.hand.rightHandValid : links.hand.leftHandValid;
		const NiMatrix33 rot = camRot * (right ? links.hand.rightHandRotation : links.hand.leftHandRotation);
		hands.pos[at] = camPos + camRot * (right ? links.hand.rightHandOffsetUnits : links.hand.leftHandOffsetUnits);
		hands.forward[at] = NiPoint3{rot.data[0][1], rot.data[1][1], rot.data[2][1]};
	}
	return hands;
}

// A swung weapon is out: drawn, and of a kind that is swung.
bool MeleeDrawn() {
	SInt32 type = -1;
	if (game::ReadPlayerWeaponState() != game::WeaponState::Drawn || game::EquippedWeaponForm(&type) == nullptr) {
		return false;
	}
	const vr::EquippedKind kind = vr::KindOfWeaponType(type);
	return kind == vr::EquippedKind::OneHand || kind == vr::EquippedKind::TwoHand;
}

// A hand's or the blade's span at a pose, in the world; a span not read is
// the pose's own place.
void SpanEnds(const game::BladePose& pose, const game::BodySpan& span, NiPoint3& a, NiPoint3& b) {
	a = span.valid ? game::BladePointAt(pose, span, 0.0f) : pose.pos;
	b = span.valid ? game::BladePointAt(pose, span, 1.0f) : pose.pos;
}

// The pushers of [Hands] BodiesGive: each hand where its controller is (the
// place contact wanted for it, with the rotation it was held at), and the
// blade on the controller's own pose - not the drawn ones, which contact
// holds at a person's surface and which so never press into them.
void StepGiving(const LivingLinks& links) {
	static game::GivePusherMemory s_memory[game::kGivePushers];
	const Config& config = GetConfig();
	game::GiveFrame f;
	f.enabled = config.bodiesGive && config.fullVrMode && links.cameraValid && links.inWorld;
	f.dtSeconds = links.dtSeconds;
	f.mark = test::HandScriptMarkedThisFrame();
	if (!f.enabled) {
		for (game::GivePusherMemory& m : s_memory) {
			m = game::GivePusherMemory{};
		}
		game::StepBodyGive(f);
		return;
	}
	const NiMatrix33& camRot = links.camera.rot;
	f.around = links.camera.pos;
	const bool handMeets[2] = {links.handShape[0] != game::HandShape::None, links.handShape[1] != game::HandShape::None};
	const game::GivePushersNow now =
		game::PushersNow(links.hand.rightHandValid, links.hand.leftHandValid, handMeets[0], handMeets[1],
		                 !handMeets[0] && MeleeDrawn(), links.bladeSpan.valid);
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	for (UInt32 side = 0; side < 2; ++side) {
		game::BladePose pose;
		pose.rot = camRot * (side == 0 ? links.hand.weaponHandRotation : links.hand.leftHandRotation);
		pose.pos = links.handWanted[side];
		SpanEnds(pose, links.handSpan[side], a, b);
		f.pusher[side] = game::PusherFrom(s_memory[side], now.hand[side], a, b, game::kGiveHandRadius, f.dtSeconds);
	}
	game::BladePose blade;
	blade.rot = camRot * links.hand.rightHandRotation;
	blade.pos = links.camera.pos + camRot * links.hand.rightHandOffsetUnits;
	SpanEnds(blade, links.bladeSpan, a, b);
	f.pusher[game::kGivePusherBlade] =
		game::PusherFrom(s_memory[game::kGivePusherBlade], now.blade, a, b, game::kGiveBladeRadius, f.dtSeconds);
	game::StepBodyGive(f);
}

}  // namespace

void StepLivingBones(const LivingLinks& links) {
	game::StepWalkerGrips(game::ReadWalkersNow(), links.dtSeconds, GetConfig().hands.walkerGrip, HandsForWalkers(links));
	StepGiving(links);
}

}  // namespace obvr::camera::hands

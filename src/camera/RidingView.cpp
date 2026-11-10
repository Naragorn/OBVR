#include "camera/RidingView.h"

#include "camera/RidingBase.h"
#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/PlayerTeleport.h"

namespace obvr::camera {

// On a horse, in first person with the headset: the base of the eyes
// held in the horse's frame (camera/RidingBase.h) - the engine's camera
// on the horse's spine rides the walk and gallop animation, and a headset
// takes that bob as the floor moving under a still body (Nadi,
// 2026-10-19: "the camera moves too much with the horse's motion"). The
// settled offset follows the saddle, the terrain and the horse's turn;
// [Look] HorseBob says how much of the cadence is laid back on top.
void ApplyRidingView(NiAVObject* cameraNode, bool firstPersonWithHeadset, float deltaSeconds, bool& traceWrote,
                     UInt32 traceFrame) {
	static RidingBaseState s_ridingBase;
	static UInt32 s_ridingBaseLines = 9000;
	const UInt32 horse = game::PlayerMountedHorse();
	float horseYaw = 0.0f;
	NiPoint3 horseAt{0.0f, 0.0f, 0.0f};
	if (horse != 0 && firstPersonWithHeadset &&
	    game::ReadActorPlacement(horse, horseYaw, horseAt)) {
		const NiPoint3 engineCamera = cameraNode->localTransform.pos;
		// The frame: the horse's scene node's rotation - its heading and,
		// on a slope, the pitch the engine gives the body with the ground
		// (horse-ride-slope.txt, 2026-10-23); the heading alone when the
		// node cannot be read.
		NiMatrix33 horseFrame = RidingFrameFromHeading(horseYaw);
		float horsePitch = 0.0f;
		float horseRoll = 0.0f;
		// The seat: the rider's own pelvis - "Bip01 Pelvis" of the
		// first-person skeleton the engine hangs under the horse's
		// ActorParent bone (game/FirstPersonArmsLogic.h), so the rider's
		// hips as the engine seats them - in the node's frame; its world
		// place and the node's are of the same update, so the frame they are
		// a frame stale by cancels. The ActorParent bone itself stands 55
		// units over the node, inside the horse's body (2026-11-10), too low
		// to stand upright over; it is logged beside the seat.
		NiPoint3 saddleInNode{0.0f, 0.0f, 0.0f};
		NiPoint3 actorParentInNode{0.0f, 0.0f, 0.0f};
		bool saddleRead = false;
		bool actorParentRead = false;
		const UInt32 ridingNode = *reinterpret_cast<const UInt32*>(horse + addr::kRefNiNodeOffset);
		if (mem::LooksLikeObjectAddress(ridingNode)) {
			const NiAVObject* const node = reinterpret_cast<const NiAVObject*>(ridingNode);
			horseFrame = node->worldTransform.rot;
			const NiPoint3 fwd = horseFrame * NiPoint3{0.0f, 1.0f, 0.0f};
			const NiPoint3 rgt = horseFrame * NiPoint3{1.0f, 0.0f, 0.0f};
			horsePitch = math::Atan2(fwd.z, math::Sqrt(fwd.x * fwd.x + fwd.y * fwd.y));
			horseRoll = math::Atan2(rgt.z, math::Sqrt(rgt.x * rgt.x + rgt.y * rgt.y));
			const char* const names[2] = {"Bip01 Pelvis", "ActorParent"};
			NiAVObject* found[2] = {nullptr, nullptr};
			game::FindNodesNamed(const_cast<NiAVObject*>(node), names, 2, found);
			const NiMatrix33 toNode = InverseMatrix(horseFrame);
			if (found[0] != nullptr) {
				saddleInNode = toNode * (found[0]->worldTransform.pos - node->worldTransform.pos);
				saddleRead = true;
			}
			if (found[1] != nullptr) {
				actorParentInNode = toNode * (found[1]->worldTransform.pos - node->worldTransform.pos);
				actorParentRead = true;
			}
		}
		// Always upright over the seat when it is read (decided 2026-11-10: no
		// switch for it).
		const NiPoint3 base = StepRidingBase(s_ridingBase, horse, engineCamera, horseAt, horseFrame,
		                                     saddleRead ? &saddleInNode : nullptr, deltaSeconds,
		                                     GetConfig().look.horseBob);
		cameraNode->localTransform.pos = base;
		// Written on the frames the riding trace wrote on (the horse moving,
		// Debug.HorseTurnProbe), so the two join by frame: the first 900
		// frames of flags alone were spent before the ride (10:13).
		if (traceWrote && s_ridingBaseLines > 0) {
			--s_ridingBaseLines;
			traceWrote = false;
			// The eyes over the saddle in the heading's frame (right,
			// forward, up): upright, the same on every incline as on the
			// level (tools/riding-inclines.py reads it per incline). The bone
			// as read this frame.
			const NiPoint3 seat = saddleRead ? saddleInNode : s_ridingBase.saddle;
			const NiPoint3 saddleWorld = horseAt + horseFrame * seat;
			float overF = 0.0f;
			float overR = 0.0f;
			float overU = 0.0f;
			RidingOffsetInHorseFrame(base, saddleWorld, horseYaw, overF, overR, overU);
			OBVR_LOG("Camera: riding base frame %u - the base %.1f %.1f %.1f, the engine's camera %.1f %.1f %.1f, "
			         "the saddle forward %.1f right %.1f up %.1f, bob %.2f, the horse at %.1f %.1f %.1f heading %.4f, "
			         "the node's pitch %.1f deg, roll %.1f deg, the seat %s right %.1f forward %.1f up %.1f, upright "
			         "%.2f, the eyes over the seat right %.1f forward %.1f up %.1f; ActorParent %s right %.1f forward %.1f "
			         "up %.1f",
			         traceFrame, static_cast<double>(base.x), static_cast<double>(base.y),
			         static_cast<double>(base.z), static_cast<double>(engineCamera.x),
			         static_cast<double>(engineCamera.y), static_cast<double>(engineCamera.z),
			         static_cast<double>(s_ridingBase.forward), static_cast<double>(s_ridingBase.right),
			         static_cast<double>(s_ridingBase.up), static_cast<double>(GetConfig().look.horseBob),
			         static_cast<double>(horseAt.x), static_cast<double>(horseAt.y), static_cast<double>(horseAt.z),
			         static_cast<double>(horseYaw), static_cast<double>(horsePitch * math::kRadiansToDegrees),
			         static_cast<double>(horseRoll * math::kRadiansToDegrees), saddleRead ? "read" : "not read",
			         static_cast<double>(seat.x), static_cast<double>(seat.y), static_cast<double>(seat.z),
			         static_cast<double>(s_ridingBase.upright),
			         static_cast<double>(overR), static_cast<double>(overF), static_cast<double>(overU),
			         actorParentRead ? "read" : "not read", static_cast<double>(actorParentInNode.x),
			         static_cast<double>(actorParentInNode.y), static_cast<double>(actorParentInNode.z));
		}
	} else if (s_ridingBase.valid) {
		ResetRidingBase(s_ridingBase);
	}
}

}  // namespace obvr::camera

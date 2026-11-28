#include "camera/hands/HandLagTrace.h"

#include <cstring>

#include "core/Config.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonHide.h"
#include "game/HandBodies.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {
namespace {

// Where the controller puts a hand's bone: the pin's own sum (game::HandBoneWorld)
// with that hand's calibration and grip, as the pin takes them.
NiPoint3 BoneTarget(bool right, const vr::HandSettings& hands, const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                    const NiMatrix33& relativeRot, const NiPoint3& offsetUnits, const NiPoint3& sharedGripMetres,
                    float perMetre, bool mirrored) {
	const NiMatrix33 calibration = game::MirroredCalibration(
		right ? game::HandCalibration(hands.rightHandRoll, hands.rightHandPitch, hands.rightHandYaw)
		      : game::HandCalibration(hands.leftHandRoll, hands.leftHandPitch, hands.leftHandYaw),
		mirrored);
	const NiPoint3 own = right ? NiPoint3{hands.rightHandGripX, hands.rightHandGripY, hands.rightHandGripZ}
	                           : NiPoint3{hands.leftHandGripX, hands.leftHandGripY, hands.leftHandGripZ};
	const NiPoint3 grip = game::MirroredGrip((sharedGripMetres + own) * perMetre, mirrored);
	return game::HandBoneWorld(cameraRot, cameraPos, relativeRot, offsetUnits, calibration, grip).pos;
}

NiPoint3 NodeAt(const char* name) {
	const NiAVObject* const node = game::FindFirstPersonNode(name);
	return node != nullptr ? node->worldTransform.pos : NiPoint3{0.0f, 0.0f, 0.0f};
}

}  // namespace

void TraceHandLag(const vr::HandModeResult& hand, const vr::HandSettings& hands, const NiMatrix33& cameraRot,
                  const NiPoint3& cameraPos, const NiPoint3& sharedGripMetres, float perMetre, float dtSeconds,
                  bool mirrored) {
	static UInt32 s_left = 0;
	static UInt32 s_frame = 0;
	static char s_mark[48] = "";
	const UInt32 frames = GetConfig().handLagTraceFrames;
	if (frames > 0 && test::HandScriptMarkedThisFrame()) {
		s_left = frames;
		s_frame = 0;
		strncpy_s(s_mark, test::HandScriptMarkName(), _TRUNCATE);
	}
	if (s_left == 0) {
		return;
	}
	--s_left;
	const NiPoint3 rc = BoneTarget(true, hands, cameraRot, cameraPos, hand.rightHandRotation, hand.rightHandOffsetUnits,
	                               sharedGripMetres, perMetre, mirrored);
	const NiPoint3 rp = BoneTarget(true, hands, cameraRot, cameraPos, hand.weaponHandRotation,
	                               hand.weaponHandOffsetUnits, sharedGripMetres, perMetre, mirrored);
	const NiPoint3 lc = BoneTarget(false, hands, cameraRot, cameraPos, hand.leftHandRotation, hand.leftHandOffsetUnits,
	                               sharedGripMetres, perMetre, mirrored);
	// The off hand as drawn (vr/HeldItemWeight.h): behind lc by its shield, bow or torch.
	const NiPoint3 lp = BoneTarget(false, hands, cameraRot, cameraPos, hand.offHandRotation, hand.offHandOffsetUnits,
	                               sharedGripMetres, perMetre, mirrored);
	const NiPoint3 rb = NodeAt(hands.rightHandBone);
	const NiPoint3 lb = NodeAt(hands.leftHandBone);
	const NiPoint3 w = NodeAt("Weapon");
	const float stepSeconds = game::HandBodyPhysicsStepSeconds();
	OBVR_LOG("Lag trace: %s %u dt %.2f steps %u %.2f | R %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f | L %.2f %.2f "
	         "%.2f %.2f %.2f %.2f | W %.2f %.2f %.2f | lag %.4f %.2f | O %.2f %.2f %.2f | off lag %.4f %.2f",
	         s_mark, s_frame++, static_cast<double>(dtSeconds * 1000.0f), game::HandBodyPhysicsSteps(),
	         static_cast<double>(stepSeconds * 1000.0f), static_cast<double>(rc.x), static_cast<double>(rc.y),
	         static_cast<double>(rc.z), static_cast<double>(rp.x), static_cast<double>(rp.y), static_cast<double>(rp.z),
	         static_cast<double>(rb.x), static_cast<double>(rb.y), static_cast<double>(rb.z), static_cast<double>(lc.x),
	         static_cast<double>(lc.y), static_cast<double>(lc.z), static_cast<double>(lb.x), static_cast<double>(lb.y),
	         static_cast<double>(lb.z), static_cast<double>(w.x), static_cast<double>(w.y), static_cast<double>(w.z),
	         static_cast<double>(hand.weaponGapMetres), static_cast<double>(hand.weaponGapDegrees),
	         static_cast<double>(lp.x), static_cast<double>(lp.y), static_cast<double>(lp.z),
	         static_cast<double>(hand.offHandGapMetres), static_cast<double>(hand.offHandGapDegrees));
}

}  // namespace obvr::camera::hands

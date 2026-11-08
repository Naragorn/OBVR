#include "camera/MarkEyes.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"

namespace obvr::camera {

void LogEyesAtMark(const NiTransform& eyes) {
	const NiPoint3 forward = ForwardOf(eyes.rot);
	game::PlayerRotation rotation{};
	const bool haveRotation = game::ReadPlayerRotation(rotation);
	OBVR_LOG("Camera: at the mark - the eyes at %.1f %.1f %.1f look along %.3f %.3f %.3f (pitch %.1f degrees, "
	         "positive up, heading %.1f); the player's rotX %.4f rotZ %.4f%s",
	         static_cast<double>(eyes.pos.x), static_cast<double>(eyes.pos.y), static_cast<double>(eyes.pos.z),
	         static_cast<double>(forward.x), static_cast<double>(forward.y), static_cast<double>(forward.z),
	         static_cast<double>(math::Asin(forward.z) * math::kRadiansToDegrees),
	         static_cast<double>(math::Atan2(forward.x, forward.y) * math::kRadiansToDegrees),
	         static_cast<double>(rotation.pitch), static_cast<double>(rotation.yaw), haveRotation ? "" : " (unread)");
	// The body's head: the centre of the player's face (its FaceGen node's
	// world bound - the node itself sits at the body's root).
	NiPoint3 face{0.0f, 0.0f, 0.0f};
	if (!game::PlayerFaceCentre(face)) {
		OBVR_LOG("Camera: at the mark - the body's head not read");
		return;
	}
	const NiPoint3 d = face - eyes.pos;
	OBVR_LOG("Camera: at the mark - the body's head %.1f units level from the eyes (%.1f %.1f), %.1f up",
	         static_cast<double>(math::Sqrt(d.x * d.x + d.y * d.y)), static_cast<double>(d.x),
	         static_cast<double>(d.y), static_cast<double>(d.z));
}

}  // namespace obvr::camera

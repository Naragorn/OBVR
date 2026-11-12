#include "camera/BodyYawHandOver.h"

#include "game/PlayerAim.h"

namespace obvr::camera {

bool WriteBodyYawKeepingView(float wantedYaw, bool keepView, BodyYawBook& book) {
	game::PlayerRotation now{};
	if (!keepView || !game::ReadPlayerRotation(now)) {
		return game::WritePlayerYaw(wantedYaw);
	}
	const float step = HandOverStep(now.yaw, wantedYaw);
	if (!game::WritePlayerYaw(wantedYaw)) {
		return false;
	}
	BookHandOver(book, wantedYaw, step);
	return true;
}

}  // namespace obvr::camera

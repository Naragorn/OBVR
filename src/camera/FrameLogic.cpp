#include "camera/FrameLogic.h"

namespace obvr::camera {

bool KeyEdge::Update(bool isDown) {
	const bool isEdge = isDown && !m_wasDown;
	m_wasDown = isDown;
	return isEdge;
}

void KeyEdge::Reset() { m_wasDown = false; }

PovEvent State::ObservePointOfView(bool thirdPerson) {
	if (!sawCameraNode) {
		sawCameraNode = true;
		isThirdPerson = thirdPerson;
		return PovEvent::FirstPass;
	}

	if (thirdPerson == isThirdPerson) {
		return PovEvent::Unchanged;
	}

	isThirdPerson = thirdPerson;
	return PovEvent::Switched;
}

bool IsDue(UInt32 frameCount, UInt32 interval) {
	if (interval == 0) {
		return false;
	}
	return (frameCount % interval) == 0;
}

}  // namespace obvr::camera

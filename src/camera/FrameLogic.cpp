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

float FrameClock::Tick(long long nowTicks, long long ticksPerSecond) {
	const long long lastTicks = m_lastTicks;
	const bool hadLast = m_hasLast;

	m_lastTicks = nowTicks;
	m_hasLast = true;

	if (!hadLast || ticksPerSecond <= 0) {
		return 0.0f;
	}

	const long long elapsed = nowTicks - lastTicks;
	if (elapsed <= 0) {
		// The counter is monotonic, so this means the caller handed in a
		// stale reading. Reporting a negative frame time would run the
		// smoothing backwards.
		return 0.0f;
	}

	const float seconds =
		static_cast<float>(static_cast<double>(elapsed) / static_cast<double>(ticksPerSecond));

	return seconds > kMaxDeltaSeconds ? kMaxDeltaSeconds : seconds;
}

void FrameClock::Reset() {
	m_lastTicks = 0;
	m_hasLast = false;
}

bool IsDue(UInt32 frameCount, UInt32 interval) {
	if (interval == 0) {
		return false;
	}
	return (frameCount % interval) == 0;
}

bool IsLeftEyeFrame(UInt32 frameCount) {
	// Even frames to the left eye. Which one goes first does not matter, only
	// that the two places asking get the same answer for the same frame.
	return (frameCount & 1u) == 0u;
}

bool BackBufferEyeIsLeft(bool isLeftEye, bool backBufferIsThisFrame) {
	return backBufferIsThisFrame ? isLeftEye : !isLeftEye;
}

float ScaledEyeHalfSeparation(float halfUnits, float scale) {
	// Written as a negated comparison on purpose: NaN fails every comparison,
	// so this one line rejects NaN, zero and negatives together.
	if (!(scale > 0.0f)) {
		return halfUnits;
	}
	if (scale < kMinEyeSeparationScale) {
		scale = kMinEyeSeparationScale;
	}
	if (scale > kMaxEyeSeparationScale) {
		scale = kMaxEyeSeparationScale;
	}
	return halfUnits * scale;
}

FrameDelivery DeliverFrame(bool hadCameraPass, bool menuIsUp, bool menusInWorld,
                           bool haveHeldEyes, bool heldLastFrame) {
	// Nothing drew a world, so there is no viewpoint to claim and no stereo
	// pair to hold. Videos, loading screens and the main menu land here, and
	// they land here whatever menusInWorld says.
	if (!hadCameraPass && !menuIsUp) {
		// Except for the one frame that closes a menu, where the menu is gone
		// and the world has not come back yet. Screening that frame is a flash
		// of the flat picture in the middle of an otherwise smooth close - and
		// it fills the eye copies with the letterboxed layout, which is where
		// the next menu's black bars came from. See the header.
		if (heldLastFrame && haveHeldEyes) {
			return FrameDelivery::HeldStereo;
		}
		return FrameDelivery::Cinema;
	}

	if (!menuIsUp) {
		return FrameDelivery::Stereo;
	}

	if (!menusInWorld) {
		return FrameDelivery::Cinema;
	}

	// A menu in the world, on a frame where Oblivion redrew what is behind it.
	if (hadCameraPass) {
		return FrameDelivery::Stereo;
	}

	// A menu in the world on a frame where it did not. Holding is the whole
	// point: switching to the cinema screen here is the flicker.
	//
	// Unless there is nothing to hold - the main menu, or a menu opened before
	// the first pair was ever captured. Then the screen is the only picture
	// available, and a picture beats the test pattern.
	return haveHeldEyes ? FrameDelivery::HeldStereo : FrameDelivery::Cinema;
}

bool MenusCanReachTheWorld(bool menusInWorld, bool hudOverlay) {
	return menusInWorld && hudOverlay;
}

bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, bool menusInWorld) {
	return frameOpen && armed && (!menuIsUp || menusInWorld);
}

bool WantsHudRedirect(FrameDelivery delivery) {
	// One question, asked once. The layer may be taken out of the frame
	// exactly when the frame is not the thing being shown - and the cinema
	// screen shows the frame, so on those the layer has to stay in it.
	//
	// This used to be its own three-argument decision, and the two drifted the
	// moment the redirect learned that menu frames carry no camera pass: the
	// main menu has no captured pair to hold, so it falls back to the cinema
	// screen, while the redirect went on taking its 2D layer away. The menu was
	// then in neither place, and the game looked hung at the title screen.
	//
	// Derived from the delivery instead, that cannot happen again: there is no
	// second rule left to disagree with the first.
	return delivery != FrameDelivery::Cinema;
}

UInt32 SweepProbeStage(UInt32 sceneCall, UInt32 configured) {
	if (configured != kProbeSweep) {
		return configured;
	}
	if (sceneCall < kSweepFirstBand) {
		return kProbeSinglePass;
	}
	if (sceneCall < kSweepSecondBand) {
		return 1;
	}
	if (sceneCall < kSweepThirdBand) {
		return kProbeSinglePass;
	}
	return 0;
}

bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, bool menusInWorld,
                          UInt32 probeRung) {
	return probeRung != kProbeSinglePass &&
	       WantsSecondScenePass(frameOpen, armed, menuIsUp, menusInWorld);
}

bool DeliversDualEyes(bool stereoDual, bool sceneHooked, UInt32 probeRung) {
	return stereoDual && sceneHooked && probeRung != kProbeSinglePass;
}

}  // namespace obvr::camera

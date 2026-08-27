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

bool FrameIsFlat(bool hadCameraPass, bool menuIsUp) { return !hadCameraPass || menuIsUp; }

bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp) {
	return frameOpen && armed && !menuIsUp;
}

bool WantsHudRedirect(bool frameOpen, bool menuIsUp) { return frameOpen && !menuIsUp; }

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

bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, UInt32 probeRung) {
	return probeRung != kProbeSinglePass && WantsSecondScenePass(frameOpen, armed, menuIsUp);
}

bool DeliversDualEyes(bool stereoDual, bool sceneHooked, UInt32 probeRung) {
	return stereoDual && sceneHooked && probeRung != kProbeSinglePass;
}

}  // namespace obvr::camera

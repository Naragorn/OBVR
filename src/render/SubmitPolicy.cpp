#include "render/SubmitPolicy.h"

#include "vr/OpenVRTypes.h"

namespace obvr::render {

bool IsRecoverable(int compositorError) {
	switch (compositorError) {
		case obvr::vr::openvr::kCompositorErrorNone:
			return true;

		// Another application has the scene. It may give it back, and until
		// then WaitGetPoses is throttled to 10 Hz rather than broken - so
		// this is worth waiting out, but only for a while.
		case obvr::vr::openvr::kCompositorErrorDoNotHaveFocus:
			return true;

		// OBVR registered as a background application and is trying to
		// submit anyway. No amount of waiting changes that; the application
		// type is fixed at registration. This is a bug in OBVR rather than a
		// condition of the machine, and it should stop loudly.
		case obvr::vr::openvr::kCompositorErrorIsNotSceneApplication:
			return false;

		// The texture came from a device the compositor is not using, or in
		// a format it will not take. Both are properties of how the texture
		// was created, so the next frame's will be identical.
		case obvr::vr::openvr::kCompositorErrorTextureIsOnWrongDevice:
		case obvr::vr::openvr::kCompositorErrorTextureUsesUnsupportedFormat:
			return false;

		default:
			// Unknown codes are treated as recoverable, which is the
			// forgiving reading, and the failure counter stops it becoming an
			// endless one. Guessing that an unfamiliar code is fatal would
			// mean a future SteamVR release could switch OBVR off over
			// something transient.
			return true;
	}
}

SubmitDecision SubmitPolicy::Observe(int compositorError) {
	if (m_stopped) {
		return SubmitDecision::StopRendering;
	}

	if (compositorError == obvr::vr::openvr::kCompositorErrorNone) {
		m_consecutiveFailures = 0;
		return SubmitDecision::Continue;
	}

	if (!IsRecoverable(compositorError)) {
		m_stopped = true;
		m_stopReason = compositorError;
		return SubmitDecision::StopRendering;
	}

	++m_consecutiveFailures;
	if (m_consecutiveFailures >= kMaxConsecutiveFailures) {
		m_stopped = true;
		m_stopReason = compositorError;
		return SubmitDecision::StopRendering;
	}

	return SubmitDecision::Skip;
}

void SubmitPolicy::Reset() {
	m_consecutiveFailures = 0;
	m_stopped = false;
	m_stopReason = 0;
}

}  // namespace obvr::render

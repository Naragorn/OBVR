#include "render/HeadsetRenderer.h"

#include "core/Log.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::render {

void HeadsetRenderer::Update(const vr::OpenVRBackend& backend) {
	if (m_policy.HasStopped()) {
		return;
	}

	// No compositor means rendering was never asked for, or the scene
	// registration was given back. Either way this is not a failure to report
	// once per frame - the reason was logged where the decision was taken.
	if (!backend.IsSceneApplication()) {
		return;
	}

	if (!m_textures.IsReady()) {
		if (m_setupAttempted) {
			return;
		}
		m_setupAttempted = true;

		UInt32 width = 0;
		UInt32 height = 0;
		if (!backend.GetRecommendedRenderTargetSize(width, height)) {
			OBVR_LOG("Render: the headset did not report a render target size");
			return;
		}

		if (!m_textures.Create(width, height)) {
			// Already logged in detail by EyeTextures. Nothing further is
			// attempted: a machine that cannot make a device this frame will
			// not make one next frame either, and retrying would turn the log
			// into the only thing being rendered.
			return;
		}
	}

	// Blocks until the compositor wants the next frame. From here on Oblivion
	// runs on the compositor's clock.
	const int waited = backend.WaitGetPoses();
	SubmitDecision decision = m_policy.Observe(waited);

	if (decision == SubmitDecision::StopRendering) {
		OBVR_LOG("Render: stopped rendering, WaitGetPoses returned %d%s", waited,
		         waited == vr::openvr::kCompositorErrorDoNotHaveFocus
		             ? " - another application holds the scene, and waiting on that "
		               "throttles the game to 10 Hz"
		             : "");
		m_textures.Destroy();
		return;
	}

	if (decision != SubmitDecision::Continue) {
		return;
	}

	// Left before right, and both before the answers are read. Submitting one
	// eye and then giving up on the other would show the compositor half a
	// frame, which it reprojects into something worse than no frame at all.
	const int left = backend.SubmitEye(vr::openvr::kEyeLeft, m_textures.GetTexture(Eye::Left));
	const int right =
		backend.SubmitEye(vr::openvr::kEyeRight, m_textures.GetTexture(Eye::Right));

	// The worse of the two decides. A fault that affects one eye affects the
	// other next frame, and acting on the good half would mean waiting for
	// the failure to happen twice.
	const int worst = left != vr::openvr::kCompositorErrorNone ? left : right;
	decision = m_policy.Observe(worst);

	if (decision == SubmitDecision::StopRendering) {
		OBVR_LOG("Render: stopped rendering, Submit returned %d (left %d, right %d)", worst,
		         left, right);
		m_textures.Destroy();
	}
}

void HeadsetRenderer::Reset() {
	m_textures.Destroy();
	m_policy.Reset();
	m_setupAttempted = false;
}

}  // namespace obvr::render

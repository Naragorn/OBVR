#include "render/HeadsetRenderer.h"

#include "core/Log.h"
#include "render/EyeGeometry.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::render {
namespace {

// Writes what the headset says about the eyes, once, when rendering starts.
//
// It changes nothing. It is here because the next milestone renders the world
// twice using exactly these numbers, and the sign convention of the frustum
// is not documented anywhere - so the four values are printed raw, as they
// arrive, and the log is what settles what they mean. A guess made now would
// be invisible until a picture came out subtly wrong.
void LogEyeGeometry(const vr::OpenVRBackend& backend) {
	for (int eye = 0; eye < 2; ++eye) {
		const char* name = eye == vr::openvr::kEyeLeft ? "left" : "right";

		EyeProjection projection;
		if (backend.GetEyeProjection(eye, projection.left, projection.right, projection.top,
		                             projection.bottom)) {
			OBVR_LOG("Render: %s eye raw=(l %.4f, r %.4f, t %.4f, b %.4f)", name,
			         static_cast<double>(projection.left),
			         static_cast<double>(projection.right),
			         static_cast<double>(projection.top),
			         static_cast<double>(projection.bottom));
			OBVR_LOG("Render: %s eye fov=(h %.1f deg, v %.1f deg) asymmetry=(h %.4f, v %.4f)",
			         name, static_cast<double>(HorizontalFovDegrees(projection)),
			         static_cast<double>(VerticalFovDegrees(projection)),
			         static_cast<double>(HorizontalAsymmetry(projection)),
			         static_cast<double>(VerticalAsymmetry(projection)));
		} else {
			OBVR_LOG("Render: %s eye reported no usable projection", name);
		}
	}

	NiPoint3 leftEye{0.0f, 0.0f, 0.0f};
	NiPoint3 rightEye{0.0f, 0.0f, 0.0f};
	if (backend.GetEyeOffset(vr::openvr::kEyeLeft, leftEye) &&
	    backend.GetEyeOffset(vr::openvr::kEyeRight, rightEye)) {
		const float ipd = InterpupillaryDistance(leftEye, rightEye);

		// Flagged rather than merely printed. A misread transform is out by a
		// factor, not by millimetres, and it would otherwise show up much
		// later as a world that feels like a doll's house for no stated
		// reason.
		OBVR_LOG("Render: eye offsets left=(%.4f, %.4f, %.4f) right=(%.4f, %.4f, %.4f)",
		         static_cast<double>(leftEye.x), static_cast<double>(leftEye.y),
		         static_cast<double>(leftEye.z), static_cast<double>(rightEye.x),
		         static_cast<double>(rightEye.y), static_cast<double>(rightEye.z));
		OBVR_LOG("Render: IPD %.1f mm%s", static_cast<double>(ipd) * 1000.0,
		         IsPlausibleIpd(ipd) ? "" : " - outside the human range, so probably misread");
	} else {
		OBVR_LOG("Render: the headset reported no eye offsets");
	}
}

}  // namespace

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

		LogEyeGeometry(backend);
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

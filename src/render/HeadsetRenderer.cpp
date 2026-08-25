#include "render/HeadsetRenderer.h"

#include "core/Log.h"
#include "render/EyeGeometry.h"
#include "render/GameFrame.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::render {
namespace {

// How much of Oblivion's frame each eye is given, as a fraction of its
// width.
//
// Below 1 because the correction needs room: putting the picture's centre on
// an optical axis that sits at 0.583 across the view means starting the crop
// at a negative coordinate if the whole width is used. 0.8 leaves enough
// margin for the measured axes with room to spare, at the cost of a fifth of
// the horizontal field of view.
//
// A constant rather than a setting, for now. It is only meaningful while a
// mono image is being shared between two eyes, and that arrangement is meant
// to be temporary.
constexpr float kMonoBoundsWidth = 0.8f;

// Writes what the headset says about the eyes, once, when rendering starts,
// and hands back where each eye's optical axis lands across its texture.
//
// The four frustum values are printed raw, as they arrive, because the sign
// convention of top and bottom is documented nowhere - and it turned out not
// to need settling: only the horizontal centre is used, which is unambiguous,
// and 0.1.0 will take a ready made matrix from GetProjectionMatrix rather
// than build one from these. Printing them anyway costs two lines and means
// nobody has to run the game again to see what the headset reported.
void LogEyeGeometry(const vr::OpenVRBackend& backend, float& crossULeft, float& crossURight) {
	for (int eye = 0; eye < 2; ++eye) {
		const char* name = eye == vr::openvr::kEyeLeft ? "left" : "right";

		EyeProjection projection;
		if (backend.GetEyeProjection(eye, projection.left, projection.right, projection.top,
		                             projection.bottom)) {
			// Horizontal only, and only because it is unambiguous: left is
			// the lower edge and right the higher, whatever the signs mean.
			// The vertical equivalent would need a convention that cannot be
			// determined from the numbers, so the cross stays vertically
			// centred rather than guessed.
			float& target = eye == vr::openvr::kEyeLeft ? crossULeft : crossURight;
			target = OpticalCentreU(projection);

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

void HeadsetRenderer::Update(const vr::OpenVRBackend& backend, const FrameRequest& request) {
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

		// The geometry first, so the log reads in the order things happened
		// and so the cross can be put on the optical axis rather than in the
		// middle of the image.
		float crossULeft = 0.5f;
		float crossURight = 0.5f;
		LogEyeGeometry(backend, crossULeft, crossURight);

		// The same two numbers serve twice: they place the cross in the test
		// pattern, and they decide which part of Oblivion's frame each eye is
		// shown. Both are the same question - where is this eye looking.
		const TextureBounds boundsLeft = MonoBounds(crossULeft, kMonoBoundsWidth);
		const TextureBounds boundsRight = MonoBounds(crossURight, kMonoBoundsWidth);
		m_boundsLeft = {boundsLeft.uMin, boundsLeft.vMin, boundsLeft.uMax, boundsLeft.vMax};
		m_boundsRight = {boundsRight.uMin, boundsRight.vMin, boundsRight.uMax,
		                 boundsRight.vMax};
		OBVR_LOG("Render: game frame bounds left u=%.3f..%.3f right u=%.3f..%.3f",
		         static_cast<double>(m_boundsLeft.uMin), static_cast<double>(m_boundsLeft.uMax),
		         static_cast<double>(m_boundsRight.uMin),
		         static_cast<double>(m_boundsRight.uMax));

		if (!m_textures.Create(width, height, crossULeft, crossURight)) {
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

	// Where the picture comes from. Oblivion's own frame is the point of
	// 0.1.0; the generated pattern is what proved the compositor path in
	// 0.0.5 and stays as both the fallback and the thing to compare against.
	int left = vr::openvr::kCompositorErrorNone;
	int right = vr::openvr::kCompositorErrorNone;
	bool submittedGameFrame = false;

	if (request.submitGameFrame) {
		if (!m_gameFrameChecked) {
			m_gameFrameChecked = true;
			m_gameFrameUsable = GetVulkanContext(request.gameDevice, m_vulkan);
			OBVR_LOG("Render: the game frame is %s",
			         m_gameFrameUsable ? "usable, submitting Oblivion's own picture"
			                           : "unavailable, falling back to the test pattern");
		}

		if (m_gameFrameUsable) {
			// Scoped so the bracket closes on every path out, including the
			// ones that throw nothing and simply return. A queue left locked
			// deadlocks Oblivion against its own renderer, and the symptom is
			// a frozen game with an empty log.
			GameFrame frame;
			if (frame.Acquire(request.gameDevice)) {
				dxvk::VRVulkanTextureData data{};
				DescribeForOpenVR(frame.GetImage(), m_vulkan, data);

				// The same image to both eyes. This is mono - Oblivion's
				// picture, flat, with no depth between the eyes - and that is
				// the whole intent of this step. Rendering the world twice is
				// a separate problem, and doing both at once would mean a
				// failure with two possible causes.
				// Different bounds per eye, or the two disagree about where
				// the picture is. The optical axes sit at different places
				// across each eye's view, so the whole texture in both puts
				// Oblivion's centre somewhere neither eye is looking.
				if (request.alternateEyes) {
					// This frame was drawn from one eye's position, so it
					// belongs to that eye and nowhere else. Giving it to both
					// would show each eye the other's viewpoint half the time,
					// which is worse than no depth at all.
					//
					// What the other eye shows meanwhile is the compositor's
					// business. It keeps the last texture submitted for an eye,
					// and each eye here gets a new one every second frame -
					// nowhere near the ten frames without any Submit that fade
					// the scene out. Whether it keeps a copy or a reference is
					// NOT established, and it decides whether this works: a
					// reference would point at a back buffer the game has since
					// overwritten, which would show the wrong eye's picture
					// rather than the previous frame's.
					const bool isLeft = request.isLeftEye;
					const int result = backend.SubmitEye(
						isLeft ? vr::openvr::kEyeLeft : vr::openvr::kEyeRight, &data,
						vr::openvr::kTextureTypeVulkan,
						isLeft ? &m_boundsLeft : &m_boundsRight);
					left = result;
					right = result;
				} else {
					left = backend.SubmitEye(vr::openvr::kEyeLeft, &data,
					                         vr::openvr::kTextureTypeVulkan, &m_boundsLeft);
					right = backend.SubmitEye(vr::openvr::kEyeRight, &data,
					                          vr::openvr::kTextureTypeVulkan, &m_boundsRight);
				}
				submittedGameFrame = true;
			}
		}
	}

	// Left before right, and both before the answers are read. Submitting one
	// eye and then giving up on the other would show the compositor half a
	// frame, which it reprojects into something worse than no frame at all.
	if (!submittedGameFrame) {
		// The pattern needs no bounds: it is generated per eye at the full
		// size the headset asked for, so each eye already has its own picture
		// rather than a share of one.
		left = backend.SubmitEye(vr::openvr::kEyeLeft, m_textures.GetTexture(Eye::Left),
		                         vr::openvr::kTextureTypeDirectX, nullptr);
		right = backend.SubmitEye(vr::openvr::kEyeRight, m_textures.GetTexture(Eye::Right),
		                          vr::openvr::kTextureTypeDirectX, nullptr);
	}

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
	m_gameFrameChecked = false;
	m_gameFrameUsable = false;
	m_vulkan = VulkanContext{};
}

}  // namespace obvr::render

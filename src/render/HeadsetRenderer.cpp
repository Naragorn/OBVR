#include "render/HeadsetRenderer.h"

#include "camera/FrameLogic.h"

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
void LogEyeGeometry(const vr::OpenVRBackend& backend, EyeProjection& outLeft,
                    EyeProjection& outRight, float& crossULeft, float& crossURight) {
	for (int eye = 0; eye < 2; ++eye) {
		const char* name = eye == vr::openvr::kEyeLeft ? "left" : "right";

		EyeProjection& projection = eye == vr::openvr::kEyeLeft ? outLeft : outRight;
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

bool HeadsetRenderer::BeginFrame(vr::OpenVRBackend& backend) {
	// A frame that was opened and never submitted is not an error - the
	// compositor treats it as a dropped frame - but a frame submitted without
	// having been opened is, because Submit is only meaningful after
	// WaitGetPoses. Clearing this first means every path out of here that
	// returns false leaves EndFrame with nothing to do.
	m_frameOpen = false;

	if (m_policy.HasStopped()) {
		return false;
	}

	// No compositor means rendering was never asked for, or the scene
	// registration was given back. Either way this is not a failure to report
	// once per frame - the reason was logged where the decision was taken.
	if (!backend.IsSceneApplication()) {
		return false;
	}
	if (!m_textures.IsReady()) {
		if (m_setupAttempted) {
			return false;
		}
		m_setupAttempted = true;

		UInt32 width = 0;
		UInt32 height = 0;
		if (!backend.GetRecommendedRenderTargetSize(width, height)) {
			OBVR_LOG("Render: the headset did not report a render target size");
			return false;
		}

		// The geometry first, so the log reads in the order things happened
		// and so the cross can be put on the optical axis rather than in the
		// middle of the image.
		float crossULeft = 0.5f;
		float crossURight = 0.5f;
		LogEyeGeometry(backend, m_leftEye, m_rightEye, crossULeft, crossURight);
		m_eyeWidth = width;
		m_eyeHeight = height;

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
			return false;
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
		return false;
	}

	if (decision != SubmitDecision::Continue) {
		return false;
	}

	// From here a frame is owed to the compositor. EndFrame is what pays it.
	m_frameOpen = true;
	return true;
}

void HeadsetRenderer::EndFrame(const vr::OpenVRBackend& backend, const FrameRequest& request) {
	// Present can run when the camera hook did not - menus and loading
	// screens draw without a camera pass. Submitting then would be a Submit
	// with no WaitGetPoses in front of it, which is not merely wasteful but
	// out of order.
	if (!m_frameOpen) {
		return;
	}
	m_frameOpen = false;

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
			submittedGameFrame = (request.alternateEyes || request.flatFrame)
			                         ? SubmitAlternateEyes(backend, request, left, right)
			                         : SubmitMono(backend, request, left, right);
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
	const SubmitDecision decision = m_policy.Observe(worst);

	if (decision == SubmitDecision::StopRendering) {
		OBVR_LOG("Render: stopped rendering, Submit returned %d (left %d, right %d)", worst,
		         left, right);
		m_textures.Destroy();
	}
}




bool HeadsetRenderer::GetHeadsetFrustum(float& tanHalfWidth, float& tanHalfHeight) const {
	if (m_eyeWidth == 0) {
		return false;
	}

	const auto widest = [](float a, float b, float c, float d) {
		const float aa = a < 0.0f ? -a : a;
		const float bb = b < 0.0f ? -b : b;
		const float cc = c < 0.0f ? -c : c;
		const float dd = d < 0.0f ? -d : d;
		float best = aa;
		if (bb > best) best = bb;
		if (cc > best) best = cc;
		if (dd > best) best = dd;
		return best;
	};

	// The furthest either eye looks, in each direction. Anything less leaves
	// one eye seeing past the edge of the picture.
	const float w =
		widest(m_leftEye.left, m_leftEye.right, m_rightEye.left, m_rightEye.right);
	const float h =
		widest(m_leftEye.top, m_leftEye.bottom, m_rightEye.top, m_rightEye.bottom);

	if (!(w > 0.0f) || !(h > 0.0f)) {
		return false;
	}

	tanHalfWidth = w;
	tanHalfHeight = h;
	return true;
}

bool HeadsetRenderer::SubmitMono(const vr::OpenVRBackend& backend, const FrameRequest& request,
                                 int& left, int& right) {
	// Scoped so the bracket closes on every path out, including the ones that
	// throw nothing and simply return. A queue left locked deadlocks Oblivion
	// against its own renderer, and the symptom is a frozen game with an empty
	// log.
	GameFrame frame;
	if (!frame.Acquire(request.gameDevice)) {
		return false;
	}

	dxvk::VRVulkanTextureData data{};
	DescribeForOpenVR(frame.GetImage(), m_vulkan, data);

	// The same image to both eyes. This is mono - Oblivion's picture, flat,
	// with no depth between the eyes.
	//
	// Different bounds per eye even so, or the two disagree about where the
	// picture is: the optical axes sit at different places across each eye's
	// view, so the whole texture in both would put Oblivion's centre somewhere
	// neither eye is looking.
	left = backend.SubmitEye(vr::openvr::kEyeLeft, &data, vr::openvr::kTextureTypeVulkan,
	                         &m_boundsLeft);
	right = backend.SubmitEye(vr::openvr::kEyeRight, &data, vr::openvr::kTextureTypeVulkan,
	                          &m_boundsRight);
	return true;
}

bool HeadsetRenderer::SubmitAlternateEyes(const vr::OpenVRBackend& backend,
                                          const FrameRequest& request, int& left, int& right) {
	// Rebuilt when the camera frustum turns up for the first time.
	//
	// The mirror may have been built during a menu, where no camera has run
	// and the placement came from a fallback. Keeping that would throw away
	// the one measurement that made the world the right size.
	const bool haveCamera = request.cameraTanHalfWidth > 0.0f;
	if (m_mirrorChecked && m_mirrorUsable && !m_mirrorUsedCamera && haveCamera) {
		OBVR_LOG("Render: the camera frustum arrived, so the eye copies are being rebuilt "
		         "with it");
		m_mirrorChecked = false;
		m_mirror.Destroy();
	}

	if (!m_mirrorChecked) {
		m_mirrorChecked = true;
		m_mirrorUsable = m_mirror.Create(request.gameDevice, m_eyeWidth, m_eyeHeight,
		                                 m_leftEye, m_rightEye, request.gameFovDegrees,
		                                 request.gameFovIsFor4x3, request.cameraTanHalfWidth,
		                                 request.cameraTanHalfHeight, request.menuScale);
		m_mirrorUsedCamera = haveCamera;
		OBVR_LOG("Render: alternate eyes are %s",
		         m_mirrorUsable ? "on, each eye holding its own last picture and its own pose"
		                        : "unavailable, falling back to one image for both eyes");
	}

	if (!m_mirrorUsable) {
		// Not a failure to report per frame, and not a reason to stop
		// rendering either. The mono path still works, so the wearer keeps a
		// picture and the log has already said why it is flat.
		return SubmitMono(backend, request, left, right);
	}

	if (request.flatFrame) {
		// No camera ran, so there are no eyes and no poses: the same picture
		// to both, with nothing claimed about where the head was. A menu has
		// no viewpoint to be wrong about, and asserting one would have the
		// compositor warp a flat image as the head moved.
		if (!m_mirror.CopyBackBuffer(request.gameDevice, true, true)) {
			return false;
		}

		EyeMirror::Submission flatHeld(m_mirror, request.gameDevice);
		if (!flatHeld.IsHeld()) {
			return false;
		}

		dxvk::VRVulkanTextureData flatLeft{};
		dxvk::VRVulkanTextureData flatRight{};
		DescribeForOpenVR(m_mirror.GetImage(true), m_vulkan, flatLeft);
		DescribeForOpenVR(m_mirror.GetImage(false), m_vulkan, flatRight);

		// The pose held from when the flat picture first appeared, not this
		// frame's.
		//
		// Handing over the current pose every frame is what made menus stick
		// to the face: the compositor was told the picture had been drawn from
		// exactly where the head is now, so there was nothing for it to
		// correct and the image rode along. Freezing the pose tells it the
		// truth instead - this picture was drawn from over there - and it
		// reprojects accordingly, which leaves the menu hanging in the room
		// with black where the head has turned away from it.
		if (!m_flatPoseValid) {
			m_flatPoseValid = backend.GetRenderPoseMatrix(m_flatPose);
		}

		left = backend.SubmitEye(vr::openvr::kEyeLeft, &flatLeft,
		                         vr::openvr::kTextureTypeVulkan, nullptr,
		                         m_flatPoseValid ? &m_flatPose : nullptr);
		right = backend.SubmitEye(vr::openvr::kEyeRight, &flatRight,
		                          vr::openvr::kTextureTypeVulkan, nullptr,
		                          m_flatPoseValid ? &m_flatPose : nullptr);
		return true;
	}

	// Back in the world, so the next flat picture gets a fresh anchor.
	m_flatPoseValid = false;


	// Which eye the picture in the back buffer actually belongs to, which
	// depends on where this is being called from - see backBufferIsThisFrame.

	//
	// This runs from the camera hook, which fires while the camera is being
	// computed - before the frame is drawn, not after. So the back buffer
	// still holds the previous frame, drawn from the previous frame's camera
	// position, which under alternate eyes is the other eye.
	//
	// Copying it into request.isLeftEye therefore gave each eye the other
	// eye's viewpoint, every frame. That is not merely a lost depth cue: it
	// is stereo with the disparity inverted, which the eyes cannot fuse, and
	// it was reported from the headset as an unstable, flickering picture
	// that got worse when the head moved sideways. Exactly the failure
	// frame_logic_test warns about, arriving through the one route that test
	// cannot see.
	//
	// !isLeftEye rather than IsLeftEyeFrame(frameCount - 1) because the two
	// are the same thing for an alternation of two, including where the frame
	// counter wraps.
	const bool backBufferEye =
		camera::BackBufferEyeIsLeft(request.isLeftEye, request.backBufferIsThisFrame);

	// The copy first, with no queue held - StretchRect puts work on the very
	// queue the bracket is about to lock, so doing it inside the bracket would
	// be asking DXVK to submit while OBVR holds its submission queue.
	if (!m_mirror.CopyBackBuffer(request.gameDevice, backBufferEye)) {
		// Said once rather than never. The whole reason the first attempt at
		// alternate eyes cost a session was that nothing failed out loud - so
		// a path that quietly falls back to the test pattern gets a line, and
		// gets it exactly once so the log stays readable.
		if (!m_copyFailureLogged) {
			m_copyFailureLogged = true;
			OBVR_LOG("Render: the frame could not be copied into the %s eye, so the test "
			         "pattern is showing instead",
			         backBufferEye ? "left" : "right");
		}
		return false;
	}

	// Remember the pose this picture was drawn with, for the eye it went to.
	//
	// The other eye keeps the pose from its own last turn, a frame ago, which
	// is precisely the fact the compositor cannot infer and has to be handed.
	vr::openvr::HmdMatrix34 pose{};
	if (backend.GetRenderPoseMatrix(pose)) {
		const int index = backBufferEye ? 0 : 1;
		m_eyePose[index] = pose;
		m_eyePoseValid[index] = true;

		// The first copy fills both eyes, so both were drawn with this pose.
		// Without this the eye that has never had a turn would be submitted
		// with no pose at all and fall back to the compositor's assumption.
		const int other = 1 - index;
		if (!m_eyePoseValid[other]) {
			m_eyePose[other] = pose;
			m_eyePoseValid[other] = true;
		}
	}

	EyeMirror::Submission held(m_mirror, request.gameDevice);
	if (!held.IsHeld()) {
		return false;
	}

	// Both eyes, every frame, and that is the entire point of this path. The
	// eye whose turn it was carries the picture just copied; the other carries
	// its own last one. A single eye returns success and is not a frame -
	// measured, in docs/verification/OBVR-aer-refused.log, where nothing
	// failed and the scene faded out anyway.
	dxvk::VRVulkanTextureData dataLeft{};
	dxvk::VRVulkanTextureData dataRight{};
	DescribeForOpenVR(m_mirror.GetImage(true), m_vulkan, dataLeft);
	DescribeForOpenVR(m_mirror.GetImage(false), m_vulkan, dataRight);

	// The whole texture, with no bounds at all. An eye texture here already
	// covers exactly that eye's frustum - the game's picture was placed inside
	// it at the right angular size, with black around it - so cropping the
	// texture as well would magnify what has just been carefully made
	// life-sized.
	left = backend.SubmitEye(vr::openvr::kEyeLeft, &dataLeft, vr::openvr::kTextureTypeVulkan,
	                         nullptr, m_eyePoseValid[0] ? &m_eyePose[0] : nullptr);
	right = backend.SubmitEye(vr::openvr::kEyeRight, &dataRight,
	                          vr::openvr::kTextureTypeVulkan, nullptr,
	                          m_eyePoseValid[1] ? &m_eyePose[1] : nullptr);
	return true;
}
void HeadsetRenderer::Reset() {
	m_textures.Destroy();
	m_policy.Reset();
	m_setupAttempted = false;
	m_frameOpen = false;
	m_gameFrameChecked = false;
	m_gameFrameUsable = false;
	m_mirror.Destroy();
	m_mirrorChecked = false;
	m_mirrorUsable = false;
	m_mirrorUsedCamera = false;
	m_copyFailureLogged = false;
	m_eyePoseValid[0] = false;
	m_eyePoseValid[1] = false;
	m_flatPoseValid = false;
	m_eyeWidth = 0;
	m_eyeHeight = 0;
	m_leftEye = EyeProjection{};
	m_rightEye = EyeProjection{};
	m_vulkan = VulkanContext{};
}

}  // namespace obvr::render

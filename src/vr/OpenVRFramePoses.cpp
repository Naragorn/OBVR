// The frame's poses: WaitGetPoses, and the poses predicted without waiting
// when the wait comes late ([Render] LatePoseWait, vr/PosePrediction.h).
// Moved out of OpenVRBackend.cpp (2026-11-20), which keeps the size ratchet.

#include <cmath>

#include "core/Log.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

namespace {

// How far apart two poses are: the positions in millimetres, the rotations in
// degrees (the angle of the one rotation that takes the first to the second).
void PoseDistance(const openvr::HmdMatrix34& a, const openvr::HmdMatrix34& b, double& mm, double& degrees) {
	const double dx = a.m[0][3] - b.m[0][3];
	const double dy = a.m[1][3] - b.m[1][3];
	const double dz = a.m[2][3] - b.m[2][3];
	mm = 1000.0 * std::sqrt(dx * dx + dy * dy + dz * dz);
	// trace(A^T B) = 1 + 2 cos(angle).
	double trace = 0.0;
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			trace += static_cast<double>(a.m[r][c]) * static_cast<double>(b.m[r][c]);
		}
	}
	double cosine = 0.5 * (trace - 1.0);
	cosine = cosine > 1.0 ? 1.0 : (cosine < -1.0 ? -1.0 : cosine);
	degrees = std::acos(cosine) * 180.0 / 3.14159265358979323846;
}

}  // namespace

int OpenVRBackend::WaitGetPoses(bool keepPredicted) {
	// LatePoseWait: the frame's poses were predicted before this wait and the
	// picture is drawn with them; the wait keeps them and fills its own array.
	const bool late = keepPredicted && m_posesPredicted;
	if (!late) {
		m_posesPredicted = false;
		m_renderPoseValid = false;
		m_framePosesValid = false;
	}

	if (m_compositor == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	auto* table = static_cast<openvr::IVRCompositorFnTable*>(m_compositor);
	if (table->WaitGetPoses == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	// Every device, into the backend's own array (not the stack): the head at
	// index 0, and the controllers, predicted to the same moment - the hands
	// drawn with them (vr/HandPoseSource.h). The game poses are declined:
	// Oblivion's logic knows nothing about a headset.
	openvr::TrackedDevicePose* const target = late ? m_latePoses : m_framePoses;
	const int result = table->WaitGetPoses(target, openvr::kMaxTrackedDeviceCount, nullptr, 0);
	if (!late) {
		m_framePosesValid = result == openvr::kCompositorErrorNone;
	}
	const openvr::TrackedDevicePose& renderPose = target[0];

	// On change of answer, not on a budget: a run says once what the first
	// frames returned and again at every transition, which is where the
	// frame the pose becomes usable on is found. ETrackingResult 200 is
	// Running_OK; the compositor codes are in the header's comment.
	if (!m_waitAnswerLogged || result != m_lastWaitResult ||
	    renderPose.poseIsValid != m_lastPoseValid ||
	    renderPose.deviceIsConnected != m_lastDeviceConnected ||
	    renderPose.trackingResult != m_lastTrackingResult) {
		m_waitAnswerLogged = true;
		m_lastWaitResult = result;
		m_lastPoseValid = renderPose.poseIsValid;
		m_lastDeviceConnected = renderPose.deviceIsConnected;
		m_lastTrackingResult = renderPose.trackingResult;
		OBVR_LOG("Poses: WaitGetPoses now returns %d%s, HMD pose valid=%d connected=%d "
		         "tracking result=%d%s",
		         result,
		         result == openvr::kCompositorErrorNone
		             ? " (ok)"
		             : (result == openvr::kCompositorErrorDoNotHaveFocus
		                    ? " (another application holds the scene focus)"
		                    : (result == openvr::kCompositorErrorIsNotSceneApplication
		                           ? " (not registered as a scene application)"
		                           : "")),
		         renderPose.poseIsValid ? 1 : 0, renderPose.deviceIsConnected ? 1 : 0,
		         renderPose.trackingResult,
		         renderPose.trackingResult == 200 ? " (running ok)" : "");
	}

	if (late) {
		NoteLatePose(result);
		return result;
	}

	// Kept, not discarded. This is the pose the compositor will reproject the
	// submitted picture against, so it is the pose the picture has to be drawn
	// with - see the comment on the declaration, and Valve's own statement
	// that rendering with any other pose "will result in incorrect behavior".
	//
	// It is also better in its own right: predicted forward to when the image
	// will be lit, rather than the head's position at the moment of asking.
	if (renderPose.poseIsValid && renderPose.deviceIsConnected) {
		m_renderOrientation = FromOpenVRMatrix(renderPose.deviceToAbsoluteTracking.m);
		m_renderPosition = PositionFromOpenVRMatrix(renderPose.deviceToAbsoluteTracking.m);
		m_renderPoseMatrix = renderPose.deviceToAbsoluteTracking;
		m_renderPoseValid = true;
	}

	return result;
}

bool OpenVRBackend::PredictFramePoses(float secondsToPhotons) {
	m_posesPredicted = false;
	m_renderPoseValid = false;
	m_framePosesValid = false;
	if (m_system == nullptr) {
		return false;
	}
	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetDeviceToAbsoluteTrackingPose == nullptr) {
		return false;
	}
	// The seated universe, the one the compositor's WaitGetPoses answers in
	// (SetTrackingSpace at the start).
	table->GetDeviceToAbsoluteTrackingPose(openvr::kTrackingUniverseSeated, secondsToPhotons, m_framePoses,
	                                       openvr::kMaxTrackedDeviceCount);
	// As WaitGetPoses: the frame goes on with a head not tracked (the
	// tracker then falls back as it always did); only a valid one is kept.
	m_framePosesValid = true;
	m_posesPredicted = true;
	const openvr::TrackedDevicePose& head = m_framePoses[0];
	if (head.poseIsValid && head.deviceIsConnected) {
		m_renderOrientation = FromOpenVRMatrix(head.deviceToAbsoluteTracking.m);
		m_renderPosition = PositionFromOpenVRMatrix(head.deviceToAbsoluteTracking.m);
		m_renderPoseMatrix = head.deviceToAbsoluteTracking;
		m_renderPoseValid = true;
	}
	return true;
}

float OpenVRBackend::SecondsFromVsyncToPhotons() const {
	if (m_vsyncToPhotons > -2.0f) {
		return m_vsyncToPhotons;
	}
	m_vsyncToPhotons = -1.0f;
	if (m_system == nullptr) {
		return m_vsyncToPhotons;
	}
	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	int error = 0;
	float seconds = -1.0f;
	if (table->GetFloatTrackedDeviceProperty != nullptr) {
		seconds = table->GetFloatTrackedDeviceProperty(openvr::kTrackedDeviceIndexHmd,
		                                               openvr::kPropSecondsFromVsyncToPhotons, &error);
	}
	if (error == 0 && seconds >= 0.0f && seconds < 0.1f) {
		m_vsyncToPhotons = seconds;
	}
	OBVR_LOG("OpenVR: the display's vsync-to-photons %s %.2f ms (property error %d) - the late pose wait predicts "
	         "the poses that far past the frame's vsync",
	         m_vsyncToPhotons >= 0.0f ? "is" : "is not said; taken as", static_cast<double>(
		         m_vsyncToPhotons >= 0.0f ? m_vsyncToPhotons * 1000.0f : 0.0f), error);
	return m_vsyncToPhotons;
}

// How far the head predicted before the wait was from the pose the wait gave:
// the first five frames, then one in 900, with the worst so far.
void OpenVRBackend::NoteLatePose(int result) {
	const openvr::TrackedDevicePose& predicted = m_framePoses[0];
	const openvr::TrackedDevicePose& waited = m_latePoses[0];
	if (result != openvr::kCompositorErrorNone || !predicted.poseIsValid || !waited.poseIsValid) {
		return;
	}
	double mm = 0.0;
	double degrees = 0.0;
	PoseDistance(predicted.deviceToAbsoluteTracking, waited.deviceToAbsoluteTracking, mm, degrees);
	m_lateWorstMm = mm > m_lateWorstMm ? mm : m_lateWorstMm;
	m_lateWorstDegrees = degrees > m_lateWorstDegrees ? degrees : m_lateWorstDegrees;
	if (m_latePoseReports < 5 || m_latePoseReports % 900 == 0) {
		OBVR_LOG("Poses: the late wait's head was %.1f mm and %.2f degrees from the one predicted before it, the "
		         "frame drawn and submitted with the predicted one (the worst so far %.1f mm, %.2f degrees)",
		         mm, degrees, m_lateWorstMm, m_lateWorstDegrees);
	}
	++m_latePoseReports;
}

}  // namespace obvr::vr

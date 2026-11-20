// The controllers' poses for a frame (vr/HandPoseSource.h): this frame's
// render poses from WaitGetPoses when the head has one, else the pose at the
// moment of asking. Methods of OpenVRBackend, apart from its file.

#include "core/Log.h"
#include "vr/HandPoseSource.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
namespace {

// How often, in hand reads, the frame pose's lead over the moment of asking
// may be measured for the log: about every three seconds at 90 Hz, two hands.
constexpr UInt32 kHandLeadEveryReads = 540;

NiPoint3 PoseTranslation(const openvr::TrackedDevicePose& pose) {
	const openvr::HmdMatrix34& m = pose.deviceToAbsoluteTracking;
	return NiPoint3{m.m[0][3], m.m[1][3], m.m[2][3]};
}

}  // namespace

bool OpenVRBackend::FrameHandPose(bool rightHand, UInt32 device, openvr::TrackedDevicePose& out) const {
	HandPoseChoice choice;
	choice.predict = m_predictHands;
	choice.headFromFrame = m_renderPoseValid;
	choice.frameHasDevices = m_framePosesValid;
	choice.device = device;
	choice.deviceCount = openvr::kMaxTrackedDeviceCount;
	choice.frameValid = device < openvr::kMaxTrackedDeviceCount && m_framePoses[device].poseIsValid &&
	                    m_framePoses[device].deviceIsConnected;
	const bool frame = ChooseHandPose(choice) == HandPoseSource::Frame;
	NoteHandPoseSource(rightHand, device, frame);
	if (frame) {
		out = m_framePoses[device];
	}
	return frame;
}

bool OpenVRBackend::HandDevicePose(bool rightHand, UInt32 device, openvr::TrackedDevicePose& out) const {
	if (FrameHandPose(rightHand, device, out)) {
		return true;
	}
	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table == nullptr || table->GetDeviceToAbsoluteTrackingPose == nullptr ||
	    device >= openvr::kMaxTrackedDeviceCount) {
		return false;
	}
	openvr::TrackedDevicePose poses[openvr::kMaxTrackedDeviceCount]{};
	table->GetDeviceToAbsoluteTrackingPose(openvr::kTrackingUniverseSeated, 0.0f, poses,
	                                      openvr::kMaxTrackedDeviceCount);
	out = poses[device];
	return out.poseIsValid && out.deviceIsConnected;
}

void OpenVRBackend::NoteHandPoseSource(bool rightHand, UInt32 device, bool frame) const {
	const char* const hand = rightHand ? "right" : "left";
	SInt32& said = m_handPoseSourceSaid[rightHand ? 0 : 1];
	if (said != (frame ? 1 : 0)) {
		said = frame ? 1 : 0;
		OBVR_LOG("Poses: the %s hand from %s", hand,
		         frame ? "this frame's render pose - predicted to when the frame is lit, as the head"
		         : !m_predictHands ? "its pose at the moment of asking ([Hands] PredictHands=0)"
		                           : "its pose at the moment of asking - this frame has no render pose for it");
	}
	// Now and then, the frame pose against the moment of asking: how far
	// ahead the compositor drew this hand (vr::HandLeadSeconds).
	if (!frame || m_handLeadLinesLeft == 0) {
		return;
	}
	if (m_handLeadCountdown > 0) {
		--m_handLeadCountdown;
		return;
	}
	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table == nullptr || table->GetDeviceToAbsoluteTrackingPose == nullptr) {
		return;
	}
	openvr::TrackedDevicePose poses[openvr::kMaxTrackedDeviceCount]{};
	table->GetDeviceToAbsoluteTrackingPose(openvr::kTrackingUniverseSeated, 0.0f, poses,
	                                      openvr::kMaxTrackedDeviceCount);
	const openvr::TrackedDevicePose& now = poses[device];
	const openvr::TrackedDevicePose& ahead = m_framePoses[device];
	const NiPoint3 velocity{now.velocity.v[0], now.velocity.v[1], now.velocity.v[2]};
	float lead = 0.0f;
	if (!now.poseIsValid || !HandLeadSeconds(PoseTranslation(ahead), PoseTranslation(now), velocity, lead)) {
		return;  // too slow to tell: asked again on the next read
	}
	m_handLeadCountdown = kHandLeadEveryReads;
	--m_handLeadLinesLeft;
	OBVR_LOG("Poses: the %s hand's frame pose %.1f ms ahead of the moment of asking (the hand at %.2f m/s) - what "
	         "the hands trailed the head by before [Hands] PredictHands",
	         hand, static_cast<double>(lead * 1000.0f), static_cast<double>(math::Sqrt(velocity.LengthSquared())));
}

bool OpenVRBackend::HandPoseMatrix(bool rightHand, openvr::HmdMatrix34& out) const {
	const UInt32 device = HandDeviceIndex(rightHand);
	if (device == openvr::kTrackedDeviceIndexInvalid || device >= openvr::kMaxTrackedDeviceCount) {
		return false;
	}
	// The same pose ReadHand takes, so the model drawn and the hand pinned
	// come from one moment.
	openvr::TrackedDevicePose pose{};
	if (!HandDevicePose(rightHand, device, pose)) {
		return false;
	}
	out = pose.deviceToAbsoluteTracking;
	return true;
}

}  // namespace obvr::vr

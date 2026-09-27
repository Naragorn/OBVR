#pragma once

// A scripted hand made into the controller state SteamVR would have given
// (test/HandScript.h): the pose in tracking space, the buttons on the bits the
// action path reports them on. Pure, covered by hand_script_test.

#include "core/MathFns.h"
#include "test/HandScript.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::test {

// The head's heading alone: its yaw about tracking up, without the pitch and
// roll. A script's hands are placed in this frame so that they stay by the
// body whichever way the head is tipped - and a headset lying on a desk is
// tipped every which way.
inline vr::Quaternion HeadingOf(const vr::Quaternion& head) {
	const NiPoint3 forward = vr::ToMatrix(head) * NiPoint3{0.0f, 0.0f, -1.0f};
	const float yaw = math::Atan2(-forward.x, -forward.z);
	return vr::FromAxisAngle(0.0f, 1.0f, 0.0f, yaw * math::kRadiansToDegrees);
}

// Pitch about x, yaw about y, roll about -z (the direction pointed along),
// applied roll first: yaw * pitch * roll.
inline vr::Quaternion ScriptRotation(float pitch, float yaw, float roll) {
	return vr::FromAxisAngle(0.0f, 1.0f, 0.0f, yaw) * vr::FromAxisAngle(1.0f, 0.0f, 0.0f, pitch) *
	       vr::FromAxisAngle(0.0f, 0.0f, 1.0f, roll);
}

inline vr::Quaternion ScriptHeadOrientation(const ScriptHead& head) {
	return ScriptRotation(head.pitch, head.yaw, 0.0f);
}

// The controller as ReadHand gives it: tracked, placed from the head, with
// the action path's bits and axes. The velocity stays zero - a scripted hand
// jumps from place to place, and a jump taken for a speed would be a swing.
inline vr::HandPose ScriptedHandPose(const ScriptHand& hand, const vr::Quaternion& headOrientation,
                                     const NiPoint3& headPosition) {
	vr::HandPose out;
	if (!hand.present) {
		return out;
	}
	const vr::Quaternion heading = HeadingOf(headOrientation);
	out.valid = true;
	out.orientation = (heading * ScriptRotation(hand.pitch, hand.yaw, hand.roll)).Normalized();
	out.position = headPosition + vr::ToMatrix(heading) * hand.position;
	out.trigger = hand.trigger;
	out.gripForce = hand.grip;
	out.thumbX = hand.stickX;
	out.thumbY = hand.stickY;
	out.thumbFromJoystickAxis = true;
	UInt64 bits = 0;
	const auto set = [&bits](bool down, UInt32 button) {
		if (down) {
			bits |= 1ull << button;
		}
	};
	set(hand.a, vr::openvr::kButtonIndexA);
	set(hand.b, vr::openvr::kButtonIndexB);
	set(hand.click, vr::openvr::kButtonIndexJoystick);
	set(hand.trackpad, vr::openvr::kButtonIndexTrackpad);
	set(hand.grip > 0.5f, vr::openvr::kButtonIndexGrip);
	set(hand.trigger > 0.5f, vr::openvr::kButtonTrigger);
	out.buttonsPressed = bits;
	// As the action path reports a controller with every action bound.
	out.actionInput = true;
	out.actionActiveMask = 0x1F;
	// A skeleton that reports every finger at the script's curl.
	out.curlValid = true;
	for (int finger = 0; finger < 5; ++finger) {
		out.curl[finger] = hand.curl;
	}
	return out;
}

}  // namespace obvr::test

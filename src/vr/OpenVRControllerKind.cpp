// OpenVRBackend::HandControllerKind, in a file of its own (OpenVRBackend.cpp
// is held at its length by the size ratchet, tests/function-sizes.txt):
// which controller a hand holds, read once per device from SteamVR, and the
// self-check told of it.

#include "core/Log.h"
#include "core/SelfCheckNotes.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

ControllerKind OpenVRBackend::HandControllerKind(bool rightHand, UInt32 device) const {
	const int hand = rightHand ? 0 : 1;
	if (m_kindDevice[hand] == device && m_kind[hand] != ControllerKind::Unknown) {
		return m_kind[hand];
	}
	// Read once per device in that hand; an unread type is asked again (at
	// most every 90 frames) until SteamVR answers.
	if (m_kindDevice[hand] == device && m_kindRetry[hand] > 0) {
		--m_kindRetry[hand];
		return m_kind[hand];
	}
	m_kindDevice[hand] = device;
	m_kindRetry[hand] = 90;
	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	char type[64] = "";
	int error = 0;
	if (table->GetStringTrackedDeviceProperty != nullptr) {
		const UInt32 length =
			table->GetStringTrackedDeviceProperty(device, openvr::kPropControllerType, type, sizeof(type), &error);
		if (error != 0 || length == 0 || length > sizeof(type)) {
			type[0] = '\0';
		}
		type[sizeof(type) - 1] = '\0';
	}
	m_kind[hand] = ControllerKindFromType(type);
	if (m_kind[hand] != ControllerKind::Unknown) {
		// The self-check's controller line (core/SelfCheckNotes.h), and once
		// the headset's model and maker beside it: Index controllers on a
		// Quest are a streaming app's emulation.
		selfcheck::NoteController(rightHand, type);
		if (!m_headsetNoted && table->GetStringTrackedDeviceProperty != nullptr) {
			m_headsetNoted = true;
			char model[128] = "";
			char maker[128] = "";
			int modelError = 0;
			int makerError = 0;
			table->GetStringTrackedDeviceProperty(openvr::kTrackedDeviceIndexHmd, openvr::kPropModelNumber, model,
			                                      sizeof(model), &modelError);
			table->GetStringTrackedDeviceProperty(openvr::kTrackedDeviceIndexHmd, openvr::kPropManufacturerName, maker,
			                                      sizeof(maker), &makerError);
			model[modelError != 0 ? 0 : sizeof(model) - 1] = '\0';
			maker[makerError != 0 ? 0 : sizeof(maker) - 1] = '\0';
			OBVR_LOG("OpenVR: the headset is \"%s\" by \"%s\"", model, maker);
			selfcheck::NoteHeadset(model, maker);
		}
		OBVR_LOG("OpenVR: the %s hand holds a \"%s\" controller - %s", rightHand ? "right" : "left", type,
		         m_kind[hand] == ControllerKind::Index ? "an Index: its ring on the trackpad held"
		                                               : "its ring on the stick's click");
	}
	return m_kind[hand];
}

}  // namespace obvr::vr

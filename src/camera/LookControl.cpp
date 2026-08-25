#include "camera/LookControl.h"

#include "core/Rotation.h"

namespace obvr::camera {

void LookControl::Configure(const LookSettings& settings) { m_settings = settings; }

void LookControl::Reset() {
	m_verticalOffset = 0.0f;
	m_hasHeading = false;
}

void LookControl::Update(const NiMatrix33& vanillaRotation, bool isThirdPerson,
                         float deltaSeconds) {
	Heading target;

	// Switched off, or a camera whose heading cannot be read because it has
	// been rolled onto its side. Either way the game's own rotation is left
	// exactly as it is - a scripted sequence that rolls the camera is doing so
	// on purpose, and second-guessing it would be worse than standing aside.
	if (!m_settings.blockVerticalLook || !HeadingOf(vanillaRotation, target)) {
		m_rotation = vanillaRotation;
		m_verticalOffset = 0.0f;
		m_hasHeading = false;
		return;
	}

	if (m_settings.smoothTurning && m_hasHeading) {
		m_heading = Approach(m_heading, target, m_settings.turnSpeed, deltaSeconds);
	} else {
		// The first frame after this becomes active has nothing to ease from.
		// Starting at the game's heading rather than at whatever was stored
		// keeps a load or a change of view from swinging the camera round.
		m_heading = target;
	}
	m_hasHeading = true;

	m_rotation = RotationFromHeading(m_heading);

	// The tilt the stick asked for, now that the rotation no longer carries
	// it. In third person it becomes height; in first person it is dropped,
	// which is what makes the vertical look keys do nothing there.
	const float targetOffset =
		isThirdPerson ? SinPitchOf(vanillaRotation) * m_settings.verticalLookRange : 0.0f;

	m_verticalOffset =
		m_settings.smoothVerticalLook
			? obvr::Approach(m_verticalOffset, targetOffset, m_settings.verticalLookSpeed,
			                 deltaSeconds)
			: targetOffset;
}

}  // namespace obvr::camera

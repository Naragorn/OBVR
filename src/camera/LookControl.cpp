#include "camera/LookControl.h"

#include "core/MathFns.h"
#include "core/Rotation.h"

namespace obvr::camera {

void LookControl::Configure(const LookSettings& settings) { m_settings = settings; }

void LookControl::Reset() {
	m_verticalOffset = 0.0f;
	m_hasHeading = false;
}

void LookControl::Update(const NiMatrix33& vanillaRotation, bool isThirdPerson,
                         float deltaSeconds, float aimPitchShare) {
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
	//
	// Up and down get their own range because the camera starts at head
	// height rather than halfway along its travel - see LookSettings. The
	// tilt is positive looking up, so it picks the range and carries the sign
	// with it: a positive down range with a negative tilt still lowers the
	// camera.
	//
	// Less whatever of the tilt is OBVR's own aim rather than the stick's.
	// The camera's pitch is positive up and the share is in the engine's
	// convention, positive down, so taking it out adds it - the same sum
	// AimTiltCorrection makes for the camera's position.
	const float tilt = aimPitchShare == 0.0f
	                       ? SinPitchOf(vanillaRotation)
	                       : math::Sin(math::Asin(SinPitchOf(vanillaRotation)) + aimPitchShare);
	const float range =
		tilt >= 0.0f ? m_settings.verticalLookUpRange : m_settings.verticalLookDownRange;
	const float targetOffset = isThirdPerson ? tilt * range : 0.0f;

	m_verticalOffset =
		m_settings.smoothVerticalLook
			? obvr::Approach(m_verticalOffset, targetOffset, m_settings.verticalLookSpeed,
			                 deltaSeconds)
			: targetOffset;
}

}  // namespace obvr::camera

#include "vr/HandMode.h"

#include <utility>

namespace obvr::vr {

// The roles switched since the last frame (the weapon handed to the other
// hand, free-weapons-spec 5.16 step 3): the weapon hand's place jumps from one
// controller to the other in a frame, which is no swing (hand-side-switch-probe
// 2026-11-20: 0.40 m, "a light swing", fatigue charged) - the swings, the
// hands' last places and the drawn weapon's lag start fresh. What a hand holds
// goes with it: its grip's hold and edges, its trigger's edges.
void HandMode::StepRolesSwitch(bool rolesSwapped) {
	if (!StepRolesSeen(m_roles, rolesSwapped)) {
		return;
	}
	m_swing = SwingDetector{};
	m_leftSwing = SwingDetector{};
	m_haveLastRight = false;
	m_haveLastLeft = false;
	m_heavyHold = HeldControl{};
	m_weaponLag = WeaponLagState{};
	std::swap(m_rightHold, m_leftHold);
	std::swap(m_rightGripEdge, m_leftGripEdge);
	std::swap(m_rightTrigger, m_leftTrigger);
	std::swap(m_rightTriggerEdge, m_leftTriggerEdge);
	std::swap(m_rightPointEdge, m_leftPointEdge);
	// The holster's grips go with their hands too: a hand that drew at a place
	// keeps its claim (no grab), and its closing is not a new press.
	std::swap(m_holster.rightGripWas, m_holster.leftGripWas);
	std::swap(m_holster.rightClaimed, m_holster.leftClaimed);
	m_holster.leftTook = false;
}

}  // namespace obvr::vr

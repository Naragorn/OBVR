#pragma once

#include "core/Types.h"

namespace obvr::vr {

// A context boundary invalidates held controls. A hand must produce a
// deliberate neutral sample while tracked before input can be accepted again.
class HandResumeGate {
public:
	void Block() { m_blocked = true; }

	bool Allow(bool tracked, bool neutral) {
		if (!tracked) {
			m_blocked = true;
			return false;
		}
		if (neutral) {
			m_blocked = false;
		}
		return !m_blocked;
	}

	bool Blocked() const { return m_blocked; }

private:
	bool m_blocked = false;
};

// Action input carries provenance. A failed/unavailable action sample is not a
// release: it cannot re-arm a hand after tracking or menu loss.
inline bool HandControlsNeutral(float trigger, UInt64 buttons, float x, float y,
                                float deadZone) {
	const bool centred = (x == 0.0f && y == 0.0f) ||
	                     (x > -deadZone && x < deadZone && y > -deadZone && y < deadZone);
	return trigger <= 0.35f && buttons == 0 && centred;
}

inline bool HandSampleCanRearm(bool actionInput, UInt32 actionActiveMask, SInt32 actionError,
                               unsigned gripActionBit, float trigger, UInt64 buttons, float x,
                               float y, float deadZone) {
	if (actionInput && (actionError != 0 || (actionActiveMask & (1u << gripActionBit)) == 0)) {
		return false;
	}
	return HandControlsNeutral(trigger, buttons, x, y, deadZone);
}

// Context numbering is intentionally pure. It distinguishes gameplay,
// ordinary game menus, OBVR's own menu, and motion mode so a held trigger or
// click cannot cross a presentation boundary.
inline int HandInputContext(bool motion, bool ownMenu, bool gameMenu, bool inWorld,
                            bool firstPerson) {
	return (motion ? 8 : 0) +
	       (ownMenu ? 3 : gameMenu ? 2 : (inWorld && (!motion || firstPerson)) ? 1 : 0);
}

inline bool MotionGameplayAllowed(bool inWorld, bool firstPerson, bool gameMenu, bool ownMenu,
                                  bool motionSpaceValid = true) {
	return inWorld && firstPerson && !gameMenu && !ownMenu && motionSpaceValid;
}

// Releasing a stick requests draw/sheath. Once requested, combat remains
// blocked until the relevant controls are neutral, preventing a context
// transition from turning a held trigger into a fresh attack.
class ReadyCombatGate {
public:
	bool Step(bool requested, bool combatHeld) {
		if (requested) {
			m_blocked = true;
		} else if (!combatHeld) {
			m_blocked = false;
		}
		return m_blocked;
	}

private:
	bool m_blocked = false;
};

}  // namespace obvr::vr
